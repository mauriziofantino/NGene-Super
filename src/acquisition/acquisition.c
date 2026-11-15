/** @file acquisition.c @brief Bounded ring subscriber and cancellable acquisition worker. */
#include "ngene/acquisition.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <string.h>
/** Default strategy workspace implemented separately from scheduling/ownership. */
int NgAcquisitionGpsWorkspace(const TNgStreamInfo *stream, void **out, TNgError *error);
/** Release the default workspace after its worker has joined. */
void NgAcquisitionGpsFree(void *context);
/** Default GPS operations, immutable for the process lifetime. */
extern const TNgAcquisitionStrategy ngGpsAcquisitionStrategy;
/** Copy default-strategy display data on its owning worker only. */
void NgAcquisitionGpsSearchSpace(void *context, TNgAcquisitionSearchSpace *space);

/** Galileo E1-B strategy resources (PRNs 1..32 in this increment). */
int NgAcquisitionGalileoWorkspace(const TNgStreamInfo *stream, void **out, TNgError *error);
/** Free Galileo private DSP resources. */
void NgAcquisitionGalileoFree(void *context);
/** Copy completed Galileo display data on its worker. */
void NgAcquisitionGalileoSearchSpace(void *context, TNgAcquisitionSearchSpace *space);
/** Galileo E1-B operations. */
extern const TNgAcquisitionStrategy ngGalileoAcquisitionStrategy;
/** Deferred display reduction; seeds are already available to tracking before these run. */
int NgAcquisitionGpsDisplay(void *context, unsigned prn, const TNgAcquisitionConfig *config,
                            TNgAcquisitionCancel cancel, void *cancelContext);
/** Galileo equivalent of the deferred confirmed-map calculation. */
int NgAcquisitionGalileoDisplay(void *context, unsigned prn, const TNgAcquisitionConfig *config,
                                TNgAcquisitionCancel cancel, void *cancelContext);

/** Shared pool; its mutex protects admission and per-capture job ownership. */
typedef struct TNgAcquisitionPool TNgAcquisitionPool;

/** One subscription retains one immutable capture until every claimed PRN finishes. */
struct TNgAcquisition {
    unsigned observedPrn;           /**< Display lease, protected by mutex. */
    double observedUntil;           /**< Monotonic lease expiry. */
    TNgAtomicInt yieldMilliseconds; /**< Cooperative stage budget. */
    uint32_t excludedMask;          /**< Assigned satellites; protected by mutex. */
    int paused;                     /**< New admission paused; protected by mutex. */
    TNgAcquisitionConfig config;    /**< Frozen decisions. */
    TNgAcquisitionCapture capture;  /**< Published capture geometry. */
    TNgComplex *storage;            /**< Native float capture, producer-owned while slot is zero. */
    size_t filled, capacity;        /**< Capture assembly counts. */
    uint64_t expectedSample, epoch; /**< Producer continuity. */
    int haveEpoch;                  /**< Continuity has an origin. */
    TNgAtomicInt slot, stop, generation; /**< Capture ownership and cancellation. */
    int captureGeneration;               /**< Frozen before capture publication. */
    TNgMutex mutex;                  /**< Snapshot/admission protection; never held during DSP. */
    TNgAcquisitionSnapshot snapshot; /**< Completed outcomes and live job identities. */
    uint64_t skipped, gaps;          /**< Producer counters. */
    const TNgAcquisitionStrategy *strategy;      /**< Immutable algorithm. */
    void *contexts[NG_ACQUISITION_MAX_PARALLEL]; /**< Exclusive workspace per claimed lane. */
    unsigned contextCount;                       /**< Prepared lane count. */
    int ownsContext;                             /**< Default strategies own their workspaces. */
    TNgAcquisitionSearchSpace spaces[32];        /**< Completed display archives. */
    TNgAcquisitionPool *pool;                    /**< Shared scheduler. */
    unsigned nextPrn, activeJobs; /**< Pool-mutex-protected capture cursor and borrowers. */
    unsigned quota;               /**< Free receiver slots, published under pool mutex. */
    uint32_t busyContexts;        /**< Pool-mutex-protected exclusive lanes. */
};

