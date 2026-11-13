/** @file receiver.c
 * @brief NGene core implementation.
 */
#include "ngene/receiver.h"
#include "ngene/ring.h"
#include "core/channel_dispatch.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Publish a common window and synchronously dispatch one ring lease.
 *
 * @details
 * Copies into the central ring, obtains one immutable view, offers recording/hooks and every
 * channel, checks state transitions, updates counters and releases the lease even on failure.
 *
 * @param ring Central stream ring.
 * @param input Borrowed conditioned input window.
 * @param channels Channel array, or NULL if count is zero.
 * @param count Number of channel entries.
 * @param dispatch Optional parallel channel worker pool, completed before lease release.
 * @param recorder Optional engine-owned recorder.
 * @param stats Read/write engine counters.
 * @param hooks Optional synchronous hooks.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR if ring, recording, hook or channel processing fails.
 */
static int Dispatch(TNgRing *ring, const TNgIqView *input, TNgChannel *channels, size_t count,
                    TNgChannelDispatch *dispatch, TNgRecorder *recorder, TNgReceiverStats *stats,
                    const TNgReceiverHooks *hooks, TNgError *error)
{
    TNgIqView window;
    size_t k;
    int result;
    if (NgRingPush(ring, input) != NG_OK || NgRingAcquire(ring, &window, 0) != NG_OK) {
        return NgFail(error, "Central ring invariant failed");
    }
    /* The same pointer, sample interval and gap metadata go to every channel.
     * Channels may keep private accumulators, never separate ring cursors. */
    if (dispatch != NULL && NgChannelDispatchProcess(dispatch, &window, error) != NG_OK) {
        NgRingRelease(ring);
        return NG_ERROR;
    }
    for (k = 0; dispatch == NULL && k < count; ++k) {
        TNgChannelState next = channels[k].state;
        result = channels[k].ops->process(channels[k].context, &channels[k].id, channels[k].state,
                                          &window, &next, error);
        if (result != NG_OK) {
            NgRingRelease(ring);
            return NG_ERROR;
        }
        if (NgChannelTransitionValid(channels[k].state, next) == 0) {
            NgRingRelease(ring);
            return NgFail(error, "Invalid state transition requested by channel %u",
                          channels[k].id.instanceId);
        }
        channels[k].state = next;
    }
    if (recorder != NULL && NgRecorderSubmit(recorder, &window) != NG_OK) {
        NgRingRelease(ring);
        return NgFail(
            error,
            "Recorder queue full or writer failed; capture stopped without silent overwrite");
    }
    if (hooks != NULL && hooks->block != NULL &&
        hooks->block(hooks->context, &window, error) != NG_OK) {
        NgRingRelease(ring);
        return NG_ERROR;
    }
    ++stats->windows;
    stats->processedSamples += window.meta.count;
    if (((window.meta.flags) & (NG_BLOCK_GAP)) != 0) {
        ++stats->gapEvents;
    }
    NgRingRelease(ring);
    return NG_OK;
}

/** @brief Coordinate frontend, conditioning and serial or parallel channel execution.
 * @details The calling thread owns the shared stream and holds each ring lease until every
 * channel callback finishes. All worker storage is allocated before transport starts, and
 * cleanup joins workers before stopping transport and destroying sample storage. Stop/EOF
 * preserves the existing partial-window and recorder-drain contracts.
 */
