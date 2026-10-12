/** @file bladerf.c @brief Native SC16 Q11 RX; preserves every ADC bit and timestamp. */
#include "frontend/bladerf_adapter.h"
#include "frontend/ingress.h"
#include <libbladeRF.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/** @brief One RX channel, owned sync workspace and shared NGene ingress. */
typedef struct {
    /** Open device, closed after worker joins and RX disables. */
    struct bladerf *device;
    /** Existing bounded ring and common integrity counters. */
    TNgIngress ingress;
    /** Worker-owned, preallocated SC16 Q11 transfer storage. */
    int16_t *buffer;
    /** Complex pairs requested per native sync call. */
    unsigned transferSamples;
    /** Joined before Destroy frees samples or USB state. */
    TNgThread thread;
    /** Atomic stop predicate. */
    TNgAtomicInt stopping;
    /** Worker/device enable lifecycle guards. */
    int started;
    /** Native RX channel is enabled until Stop disables it. */
    int enabled;
    /** Prevents restart of a closed ingress ring. */
    int hasRun;
    /** Validity and origin of device-relative hardware sample timestamps. */
    int haveTimestamp;
    /** Sample-clock origin and expected next device timestamp. */
    uint64_t origin;
    /** Expected next native sample-clock tick. */
    uint64_t expectedTimestamp;
    /** Copied native serial string; stable for the source lifetime. */
    char serial[128];
} TNgBladerfSource;

/** @brief Open an unambiguous device by serial without starting RX.
 * @param config Desired serial; empty requires exactly one connected bladeRF.
 * @param device Receives owned native device handle.
 * @param serial Receives copied identity in 128 bytes.
 * @param error Optional diagnostic.
 * @return NG_OK on success, NG_ERROR on ambiguity or unavailable FPGA/device.
 */
static int OpenDevice(const TNgConfig *config, struct bladerf **device, char serial[128],
                      TNgError *error)
{
    struct bladerf_devinfo *list = NULL;
    int count = bladerf_get_device_list(&list);
    int index, selected = -1, matches = 0, result;
    if (count < 0) {
        return NgFail(error, "bladeRF enumerate: %s", bladerf_strerror(count));
    }
    for (index = 0; index < count; ++index) {
        if (config->serial[0] == '\0' || strcmp(config->serial, list[index].serial) == 0) {
            selected = index;
            ++matches;
        }
    }
    if (matches != 1) {
        bladerf_free_device_list(list);
        return NgFail(error, "Expected one matching bladeRF, found %d; select its serial", matches);
    }
    (void)snprintf(serial, 128, "%s", list[selected].serial);
    result = bladerf_open_with_devinfo(device, &list[selected]);
    bladerf_free_device_list(list);
    if (result != 0) {
        return NgFail(error, "Cannot open bladeRF: %s", bladerf_strerror(result));
    }
    if (bladerf_is_fpga_configured(*device) != 1) {
        bladerf_close(*device);
        *device = NULL;
        return NgFail(error, "bladeRF FPGA is not configured; install the matching vendor FPGA "
                             "image/autoload configuration");
    }
    return NG_OK;
}

/** @brief Query native scaled ranges after setting frequency, before configuring gain.
 * @param device Open configured native device.
 * @param caps Receives copied supported limits.
 * @param error Optional diagnostic.
 * @return NG_OK on success, NG_ERROR on invalid or unrepresentable vendor bounds.
 */
static int QueryRanges(struct bladerf *device, TNgFrontendCaps *caps, TNgError *error)
{
    const struct bladerf_range *rate = NULL, *frequency = NULL, *gain = NULL;
    double minimum, maximum, step;
    if (bladerf_get_sample_rate_range(device, BLADERF_CHANNEL_RX(0), &rate) != 0 ||
        bladerf_get_frequency_range(device, BLADERF_CHANNEL_RX(0), &frequency) != 0 ||
        bladerf_get_gain_range(device, BLADERF_CHANNEL_RX(0), &gain) != 0 || rate == NULL ||
        frequency == NULL || gain == NULL) {
        return NgFail(error, "Cannot query bladeRF native ranges");
    }
    minimum = (double)rate->min * rate->scale;
    maximum = (double)rate->max * rate->scale;
    if (minimum < 1 || maximum > UINT32_MAX || maximum < minimum) {
        return NgFail(error, "Unsupported bladeRF rate range");
    }
    caps->sampleRateMinHz = (uint32_t)minimum;
    caps->sampleRateMaxHz = (uint32_t)maximum;
    minimum = (double)frequency->min * frequency->scale;
    maximum = (double)frequency->max * frequency->scale;
    if (minimum < 0 || maximum > 6000000000.0 || maximum < minimum) {
        return NgFail(error, "Unsupported bladeRF frequency range");
    }
    caps->frequencyMinHz = (uint64_t)minimum;
    caps->frequencyMaxHz = (uint64_t)maximum;
    minimum = (double)gain->min * gain->scale;
    maximum = (double)gain->max * gain->scale;
    step = (double)gain->step * gain->scale;
    if (minimum < (double)INT_MIN || maximum > (double)INT_MAX || step < 1 ||
        step > (double)INT_MAX) {
        return NgFail(error, "Unsupported bladeRF integer gain range");
    }
    caps->gainMinDb = (int)minimum;
    caps->gainMaxDb = (int)maximum;
    caps->gainStepDb = (int)step;
    caps->format = NG_FORMAT_SC16_Q11;
    return NG_OK;
}

