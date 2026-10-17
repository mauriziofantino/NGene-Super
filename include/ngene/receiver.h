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

/** @brief Explicit run-local channel execution policy; OS scheduling chooses the cores. */
typedef struct {
    /** Positive maximum channel workers, capped to channel count; one keeps serial dispatch.
     * Private or synchronized channel contexts are required when greater than one. */
    size_t workerCount;
} TNgReceiverExecution;

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

/** @brief Run the shared-stream engine with optional bounded parallel channel dispatch.
 * @details NULL execution preserves serial processing. Otherwise workerCount must be positive;
 * the effective count is capped to channelCount and no pool is started for zero/one channel
 * or record-only runs. Workers and result storage are created before frontend start and joined
 * before return; no per-window allocation or CPU pinning is performed. The OS places workers
 * on cores, while idle workers dynamically claim channel jobs. All channels complete a window
 * before its ring lease is released or the next window begins. Channel workspace must be
 * private or explicitly synchronized; never rely on callback order in parallel mode. Every
 * channel runs once in parallel mode even if another fails, then the first error in registration
 * order is reported. Successful valid transitions are committed after the completion barrier.
 * Frontend, conditioning, recording submission and hooks remain on the calling receiver thread.
 * @param config Validated core configuration matching the frontend descriptor.
 * @param frontend Opened caller-owned frontend, destroyed by caller after return.
 * @param conditioner Borrowed rate- and native-format-preserving conditioner.
 * @param channels Caller-owned validated registrations, unchanged throughout the run except
 * engine-owned state transitions; NULL permitted when channelCount is zero.
 * @param channelCount Number of channels receiving each common immutable window.
 * @param stop Optional nonblocking stop predicate, called between complete windows.
 * @param stopContext Borrowed context retained throughout the run.
 * @param stats Writable non-NULL final engine counters.
 * @param execution Optional fixed worker-count policy, read before frontend start.
 * @param hooks Optional synchronous observer hooks with contexts live throughout the run.
 * @param error Optional error destination, written by the receiver after all workers finish.
 * @return NG_OK on stop/EOF; NG_ERROR on invalid policy, resources, transport or module failure.
 */
int NgReceiverRunScheduled(const TNgConfig *config, TNgFrontend *frontend,
                           const TNgConditioner *conditioner, TNgChannel *channels,
                           size_t channelCount, TNgStopRequested stop, void *stopContext,
                           TNgReceiverStats *stats, const TNgReceiverExecution *execution,
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
 * @param conditioner Rate- and native-format-preserving conditioner with valid context for this
 * run.
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
 * @param conditioner Rate- and native-format-preserving conditioner with valid context for this
 * run.
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