/** A worker has no RF cursor; it borrows one published capture and a private workspace. */
typedef struct {
    TNgAcquisition *acquisition;   /**< Borrowed subscription, retained until job release. */
    TNgAcquisitionCapture capture; /**< Frozen per-job observer/capture geometry. */
    unsigned prn, lane;            /**< PRN and exclusively claimed workspace. */
    int generation;                /**< Capture cancellation generation. */
} TNgAcquisitionJob;

/** All constellation jobs share this bounded set of OS workers. */
struct TNgAcquisitionPool {
    TNgMutex mutex;          /**< Admission, ownership and shutdown protection. */
    TNgCondition work, idle; /**< Published work and completed borrowers. */
    TNgThread workers[NG_ACQUISITION_MAX_PARALLEL]; /**< Prepared worker handles. */
    unsigned started, capacity, active, nextSystem; /**< Pool counters and fair cursor. */
    TNgAtomicInt limit;                             /**< Adaptive shared admission cap. */
    int stopping;                                   /**< Pool shutdown predicate. */
    TNgAcquisition *subscriptions[2]; /**< GPS/Galileo or caller strategy subscriptions. */
};

/** Reserve CPU allowance for ingress, with at least four logical acquisition lanes. */
static unsigned Capacity(void)
{
    unsigned count = NgLogicalProcessorCount();
    if (count > 1)
        --count;
    if (count < 4)
        count = 4;
    return count > NG_ACQUISITION_MAX_PARALLEL ? NG_ACQUISITION_MAX_PARALLEL : count;
}

/** Wake without retaining any subscription mutex. */
static void Wake(TNgAcquisitionPool *pool)
{
    NgMutexLock(&pool->mutex);
    NgConditionSignal(&pool->work);
    NgMutexUnlock(&pool->mutex);
}

/** A job retains its own cancellation generation; concurrent PRNs never overwrite it. */
static int Cancelled(void *context)
{
    TNgAcquisitionJob *job = context;
    return NgAtomicGet(&job->acquisition->stop) ||
           NgAtomicGet(&job->acquisition->generation) != job->generation;
}

/** Background stage boundary; no snapshot or scheduler mutex is held here. */
static int WorkCancelled(void *context)
{
    TNgAcquisitionJob *job = context;
    int delay = NgAtomicGet(&job->acquisition->yieldMilliseconds);
    if (Cancelled(job))
        return 1;
    if (delay)
        NgSleepMs((unsigned)delay);
    return Cancelled(job);
}

/** Update the compatibility PRN and release a capture only after its last borrower. */
static void RefreshJobs(TNgAcquisition *a)
{
    unsigned k;
    a->snapshot.activeSearches = 0;
    a->snapshot.currentPrn = 0;
    for (k = 0; k < a->contextCount; ++k)
        if (a->snapshot.currentPrns[k]) {
            if (!a->snapshot.currentPrn)
                a->snapshot.currentPrn = a->snapshot.currentPrns[k];
            a->snapshot.activeSearches++;
        }
    a->snapshot.searching = a->snapshot.activeSearches != 0;
    if (!a->activeJobs && a->nextPrn > a->config.lastPrn)
        NgAtomicSet(&a->slot, 0);
}

/** Claim one PRN fairly across ready constellations; caller holds pool mutex. */
static int Claim(TNgAcquisitionPool *pool, TNgAcquisitionJob *job)
{
    unsigned attempt;
    for (attempt = 0; attempt < 2; ++attempt) {
        unsigned system = (pool->nextSystem + attempt) % 2;
        TNgAcquisition *a = pool->subscriptions[system];
        unsigned prn, lane;
        if (!a || !NgAtomicGet(&a->slot))
            continue;
        NgMutexLock(&a->mutex);
        if (NgAtomicGet(&a->stop) || a->paused ||
            NgAtomicGet(&a->generation) != a->captureGeneration)
            a->nextPrn = a->config.lastPrn + 1;
        while (a->nextPrn <= a->config.lastPrn &&
               (a->excludedMask & (UINT32_C(1) << (a->nextPrn - 1))))
            ++a->nextPrn;
        if (a->nextPrn > a->config.lastPrn) {
            RefreshJobs(a);
            NgMutexUnlock(&a->mutex);
            continue;
        }
        if (pool->active >= (unsigned)NgAtomicGet(&pool->limit) || a->activeJobs >= a->quota ||
            a->activeJobs >= a->contextCount) {
            NgMutexUnlock(&a->mutex);
            continue;
        }
        prn = a->nextPrn++;
        for (lane = 0; lane < a->contextCount; ++lane)
            if (!(a->busyContexts & (UINT32_C(1) << lane)))
                break;
        a->busyContexts |= UINT32_C(1) << lane;
        a->activeJobs++;
        pool->active++;
        a->snapshot.currentPrns[lane] = prn;
        RefreshJobs(a);
        job->acquisition = a;
        job->capture = a->capture;
        job->capture.displayPrn = NgMonotonicSeconds() < a->observedUntil ? a->observedPrn : 0;
        job->generation = a->captureGeneration;
        job->prn = prn;
        job->lane = lane;
        pool->nextSystem = 1 - system;
        NgMutexUnlock(&a->mutex);
        return 1;
    }
    return 0;
}

