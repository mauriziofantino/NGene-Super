/** @file frontend.c
 * @brief NGene frontend implementation.
 */
#include "ngene/frontend.h"
#include <string.h>
#include "frontend/file.h"
#ifdef NGENE_HAVE_SIGE
#include "frontend/sige.h"
#endif
#ifdef NGENE_HAVE_HACKRF
#include "frontend/hackrf_adapter.h"
#endif
#ifdef NGENE_HAVE_RTLSDR
#include "frontend/rtlsdr_adapter.h"
#endif
#ifdef NGENE_HAVE_BLADERF
#include "frontend/bladerf_adapter.h"
#endif
/**
 * @brief Open and configure a native frontend without starting reception.
 *
 * @details
 * Selects HackRF or the deterministic file replay source from config. Each descriptor owns one
 * frontend; independent receivers may run concurrently. The receiver controls start/stop; the
 * caller destroys the instance after use.
 *
 * Parameter config: Validated receiver configuration.
 * Parameter frontend: Receives owned frontend context/operations/stream description.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for configuration, device or allocation errors.
 */
int NgFrontendOpenNative(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    if (frontend == NULL || NgConfigValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    memset(frontend, 0, sizeof(*frontend));
    if (config->frontend == NG_FRONTEND_SIGE) {
#ifdef NGENE_HAVE_SIGE
        return NgSigeOpen(config, frontend, error);
#else
        return NgFail(error, "SiGe support is not compiled in");
#endif
    }
    if (config->frontend == NG_FRONTEND_FILE) {
        return NgFileOpen(config, frontend, error);
    }
    if (config->frontend == NG_FRONTEND_HACKRF) {
#ifdef NGENE_HAVE_HACKRF
        return NgHackrfOpen(config, frontend, error);
#endif
    }
    if (config->frontend == NG_FRONTEND_RTLSDR) {
#ifdef NGENE_HAVE_RTLSDR
        return NgRtlsdrOpen(config, frontend, error);
#endif
    }
    if (config->frontend == NG_FRONTEND_BLADERF) {
#ifdef NGENE_HAVE_BLADERF
        return NgBladerfOpen(config, frontend, error);
#endif
    }
    return NgFail(error, "Selected native frontend is disabled in this build");
}

/**
 * @brief Destroy an opened frontend after reception stops.
 *
 * @details
 * Calls the native implementation destructor and clears the public descriptor. Do not access
 * borrowed views or stream strings afterwards; NULL is accepted.
 *
 * Parameter frontend: Frontend whose transport has been stopped.
 */
void NgFrontendDestroy(TNgFrontend *frontend)
{
    if (frontend != NULL && frontend->ops != NULL) {
        frontend->ops->destroy(frontend->context);
        memset(frontend, 0, sizeof(*frontend));
    }
}

/**
 * @brief Report whether the native HackRF adapter is compiled in.
 *
 * @details
 * This does not enumerate USB devices or establish whether a device is connected.
 *
 * Result: Nonzero if compiled with libhackrf, otherwise zero.
 */
int NgHackrfAvailable(void)
{
#ifdef NGENE_HAVE_HACKRF
    return 1;
#else
    return 0;
#endif
}

/**
 * @brief Enumerate native HackRF devices to a text stream.
 *
 * @details
 * Does not start sample reception. Initializes and cleans up the vendor library for
 * enumeration; do not run concurrently with an active frontend instance.
 *
 * Parameter output: Writable output stream retained by caller.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on successful enumeration; NG_ERROR if unavailable or the vendor API fails.
 */
int NgFrontendListHackrf(FILE *output, TNgError *error)
{
#ifdef NGENE_HAVE_HACKRF
    return NgHackrfList(output, error);
#else
    (void)output;
    return NgFail(error, "HackRF support is disabled");
#endif
}

/** @brief Query compiled support without opening USB hardware.
 * Parameter kind Frontend kind, including file replay.
 * Result: Nonzero if the corresponding native driver is compiled in.
 */
int NgFrontendAvailable(TNgFrontendKind kind)
{
    if(kind == NG_FRONTEND_SIGE) {
#ifdef NGENE_HAVE_SIGE
        return 1;
#else
        return 0;
#endif
    }
    if (kind == NG_FRONTEND_FILE) {
        return 1;
    }
#ifdef NGENE_HAVE_HACKRF
    if (kind == NG_FRONTEND_HACKRF) {
        return 1;
    }
#endif
#ifdef NGENE_HAVE_RTLSDR
    if (kind == NG_FRONTEND_RTLSDR) {
        return 1;
    }
#endif
#ifdef NGENE_HAVE_BLADERF
    if (kind == NG_FRONTEND_BLADERF) {
        return 1;
    }
#endif
    return 0;
}

/** @brief Enumerate one native driver without starting reception.
 * Parameter kind Compiled native frontend type.
 * Parameter output Caller-owned writable text stream.
 * Parameter error Optional diagnostic destination.
 * Result: NG_OK for successful enumeration (including zero devices), NG_ERROR otherwise.
 */
int NgFrontendList(TNgFrontendKind kind, FILE *output, TNgError *error)
{
    if (output == NULL) {
        return NgFail(error, "Missing enumeration output");
    }
#ifdef NGENE_HAVE_SIGE
    if(kind == NG_FRONTEND_SIGE) return NgSigeList(output,error);
#endif
#ifdef NGENE_HAVE_HACKRF
    if (kind == NG_FRONTEND_HACKRF) {
        return NgHackrfList(output, error);
    }
#endif
#ifdef NGENE_HAVE_RTLSDR
    if (kind == NG_FRONTEND_RTLSDR) {
        return NgRtlsdrList(output, error);
    }
#endif
#ifdef NGENE_HAVE_BLADERF
    if (kind == NG_FRONTEND_BLADERF) {
        return NgBladerfList(output, error);
    }
#endif
    (void)kind;
    return NgFail(error, "Selected native frontend is disabled in this build");
}

/** @brief Query device-specific limits outside sample processing.
 * @details RTL-SDR/bladeRF briefly open the selected device to read its native gain/rate
 * capabilities, then close it. Call only while stopped; no firmware is loaded or flashed.
 * Parameter config Source type and optional serial identifier.
 * Parameter caps Writable copied result; no vendor-owned pointers escape.
 * Parameter error Optional diagnostic destination.
 * Result: NG_OK on success, NG_ERROR on unsupported driver, device or query failure.
 */
int NgFrontendCapabilities(const TNgConfig *config, TNgFrontendCaps *caps, TNgError *error)
{
    if (config == NULL || caps == NULL) {
        return NgFail(error, "Missing capability request");
    }
    memset(caps, 0, sizeof(*caps));
#ifdef NGENE_HAVE_RTLSDR
    if (config->frontend == NG_FRONTEND_RTLSDR) {
        return NgRtlsdrCapabilities(config, caps, error);
    }
#endif
#ifdef NGENE_HAVE_BLADERF
    if (config->frontend == NG_FRONTEND_BLADERF) {
        return NgBladerfCapabilities(config, caps, error);
    }
#endif
    if (!NgFrontendAvailable(config->frontend)) {
        return NgFail(error, "Selected native frontend is disabled in this build");
    }
    if(config->frontend == NG_FRONTEND_SIGE) {
        caps->format=NG_FORMAT_CI8;caps->sampleRateMinHz=caps->sampleRateMaxHz=NG_SIGE_RATE_HZ;
        caps->frequencyMinHz=caps->frequencyMaxHz=NG_SIGE_CENTER_HZ;return NG_OK;
    }
    caps->format = config->frontend == NG_FRONTEND_FILE ? config->replayFormat : NG_FORMAT_CI8;
    caps->sampleRateMinHz =
        config->frontend == NG_FRONTEND_FILE ? 1u : NG_HACKRF_MIN_SAMPLE_RATE_HZ;
    caps->sampleRateMaxHz =
        config->frontend == NG_FRONTEND_FILE ? 61440000u : NG_FRONTEND_MAX_SAMPLE_RATE_HZ;
    caps->frequencyMaxHz = UINT64_C(6000000000);
    return NG_OK;
}
