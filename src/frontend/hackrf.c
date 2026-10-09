/** @file hackrf.c @brief Native libhackrf adapter. No RF transmission or firmware writes. */
#include "frontend/hackrf_adapter.h"
#include "ngene/ring.h"
#include "platform/platform.h"
#include <hackrf.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/** @brief Apply one native vendor setting; clean up the owned source on failure. */
#define SET_HACKRF(call)                                                                           \
    do {                                                                                           \
        code = (call);                                                                             \
        if (code != HACKRF_SUCCESS) {                                                              \
            NgFail(error, "%s: %s", #call, hackrf_error_name((enum hackrf_error)code));            \
            goto failed;                                                                           \
        }                                                                                          \
    } while (0)

/** @brief Native USB/device state and preallocated copied callback ingress. */
typedef struct {
    /** Native device handle or copied device identity; owned for this instance lifetime. */
    hackrf_device *device;
    /** Owned preallocated queue containing copies of vendor USB sample buffers. */
    TNgRing *ingress;
    /** Copied validated configuration; owned by this instance. */
    TNgConfig config;
    /** Atomic callback failure reason; zero means no asynchronous failure. */
    TNgAtomicInt callbackError;
    /** Callback-owned count of received complex pairs. */
    uint64_t received;
    /** Callback-owned count of ingress-overflow losses. */
    uint64_t dropped;
    /** Protects published counters; acquired once per USB transfer. */
    TNgMutex countersMutex;
    /** Tracks mutex initialization for cleanup after partial open failures. */
    int countersInitialized;
    /** Synchronized snapshot of callback received pairs. */
    uint64_t publishedReceived;
    /** Synchronized snapshot of callback ingress losses. */
    uint64_t publishedDropped;
    /** Nonzero after transport start and before a successful stop. */
    int started;
    /** Nonzero when this instance owes a matching hackrf_exit call. */
    int libraryInitialized;
    /** Copied device serial retained after enumeration resources are released. */
    char serial[128];
} TNgHackrfSource;

/**
 * @brief Copy vendor-owned IQ bytes into bounded ingress chunks.
 *
 * @details
 * Vendor callback thread only. Validates even byte count/index bounds, preserves signed CI8
 * bits, updates loss indices and honors stop/drop-newest policy. No allocation, disk/network
 * I/O or vendor API calls.
 *
 * @param transfer Borrowed libhackrf transfer; rx_ctx points to source state.
 *
 * @return Zero for a consumed transfer; -1 to stop reception after invalid data/overflow.
 */
static int ConsumeTransfer(hackrf_transfer *transfer)
{
    TNgHackrfSource *s = transfer->rx_ctx;
    size_t count;
    size_t offset = 0;
    if (transfer->valid_length < 0 || ((transfer->valid_length) & (1)) != 0 ||
        transfer->valid_length > transfer->buffer_length) {
        NgAtomicSet(&s->callbackError, 1);
        return -1;
    }
    count = (size_t)transfer->valid_length / sizeof(TNgIq8);
    if (s->received > UINT64_MAX - count) {
        NgAtomicSet(&s->callbackError, 1);
        return -1;
    }
    while (offset < count) {
        TNgIqView view;
        int result;
        memset(&view, 0, sizeof(view));
        view.samples = (const TNgIq8 *)(const void *)(transfer->buffer + offset * sizeof(TNgIq8));
        view.meta.firstSample = s->received + offset;
        view.meta.count = count - offset;
        if (view.meta.count > s->config.windowSamples) {
            view.meta.count = s->config.windowSamples;
        }
        result = NgRingPush(s->ingress, &view);
        if (result != NG_OK) {
            s->dropped += view.meta.count;
            if (result != NG_FULL || s->config.overflowPolicy == NG_OVERFLOW_STOP) {
                s->dropped += count - offset - view.meta.count;
                s->received += count;
                NgAtomicSet(&s->callbackError, 2);
                return -1;
            }
        }
        offset += view.meta.count;
    }
    s->received += count;
    return 0;
}

/**
 * @brief Consume one USB transfer and publish counters once.
 *
 * @details
 * Vendor-owned sample storage is copied before callback returns. Counter publication is
 * outside the chunk loop; no retained vendor pointer.
 *
 * @param transfer Borrowed libhackrf transfer with valid source rx_ctx.
 *
 * @return Zero to continue reception; nonzero to stop on callback error.
 */
static int OnSamples(hackrf_transfer *transfer)
{
    TNgHackrfSource *s = transfer->rx_ctx;
    int result = ConsumeTransfer(transfer);
    NgMutexLock(&s->countersMutex);
    s->publishedReceived = s->received;
    s->publishedDropped = s->dropped;
    NgMutexUnlock(&s->countersMutex);
    return result;
}

/**
 * @brief Start the HackRF sample source.
 *
 * @details
 * Receiver-thread lifecycle operation. Starts the vendor asynchronous RX callback after
 * ingress allocation; one run per source instance.
 *
 * @param context Owned source context.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR on lifecycle/vendor failure.
 */
static int Start(void *context, TNgError *error)
{
    TNgHackrfSource *s = context;
    int code = hackrf_start_rx(s->device, OnSamples, s);
    if (code != HACKRF_SUCCESS) {
        return NgFail(error, "hackrf_start_rx: %s", hackrf_error_name((enum hackrf_error)code));
    }
    s->started = 1;
    code = hackrf_set_antenna_enable(s->device, (uint8_t)s->config.antennaPower);
    if (code != HACKRF_SUCCESS) {
        return NgFail(error, "HackRF antenna power configuration failed: %s",
                      hackrf_error_name((enum hackrf_error)code));
    }
    return NG_OK;
}

/**
 * @brief Borrow the next HackRF complex sample block.
 *
 * @details
 * One reader/lease at a time. Reads the ingress queue and surfaces callback failures; samples
 * stay valid until release.
 *
 * @param context Owned source context.
 * @param view Receives borrowed view.
 * @param timeoutMs Maximum wait in milliseconds.
 * @param error Optional error destination.
 *
 * @return NG_OK for a borrowed block; NG_TIMEOUT for pacing/empty input; NG_END at EOF;
 * NG_ERROR on lifecycle/transport failure.
 */
static int Acquire(void *context, TNgIqView *view, unsigned timeoutMs, TNgError *error)
{
    TNgHackrfSource *s = context;
    int result;
    int failure;
    failure = NgAtomicGet(&s->callbackError);
    if (failure != 0) {
        return NgFail(error, "HackRF callback stopped: %s",
                      failure == 2 ? "ingress overflow" : "invalid transfer/index");
    }
    result = NgRingAcquire(s->ingress, view, timeoutMs);
    if (result == NG_TIMEOUT && hackrf_is_streaming(s->device) != HACKRF_TRUE) {
        return NgFail(error,
                      "HackRF stream stopped/disconnected; duration of missing RF data is unknown");
    }
    return result;
}

/**
 * @brief Relinquish the HackRF source sample lease.
 *
 * @details
 * Caller must stop using the view before return. No allocation or filesystem/network work.
 *
 * @param context Owned source context with an outstanding lease.
 */
static void Release(void *context)
{
    NgRingRelease(((TNgHackrfSource *)context)->ingress);
}

/**
 * @brief Stop the HackRF source transport.
 *
 * @details
 * Called outside the vendor sample callback. Stops asynchronous RX before final
 * counters/destroy.
 *
 * @param context Owned source context.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR on vendor stop failure.
 */
static int Stop(void *context, TNgError *error)
{
    TNgHackrfSource *s = context;
    int result = HACKRF_SUCCESS;
    if (s->started != 0) {
        result = hackrf_stop_rx(s->device);
        s->started = 0;
    }
    NgRingClose(s->ingress);
    if (result != HACKRF_SUCCESS && result != HACKRF_ERROR_STREAMING_STOPPED) {
        return NgFail(error, "hackrf_stop_rx: %s", hackrf_error_name((enum hackrf_error)result));
    }
    return NG_OK;
}

/**
 * @brief Copy HackRF frontend sample/loss counters.
 *
 * @details
 * Called by receiver thread; returns known software losses, not every RF/USB event. Counters
 * are copied under a publication mutex.
 *
 * @param context Owned source context.
 * @param out Receives copied counters in complex pairs.
 */
static void Stats(void *context, TNgFrontendStats *out)
{
    TNgHackrfSource *s = context;
    NgMutexLock(&s->countersMutex);
    out->receivedSamples = s->publishedReceived;
    out->droppedSamples = s->publishedDropped;
    NgMutexUnlock(&s->countersMutex);
    out->queuedSamples = NgRingPendingSamples(s->ingress);
    out->discardedOnStop = (s->started) != 0 ? 0 : out->queuedSamples;
}

/**
 * @brief Release the HackRF source context after transport stops.
 *
 * @details
 * No outstanding reader view or callback may use the storage. Closes vendor device/library and
 * destroys ingress/counter resources.
 *
 * @param context Owned source context to consume.
 */
static void Destroy(void *context)
{
    TNgHackrfSource *s = context;
    if (s == NULL) {
        return;
    }
    if (s->started != 0) {
        (void)hackrf_stop_rx(s->device);
    }
    if (s->device != NULL) {
        (void)hackrf_close(s->device);
    }
    if (s->libraryInitialized != 0) {
        (void)hackrf_exit();
    }
    NgRingDestroy(s->ingress);
    if (s->countersInitialized != 0) {
        NgMutexDestroy(&s->countersMutex);
    }
    free(s);
}

/** Immutable native operations table; no per-run mutable state. */
static const TNgFrontendOps operations = {Start, Acquire, Release, Stop, Stats, Destroy};

/**
 * @brief Create and configure the native HackRF source adapter.
 *
 * @details
 * Acquires vendor-library/device resources, applies hardware sample rate/tuning/gains and
 * allocates ingress storage. Reception begins only through the frontend start operation.
 * Caller destroys after stop.
 *
 * Parameter config: Validated HackRF configuration.
 * Parameter frontend: Receives owned frontend descriptor on success.
 * Parameter error: Optional device/configuration error destination.
 *
 * Result: NG_OK on success; NG_ERROR for library, USB, configuration or allocation errors.
 */
int NgHackrfOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    TNgHackrfSource *s = calloc(1, sizeof(*s));
    hackrf_device_list_t *list = NULL;
    uint32_t bandwidth;
    int code;
    if (s == NULL) {
        return NgFail(error, "Cannot allocate HackRF adapter");
    }
    if (NgMutexInit(&s->countersMutex) != 0) {
        free(s);
        return NgFail(error, "Cannot initialize counters");
    }
    s->countersInitialized = 1;
    s->config = *config;
    if (NgRingCreate(config->ingressBlocks, config->windowSamples, &s->ingress, error) != NG_OK) {
        Destroy(s);
        return NG_ERROR;
    }
    code = hackrf_init();
    if (code != HACKRF_SUCCESS) {
        NgFail(error, "hackrf_init: %s", hackrf_error_name((enum hackrf_error)code));
        goto failed;
    }
    s->libraryInitialized = 1;
    if (config->serial[0] != '\0') {
        (void)snprintf(s->serial, sizeof(s->serial), "%s", config->serial);
        code = hackrf_open_by_serial(config->serial, &s->device);
    } else {
        list = hackrf_device_list();
        if (list == NULL || list->devicecount != 1) {
            NgFail(
                error,
                "Expected one HackRF; found %d. Use --list-devices and configure frontend.serial",
                list == NULL ? 0 : list->devicecount);
            if (list != NULL) {
                hackrf_device_list_free(list);
            }
            goto failed;
        }
        (void)snprintf(s->serial, sizeof(s->serial), "%s",
                       list->serial_numbers[0] == NULL ? "unavailable" : list->serial_numbers[0]);
        code = hackrf_device_list_open(list, 0, &s->device);
        hackrf_device_list_free(list);
    }
    if (code != HACKRF_SUCCESS) {
        NgFail(error, "Cannot open HackRF: %s", hackrf_error_name((enum hackrf_error)code));
        goto failed;
    }
    bandwidth = config->bandwidthHz == 0
                    ? hackrf_compute_baseband_filter_bw_round_down_lt(config->sampleRateHz)
                    : config->bandwidthHz;
    if (bandwidth == 0 || hackrf_compute_baseband_filter_bw(bandwidth) != bandwidth) {
        NgFail(error, "Unsupported HackRF bandwidth: %u Hz", bandwidth);
        goto failed;
    }
    SET_HACKRF(hackrf_set_sample_rate(s->device, (double)config->sampleRateHz));
    SET_HACKRF(hackrf_set_baseband_filter_bandwidth(s->device, bandwidth));
    SET_HACKRF(hackrf_set_freq(s->device, config->centerFrequencyHz));
    SET_HACKRF(hackrf_set_lna_gain(s->device, config->lnaGainDb));
    SET_HACKRF(hackrf_set_vga_gain(s->device, config->vgaGainDb));
    SET_HACKRF(hackrf_set_amp_enable(s->device, (uint8_t)config->rfAmp));
    frontend->ops = &operations;
    frontend->context = s;
    frontend->info.sampleRateHz = config->sampleRateHz;
    frontend->info.centerFrequencyHz = config->centerFrequencyHz;
    frontend->info.signalFrequencyHz =
        (config->signalFrequencyHz) != 0 ? config->signalFrequencyHz : config->centerFrequencyHz;
    frontend->info.digitalIfHz =
        (int64_t)frontend->info.signalFrequencyHz - (int64_t)config->centerFrequencyHz;
    frontend->info.bandwidthHz = bandwidth;
    frontend->info.format = NG_FORMAT_CI8;
    frontend->info.sourceName = "hackrf";
    frontend->info.clockDomain = "device-no-hardware-timestamp";
    frontend->info.deviceIdentifier = s->serial;
    return NG_OK;
failed:
    Destroy(s);
    return NG_ERROR;
}