/** Copy a native strategy's completed map on the worker that owns its private workspace. */
static void CopySpace(TNgAcquisitionJob *job, const TNgAcquisitionResult *result)
{
    TNgAcquisition *a = job->acquisition;
    TNgAcquisitionSearchSpace *space = &a->spaces[job->prn - 1];
    if (a->strategy == &ngGalileoAcquisitionStrategy)
        NgAcquisitionGalileoSearchSpace(a->contexts[job->lane], space);
    else
        NgAcquisitionGpsSearchSpace(a->contexts[job->lane], space);
    space->result = *result;
}

/** Execute one unchanged decision and then its optional, lower-priority map. */
static void Execute(TNgAcquisitionJob *job)
{
    TNgAcquisition *a = job->acquisition;
    TNgAcquisitionResult result = {0};
    TNgError error = {{0}};
    void *context = a->contexts[job->lane];
    int native =
        a->strategy == &ngGpsAcquisitionStrategy || a->strategy == &ngGalileoAcquisitionStrategy;
    int status = a->strategy->search(context, &job->capture, job->prn, &a->config, WorkCancelled,
                                     job, &result, &error);
    result.completedSeconds = NgMonotonicSeconds();
    NgMutexLock(&a->mutex);
    if (!Cancelled(job)) {
        a->snapshot.results[job->prn - 1] = result;
        a->snapshot.searches++;
        a->snapshot.currentPrns[job->lane] = 0;
        RefreshJobs(a);
        if (status == NG_OK && native && job->capture.displayPrn == job->prn)
            CopySpace(job, &result);
        if (status == NG_ERROR)
            snprintf(a->snapshot.message, sizeof(a->snapshot.message), "%s", error.message);
    }
    NgMutexUnlock(&a->mutex);
    if (status == NG_OK && native && result.confirmed && !Cancelled(job) &&
        job->capture.displayPrn != job->prn) {
        int display =
            a->strategy == &ngGpsAcquisitionStrategy
                ? NgAcquisitionGpsDisplay(context, job->prn, &a->config, WorkCancelled, job)
                : NgAcquisitionGalileoDisplay(context, job->prn, &a->config, WorkCancelled, job);
        if (display == NG_OK && !Cancelled(job)) {
            NgMutexLock(&a->mutex);
            CopySpace(job, &result);
            NgMutexUnlock(&a->mutex);
        }
    }
}

/** Workers claim bounded jobs; captures are released only after all their jobs finish. */
static void SearchWorker(void *context)
{
    TNgAcquisitionPool *pool = context;
    NgThreadSchedulingPriority(-1);
    NgMutexLock(&pool->mutex);
    while (!pool->stopping) {
        TNgAcquisitionJob job;
        if (!Claim(pool, &job)) {
            NgConditionWait(&pool->work, &pool->mutex, UINT_MAX);
            continue;
        }
        NgMutexUnlock(&pool->mutex);
        Execute(&job);
        NgMutexLock(&pool->mutex);
        TNgAcquisition *a = job.acquisition;
        NgMutexLock(&a->mutex);
        a->activeJobs--;
        pool->active--;
        a->busyContexts &= ~(UINT32_C(1) << job.lane);
        a->snapshot.currentPrns[job.lane] = 0;
        RefreshJobs(a);
        NgMutexUnlock(&a->mutex);
        NgConditionSignal(&pool->idle);
        NgConditionSignal(&pool->work);
    }
    NgMutexUnlock(&pool->mutex);
}

