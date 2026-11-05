/** One native reader and one canonical producer per instance. No raw device pointer
 * escapes its lease. The receiver reads a separate aligned SPSC common ring. */
#include "ngene/frontend.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    TNgFrontend native;
    TNgRing *ring;
    TNgThread worker;
    TNgMutex mutex;
    TNgCondition changed;
    TNgAtomicInt stop;
    TNgFrontendStats stats;
    TNgError failure;
    TNgOverflowPolicy policy;
    uint64_t dropped, pending;
    int ready, started, hasRun, startupFailed, running, finished, failed;
} Common;

static void Publish(Common *s)
{
    TNgFrontendStats stats = {0};
    if (s->native.ops->stats) {
        s->native.ops->stats(s->native.context, &stats);
    }
    stats.droppedSamples += s->dropped;
    NgMutexLock(&s->mutex);
    s->stats = stats;
    NgMutexUnlock(&s->mutex);
}

static void Worker(void *context)
{
    Common *s = context;
    TNgError error = {{0}};
    int rc, stopFailed = 0;
    rc = s->native.ops->start(s->native.context, &error);
    NgMutexLock(&s->mutex);
    s->ready = 1;
    s->running = rc == NG_OK;
    if (rc != NG_OK) {
        s->startupFailed = 1;
        s->failed = 1;
        s->failure = error;
    }
    NgConditionSignal(&s->changed);
    NgMutexUnlock(&s->mutex);
    while (rc == NG_OK && !NgAtomicGet(&s->stop)) {
        TNgIqView view = {0};
        rc = s->native.ops->acquire(s->native.context, &view, 50, &error);
        if (rc == NG_TIMEOUT) {
            rc = NG_OK;
            continue;
        }
        if (rc == NG_END) {
            rc = NG_OK;
            break;
        }
        if (rc != NG_OK) {
            break;
        }
        if (s->pending) {
            int unknown = (view.meta.flags & NG_BLOCK_GAP) != 0 && !view.meta.missingKnown;
            view.meta.flags |= NG_BLOCK_GAP;
            if (!unknown) {
                if (UINT64_MAX - view.meta.missingBefore < s->pending) {
                    rc = NgFail(&error, "Common stream gap overflow");
                } else {
                    view.meta.missingBefore += s->pending;
                    view.meta.missingKnown = 1;
                }
            }
        }
        if (rc == NG_OK) {
            /* A live native queue can deliver many completed reads in one
             * scheduling slice. Give the consumer a bounded chance to release
             * a slot instead of declaring loss as soon as the worker catches it.
             * Native overflow/disconnection remains the adapter's responsibility;
             * a genuinely stalled consumer still fails after the 50 ms wait. */
            rc = s->policy == NG_OVERFLOW_DROP_NEWEST
                     ? NgRingPushConverted(s->ring, &view)
                     : NgRingPushConvertedWait(s->ring, &view, 50);
            /* Replay has no RF deadline and may keep waiting, polling stop. */
            while (rc == NG_FULL && s->native.info.sourceName &&
                   strcmp(s->native.info.sourceName, "file") == 0 && !NgAtomicGet(&s->stop)) {
                rc = NgRingPushConvertedWait(s->ring, &view, 50);
            }
        }
        s->native.ops->release(s->native.context);
        if (rc == NG_FULL) {
            if (UINT64_MAX - s->dropped < view.meta.count ||
                UINT64_MAX - s->pending < view.meta.count) {
                rc = NgFail(&error, "Common stream counter overflow");
            } else {
                s->dropped += view.meta.count;
                s->pending += view.meta.count;
                rc = s->policy == NG_OVERFLOW_STOP
                         ? NgFail(&error, "Common stream reader stalled for 50 ms; ring full, continuity lost")
                         : NG_OK;
            }
        } else if (rc == NG_OK) {
            s->pending = 0;
        } else if (!error.message[0]) {
            NgFail(&error, "Common stream conversion/publication failed");
        }
        Publish(s);
    }
    if (s->running) {
        TNgError stopError = {{0}};
        if (s->native.ops->stop(s->native.context, &stopError) != NG_OK && rc == NG_OK) {
            rc = NG_ERROR;
            error = stopError;
            stopFailed = 1;
        }
    }
    Publish(s);
    NgMutexLock(&s->mutex);
    if (rc != NG_OK && (!NgAtomicGet(&s->stop) || stopFailed)) {
        s->failed = 1;
        s->failure = error;
    }
    s->finished = 1;
    NgMutexUnlock(&s->mutex);
    NgRingClose(s->ring);
}