/**
 * @brief Enumerate devices with the compiled libhackrf API.
 *
 * @details
 * Internal implementation of NgFrontendListHackrf; owns temporary vendor enumeration resources
 * only.
 *
 * Parameter output: Writable output stream retained by caller.
 * Parameter error: Optional vendor error destination.
 *
 * Result: NG_OK on success; NG_ERROR for initialization/enumeration failures.
 */
int NgHackrfList(FILE *output, TNgError *error)
{
    hackrf_device_list_t *list;
    int i;
    int code = hackrf_init();
    if (code != HACKRF_SUCCESS) {
        return NgFail(error, "hackrf_init: %s", hackrf_error_name((enum hackrf_error)code));
    }
    list = hackrf_device_list();
    if (list == NULL) {
        (void)hackrf_exit();
        return NgFail(error, "Cannot enumerate HackRF devices");
    }
    fprintf(output, "libhackrf %s; devices=%d\n", hackrf_library_version(), list->devicecount);
    for (i = 0; i < list->devicecount; ++i) {
        fprintf(output, "%d serial=%s\n", i,
                list->serial_numbers[i] == NULL ? "unavailable" : list->serial_numbers[i]);
    }
    hackrf_device_list_free(list);
    (void)hackrf_exit();
    return NG_OK;
}