/** Stop an idle pool before linking subscriptions or releasing its final owner. */
static void DestroyPool(TNgAcquisitionPool *pool)
{
    unsigned k;
    if (!pool)
        return;
    NgMutexLock(&pool->mutex);
    pool->stopping = 1;
    NgConditionSignal(&pool->work);
    NgMutexUnlock(&pool->mutex);
    for (k = 0; k < pool->started; ++k)
        NgThreadJoin(pool->workers[k]);
    NgConditionDestroy(&pool->idle);
    NgConditionDestroy(&pool->work);
    NgMutexDestroy(&pool->mutex);
    free(pool);
}

/** Prepare all threads before RX; the pool never allocates in the sample path. */
static TNgAcquisitionPool *CreatePool(TNgAcquisition *a, TNgAcquisition *b, TNgError *error)
{
    TNgAcquisitionPool *pool = calloc(1, sizeof(*pool));
    unsigned wanted = a->contextCount + (b ? b->contextCount : 0), k;
    if (!pool) {
        NgFail(error, "Cannot allocate acquisition pool");
        return NULL;
    }
    if (NgMutexInit(&pool->mutex)) {
        free(pool);
        NgFail(error, "Cannot initialize acquisition pool");
        return NULL;
    }
    if (NgConditionInit(&pool->work)) {
        NgMutexDestroy(&pool->mutex);
        free(pool);
        NgFail(error, "Cannot initialize acquisition work condition");
        return NULL;
    }
    if (NgConditionInit(&pool->idle)) {
        NgConditionDestroy(&pool->work);
        NgMutexDestroy(&pool->mutex);
        free(pool);
        NgFail(error, "Cannot initialize acquisition completion condition");
        return NULL;
    }
    pool->subscriptions[0] = a;
    pool->subscriptions[1] = b;
    pool->capacity = wanted < Capacity() ? wanted : Capacity();
    NgAtomicSet(&pool->limit, (int)pool->capacity);
    for (k = 0; k < pool->capacity; ++k) {
        if (NgThreadStart(&pool->workers[k], SearchWorker, pool)) {
            DestroyPool(pool);
            NgFail(error, "Cannot prepare acquisition pool worker");
            return NULL;
        }
        pool->started++;
    }
    return pool;
}

/** Default first functional GPS configuration. */
void NgAcquisitionDefaults(TNgAcquisitionConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->enabled = 1;
    config->falseAlarmProbability = 1e-9;
    config->firstPrn = 1;
    config->lastPrn = 32;
}

/** Reject malformed module options before worker creation. */
int NgAcquisitionValidate(const TNgAcquisitionConfig *config, TNgError *error)
{
    if (!isfinite(config->falseAlarmProbability) || config->falseAlarmProbability <= 0 ||
        config->falseAlarmProbability >= 1 || config->firstPrn < 1 || config->lastPrn > 32 ||
        config->lastPrn < config->firstPrn || (config->enabled != 0 && config->enabled != 1) ||
        (config->squareCarrier != 0 && config->squareCarrier != 1) ||
        (config->xorCorrelation != 0 && config->xorCorrelation != 1))
        return NgFail(error, "Invalid GPS acquisition PRN range or mode");
    return NG_OK;
}

