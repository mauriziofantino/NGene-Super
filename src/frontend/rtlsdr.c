/** @file rtlsdr.c @brief Native librtlsdr CU8 acquisition; vendor calls remain in this adapter. */
#include "frontend/rtlsdr_adapter.h"
#include "frontend/ingress.h"
#include <rtl-sdr.h>
#include <stdlib.h>
#include <string.h>

/** @brief RX worker and native device; IQ buffers are always copied before callback return. */
typedef struct {
    /** Owned vendor device, closed only after RX joins. */
    rtlsdr_dev_t *device;
    /** Existing NGene ring and common integrity counters. */
    TNgIngress ingress;
    /** Worker joined by Stop. */
    TNgThread thread;
    /** Atomic shutdown and completion flags. */
    TNgAtomicInt stopping;
    /** Worker completion flag, published after native dispatcher exits. */
    TNgAtomicInt done;
    /** Nonzero after thread creation and before join. */
    int started;
    /** Prevents a second run from reusing closed ingress. */
    int hasRun;
    /** Stable copied serial identity. */
    char serial[128];
} TNgRtlsdrSource;

/** @brief Select an unambiguous serial and open one device without RX.
 * @param config Device serial; empty requires exactly one connected RTL-SDR.
 * @param device Receives owned native handle on success.
 * @param serial Receives NUL-terminated device identity in 128 bytes.
 * @param error Optional diagnostic.
 * @return NG_OK on success, NG_ERROR on ambiguity or native failure.
 */
static int OpenDevice(const TNgConfig *config, rtlsdr_dev_t **device, char serial[128],
                      TNgError *error)
{
    uint32_t count = rtlsdr_get_device_count();
    uint32_t selected = 0;
    uint32_t i;
    unsigned matches = 0;
    char manufacturer[256], product[256], identity[256];
    for (i = 0; i < count; ++i) {
        if (rtlsdr_get_device_usb_strings(i, manufacturer, product, identity) != 0) {
            continue;
        }
        if (config->serial[0] == '\0' || strcmp(config->serial, identity) == 0) {
            selected = i;
            ++matches;
            if (strlen(identity) >= 128) {
                return NgFail(error, "RTL-SDR serial exceeds NGene bounds");
            }
            memcpy(serial, identity, strlen(identity) + 1u);
        }
    }
    if (matches != 1) {
        return NgFail(error, "Expected one matching RTL-SDR, found %u; select its serial", matches);
    }
    if (rtlsdr_open(device, selected) != 0) {
        return NgFail(error, "Cannot open RTL-SDR; check USB access/driver");
    }
    return NG_OK;
}

/** @brief Copy one native CU8 transfer; no DSP, allocation or IO occurs here.
 * @param data Borrowed vendor buffer, valid only during this callback.
 * @param length Even interleaved component byte count.
 * @param context Owned source retained until asynchronous RX joins.
 */
static void OnSamples(unsigned char *data, uint32_t length, void *context)
{
    TNgRtlsdrSource *source = context;
    TNgIqView view = {0};
    if (NgAtomicGet(&source->stopping) != 0 || NgAtomicGet(&source->ingress.failure) != 0) {
        return;
    }
    if (data == NULL || length == 0 || (length & 1u) != 0) {
        NgIngressTransportError(&source->ingress);
        return;
    }
    view.samples = (const TNgIq8 *)(const void *)data;
    view.format = NG_FORMAT_CU8;
    view.meta.firstSample = source->ingress.received;
    view.meta.count = (size_t)length / 2u;
    (void)NgIngressSubmit(&source->ingress, &view);
}

/** @brief Run librtlsdr's blocking asynchronous dispatcher on a private worker.
 * @param context Source retained until Stop joins; GUI threads never call read_async.
 */
static void Receive(void *context)
{
    TNgRtlsdrSource *source = context;
    int result = 0;
    if (NgAtomicGet(&source->stopping) == 0) {
        result = rtlsdr_read_async(source->device, OnSamples, source, 0, 262144u);
    }
    if (NgAtomicGet(&source->stopping) == 0 && NgAtomicGet(&source->ingress.failure) == 0) {
        (void)result;
        NgIngressTransportError(&source->ingress);
    }
    NgRingClose(source->ingress.ring);
    NgAtomicSet(&source->done, 1);
}

/** @brief Start one newly opened source.
 * @param context Owned source.
 * @param error Optional diagnostic.
 * @return NG_OK on worker start, NG_ERROR on lifecycle or USB reset failure.
 */
static int Start(void *context, TNgError *error)
{
    TNgRtlsdrSource *source = context;
    if (source->hasRun != 0 || rtlsdr_reset_buffer(source->device) != 0) {
        return NgFail(error, "Cannot start/reset RTL-SDR source");
    }
    source->hasRun = 1;
    if (NgThreadStart(&source->thread, Receive, source) != 0) {
        return NgFail(error, "Cannot start RTL-SDR worker");
    }
    source->started = 1;
    return NG_OK;
}

