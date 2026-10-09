/** @file receiver.h @brief Central scheduler, the only consumer of the stream ring. */
#ifndef NGENE_RECEIVER_H
#define NGENE_RECEIVER_H
#include "ngene/channel.h"
#include "ngene/conditioning.h"
#include "ngene/frontend.h"
#include "ngene/recording.h"
/** @brief Nonblocking stop predicate polled by the engine/service.
 * @param context Borrowed owner context retained throughout the run.
 * @return Nonzero to request an orderly stop, zero to continue.
 */
typedef int (*TNgStopRequested)(void *context);

/** @brief Common-engine, transport and recorder counters for one run. */
typedef struct {
    /** Number of complete windows processed or written, according to the object. */
    uint64_t windows;
    /** Complex pairs dispatched by the shared engine. */
    uint64_t processedSamples;
    /** Detected stream discontinuities; does not estimate unknown sample losses. */
    uint64_t gapEvents;
    /** Incomplete engine-window samples excluded from channel processing. */
    uint64_t discardedPartialSamples;
    /** Largest number of simultaneously occupied engine-ring slots. */
    size_t ringHighWater;
    /** Elapsed run duration from the monotonic process clock. */
    double wallSeconds;
    /** Selected transport implementation or frontend statistics, according to the object. */
    TNgFrontendStats frontend;
    /** Copied disk-writer statistics; not a borrowed recorder handle. */
    TNgRecordingStats recording;
} TNgReceiverStats;

/** Optional synchronous engine hooks; never retain a borrowed block.
 * Transport/UI work must use snapshots or bounded queues, not these callbacks.
 */
typedef struct {
    /** Opaque implementation or callback state; ownership follows the enclosing API. */
    void *context;
    /** Nonzero delegates recording to engine hooks rather than the local recorder. */
    int externalRecording;
    /** @brief Report successful transport startup with applied metadata.
     * @param context Borrowed hooks context.
     * @param info Read-only stream information; copy retained strings if needed after run.
     */
    void (*started)(void *context, const TNgStreamInfo *info);
    /** @brief Observe a common conditioned window synchronously.
     * @details Copy only into bounded queues; never retain input or perform disk/network I/O.
     * @param context Borrowed hooks context.
     * @param view Immutable post-conditioning view valid until callback return.
     * @param error Optional error destination.
     * @return NG_OK on success; NG_ERROR to stop on observer/recording failure.
     */
    int (*block)(void *context, const TNgIqView *view, TNgError *error);
    /** @brief Observe copied run counters at the engine update cadence.
     * @details Optional status work must not wait for GUI or network consumers.
     * @param context Borrowed hooks context.
     * @param stats Read-only counters, valid only during this callback.
     */
    void (*progress)(void *context, const TNgReceiverStats *stats);
    /** @brief Observe the original frontend window before conditioning.
     * @details Copy into bounded queues if needed; never retain input or perform I/O.
     * @param context Borrowed hooks context.
     * @param view Immutable pre-conditioning view valid until callback return.
     * @param error Optional error destination.
     * @return NG_OK on success; NG_ERROR to stop on tap/recording failure.
     */
    int (*rawBlock)(void *context, const TNgIqView *view, TNgError *error);
} TNgReceiverHooks;

/**
 * @brief Run the synchronous shared-stream receiver engine.
 *
 * @details
 * Starts the frontend, assembles common windows, applies conditioning, feeds the single ring
 * and dispatches the same view to every channel. Stop/EOF drains recorder work and stops
 * transport. Blocks until completion. Channels may update state but must not retain samples;
 * no acquisition/tracking algorithm is implied by capture mode.
 *
 * @param config Validated core configuration matching the frontend descriptor.
 * @param frontend Opened frontend; caller retains ownership and destroys after the run.
 * @param conditioner Rate-preserving CI8 conditioner with valid context for this run.
 * @param channels Caller-owned channel array; NULL permitted when count is zero.
 * @param channelCount Number of channels; each sees the same borrowed window.
 * @param stop Optional stop predicate called by the engine.
 * @param stopContext Opaque context passed to the stop predicate.
 * @param stats Receives final engine counters; writable and non-NULL.
 * @param hooks Optional engine hooks; contexts remain live throughout the run.
 * @param error Optional error destination.
 *
 * @return NG_OK for requested stop or clean EOF; NG_ERROR for transport, module, recorder or
 * contract failures.
 */
int NgReceiverRunEx(const TNgConfig *config, TNgFrontend *frontend,
                    const TNgConditioner *conditioner, TNgChannel *channels, size_t channelCount,
                    TNgStopRequested stop, void *stopContext, TNgReceiverStats *stats,
                    const TNgReceiverHooks *hooks, TNgError *error);

/**
 * @brief Run the synchronous shared-stream receiver engine.
 *
 * @details
 * Starts the frontend, assembles common windows, applies conditioning, feeds the single ring
 * and dispatches the same view to every channel. Stop/EOF drains recorder work and stops
 * transport. Blocks until completion. Channels may update state but must not retain samples;
 * no acquisition/tracking algorithm is implied by capture mode.
 *
 * @param config Validated core configuration matching the frontend descriptor.
 * @param frontend Opened frontend; caller retains ownership and destroys after the run.
 * @param conditioner Rate-preserving CI8 conditioner with valid context for this run.
 * @param channels Caller-owned channel array; NULL permitted when count is zero.
 * @param channelCount Number of channels; each sees the same borrowed window.
 * @param stop Optional stop predicate called by the engine.
 * @param stopContext Opaque context passed to the stop predicate.
 * @param stats Receives final engine counters; writable and non-NULL.
 * @param error Optional error destination.
 *
 * @return NG_OK for requested stop or clean EOF; NG_ERROR for transport, module, recorder or
 * contract failures.
 */
int NgReceiverRun(const TNgConfig *config, TNgFrontend *frontend, const TNgConditioner *conditioner,
                  TNgChannel *channels, size_t channelCount, TNgStopRequested stop,
                  void *stopContext, TNgReceiverStats *stats, TNgError *error);
#endif