/** Copy common-ring samples into the producer-owned slot; never retain the borrowed view. */
static int Process(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgAcquisition *acquisition = context;
    size_t k;
    size_t take;
    (void)id;
    (void)error;
    *next = state;
    if (!acquisition->haveEpoch || window->meta.epoch != acquisition->epoch ||
        window->meta.firstSample != acquisition->expectedSample ||
        (window->meta.flags & NG_BLOCK_GAP) != 0) {
        if (acquisition->haveEpoch)
            acquisition->gaps++;
        int generation = NgAtomicGet(&acquisition->generation);
        NgAtomicSet(&acquisition->generation, generation == INT_MAX ? 0 : generation + 1);
        acquisition->filled = 0;
        acquisition->epoch = window->meta.epoch;
        acquisition->haveEpoch = 1;
    }
    acquisition->expectedSample = window->meta.firstSample + window->meta.count;
    NgMutexLock(&acquisition->mutex);
    int paused = acquisition->paused;
    NgMutexUnlock(&acquisition->mutex);
    if (paused && NgAtomicGet(&acquisition->slot) == 0) {
        acquisition->filled = 0;
    } else if (NgAtomicGet(&acquisition->slot) != 0) {
        acquisition->skipped += window->meta.count;
    } else {
        if (acquisition->filled == 0)
            acquisition->capture.origin = window->meta;
        take = acquisition->capacity - acquisition->filled;
        if (take > window->meta.count)
            take = window->meta.count;
        for (k = 0; k < take; ++k) {
            acquisition->storage[acquisition->filled + k].i = (float)NgIqComponent(window, k, 0);
            acquisition->storage[acquisition->filled + k].q = (float)NgIqComponent(window, k, 1);
        }
        acquisition->filled += take;
        if (acquisition->filled == acquisition->capacity) {
            acquisition->capture.count = acquisition->capacity;
            acquisition->capture.origin.count = acquisition->capacity;
            acquisition->captureGeneration = NgAtomicGet(&acquisition->generation);
            acquisition->filled = 0;
            acquisition->skipped += window->meta.count - take;
            acquisition->nextPrn = acquisition->config.firstPrn;
            NgAtomicSet(&acquisition->slot, 1);
            Wake(acquisition->pool);
        }
    }
    if (NgMutexTryLock(&acquisition->mutex)) {
        acquisition->snapshot.skippedSamples = acquisition->skipped;
        acquisition->snapshot.gaps = acquisition->gaps;
        NgMutexUnlock(&acquisition->mutex);
    }
    return NG_OK;
}

/** Common-ring callback, native CI8/CI16 without narrowing. */
static const TNgChannelOps acquisitionOps = {
    Process, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL};

/** Release private workspaces; caller decides whether the supplied first context is owned. */
static void FreeContexts(TNgAcquisition *a, unsigned first)
{
    unsigned k;
    for (k = first; k < a->contextCount; ++k) {
        if (a->strategy == &ngGalileoAcquisitionStrategy)
            NgAcquisitionGalileoFree(a->contexts[k]);
        else if (a->strategy == &ngGpsAcquisitionStrategy)
            NgAcquisitionGpsFree(a->contexts[k]);
    }
}

/** Create exclusive native workspaces and a bounded standalone pool before reception. */
static int CreateWithDuration(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                              const TNgAcquisitionStrategy *strategy, void *context,
                              TNgAcquisition **out, TNgChannel *channel, TNgError *error,
                              unsigned durationMs)
{
    TNgAcquisition *a;
    unsigned k, wanted;
    int native = strategy == &ngGpsAcquisitionStrategy || strategy == &ngGalileoAcquisitionStrategy;
    *out = NULL;
    memset(channel, 0, sizeof(*channel));
    if (NgAcquisitionValidate(config, error) != NG_OK)
        return NG_ERROR;
    if (!config->enabled)
        return NG_OK;
    if (!stream || stream->sampleRateHz < 2048000 || stream->sampleRateHz > 20000000 || !strategy ||
        !strategy->search)
        return NgFail(error, "Acquisition supports native rates 2.048..20 Msps");
    a = calloc(1, sizeof(*a));
    if (!a)
        return NgFail(error, "Acquisition allocation failed");
    a->capacity = (size_t)(((uint64_t)stream->sampleRateHz * durationMs + 999u) / 1000u);
    a->storage = calloc(a->capacity, sizeof(*a->storage));
    if (!a->storage || NgMutexInit(&a->mutex)) {
        free(a->storage);
        free(a);
        return NgFail(error, "Acquisition capture/mutex allocation failed");
    }
    a->config = *config;
    a->strategy = strategy;
    a->capture.samples = a->storage;
    a->capture.stream = *stream;
    a->contexts[0] = context;
    a->contextCount = 1;
    wanted = native ? Capacity() : 1;
    if (wanted > config->lastPrn - config->firstPrn + 1)
        wanted = config->lastPrn - config->firstPrn + 1;
    for (k = 1; k < wanted; ++k) {
        int result = strategy == &ngGpsAcquisitionStrategy
                         ? NgAcquisitionGpsWorkspace(stream, &a->contexts[k], error)
                         : NgAcquisitionGalileoWorkspace(stream, &a->contexts[k], error);
        if (result != NG_OK) {
            FreeContexts(a, 1);
            NgMutexDestroy(&a->mutex);
            free(a->storage);
            free(a);
            return NG_ERROR;
        }
        a->contextCount++;
    }
    a->quota = wanted;
    a->nextPrn = config->firstPrn;
    a->snapshot.enabled = 1;
    a->pool = CreatePool(a, NULL, error);
    if (!a->pool) {
        FreeContexts(a, 1);
        NgMutexDestroy(&a->mutex);
        free(a->storage);
        free(a);
        return NG_ERROR;
    }
    channel->ops = &acquisitionOps;
    channel->context = a;
    channel->state = NG_CHANNEL_ACQUISITION;
    channel->id.constellation = 1;
    channel->id.signal = 1;
    *out = a;
    return NG_OK;
}

