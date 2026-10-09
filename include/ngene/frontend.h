/** @file frontend.h @brief Native frontend boundary; no vendor types escape. */
#ifndef NGENE_FRONTEND_H
#define NGENE_FRONTEND_H
#include "ngene/config.h"

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
} TNgFrontendStats;

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
