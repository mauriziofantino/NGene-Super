/** @file frontend.h @brief Native frontend boundary; no vendor types escape. */
#ifndef NGENE_FRONTEND_H
#define NGENE_FRONTEND_H
#include "ngene/config.h"
#include "ngene/ring.h"

/** @brief Availability is independent of a counter's numeric value. */
typedef enum {
    NG_DIAGNOSTIC_UNSUPPORTED,
    NG_DIAGNOSTIC_VALID,
    NG_DIAGNOSTIC_UNAVAILABLE,
    NG_DIAGNOSTIC_ERROR
} TNgDiagnosticState;

/** @brief Transport counters measured in complex pairs. */
typedef struct {
    /** Complex pairs delivered by the transport before known drops. */
    uint64_t receivedSamples;
    /** Complex pairs lost after reception due to bounded ingress overflow. */
    uint64_t droppedSamples;
    /** Complex pairs left in transport queues when reception is stopped. */
    uint64_t discardedOnStop;
    /** Complex pairs currently awaiting consumption or disk writing. */
    uint64_t queuedSamples;
    /** Existing ingress-ring counters; all zero and ingressAvailable false for files. */
    TNgRingStats ingress;
    /** Nonzero if the source has a measured preallocated ingress ring. */
    int ingressAvailable;
    /** Observed vendor/stream errors; excludes software queue overflow. */
    uint64_t transportErrors;
    /** Device/library-reported discontinuity events; not necessarily hardware-only loss. */
    uint64_t deviceGapEvents;
    /** Exact missing pairs from hardware timestamps when available. */
    uint64_t deviceMissingSamples;
    /** Availability of the device continuity counters. */
    TNgDiagnosticState deviceContinuityState;
    /** Hardware-only loss cannot be inferred from zero software loss. */
    TNgDiagnosticState hardwareLossState;
} TNgFrontendStats;

/** @brief Owned frontend limits; zero bounds denote an unavailable queried capability. */
typedef struct {
    /** Native input representation. */
    TNgSampleFormat format;
    /** Nominal sample-rate bounds in complex pairs/s. */
    uint32_t sampleRateMinHz;
    /** Highest supported nominal complex sample rate. */
    uint32_t sampleRateMaxHz;
    /** Hardware tuning range, in Hz. */
    uint64_t frequencyMinHz;
    /** Maximum supported hardware tuning frequency in Hz. */
    uint64_t frequencyMaxHz;
    /** Copied discrete tuner gains in tenth-dB; RTL-SDR only. */
    int tunerGains[64];
    /** Number of populated tunerGains elements. */
    size_t tunerGainCount;
    /** bladeRF overall RX gain bounds in dB. */
    int gainMinDb;
    /** Maximum overall RX gain in dB. */
    int gainMaxDb;
    /** Supported increment of overall RX gain in dB. */
    int gainStepDb;
} TNgFrontendCaps;

/** @brief Query compiled support without opening USB hardware.
 * @param kind Frontend kind, including file replay.
 * @return Nonzero if the corresponding native driver is compiled in.
 */
int NgFrontendAvailable(TNgFrontendKind kind);
/** @brief Enumerate one native driver without starting reception.
 * @param kind Compiled native frontend type.
 * @param output Caller-owned writable text stream.
 * @param error Optional diagnostic destination.
 * @return NG_OK for successful enumeration (including zero devices), NG_ERROR otherwise.
 */
int NgFrontendList(TNgFrontendKind kind, FILE *output, TNgError *error);
/** @brief Query device-specific limits outside sample processing.
 * @details RTL-SDR/bladeRF briefly open the selected device to read its native gain/rate
 * capabilities, then close it. Call only while stopped; no firmware is loaded or flashed.
 * @param config Source type and optional serial identifier.
 * @param caps Writable copied result; no vendor-owned pointers escape.
 * @param error Optional diagnostic destination.
 * @return NG_OK on success, NG_ERROR on unsupported driver, device or query failure.
 */
int NgFrontendCapabilities(const TNgConfig *config, TNgFrontendCaps *caps, TNgError *error);