static int Start(void *context, TNgError *error)
{
    Common *s = context;
    if (s->hasRun) {
        return NgFail(error, "Common stream supports one run");
    }
    if (NgThreadStart(&s->worker, Worker, s) != 0) {
        return NgFail(error, "Cannot start common stream worker");
    }
    s->started = 1;
    s->hasRun = 1;
    NgMutexLock(&s->mutex);
    while (!s->ready) {
        NgConditionWait(&s->changed, &s->mutex, 100);
    }
    if (s->startupFailed) {
        if (error) {
            *error = s->failure;
        }
        NgMutexUnlock(&s->mutex);
        return NG_ERROR;
    }
    NgMutexUnlock(&s->mutex);
    return NG_OK;
}

static int Acquire(void *context, TNgIqView *view, unsigned timeout, TNgError *error)
{
    Common *s = context;
    int rc = NgRingAcquire(s->ring, view, timeout);
    if (rc != NG_OK) {
        NgMutexLock(&s->mutex);
        if (s->failed) {
            if (error) {
                *error = s->failure;
            }
            rc = NG_ERROR;
        }
        NgMutexUnlock(&s->mutex);
    }
    return rc;
}

static void Release(void *context)
{
    NgRingRelease(((Common *)context)->ring);
}

static int Stop(void *context, TNgError *error)
{
    Common *s = context;
    int failed;
    NgAtomicSet(&s->stop, 1);
    if (s->started) {
        NgThreadJoin(s->worker);
        s->started = 0;
    }
    NgMutexLock(&s->mutex);
    failed = s->failed;
    if (failed && error) {
        *error = s->failure;
    }
    NgMutexUnlock(&s->mutex);
    return failed ? NG_ERROR : NG_OK;
}

static void Stats(void *context, TNgFrontendStats *stats)
{
    Common *s = context;
    int finished;
    NgMutexLock(&s->mutex);
    *stats = s->stats;
    finished = s->finished;
    NgMutexUnlock(&s->mutex);
    NgRingSnapshot(s->ring, s->native.info.sampleRateHz, &stats->ingress);
    stats->ingressAvailable = 1;
    stats->queuedSamples += stats->ingress.pendingSamples;
    if (finished) {
        stats->discardedOnStop += stats->ingress.pendingSamples;
    }
}

static void Destroy(void *context)
{
    Common *s = context;
    TNgError error = {{0}};
    Stop(s, &error);
    NgFrontendDestroy(&s->native);
    NgRingDestroy(s->ring);
    NgConditionDestroy(&s->changed);
    NgMutexDestroy(&s->mutex);
    free(s);
}

static const TNgFrontendOps ops = {Start, Acquire, Release, Stop, Stats, Destroy};

int NgFrontendCommon(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    Common *s = calloc(1, sizeof(*s));
    TNgSampleFormat target;
    if (!s) {
        return NgFail(error, "Cannot allocate common frontend");
    }
    if (NgMutexInit(&s->mutex) != 0) {
        free(s);
        return NgFail(error, "Cannot initialize common mutex");
    }
    if (NgConditionInit(&s->changed) != 0) {
        NgMutexDestroy(&s->mutex);
        free(s);
        return NgFail(error, "Cannot initialize common condition");
    }
    s->native = *frontend;
    s->policy = config->overflowPolicy;
    target = config->streamFormat                         ? config->streamFormat
             : NgSampleBytes(frontend->info.format) == 4u ? NG_FORMAT_CI16
                                                          : NG_FORMAT_CI8;
    if (NgRingCreateFormat(config->ingressBlocks, config->windowSamples, target, &s->ring, error) !=
        NG_OK) {
        NgConditionDestroy(&s->changed);
        NgMutexDestroy(&s->mutex);
        free(s);
        return NG_ERROR;
    }
    if (!frontend->info.adcBits) {
        frontend->info.adcBits = frontend->info.format == NG_FORMAT_SC16_Q11 ? 12u
                                 : frontend->info.format == NG_FORMAT_CI16   ? 16u
                                                                             : 8u;
    }
    frontend->info.nativeFormat = frontend->info.format;
    frontend->info.format = target;
    frontend->ops = &ops;
    frontend->context = s;
    return NG_OK;
}

int NgFrontendOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    if (NgFrontendOpenNative(config, frontend, error) != NG_OK) {
        return NG_ERROR;
    }
    if (NgFrontendCommon(config, frontend, error) != NG_OK) {
        NgFrontendDestroy(frontend);
        return NG_ERROR;
    }
    return NG_OK;
}