/** @brief Acquire one immutable CU8 ingress lease.
 * @param context Started source.
 * @param view Receives borrowed native view until Release.
 * @param timeoutMs Reader timeout, zero for polling.
 * @param error Optional diagnostic.
 * @return NG_OK, NG_TIMEOUT, NG_END, or NG_ERROR with a distinct overflow/transport reason.
 */
static int Acquire(void *context, TNgIqView *view, unsigned timeoutMs, TNgError *error)
{
    TNgRtlsdrSource *source = context;
    int result;
    int failure = NgAtomicGet(&source->ingress.failure);
    if (failure != 0) {
        return NgFail(error, "RTL-SDR stopped: %s",
                      failure == 2 ? "software ingress overflow" : "transport/invalid transfer");
    }
    result = NgRingAcquire(source->ingress.ring, view, timeoutMs);
    /* Error-triggered close during a wait must not become successful EOF. */
    failure = NgAtomicGet(&source->ingress.failure);
    if (result != NG_OK && failure != 0) {
        return NgFail(error, "RTL-SDR stopped: %s",
                      failure == 2 ? "software ingress overflow" : "transport/invalid transfer");
    }
    return result;
}

/** @brief End one borrowed IQ block.
 * @param context Source with an outstanding ingress lease.
 */
static void Release(void *context)
{
    NgRingRelease(((TNgRtlsdrSource *)context)->ingress.ring);
}

/** @brief Cancel RX and join before releasing USB/buffer resources.
 * @details Repeats cancellation until the dispatcher exits: a cancellation before read_async
 * enters its running state can otherwise be lost. No cancellation is done by the GUI or by
 * the ring. Native libusb cancellation is executed outside the sample callback.
 * @param context Owned source.
 * @param error Unused diagnostic destination.
 * @return NG_OK once the RX worker is joined.
 */
static int Stop(void *context, TNgError *error)
{
    TNgRtlsdrSource *source = context;
    (void)error;
    NgAtomicSet(&source->stopping, 1);
    if (source->started != 0) {
        while (NgAtomicGet(&source->done) == 0) {
            (void)rtlsdr_cancel_async(source->device);
            NgSleepMs(1);
        }
        NgThreadJoin(source->thread);
        source->started = 0;
    }
    NgRingClose(source->ingress.ring);
    return NG_OK;
}

/** @brief Copy common queue/transport counters; hardware losses remain unsupported.
 * @param context Owned source.
 * @param stats Receives copied diagnostics, never vendor pointers.
 */
static void Stats(void *context, TNgFrontendStats *stats)
{
    TNgRtlsdrSource *source = context;
    NgIngressSnapshot(&source->ingress, source->started == 0, stats);
}

/** @brief Stop if necessary, then close device and free owned ingress.
 * @param context Owned source or NULL.
 */
static void Destroy(void *context)
{
    TNgRtlsdrSource *source = context;
    if (source == NULL) {
        return;
    }
    if (source->started != 0) {
        (void)Stop(source, NULL);
    }
    if (source->device != NULL) {
        (void)rtlsdr_close(source->device);
    }
    NgIngressDestroy(&source->ingress);
    free(source);
}

/** @brief Validate the selected manual gain against its native discrete list.
 * @param device Open native handle.
 * @param requested Gain in tenth-dB.
 * @return Nonzero for a supported exact gain; zero otherwise.
 */
static int GainSupported(rtlsdr_dev_t *device, int requested)
{
    int gains[64];
    int count = rtlsdr_get_tuner_gains(device, NULL);
    int i;
    if (count <= 0 || count > 64 || rtlsdr_get_tuner_gains(device, gains) != count) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        if (gains[i] == requested) {
            return 1;
        }
    }
    return 0;
}

/** @brief Open native RTL-SDR; reception starts through the shared frontend operations.
 * Parameter config Validated RTL-SDR settings.
 * Parameter frontend Receives owned source on success.
 * Parameter error Optional diagnostic.
 * Result: NG_OK on success, NG_ERROR after cleanup on failure.
 */
int NgRtlsdrOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    static const TNgFrontendOps operations = {Start, Acquire, Release, Stop, Stats, Destroy};
    TNgRtlsdrSource *source = calloc(1, sizeof(*source));
    if (source == NULL) {
        return NgFail(error, "Cannot allocate RTL-SDR source");
    }
    if (config->centerFrequencyHz > UINT32_MAX) {
        free(source);
        return NgFail(error, "RTL-SDR tuning exceeds its 32-bit native frequency range");
    }
    if (NgIngressInit(&source->ingress, config, NG_FORMAT_CU8, error) != NG_OK) {
        free(source);
        return NG_ERROR;
    }
    if (OpenDevice(config, &source->device, source->serial, error) != NG_OK) {
        goto failed;
    }
    if (rtlsdr_set_sample_rate(source->device, config->sampleRateHz) != 0 ||
        rtlsdr_set_center_freq(source->device, (uint32_t)config->centerFrequencyHz) != 0 ||
        rtlsdr_set_tuner_gain_mode(source->device, config->rtlAutoGain == 0) != 0 ||
        (config->rtlAutoGain == 0 &&
         (!GainSupported(source->device, config->rtlGainTenthsDb) ||
          rtlsdr_set_tuner_gain(source->device, config->rtlGainTenthsDb) != 0)) ||
        (config->rtlPpm != 0 && rtlsdr_set_freq_correction(source->device, config->rtlPpm) != 0) ||
        (config->bandwidthHz != 0 &&
         rtlsdr_set_tuner_bandwidth(source->device, config->bandwidthHz) != 0) ||
        rtlsdr_set_bias_tee(source->device, config->rtlBiasTee) != 0) {
        NgFail(error,
               "RTL-SDR rejected sample rate, tuning, discrete gain, PPM, bandwidth or bias tee");
        goto failed;
    }
    frontend->context = source;
    frontend->ops = &operations;
    frontend->info.sampleRateHz = rtlsdr_get_sample_rate(source->device);
    source->ingress.config.sampleRateHz = frontend->info.sampleRateHz;
    frontend->info.centerFrequencyHz = rtlsdr_get_center_freq(source->device);
    frontend->info.signalFrequencyHz =
        config->signalFrequencyHz ? config->signalFrequencyHz : frontend->info.centerFrequencyHz;
    frontend->info.digitalIfHz =
        (int64_t)frontend->info.signalFrequencyHz - (int64_t)frontend->info.centerFrequencyHz;
    frontend->info.bandwidthHz = config->bandwidthHz;
    frontend->info.format = NG_FORMAT_CU8;
    frontend->info.sourceName = "rtlsdr";
    frontend->info.clockDomain = "device-no-hardware-timestamp";
    frontend->info.deviceIdentifier = source->serial;
    if (frontend->info.sampleRateHz == 0 || frontend->info.centerFrequencyHz == 0) {
        NgFail(error, "RTL-SDR did not report an applied rate/frequency");
        goto failed;
    }
    return NG_OK;
failed:
    Destroy(source);
    memset(frontend, 0, sizeof(*frontend));
    return NG_ERROR;
}

/** @brief Enumerate native USB devices without starting RX.
 * Parameter output Caller-owned output stream.
 * Parameter error Optional diagnostic.
 * Result: NG_OK on enumeration, NG_ERROR on invalid output.
 */
int NgRtlsdrList(FILE *output, TNgError *error)
{
    uint32_t count = rtlsdr_get_device_count(), i;
    if (output == NULL) {
        return NgFail(error, "Missing enumeration output");
    }
    fprintf(output, "RTL-SDR devices=%u\n", count);
    for (i = 0; i < count; ++i) {
        char manufacturer[256], product[256], serial[256];
        if (rtlsdr_get_device_usb_strings(i, manufacturer, product, serial) == 0) {
            fprintf(output, "%u serial=%s product=%s\n", i, serial, product);
        }
    }
    return ferror(output) ? NgFail(error, "Enumeration write failed") : NG_OK;
}

/** @brief Read the selected tuner's discrete native gains and limits.
 * Parameter config Device serial and source settings.
 * Parameter caps Copied capability result.
 * Parameter error Optional diagnostic.
 * Result: NG_OK on success, NG_ERROR on device/query failure.
 */
int NgRtlsdrCapabilities(const TNgConfig *config, TNgFrontendCaps *caps, TNgError *error)
{
    rtlsdr_dev_t *device = NULL;
    char serial[128];
    int count;
    memset(caps, 0, sizeof(*caps));
    if (OpenDevice(config, &device, serial, error) != NG_OK) {
        return NG_ERROR;
    }
    count = rtlsdr_get_tuner_gains(device, NULL);
    if (count <= 0 || count > 64 || rtlsdr_get_tuner_gains(device, caps->tunerGains) != count) {
        rtlsdr_close(device);
        return NgFail(error, "Cannot query RTL-SDR discrete gains");
    }
    caps->tunerGainCount = (size_t)count;
    caps->sampleRateMinHz = 225001u;
    caps->sampleRateMaxHz = 3200000u;
    caps->frequencyMaxHz = UINT32_MAX;
    caps->format = NG_FORMAT_CU8;
    rtlsdr_close(device);
    return NG_OK;
}