/** @brief Native source operations, called by one receiver reader thread.
 * @details An opened instance supports one run. No vendor types escape this interface.
 */
typedef struct {
    /** @brief Start a freshly opened source.
     * @param context Owned native source state.
     * @param error Optional error destination.
     * @return NG_OK on success or NG_ERROR on transport failure.
     */
    int (*start)(void *context, TNgError *error);
    /** @brief Borrow one nonempty block of at most the configured window size.
     * @details One outstanding lease only. Samples remain valid until release; indices
     * account for known losses and unknown gaps are explicit. Never retain a vendor pointer.
     * @param context Owned native source state.
     * @param view Receives immutable borrowed samples and metadata on NG_OK.
     * @param timeoutMs Maximum wait in milliseconds, zero for polling.
     * @param error Optional error destination.
     * @return NG_OK, NG_TIMEOUT, NG_END or NG_ERROR.
     */
    int (*acquire)(void *context, TNgIqView *view, unsigned timeoutMs, TNgError *error);
    /** @brief End the outstanding borrow; samples must no longer be accessed.
     * @param context Native source with one outstanding lease.
     */
    void (*release)(void *context);
    /** @brief Stop and join transport activity before final statistics or destroy.
     * @param context Owned native source state.
     * @param error Optional error destination.
     * @return NG_OK on success or NG_ERROR on transport failure.
     */
    int (*stop)(void *context, TNgError *error);
    /** @brief Copy frontend counters; callable by the engine while running.
     * @param context Live source state.
     * @param stats Receives known software sample/loss counters, in complex pairs.
     */
    void (*stats)(void *context, TNgFrontendStats *stats);
    /** @brief Release a stopped source and all native resources.
     * @param context Owned state with no live callbacks or outstanding leases.
     */
    void (*destroy)(void *context);
} TNgFrontendOps;

/** @brief Owned source context with borrowed immutable operations and metadata. */
typedef struct {
    /** Borrowed immutable operations table; implementation owns its storage. */
    const TNgFrontendOps *ops;
    /** Opaque implementation or callback state; ownership follows the enclosing API. */
    void *context;
    /** Stream metadata; borrowed strings remain valid until frontend destruction. */
    TNgStreamInfo info;
} TNgFrontend;

/**
 * @brief Open and configure a native frontend without starting reception.
 *
 * @details
 * Selects HackRF or a recorded CI8 file source from config. Instances have independent buffers and
 * clocks; separate engines can run concurrently. The receiver controls start/stop; the caller
 * destroys the instance after use.
 *
 * @param config Validated receiver configuration.
 * @param frontend Receives owned frontend context/operations/stream description.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for configuration, device or allocation errors.
 */
int NgFrontendOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error);
/** Native boundary for calibration and driver fixtures; operational clients use Open. */
int NgFrontendOpenNative(const TNgConfig *config, TNgFrontend *frontend, TNgError *error);
/** Wrap an owned native source in a dedicated common-stream worker. */
int NgFrontendCommon(const TNgConfig *config, TNgFrontend *frontend, TNgError *error);
/**
 * @brief Destroy an opened frontend after reception stops.
 *
 * @details
 * Calls the native implementation destructor and clears the public descriptor. Do not access
 * borrowed views or stream strings afterwards; NULL is accepted.
 *
 * @param frontend Frontend whose transport has been stopped.
 */
void NgFrontendDestroy(TNgFrontend *frontend);
/**
 * @brief Report whether the native HackRF adapter is compiled in.
 *
 * @details
 * This does not enumerate USB devices or establish whether a device is connected.
 *
 * @return Nonzero if compiled with libhackrf, otherwise zero.
 */
int NgHackrfAvailable(void);
/**
 * @brief Enumerate native HackRF devices to a text stream.
 *
 * @details
 * Does not start sample reception. Initializes and cleans up the vendor library for
 * enumeration; do not run concurrently with an active frontend instance.
 *
 * @param output Writable output stream retained by caller.
 * @param error Optional error destination.
 *
 * @return NG_OK on successful enumeration; NG_ERROR if unavailable or the vendor API fails.
 */
int NgFrontendListHackrf(FILE *output, TNgError *error);
#endif