/** Custom strategies remain exclusive unless the caller supplies separate subscriptions. */
int NgAcquisitionCreateWithStrategy(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                                    const TNgAcquisitionStrategy *strategy, void *context,
                                    TNgAcquisition **out, TNgChannel *channel, TNgError *error)
{
    return CreateWithDuration(config, stream, strategy, context, out, channel, error, 56);
}

/** Register native GPS with private SIMD workspaces on one frozen shared capture. */
int NgAcquisitionCreate(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                        TNgAcquisition **out, TNgChannel *channel, TNgError *error)
{
    void *workspace = NULL;
    int status;
    *out = NULL;
    if (NgAcquisitionValidate(config, error) != NG_OK)
        return NG_ERROR;
    if (config->enabled && NgAcquisitionGpsWorkspace(stream, &workspace, error) != NG_OK)
        return NG_ERROR;
    status = CreateWithDuration(config, stream, &ngGpsAcquisitionStrategy, workspace, out, channel,
                                error, 56);
    if (status != NG_OK || !*out)
        NgAcquisitionGpsFree(workspace);
    else
        (*out)->ownsContext = 1;
    return status;
}

/** Register native Galileo; linking shares workers, not mutable DSP state or RF cursors. */
int NgAcquisitionCreateGalileo(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                               TNgAcquisition **out, TNgChannel *channel, TNgError *error)
{
    void *workspace = NULL;
    int status;
    *out = NULL;
    if (NgAcquisitionValidate(config, error) != NG_OK)
        return NG_ERROR;
    if (config->enabled && NgAcquisitionGalileoWorkspace(stream, &workspace, error) != NG_OK)
        return NG_ERROR;
    status = CreateWithDuration(config, stream, &ngGalileoAcquisitionStrategy, workspace, out,
                                channel, error, 224);
    if (status != NG_OK || !*out)
        NgAcquisitionGalileoFree(workspace);
    else {
        (*out)->ownsContext = 1;
        channel->id.instanceId = 1;
        channel->id.constellation = 2;
    }
    return status;
}

/** Merge idle subscriptions before their first common window. */
int NgAcquisitionLink(TNgAcquisition *a, TNgAcquisition *b, TNgError *error)
{
    TNgAcquisitionPool *pool;
    if (!a || !b || a->pool == b->pool)
        return NG_OK;
    if (NgAtomicGet(&a->slot) || NgAtomicGet(&b->slot))
        return NgFail(error, "Acquisition pools must link before RX");
    DestroyPool(a->pool);
    DestroyPool(b->pool);
    a->pool = b->pool = NULL;
    pool = CreatePool(a, b, error);
    if (!pool)
        return NG_ERROR;
    a->pool = b->pool = pool;
    return NG_OK;
}