/** @brief Receive contiguous SC16 Q11 blocks with native continuity metadata.
 * @details Native metadata overruns mean a device/library stream discontinuity; they
 * do not establish that the loss happened exclusively inside the RF hardware. Positive
 * timestamp deltas count missing complex pairs exactly. No sample narrowing is performed.
 * @param context Owned started source retained until Stop joins.
 */
static void Receive(void *context)
{
    TNgBladerfSource *source = context;
    while (NgAtomicGet(&source->stopping) == 0 && NgAtomicGet(&source->ingress.failure) == 0) {
        struct bladerf_metadata metadata;
        TNgIqView view = {0};
        uint64_t missing = 0;
        int result;
        memset(&metadata, 0, sizeof(metadata));
        metadata.flags = (uint32_t)BLADERF_META_FLAG_RX_NOW;
        result = bladerf_sync_rx(source->device, source->buffer, source->transferSamples, &metadata,
                                 100);
        if (NgAtomicGet(&source->stopping) != 0) {
            break;
        }
        if (result == BLADERF_ERR_TIMEOUT) {
            continue;
        }
        if (result != 0 || metadata.actual_count == 0 ||
            metadata.actual_count > source->transferSamples ||
            metadata.timestamp > UINT64_MAX - metadata.actual_count) {
            NgIngressTransportError(&source->ingress);
            break;
        }
        if (source->haveTimestamp == 0) {
            source->origin = metadata.timestamp;
            source->expectedTimestamp = metadata.timestamp;
            source->haveTimestamp = 1;
        }
        if (metadata.timestamp < source->expectedTimestamp) {
            NgIngressTransportError(&source->ingress);
            break;
        }
        missing = metadata.timestamp - source->expectedTimestamp;
        view.samples = (const TNgIq8 *)(const void *)source->buffer;
        view.format = NG_FORMAT_SC16_Q11;
        view.meta.count = metadata.actual_count;
        view.meta.firstSample = metadata.timestamp - source->origin;
        view.meta.hardwareTimestamp = metadata.timestamp;
        view.meta.flags = NG_BLOCK_HARDWARE_TIME;
        if (missing != 0 || (metadata.status & BLADERF_META_STATUS_OVERRUN) != 0) {
            view.meta.flags |= NG_BLOCK_GAP;
            view.meta.missingBefore = missing;
            view.meta.missingKnown = missing != 0;
            NgIngressDeviceGap(&source->ingress, missing);
        }
        source->expectedTimestamp = metadata.timestamp + metadata.actual_count;
        if (NgIngressSubmit(&source->ingress, &view) != NG_OK) {
            break;
        }
    }
    NgRingClose(source->ingress.ring);
}

/** @brief Enable the configured RX channel and start its native worker.
 * @param context Open configured source.
 * @param error Optional diagnostic.
 * @return NG_OK on start, NG_ERROR on native/lifecycle failure.
 */
static int Start(void *context, TNgError *error)
{
    TNgBladerfSource *source = context;
    int result;
    if (source->hasRun != 0) {
        return NgFail(error, "bladeRF instance supports one run");
    }
    source->hasRun = 1;
    result = bladerf_enable_module(source->device, BLADERF_CHANNEL_RX(0), true);
    if (result != 0) {
        return NgFail(error, "bladeRF RX enable: %s", bladerf_strerror(result));
    }
    source->enabled = 1;
    if (NgThreadStart(&source->thread, Receive, source) != 0) {
        return NgFail(error, "Cannot start bladeRF RX worker");
    }
    source->started = 1;
    return NG_OK;
}