int NgReceiverRunScheduled(const TNgConfig *config, TNgFrontend *frontend,
                           const TNgConditioner *conditioner, TNgChannel *channels,
                           size_t channelCount, TNgStopRequested stop, void *stopContext,
                           TNgReceiverStats *stats, const TNgReceiverExecution *execution,
                           const TNgReceiverHooks *hooks, TNgError *error)
{
    TNgRing *ring = NULL;
    TNgChannelDispatch *dispatch = NULL;
    TNgRecorder *recorder = NULL;
    TNgIq8 *assembly = NULL;
    TNgIqView pending;
    uint64_t expected = 0;
    uint64_t epoch = 0;
    int haveEpoch = 0;
    int result = NG_OK;
    int startAttempted = 0;
    double started = 0;
    double lastData = 0;
    double lastReport = 0;
    size_t k;
    TNgError cleanupError = {{0}};
    if (stats == NULL || frontend == NULL || frontend->ops == NULL || conditioner == NULL ||
        frontend->ops->start == NULL || frontend->ops->acquire == NULL ||
        frontend->ops->release == NULL || frontend->ops->stop == NULL ||
        conditioner->process == NULL || conditioner->name == NULL ||
        (channelCount != 0 && channels == NULL)) {
        return NgFail(error, "Missing receiver interface");
    }
    memset(stats, 0, sizeof(*stats));
    memset(&pending, 0, sizeof(pending));
    if (execution != NULL && execution->workerCount == 0) {
        return NgFail(error, "Channel worker count must be positive");
    }
    if (NgConfigValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    if (NgSampleBytes(frontend->info.format) == 0 || frontend->info.sampleRateHz == 0) {
        return NgFail(error, "Frontend format/rate does not match increment-1 contract");
    }
    for (k = 0; k < channelCount; ++k) {
        size_t j;
        if (channels[k].ops == NULL || channels[k].ops->process == NULL ||
            NgChannelTransitionValid(channels[k].state, channels[k].state) == 0) {
            return NgFail(error, "Invalid channel registration");
        }
        if ((channels[k].ops->nativeFormats != 0 &&
             (channels[k].ops->nativeFormats & (1u << frontend->info.format)) == 0) ||
            (channels[k].ops->nativeFormats == 0 && frontend->info.format != NG_FORMAT_CI8)) {
            return NgFail(error, "Channel %u does not support the native frontend sample format",
                          channels[k].id.instanceId);
        }
        for (j = 0; j < k; ++j) {
            if (channels[j].id.instanceId == channels[k].id.instanceId) {
                return NgFail(error, "Duplicate channel instance_id");
            }
        }
    }
    if (NgRingCreateFormat(config->ringBlocks, config->windowSamples, frontend->info.format, &ring,
                           error) != NG_OK) {
        return NG_ERROR;
    }
    assembly = malloc(config->windowSamples * NgSampleBytes(frontend->info.format));
    if (assembly == NULL) {
        result = NgFail(error, "Cannot allocate frame assembly");
        goto cleanup;
    }
    pending.samples = assembly;
    pending.format = frontend->info.format;
    if (NgChannelDispatchCreate(channels, (config->recordOnly) != 0 ? 0 : channelCount,
                                execution != NULL ? execution->workerCount : 1, &dispatch,
                                error) != NG_OK) {
        result = NG_ERROR;
        goto cleanup;
    }
    if (config->recordingEnabled != 0 && (hooks == NULL || hooks->externalRecording == 0) &&
        NgRecorderOpen(config, &frontend->info, conditioner->name, &recorder, error) != NG_OK) {
        result = NG_ERROR;
        goto cleanup;
    }
    startAttempted = 1;
    if (frontend->ops->start(frontend->context, error) != NG_OK) {
        result = NG_ERROR;
        goto cleanup;
    }
    started = NgMonotonicSeconds();
    lastData = started;
    if (hooks != NULL && hooks->started != NULL) {
        hooks->started(hooks->context, &frontend->info);
    }
    for (;;) {
        TNgIqView input = {0};
        TNgIqView conditioned;
        size_t offset = 0;
        int gap;
        uint64_t missing = 0;
        int missingKnown = 0;
        double now = NgMonotonicSeconds();
        if (hooks != NULL && hooks->progress != NULL && now - lastReport >= 0.1) {
            stats->wallSeconds = now - started;
            stats->ringHighWater = NgRingHighWater(ring);
            if (frontend->ops->stats != NULL) {
                frontend->ops->stats(frontend->context, &stats->frontend);
            }
            hooks->progress(hooks->context, stats);
            lastReport = now;
        }
        if ((stop != NULL && stop(stopContext) != 0) ||
            (config->runSeconds > 0 && NgMonotonicSeconds() - started >= config->runSeconds)) {
            break;
        }
        result = frontend->ops->acquire(frontend->context, &input, 50, error);
        if (result == NG_END) {
            result = NG_OK;
            break;
        }
        if (result == NG_TIMEOUT) {
            if (NgMonotonicSeconds() - lastData > (double)config->idleTimeoutMs * 0.001) {
                result = NgFail(error, "No input within idle_timeout_ms");
                break;
            }
            result = NG_OK;
            continue;
        }
        if (result != NG_OK) {
            result = NG_ERROR;
            break;
        }
        if (input.samples == NULL || input.meta.count == 0 ||
            input.meta.count > config->windowSamples ||
            input.meta.firstSample > UINT64_MAX - input.meta.count ||
            (input.format == 0 ? NG_FORMAT_CI8 : input.format) != frontend->info.format ||
            ((input.meta.flags & NG_BLOCK_HARDWARE_TIME) != 0 &&
             input.meta.hardwareTimestamp > UINT64_MAX - input.meta.count)) {
            frontend->ops->release(frontend->context);
            result = NgFail(error, "Invalid source block");
            break;
        }
        if (haveEpoch != 0 && (input.meta.epoch < epoch ||
                               (input.meta.epoch == epoch && input.meta.firstSample < expected))) {
            frontend->ops->release(frontend->context);
            result = NgFail(error, "Source time moved backwards without a new epoch");
            break;
        }
        {
            TNgIqView raw = input;
            if (((raw.meta.flags) & (NG_BLOCK_GAP)) == 0 &&
                ((haveEpoch == 0 && raw.meta.firstSample != 0) ||
                 (haveEpoch != 0 &&
                  (raw.meta.epoch != epoch || raw.meta.firstSample != expected)))) {
                raw.meta.flags |= NG_BLOCK_GAP;
                raw.meta.missingKnown = !haveEpoch || raw.meta.epoch == epoch;
                raw.meta.missingBefore =
                    (raw.meta.missingKnown) != 0 ? raw.meta.firstSample - expected : 0;
            }
            if ((recorder != NULL && config->recordingPreConditioning != 0 &&
                 NgRecorderSubmit(recorder, &raw) != NG_OK) ||
                (hooks != NULL && hooks->rawBlock != NULL &&
                 hooks->rawBlock(hooks->context, &raw, error) != NG_OK)) {
                frontend->ops->release(frontend->context);
                result = NgFail(error, "Pre-conditioning recording failed");
                break;
            }
        }
        if (conditioner->process(conditioner->context, &input, &conditioned, error) != NG_OK ||
            conditioned.samples == NULL || conditioned.meta.count != input.meta.count ||
            conditioned.meta.firstSample != input.meta.firstSample ||
            conditioned.meta.epoch != input.meta.epoch ||
            conditioned.meta.flags != input.meta.flags ||
            conditioned.meta.missingKnown != input.meta.missingKnown ||
            conditioned.meta.missingBefore != input.meta.missingBefore ||
            conditioned.format != input.format ||
            conditioned.meta.hardwareTimestamp != input.meta.hardwareTimestamp) {
            frontend->ops->release(frontend->context);
            result = NgFail(error, "Conditioner violated the rate-preserving stream contract");
            break;
        }
        lastData = NgMonotonicSeconds();
        if (haveEpoch != 0 &&
            (conditioned.meta.epoch < epoch ||
             (conditioned.meta.epoch == epoch && conditioned.meta.firstSample < expected))) {
            frontend->ops->release(frontend->context);
            result = NgFail(error, "Source time moved backwards without a new epoch");
            break;
        }
        gap = (conditioned.meta.flags & NG_BLOCK_GAP) != 0 ||
              (!haveEpoch && conditioned.meta.firstSample != 0) ||
              (haveEpoch &&
               (conditioned.meta.epoch != epoch || conditioned.meta.firstSample != expected));
        if (gap != 0) {
            if ((haveEpoch == 0 || conditioned.meta.epoch == epoch) &&
                conditioned.meta.firstSample >= expected) {
                missing = conditioned.meta.firstSample - expected;
                missingKnown = 1;
            }
            if (((conditioned.meta.flags) & (NG_BLOCK_GAP)) != 0) {
                missing = conditioned.meta.missingBefore;
                missingKnown = conditioned.meta.missingKnown;
            }
            if (pending.meta.count != 0) {
                pending.meta.flags |= NG_BLOCK_PARTIAL;
                result =
                    Dispatch(ring, &pending, channels, (config->recordOnly) != 0 ? 0 : channelCount,
                             dispatch, (config->recordingPreConditioning) != 0 ? NULL : recorder,
                             stats, hooks, error);
                pending.meta.count = 0;
                if (result != NG_OK) {
                    frontend->ops->release(frontend->context);
                    break;
                }
            }
        }
        epoch = conditioned.meta.epoch;
        expected = conditioned.meta.firstSample + conditioned.meta.count;
        haveEpoch = 1;
        while (offset < conditioned.meta.count) {
            size_t count = conditioned.meta.count - offset;
            if (pending.meta.count == 0) {
                memset(&pending.meta, 0, sizeof(pending.meta));
                pending.meta.epoch = epoch;
                pending.meta.firstSample = conditioned.meta.firstSample + offset;
                if ((conditioned.meta.flags & NG_BLOCK_HARDWARE_TIME) != 0) {
                    pending.meta.flags |= NG_BLOCK_HARDWARE_TIME;
                    pending.meta.hardwareTimestamp = conditioned.meta.hardwareTimestamp + offset;
                }
                if (gap != 0 && offset == 0) {
                    pending.meta.flags |= NG_BLOCK_GAP;
                    pending.meta.missingBefore = missing;
                    pending.meta.missingKnown = missingKnown;
                }
            }
            if (count > config->windowSamples - pending.meta.count) {
                count = config->windowSamples - pending.meta.count;
            }
            memcpy((unsigned char *)(void *)assembly +
                       pending.meta.count * NgSampleBytes(pending.format),
                   (const unsigned char *)(const void *)conditioned.samples +
                       offset * NgSampleBytes(pending.format),
                   count * NgSampleBytes(pending.format));
            pending.meta.count += count;
            offset += count;
            if (pending.meta.count == config->windowSamples) {
                result =
                    Dispatch(ring, &pending, channels, (config->recordOnly) != 0 ? 0 : channelCount,
                             dispatch, (config->recordingPreConditioning) != 0 ? NULL : recorder,
                             stats, hooks, error);
                pending.meta.count = 0;
                if (result != NG_OK) {
                    break;
                }
            }
        }
        frontend->ops->release(frontend->context);
        if (result != NG_OK) {
            break;
        }
    }
    if (result == NG_OK && pending.meta.count != 0) {
        pending.meta.flags |= NG_BLOCK_PARTIAL;
        result = Dispatch(ring, &pending, channels, (config->recordOnly) != 0 ? 0 : channelCount,
                          dispatch, (config->recordingPreConditioning) != 0 ? NULL : recorder,
                          stats, hooks, error);
        if (result == NG_OK) {
            pending.meta.count = 0;
        }
    }
    stats->discardedPartialSamples = pending.meta.count;
cleanup:
    NgChannelDispatchDestroy(dispatch);
    if (startAttempted != 0 && frontend->ops->stop(frontend->context, &cleanupError) != NG_OK &&
        result == NG_OK) {
        if (error != NULL) {
            *error = cleanupError;
        }
        result = NG_ERROR;
    }
    if (frontend->ops->stats != NULL) {
        frontend->ops->stats(frontend->context, &stats->frontend);
    }
    if (recorder != NULL && NgRecorderFinish(recorder, result == NG_OK, &stats->frontend,
                                             &stats->recording, &cleanupError) != NG_OK) {
        if (error != NULL) {
            *error = cleanupError;
        }
        result = NG_ERROR;
    }
    if (started > 0) {
        stats->wallSeconds = NgMonotonicSeconds() - started;
    }
    stats->ringHighWater = NgRingHighWater(ring);
    free(assembly);
    NgRingDestroy(ring);
    return result;
}

/** @brief Preserve the original serial engine API and synchronous callback ordering. */
int NgReceiverRunEx(const TNgConfig *config, TNgFrontend *frontend,
                    const TNgConditioner *conditioner, TNgChannel *channels, size_t channelCount,
                    TNgStopRequested stop, void *stopContext, TNgReceiverStats *stats,
                    const TNgReceiverHooks *hooks, TNgError *error)
{
    return NgReceiverRunScheduled(config, frontend, conditioner, channels, channelCount, stop,
                                  stopContext, stats, NULL, hooks, error);
}

/**
 * @brief Run the synchronous shared-stream receiver engine.
 *
 * @details
 * Starts the frontend, assembles common windows, applies conditioning, feeds the single ring
 * and dispatches the same view to every channel. Stop/EOF drains recorder work and stops
 * transport. Blocks until completion. Channels may update state but must not retain samples;
 * no acquisition/tracking algorithm is implied by capture mode.
 *
 * Parameter config: Validated core configuration matching the frontend descriptor.
 * Parameter frontend: Opened frontend; caller retains ownership and destroys after the run.
 * Parameter conditioner: Rate- and native-format-preserving conditioner with valid context for this
 * run. Parameter channels: Caller-owned channel array; NULL permitted when channelCount is zero.
 * Parameter channelCount: Number of channels; each sees the same borrowed window.
 * Parameter stop: Optional stop predicate called by the engine.
 * Parameter stopContext: Opaque context passed to the stop predicate.
 * Parameter stats: Receives final engine counters; writable and non-NULL.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK for requested stop or clean EOF; NG_ERROR for transport, module, recorder or
 * contract failures.
 */
int NgReceiverRun(const TNgConfig *config, TNgFrontend *frontend, const TNgConditioner *conditioner,
                  TNgChannel *channels, size_t channelCount, TNgStopRequested stop,
                  void *stopContext, TNgReceiverStats *stats, TNgError *error)
{
    return NgReceiverRunEx(config, frontend, conditioner, channels, channelCount, stop, stopContext,
                           stats, NULL, error);
}