/** Detach after RX; no workspace is freed until all outstanding borrowers return. */
void NgAcquisitionDestroy(TNgAcquisition *a)
{
    TNgAcquisitionPool *pool;
    unsigned k;
    int remaining = 0;
    if (!a)
        return;
    NgAtomicSet(&a->stop, 1);
    pool = a->pool;
    if (pool) {
        NgMutexLock(&pool->mutex);
        NgConditionSignal(&pool->work);
        while (a->activeJobs)
            NgConditionWait(&pool->idle, &pool->mutex, UINT_MAX);
        for (k = 0; k < 2; k++) {
            if (pool->subscriptions[k] == a)
                pool->subscriptions[k] = NULL;
            if (pool->subscriptions[k])
                remaining = 1;
        }
        NgMutexUnlock(&pool->mutex);
        if (!remaining)
            DestroyPool(pool);
    }
    if (a->ownsContext)
        FreeContexts(a, 0);
    else
        FreeContexts(a, 1);
    NgMutexDestroy(&a->mutex);
    free(a->storage);
    free(a);
}

/** Publish slot masks and wake only when eligibility changes. */
void NgAcquisitionSetAdmission(TNgAcquisition *a, uint32_t mask, int paused)
{
    int changed;
    if (!a)
        return;
    NgMutexLock(&a->mutex);
    changed = a->excludedMask != mask || a->paused != (paused != 0);
    a->excludedMask = mask;
    a->paused = paused != 0;
    NgMutexUnlock(&a->mutex);
    if (changed)
        Wake(a->pool);
}

/** Publish adaptive total concurrency and this subscription's free-slot quota. */
void NgAcquisitionSetParallelism(TNgAcquisition *a, unsigned totalLimit, unsigned quota)
{
    TNgAcquisitionPool *pool;
    if (!a || !a->pool)
        return;
    pool = a->pool;
    if (totalLimit > pool->capacity)
        totalLimit = pool->capacity;
    if ((unsigned)NgAtomicGet(&pool->limit) == totalLimit && a->quota == quota)
        return;
    NgMutexLock(&pool->mutex);
    int changed = (unsigned)NgAtomicGet(&pool->limit) != totalLimit || a->quota != quota;
    NgAtomicSet(&pool->limit, (int)totalLimit);
    a->quota = quota;
    if (changed)
        NgConditionSignal(&pool->work);
    NgMutexUnlock(&pool->mutex);
}

/** Publish display interest without touching search decisions or the owned capture. */
void NgAcquisitionObserve(TNgAcquisition *acquisition, unsigned prn)
{
    if (!acquisition)
        return;
    NgMutexLock(&acquisition->mutex);
    acquisition->observedPrn = prn <= 32 ? prn : 0;
    acquisition->observedUntil = NgMonotonicSeconds() + 2;
    NgMutexUnlock(&acquisition->mutex);
}

/** Scheduling budget affects only cooperative waiting, never acquisition arithmetic. */
void NgAcquisitionSetBudget(TNgAcquisition *acquisition, unsigned yieldMilliseconds)
{
    if (acquisition)
        NgAtomicSet(&acquisition->yieldMilliseconds,
                    (int)(yieldMilliseconds > 20 ? 20 : yieldMilliseconds));
}

/** Snapshot memory is copied under its own short lock. */
void NgAcquisitionRead(TNgAcquisition *acquisition, TNgAcquisitionSnapshot *snapshot)
{
    if (acquisition == NULL) {
        memset(snapshot, 0, sizeof(*snapshot));
        return;
    }
    NgMutexLock(&acquisition->mutex);
    *snapshot = acquisition->snapshot;
    if (acquisition->pool) {
        snapshot->parallelCapacity = acquisition->pool->capacity;
        snapshot->parallelLimit = (unsigned)NgAtomicGet(&acquisition->pool->limit);
    }
    NgMutexUnlock(&acquisition->mutex);
    if (acquisition->pool) {
        NgMutexLock(&acquisition->pool->mutex);
        snapshot->poolActive = acquisition->pool->active;
        NgMutexUnlock(&acquisition->pool->mutex);
    }
}

void NgAcquisitionReadSearchSpace(TNgAcquisition *acquisition, unsigned prn,
                                  TNgAcquisitionSearchSpace *space)
{
    memset(space, 0, sizeof(*space));
    if (acquisition == NULL || prn < 1 || prn > 32)
        return;
    NgMutexLock(&acquisition->mutex);
    *space = acquisition->spaces[prn - 1];
    NgMutexUnlock(&acquisition->mutex);
}