/** @brief Borrow one SC16 Q11 ingress block.
 * @param context Owned source.
 * @param view Receives native borrowed storage until Release.
 * @param timeoutMs Ring wait timeout, zero for polling.
 * @param error Optional diagnostic.
 * @return NG_OK, NG_TIMEOUT, NG_END, or NG_ERROR for distinct software/transport failure.
 */
static int Acquire(void *context, TNgIqView *view, unsigned timeoutMs, TNgError *error)
{
    TNgBladerfSource *source = context;
    int failure = NgAtomicGet(&source->ingress.failure);
    if (failure != 0) {
        return NgFail(error, "bladeRF stopped: %s",
                      failure == 2 ? "software ingress overflow" : "transport/metadata error");
    }
    return NgRingAcquire(source->ingress.ring, view, timeoutMs);
}

/** @brief Relinquish the current native IQ block.
 * @param context Source with one outstanding lease.
 */
static void Release(void *context)
{
    NgRingRelease(((TNgBladerfSource *)context)->ingress.ring);
}

/** @brief Join the bounded-timeout RX worker, then disable the native RX channel.
 * @param context Owned source.
 * @param error Optional native-stop diagnostic.
 * @return NG_OK after a clean stop, NG_ERROR if native RX disable fails.
 */
static int Stop(void *context, TNgError *error)
{
    TNgBladerfSource *source = context;
    int result = 0;
    NgAtomicSet(&source->stopping, 1);
    if (source->started != 0) {
        NgThreadJoin(source->thread);
        source->started = 0;
    }
    if (source->enabled != 0) {
        result = bladerf_enable_module(source->device, BLADERF_CHANNEL_RX(0), false);
        source->enabled = 0;
    }
    NgRingClose(source->ingress.ring);
    return result == 0 ? NG_OK : NgFail(error, "bladeRF RX disable: %s", bladerf_strerror(result));
}

/** @brief Copy common queue and vendor metadata counters.
 * @param context Owned source.
 * @param stats Receives availability-tagged diagnostics; hardware-only loss remains unsupported.
 */
static void Stats(void *context, TNgFrontendStats *stats)
{
    TNgBladerfSource *source = context;
    NgIngressSnapshot(&source->ingress, source->started == 0, stats);
}

/** @brief Stop, close and release all adapter-owned resources.
 * @param context Owned source, or NULL.
 */
static void Destroy(void *context)
{
    TNgBladerfSource *source = context;
    if (source == NULL) {
        return;
    }
    if (source->started != 0 || source->enabled != 0) {
        (void)Stop(source, NULL);
    }
    if (source->device != NULL) {
        bladerf_close(source->device);
    }
    NgIngressDestroy(&source->ingress);
    free(source->buffer);
    free(source);
}

/** @brief Configure one RX channel using native libbladeRF and SC16 Q11 metadata.
 * Parameter config Validated device settings, queue dimensions and overflow policy.
 * Parameter frontend Receives owned source on success.
 * Parameter error Optional diagnostic.
 * Result: NG_OK on success, NG_ERROR after releasing partial resources on failure.
 */
int NgBladerfOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    static const TNgFrontendOps operations = {Start, Acquire, Release, Stop, Stats, Destroy};
    TNgBladerfSource *source = calloc(1, sizeof(*source));
    TNgFrontendCaps caps = {0};
    unsigned actualRate, actualBandwidth;
    uint64_t actualFrequency;
    int result;
    if (source == NULL) {
        return NgFail(error, "Cannot allocate bladeRF source");
    }
    if (NgIngressInit(&source->ingress, config, NG_FORMAT_SC16_Q11, error) != NG_OK) {
        free(source);
        return NG_ERROR;
    }
    source->transferSamples = 16384u;
    source->buffer = malloc((size_t)source->transferSamples * 4u);
    if (source->buffer == NULL) {
        NgFail(error, "Cannot preallocate bladeRF RX workspace");
        goto failed;
    }
    if (OpenDevice(config, &source->device, source->serial, error) != NG_OK) {
        goto failed;
    }
    result =
        bladerf_set_frequency(source->device, BLADERF_CHANNEL_RX(0), config->centerFrequencyHz);
    if (result != 0 || QueryRanges(source->device, &caps, error) != NG_OK) {
        NgFail(error, "bladeRF tuning/range query failed");
        goto failed;
    }
    if (config->sampleRateHz < caps.sampleRateMinHz ||
        config->sampleRateHz > caps.sampleRateMaxHz || config->bladeGainDb < caps.gainMinDb ||
        config->bladeGainDb > caps.gainMaxDb ||
        (config->bladeGainDb - caps.gainMinDb) % caps.gainStepDb != 0) {
        NgFail(error, "bladeRF rate or gain outside this device's native limits/steps");
        goto failed;
    }
    if (bladerf_set_sample_rate(source->device, BLADERF_CHANNEL_RX(0), config->sampleRateHz,
                                &actualRate) != 0 ||
        bladerf_set_bandwidth(source->device, BLADERF_CHANNEL_RX(0),
                              config->bandwidthHz ? config->bandwidthHz : config->sampleRateHz,
                              &actualBandwidth) != 0 ||
        bladerf_set_gain_mode(source->device, BLADERF_CHANNEL_RX(0), BLADERF_GAIN_MGC) != 0 ||
        bladerf_set_gain(source->device, BLADERF_CHANNEL_RX(0), config->bladeGainDb) != 0 ||
        bladerf_get_frequency(source->device, BLADERF_CHANNEL_RX(0), &actualFrequency) != 0) {
        NgFail(error, "bladeRF rejected rate, bandwidth or manual gain");
        goto failed;
    }
    /* Bias tee is an explicit optional board feature; unsupported boards reject enable. */
    result = bladerf_set_bias_tee(source->device, BLADERF_CHANNEL_RX(0), config->bladeBiasTee != 0);
    if (result != 0 && !(result == BLADERF_ERR_UNSUPPORTED && config->bladeBiasTee == 0)) {
        NgFail(error, "bladeRF bias tee: %s", bladerf_strerror(result));
        goto failed;
    }
    result = bladerf_sync_config(source->device, BLADERF_RX_X1, BLADERF_FORMAT_SC16_Q11_META, 16u,
                                 16384u, 8u, 100u);
    if (result != 0) {
        NgFail(error, "bladeRF native metadata RX configuration: %s", bladerf_strerror(result));
        goto failed;
    }
    source->ingress.config.sampleRateHz = actualRate;
    source->ingress.published.deviceContinuityState = NG_DIAGNOSTIC_VALID;
    frontend->ops = &operations;
    frontend->context = source;
    frontend->info.sampleRateHz = actualRate;
    frontend->info.centerFrequencyHz = actualFrequency;
    frontend->info.signalFrequencyHz =
        config->signalFrequencyHz ? config->signalFrequencyHz : actualFrequency;
    frontend->info.digitalIfHz =
        (int64_t)frontend->info.signalFrequencyHz - (int64_t)actualFrequency;
    frontend->info.bandwidthHz = actualBandwidth;
    frontend->info.format = NG_FORMAT_SC16_Q11;
    frontend->info.sourceName = "bladerf";
    frontend->info.clockDomain = "device-hardware-sample-timestamp";
    frontend->info.deviceIdentifier = source->serial;
    return NG_OK;
failed:
    Destroy(source);
    return NG_ERROR;
}

/** @brief Enumerate native bladeRF devices without starting RX.
 * Parameter output Writable caller-owned stream.
 * Parameter error Optional diagnostic.
 * Result: NG_OK on enumeration (including no devices), NG_ERROR on native failure.
 */
int NgBladerfList(FILE *output, TNgError *error)
{
    struct bladerf_devinfo *list = NULL;
    int count = bladerf_get_device_list(&list), i;
    if (count < 0) {
        return NgFail(error, "bladeRF enumerate: %s", bladerf_strerror(count));
    }
    fprintf(output, "bladeRF devices=%d\n", count);
    for (i = 0; i < count; ++i) {
        fprintf(output, "%d serial=%s USB=%u:%u\n", i, list[i].serial, list[i].usb_bus,
                list[i].usb_addr);
    }
    bladerf_free_device_list(list);
    return ferror(output) ? NgFail(error, "Enumeration write failed") : NG_OK;
}

/** @brief Copy frequency-dependent native gain/rate/tuning ranges while stopped.
 * Parameter config Device serial and desired tuning.
 * Parameter caps Receives copied bounds, no borrowed vendor pointers.
 * Parameter error Optional diagnostic.
 * Result: NG_OK on success, NG_ERROR on device/query failure.
 */
int NgBladerfCapabilities(const TNgConfig *config, TNgFrontendCaps *caps, TNgError *error)
{
    struct bladerf *device = NULL;
    char serial[128];
    int result;
    memset(caps, 0, sizeof(*caps));
    if (OpenDevice(config, &device, serial, error) != NG_OK) {
        return NG_ERROR;
    }
    result = bladerf_set_frequency(device, BLADERF_CHANNEL_RX(0), config->centerFrequencyHz);
    if (result == 0) {
        result = QueryRanges(device, caps, error);
    } else {
        result = NgFail(error, "bladeRF tuning for gain query failed");
    }
    bladerf_close(device);
    return result;
}
