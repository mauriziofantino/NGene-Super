/** @file acquisition.c @brief Bounded ring subscriber and cancellable acquisition worker. */
#include "ngene/acquisition.h"
#include "acquisition/grid.h"
#include "acquisition/workspace.h"
#include "platform/platform.h"
#include <inttypes.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
/** Copy default-strategy display data on its owning worker only. */
void NgAcquisitionGpsSearchSpace(void *context, TNgAcquisitionSearchSpace *space);

/** Copy completed Galileo display data on its worker. */
void NgAcquisitionGalileoSearchSpace(void *context, TNgAcquisitionSearchSpace *space);
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
    uint64_t excludedMask;          /**< Assigned satellites; protected by mutex. */
    int paused;                     /**< New admission paused; protected by mutex. */
    TNgAcquisitionConfig config;    /**< Frozen decisions. */
    TNgAcquisitionCapture capture;  /**< Published capture geometry. */
#ifdef NGENE_PROFILE_ACQUISITION_JOBS
    double capturePublished;
#endif
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
    TNgConstellation constellation;              /**< Namespace of the runner's PRNs. */
    TNgAcquisitionResult highResults[NG_SATELLITE_MAX_PRN - NG_LEGACY_MAX_PRN];
    TNgAcquisitionSearchSpace *spaces;            /**< Completed display archives. */
    TNgAcquisitionPool *pool;                     /**< Shared scheduler. */
    unsigned nextPrn, captureLastPrn, activeJobs; /**< Published capture cursor and borrowers. */
    unsigned nextCapturePrn; /**< Producer cursor for bounded B1C search batches. */
    unsigned quota;          /**< Free receiver slots, published under pool mutex. */
    uint32_t busyContexts;   /**< Pool-mutex-protected exclusive lanes. */
};

/** A worker has no RF cursor; it borrows one published capture and a private workspace. */
typedef struct {
    TNgAcquisition *acquisition;   /**< Borrowed subscription, retained until job release. */
    TNgAcquisitionCapture capture; /**< Frozen per-job observer/capture geometry. */
    unsigned prn;
    unsigned lane;  /**< PRN and exclusively claimed workspace. */
    int generation; /**< Capture cancellation generation. */
    int permit;     /**< Owns one cooperative DSP-stage permit. */
    double cpuMark; /**< Thread CPU seconds at the last charged boundary. */
#ifdef NGENE_PROFILE_ACQUISITION_JOBS
    double published, claimed, permitWait;
#endif
} TNgAcquisitionJob;

/** All constellation jobs share this bounded set of OS workers. */
struct TNgAcquisitionPool {
    TNgMutex mutex;          /**< Admission, ownership and shutdown protection. */
    TNgCondition work, idle; /**< Published work and completed borrowers. */
    TNgThread workers[NG_ACQUISITION_MAX_PARALLEL]; /**< Prepared worker handles. */
    unsigned started, capacity, active, nextSystem; /**< Pool counters and fair cursor. */
    TNgAtomicInt limit;                             /**< Adaptive shared admission cap. */
    TNgAtomicInt executionLimit;                    /**< Adaptive cap, including borrowed jobs. */
    unsigned executing;                             /**< Stages currently holding a permit. */
    int stopping;                                   /**< Pool shutdown predicate. */
    TNgAcquisition *subscriptions[NG_CONSTELLATION_COUNT];
    unsigned subscriptionCount; /**< Stable slots, including detached subscriptions. */
    double cpuBudget;           /**< CPU seconds per signal second; zero disables the budget. */
    double cpuCredit;           /**< Remaining CPU seconds; may go negative by one stage. */
    int budgetActive;           /**< A receiver advances signal time for this pool. */
    int haveSignal;             /**< signalEpoch/signalSample hold the last credited position. */
    uint64_t signalEpoch, signalSample; /**< Last credited signal position. */
};

/** Unused credit is capped at this much signal time worth of budget, so an idle period
 * cannot buy a later burst; a new budget starts with the full cap. */
#define NG_ACQUISITION_CREDIT_SECONDS 0.25

/** Four prepared search lanes. Lanes only hold workspaces and idle threads: actual CPU use
 * is bounded by the execution limit and, under a receiver, by the signal-time CPU budget.
 * More lanes would add memory and runnable threads that compete with tracking. */
static unsigned Capacity(void)
{
    return 4u;
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
    TNgAcquisitionPool *pool = job->acquisition->pool;
    int delay = NgAtomicGet(&job->acquisition->yieldMilliseconds);
    double cpu = NgThreadCpuSeconds();
    NgMutexLock(&pool->mutex);
    if (pool->budgetActive) {
        pool->cpuCredit -= cpu - job->cpuMark;
    }
    job->cpuMark = cpu;
    if (job->permit) {
        --pool->executing;
        job->permit = 0;
        NgConditionSignal(&pool->work);
    }
    NgMutexUnlock(&pool->mutex);
    if (Cancelled(job)) {
        return 1;
    }
    if (delay) {
        NgSleepMs((unsigned)delay);
    }
#ifdef NGENE_PROFILE_ACQUISITION_JOBS
    double startedWaiting = NgMonotonicSeconds();
#endif
    NgMutexLock(&pool->mutex);
    while (!pool->stopping && !Cancelled(job) &&
           (pool->executing >= (unsigned)NgAtomicGet(&pool->executionLimit) ||
            (pool->budgetActive && pool->cpuCredit <= 0))) {
        NgConditionWait(&pool->work, &pool->mutex, 20);
    }
    int cancelled = pool->stopping || Cancelled(job);
    if (!cancelled) {
        ++pool->executing;
        job->permit = 1;
    }
    NgMutexUnlock(&pool->mutex);
#ifdef NGENE_PROFILE_ACQUISITION_JOBS
    job->permitWait += NgMonotonicSeconds() - startedWaiting;
#endif
    /* Waiting is not charged: the next stage is measured from here. */
    job->cpuMark = NgThreadCpuSeconds();
    return cancelled;
}

/** Update the compatibility PRN and release a capture only after its last borrower. */
static void RefreshJobs(TNgAcquisition *acquisition)
{
    unsigned contextIndex;
    acquisition->snapshot.activeSearches = 0;
    acquisition->snapshot.currentPrn = 0;
    for (contextIndex = 0; contextIndex < acquisition->contextCount; ++contextIndex) {
        if (acquisition->snapshot.currentPrns[contextIndex]) {
            if (!acquisition->snapshot.currentPrn) {
                acquisition->snapshot.currentPrn = acquisition->snapshot.currentPrns[contextIndex];
            }
            acquisition->snapshot.activeSearches++;
        }
    }
    acquisition->snapshot.searching = acquisition->snapshot.activeSearches != 0;
    if (!acquisition->activeJobs && acquisition->nextPrn > acquisition->captureLastPrn) {
        NgAtomicSet(&acquisition->slot, 0);
    }
}

/** Claim one PRN fairly across ready constellations; caller holds pool mutex. */
static int Claim(TNgAcquisitionPool *pool, TNgAcquisitionJob *job)
{
    unsigned attempt;
    if (pool->budgetActive && pool->cpuCredit <= 0) {
        return 0;
    }
    for (attempt = 0; attempt < pool->subscriptionCount; ++attempt) {
        unsigned system = (pool->nextSystem + attempt) % pool->subscriptionCount;
        TNgAcquisition *acquisition = pool->subscriptions[system];
        unsigned prn, lane;
        if (!acquisition || !NgAtomicGet(&acquisition->slot)) {
            continue;
        }
        NgMutexLock(&acquisition->mutex);
        if (NgAtomicGet(&acquisition->stop) || acquisition->paused ||
            NgAtomicGet(&acquisition->generation) != acquisition->captureGeneration) {
            acquisition->nextPrn = acquisition->captureLastPrn + 1;
        }
        while (acquisition->nextPrn <= acquisition->captureLastPrn &&
               (acquisition->excludedMask & (UINT64_C(1) << (acquisition->nextPrn - 1)))) {
            ++acquisition->nextPrn;
        }
        if (acquisition->nextPrn > acquisition->captureLastPrn) {
            RefreshJobs(acquisition);
            NgMutexUnlock(&acquisition->mutex);
            continue;
        }
        if (pool->active >= (unsigned)NgAtomicGet(&pool->limit) ||
            acquisition->activeJobs >= acquisition->quota ||
            acquisition->activeJobs >= acquisition->contextCount) {
            NgMutexUnlock(&acquisition->mutex);
            continue;
        }
        prn = acquisition->nextPrn++;
        for (lane = 0; lane < acquisition->contextCount; ++lane) {
            if (!(acquisition->busyContexts & (UINT32_C(1) << lane))) {
                break;
            }
        }
        acquisition->busyContexts |= UINT32_C(1) << lane;
        acquisition->activeJobs++;
        pool->active++;
        acquisition->snapshot.currentPrns[lane] = prn;
        RefreshJobs(acquisition);
        job->acquisition = acquisition;
        job->capture = acquisition->capture;
        job->capture.displayPrn =
            NgMonotonicSeconds() < acquisition->observedUntil ? acquisition->observedPrn : 0;
        job->generation = acquisition->captureGeneration;
        job->prn = prn;
        job->lane = lane;
        job->permit = 0;
        job->cpuMark = NgThreadCpuSeconds();
#ifdef NGENE_PROFILE_ACQUISITION_JOBS
        job->published = acquisition->capturePublished;
        job->claimed = NgMonotonicSeconds();
        job->permitWait = 0;
#endif
        pool->nextSystem = (system + 1) % pool->subscriptionCount;
        NgMutexUnlock(&acquisition->mutex);
        return 1;
    }
    return 0;
}

/** Copy a native strategy's completed map on the worker that owns its private workspace. */
static void CopySpace(TNgAcquisitionJob *job, const TNgAcquisitionResult *result)
{
    TNgAcquisition *acquisition = job->acquisition;
    TNgAcquisitionSearchSpace *space =
        &acquisition->spaces[job->prn - acquisition->config.firstPrn];
    if (acquisition->strategy == &ngGalileoAcquisitionStrategy) {
        NgAcquisitionGalileoSearchSpace(acquisition->contexts[job->lane], space);
    } else {
        NgAcquisitionGpsSearchSpace(acquisition->contexts[job->lane], space);
    }
    space->result = *result;
}

/** Execute one unchanged decision and then its optional, lower-priority map. */
static void Execute(TNgAcquisitionJob *job)
{
    TNgAcquisition *acquisition = job->acquisition;
    TNgAcquisitionResult result = {0};
    TNgError error = {{0}};
    void *context = acquisition->contexts[job->lane];
    int native = acquisition->strategy == &ngGpsAcquisitionStrategy ||
                 acquisition->strategy == &ngGalileoAcquisitionStrategy;
    int status =
        acquisition->strategy->search(context, &job->capture, job->prn, &acquisition->config,
                                      WorkCancelled, job, &result, &error);
    result.completedSeconds = NgMonotonicSeconds();
    NgMutexLock(&acquisition->mutex);
    int published = !Cancelled(job);
    if (published) {
        if (job->prn <= NG_LEGACY_MAX_PRN) {
            acquisition->snapshot.results[job->prn - 1] = result;
        } else {
            acquisition->highResults[job->prn - NG_LEGACY_MAX_PRN - 1] = result;
        }
        acquisition->snapshot.searches++;
        acquisition->snapshot.currentPrns[job->lane] = 0;
        RefreshJobs(acquisition);
        if (status == NG_OK && native && job->capture.displayPrn == job->prn) {
            CopySpace(job, &result);
        }
        if (status == NG_ERROR) {
            snprintf(acquisition->snapshot.message, sizeof(acquisition->snapshot.message), "%s",
                     error.message);
        }
    }
    NgMutexUnlock(&acquisition->mutex);
#ifdef NGENE_PROFILE_ACQUISITION_JOBS
    fprintf(
        stderr, "f6_job,%s,%" PRIu64 ",%" PRIu64 ",%u,%d,%d,%d,%.17g,%.17g,%.17g,%.9f,%.9f,%.9f\n",
        acquisition->strategy == &ngGalileoAcquisitionStrategy ? "galileo"
        : acquisition->strategy == &ngGpsAcquisitionStrategy   ? "gps"
                                                               : "custom",
        job->capture.origin.epoch, job->capture.origin.firstSample, job->prn, status, published,
        result.confirmed, result.dopplerHz, result.codePhaseChips, result.peakRatio,
        job->claimed - job->published, job->permitWait, result.completedSeconds - job->published);
#endif
    if (status == NG_OK && native && result.confirmed && !Cancelled(job) &&
        job->capture.displayPrn != job->prn) {
        int display = acquisition->strategy == &ngGpsAcquisitionStrategy
                          ? NgAcquisitionGpsDisplay(context, job->prn, &acquisition->config,
                                                    WorkCancelled, job)
                          : NgAcquisitionGalileoDisplay(context, job->prn, &acquisition->config,
                                                        WorkCancelled, job);
        if (display == NG_OK && !Cancelled(job)) {
            NgMutexLock(&acquisition->mutex);
            CopySpace(job, &result);
            NgMutexUnlock(&acquisition->mutex);
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
        double cpu = NgThreadCpuSeconds();
        NgMutexLock(&pool->mutex);
        if (pool->budgetActive) {
            pool->cpuCredit -= cpu - job.cpuMark;
        }
        if (job.permit) {
            --pool->executing;
        }
        TNgAcquisition *acquisition = job.acquisition;
        NgMutexLock(&acquisition->mutex);
        acquisition->activeJobs--;
        pool->active--;
        acquisition->busyContexts &= ~(UINT32_C(1) << job.lane);
        acquisition->snapshot.currentPrns[job.lane] = 0;
        RefreshJobs(acquisition);
        NgMutexUnlock(&acquisition->mutex);
        NgConditionSignal(&pool->idle);
        NgConditionSignal(&pool->work);
    }
    NgMutexUnlock(&pool->mutex);
}

/** Stop an idle pool before linking subscriptions or releasing its final owner. */
static void DestroyPool(TNgAcquisitionPool *pool)
{
    unsigned workerIndex;
    if (!pool) {
        return;
    }
    NgMutexLock(&pool->mutex);
    pool->stopping = 1;
    NgConditionSignal(&pool->work);
    NgMutexUnlock(&pool->mutex);
    for (workerIndex = 0; workerIndex < pool->started; ++workerIndex) {
        NgThreadJoin(pool->workers[workerIndex]);
    }
    NgConditionDestroy(&pool->idle);
    NgConditionDestroy(&pool->work);
    NgMutexDestroy(&pool->mutex);
    free(pool);
}

/** Prepare all threads before RX; the pool never allocates in the sample path. */
static TNgAcquisitionPool *CreatePool(TNgAcquisition *const *members, unsigned count,
                                      TNgError *error)
{
    TNgAcquisitionPool *pool = calloc(1, sizeof(*pool));
    unsigned wanted = 0;
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
    pool->subscriptionCount = count;
    for (unsigned memberIndex = 0; memberIndex < count; ++memberIndex) {
        pool->subscriptions[memberIndex] = members[memberIndex];
        wanted += members[memberIndex]->contextCount;
    }
    pool->cpuBudget = members[0]->config.cpuBudget;
    pool->capacity = wanted < Capacity() ? wanted : Capacity();
    NgAtomicSet(&pool->limit, (int)pool->capacity);
    NgAtomicSet(&pool->executionLimit, (int)pool->capacity);
    for (unsigned workerIndex = 0; workerIndex < pool->capacity; ++workerIndex) {
        if (NgThreadStart(&pool->workers[workerIndex], SearchWorker, pool)) {
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
    config->cpuBudget = 1.0;
}

int NgAcquisitionValidateGrid(const char *signal, uint32_t rateHz, unsigned periodMs, size_t size,
                              TNgError *error)
{
    if (periodMs == 0) {
        return NgFail(error, "%s acquisition code period must be positive", signal);
    }
    if (!NgSearchFftSupportsSize(size)) {
        return NgFail(error, "%s acquisition FFT size %zu is unsupported by the search FFT", signal,
                      size);
    }
    /* The compression boundaries are floor(k * Fs * periodMs / (1000 * N)).
     * If the step is below one, the first cell is empty even when a rounded-up
     * buffer can store N native pairs. A step >= 1 makes every cell nonempty. */
    uint64_t nativePairs = (uint64_t)rateHz * periodMs / 1000u;
    if (nativePairs < size) {
        uint64_t minimumRate = ((uint64_t)size * 1000u + periodMs - 1u) / periodMs;
        return NgFail(error,
                      "%s acquisition FFT size %zu requires at least %" PRIu64
                      " pairs/s for a %u ms code period. Effective rate is %" PRIu32
                      " pairs/s (%" PRIu64 " native pairs per period)",
                      signal, size, minimumRate, periodMs, rateHz, nativePairs);
    }
    return NG_OK;
}

/** Reject malformed module options before worker creation. */
static int ValidateForConstellation(const TNgAcquisitionConfig *config,
                                    TNgConstellation constellation, TNgError *error)
{
    if (!config || !isfinite(config->falseAlarmProbability) || config->falseAlarmProbability <= 0 ||
        config->falseAlarmProbability >= 1 || config->firstPrn < 1 ||
        !NgSatellitePrnValid(constellation, config->lastPrn) ||
        config->lastPrn < config->firstPrn || (config->enabled != 0 && config->enabled != 1) ||
        (config->squareCarrier != 0 && config->squareCarrier != 1) ||
        (config->xorCorrelation != 0 && config->xorCorrelation != 1) ||
        config->fftMode < NG_SEARCH_FFT_FLOAT || config->fftMode > NG_SEARCH_FFT_INTEGER_SCALED) {
        return NgFail(error, "Invalid acquisition PRN range or mode");
    }
    if (config->fftSize != 0 && config->fftSize != 4096 && config->fftSize != 8192 &&
        config->fftSize != 16384) {
        return NgFail(error,
                      "Galileo acquisition FFT size must be 0 (automatic), 4096, 8192 or 16384");
    }
    if (!isfinite(config->cpuBudget) || config->cpuBudget < 0 ||
        config->cpuBudget > NG_ACQUISITION_MAX_PARALLEL) {
        return NgFail(error, "Acquisition CPU budget must be 0..32 cores (0 disables it)");
    }
    return NG_OK;
}

int NgAcquisitionValidate(const TNgAcquisitionConfig *config, TNgError *error)
{
    return ValidateForConstellation(config, NG_CONSTELLATION_GPS, error);
}

/** Copy common-ring samples into the producer-owned slot; never retain the borrowed view. */
static int Process(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgAcquisition *acquisition = context;
    size_t sampleIndex;
    size_t take;
    (void)id;
    (void)error;
    *next = state;
    if (!acquisition->haveEpoch || window->meta.epoch != acquisition->epoch ||
        window->meta.firstSample != acquisition->expectedSample ||
        (window->meta.flags & NG_BLOCK_GAP) != 0) {
        if (acquisition->haveEpoch) {
            acquisition->gaps++;
        }
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
        if (acquisition->filled == 0) {
            acquisition->capture.origin = window->meta;
        }
        take = acquisition->capacity - acquisition->filled;
        if (take > window->meta.count) {
            take = window->meta.count;
        }
        for (sampleIndex = 0; sampleIndex < take; ++sampleIndex) {
            acquisition->storage[acquisition->filled + sampleIndex].i =
                (float)NgIqComponent(window, sampleIndex, 0);
            acquisition->storage[acquisition->filled + sampleIndex].q =
                (float)NgIqComponent(window, sampleIndex, 1);
        }
        acquisition->filled += take;
        if (acquisition->filled == acquisition->capacity) {
            acquisition->capture.count = acquisition->capacity;
            acquisition->capture.origin.count = acquisition->capacity;
            acquisition->captureGeneration = NgAtomicGet(&acquisition->generation);
            acquisition->filled = 0;
            acquisition->skipped += window->meta.count - take;
            acquisition->nextPrn = acquisition->nextCapturePrn;
            acquisition->captureLastPrn = acquisition->config.lastPrn;
            if (acquisition->strategy == &ngBeidouAcquisitionStrategy &&
                acquisition->captureLastPrn - acquisition->nextPrn >= 4) {
                acquisition->captureLastPrn = acquisition->nextPrn + 3;
            }
            acquisition->nextCapturePrn = acquisition->captureLastPrn == acquisition->config.lastPrn
                                              ? acquisition->config.firstPrn
                                              : acquisition->captureLastPrn + 1;
#ifdef NGENE_PROFILE_ACQUISITION_JOBS
            acquisition->capturePublished = NgMonotonicSeconds();
#endif
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
    Process, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL, 0};

/** Release private workspaces; caller decides whether the supplied first context is owned. */
static void FreeContexts(TNgAcquisition *acquisition, unsigned first)
{
    unsigned contextIndex;
    for (contextIndex = first; contextIndex < acquisition->contextCount; ++contextIndex) {
        if (acquisition->strategy == &ngGalileoAcquisitionStrategy ||
            acquisition->strategy == &ngBeidouAcquisitionStrategy) {
            NgAcquisitionGalileoFree(acquisition->contexts[contextIndex]);
        } else if (acquisition->strategy == &ngGpsAcquisitionStrategy) {
            NgAcquisitionGpsFree(acquisition->contexts[contextIndex]);
        }
    }
}

/** Create exclusive native workspaces and a bounded standalone pool before reception. */
static int CreateWithDuration(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                              const TNgAcquisitionStrategy *strategy, void *context,
                              TNgAcquisition **out, TNgChannel *channel, TNgError *error,
                              TNgConstellation constellation, unsigned durationMs)
{
    TNgAcquisition *acquisition;
    unsigned contextIndex;
    unsigned wanted;
    int native = strategy == &ngGpsAcquisitionStrategy || strategy == &ngGalileoAcquisitionStrategy;
    *out = NULL;
    memset(channel, 0, sizeof(*channel));
    if (ValidateForConstellation(config, constellation, error) != NG_OK) {
        return NG_ERROR;
    }
    if (!config->enabled) {
        return NG_OK;
    }
    if (!stream || stream->sampleRateHz < 2048000 || stream->sampleRateHz > 20000000 || !strategy ||
        !strategy->search) {
        return NgFail(error, "Acquisition supports native rates 2.048..20 Msps");
    }
    uint64_t pairs = ((uint64_t)stream->sampleRateHz * durationMs + 999u) / 1000u;
    if (!pairs || pairs > SIZE_MAX / sizeof(TNgComplex)) {
        return NgFail(error, "Invalid acquisition capture duration");
    }
    acquisition = calloc(1, sizeof(*acquisition));
    if (!acquisition) {
        return NgFail(error, "Acquisition allocation failed");
    }
    acquisition->capacity = (size_t)pairs;
    acquisition->storage = calloc(acquisition->capacity, sizeof(*acquisition->storage));
    if (native) {
        acquisition->spaces =
            calloc(config->lastPrn - config->firstPrn + 1, sizeof(*acquisition->spaces));
    }
    if (!acquisition->storage || (native && !acquisition->spaces) ||
        NgMutexInit(&acquisition->mutex)) {
        free(acquisition->spaces);
        free(acquisition->storage);
        free(acquisition);
        return NgFail(error, "Acquisition capture/mutex allocation failed");
    }
    acquisition->config = *config;
    acquisition->constellation = constellation;
    acquisition->strategy = strategy;
    acquisition->capture.samples = acquisition->storage;
    acquisition->capture.stream = *stream;
    acquisition->contexts[0] = context;
    acquisition->contextCount = 1;
    wanted = native || strategy == &ngBeidouAcquisitionStrategy ? Capacity() : 1;
    if (strategy == &ngBeidouAcquisitionStrategy && wanted > 4) {
        wanted = 4;
    }
    if (wanted > config->lastPrn - config->firstPrn + 1) {
        wanted = config->lastPrn - config->firstPrn + 1;
    }
    for (contextIndex = 1; contextIndex < wanted; ++contextIndex) {
        int result;
        if (strategy == &ngGpsAcquisitionStrategy) {
            result = NgAcquisitionGpsWorkspace(stream, config->fftMode,
                                               &acquisition->contexts[contextIndex], error);
        } else if (strategy == &ngGalileoAcquisitionStrategy) {
            result = NgAcquisitionGalileoWorkspace(stream, config->fftSize, config->fftMode,
                                                   &acquisition->contexts[contextIndex], error);
        } else {
            result =
                NgAcquisitionBeidouWorkspace(stream, &acquisition->contexts[contextIndex], error);
        }
        if (result != NG_OK) {
            FreeContexts(acquisition, 1);
            NgMutexDestroy(&acquisition->mutex);
            free(acquisition->spaces);
            free(acquisition->storage);
            free(acquisition);
            return NG_ERROR;
        }
        acquisition->contextCount++;
    }
    acquisition->quota = wanted;
    acquisition->nextPrn = acquisition->nextCapturePrn = config->firstPrn;
    acquisition->captureLastPrn = config->lastPrn;
    acquisition->snapshot.enabled = 1;
    acquisition->pool = CreatePool(&acquisition, 1, error);
    if (!acquisition->pool) {
        FreeContexts(acquisition, 1);
        NgMutexDestroy(&acquisition->mutex);
        free(acquisition->spaces);
        free(acquisition->storage);
        free(acquisition);
        return NG_ERROR;
    }
    channel->ops = &acquisitionOps;
    channel->context = acquisition;
    channel->state = NG_CHANNEL_ACQUISITION;
    channel->id.constellation = (uint32_t)constellation;
    channel->id.signal = NG_PRIMARY_SIGNAL;
    *out = acquisition;
    return NG_OK;
}

/** Custom strategies remain exclusive unless the caller supplies separate subscriptions. */
int NgAcquisitionCreateWithStrategy(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                                    const TNgAcquisitionStrategy *strategy, void *context,
                                    TNgAcquisition **out, TNgChannel *channel, TNgError *error)
{
    return CreateWithDuration(config, stream, strategy, context, out, channel, error,
                              NG_CONSTELLATION_GPS, 56);
}

int NgAcquisitionCreateForConstellation(const TNgAcquisitionConfig *config,
                                        const TNgStreamInfo *stream, TNgConstellation constellation,
                                        unsigned durationMs, const TNgAcquisitionStrategy *strategy,
                                        void *context, TNgAcquisition **out, TNgChannel *channel,
                                        TNgError *error)
{
    return CreateWithDuration(config, stream, strategy, context, out, channel, error, constellation,
                              durationMs);
}

/** Register native GPS with private SIMD workspaces on one frozen shared capture. */
int NgAcquisitionCreate(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                        TNgAcquisition **out, TNgChannel *channel, TNgError *error)
{
    void *workspace = NULL;
    int status;
    *out = NULL;
    if (NgAcquisitionValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    if (config->enabled &&
        NgAcquisitionGpsWorkspace(stream, config->fftMode, &workspace, error) != NG_OK) {
        return NG_ERROR;
    }
    status = CreateWithDuration(config, stream, &ngGpsAcquisitionStrategy, workspace, out, channel,
                                error, NG_CONSTELLATION_GPS, 56);
    if (status != NG_OK || !*out) {
        NgAcquisitionGpsFree(workspace);
    } else {
        (*out)->ownsContext = 1;
    }
    return status;
}

/** Register native Galileo; linking shares workers, not mutable DSP state or RF cursors. */
int NgAcquisitionCreateGalileo(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                               TNgAcquisition **out, TNgChannel *channel, TNgError *error)
{
    void *workspace = NULL;
    int status;
    *out = NULL;
    if (NgAcquisitionValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    if (config->enabled && NgAcquisitionGalileoWorkspace(stream, config->fftSize, config->fftMode,
                                                         &workspace, error) != NG_OK) {
        return NG_ERROR;
    }
    status = CreateWithDuration(config, stream, &ngGalileoAcquisitionStrategy, workspace, out,
                                channel, error, NG_CONSTELLATION_GALILEO, 224);
    if (status != NG_OK || !*out) {
        NgAcquisitionGalileoFree(workspace);
    } else {
        (*out)->ownsContext = 1;
        channel->id.instanceId = 1;
    }
    return status;
}

int NgAcquisitionValidateBeidou(const TNgAcquisitionConfig *config, TNgError *error)
{
    return ValidateForConstellation(config, NG_CONSTELLATION_BEIDOU, error);
}

int NgAcquisitionCreateBeidou(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                              TNgAcquisition **out, TNgChannel *channel, TNgError *error)
{
    void *workspace = NULL;
    *out = NULL;
    if (ValidateForConstellation(config, NG_CONSTELLATION_BEIDOU, error) != NG_OK) {
        return NG_ERROR;
    }
    if (config->enabled && NgAcquisitionBeidouWorkspace(stream, &workspace, error) != NG_OK) {
        return NG_ERROR;
    }
    int status = CreateWithDuration(config, stream, &ngBeidouAcquisitionStrategy, workspace, out,
                                    channel, error, NG_CONSTELLATION_BEIDOU, 570);
    if (*out) {
        (*out)->ownsContext = 1;
        channel->id.instanceId = NG_CONSTELLATION_BEIDOU - 1;
    } else {
        NgAcquisitionGalileoFree(workspace);
    }
    return status;
}

/** Merge idle subscriptions before their first common window. */
int NgAcquisitionLink(TNgAcquisition *firstRunner, TNgAcquisition *secondRunner, TNgError *error)
{
    if (!firstRunner || !secondRunner || firstRunner->pool == secondRunner->pool) {
        return NG_OK;
    }
    TNgAcquisitionPool *previous[] = {firstRunner->pool, secondRunner->pool};
    TNgAcquisition *members[NG_CONSTELLATION_COUNT];
    unsigned count = 0;
    for (unsigned poolIndex = 0; poolIndex < 2; ++poolIndex) {
        for (unsigned subscriptionIndex = 0;
             subscriptionIndex < previous[poolIndex]->subscriptionCount; ++subscriptionIndex) {
            TNgAcquisition *member = previous[poolIndex]->subscriptions[subscriptionIndex];
            if (!member) {
                continue;
            }
            if (member->haveEpoch || NgAtomicGet(&member->slot)) {
                return NgFail(error, "Acquisition pools must link before RX");
            }
            if (count == NG_CONSTELLATION_COUNT) {
                return NgFail(error, "Acquisition pool supports at most three subscriptions");
            }
            members[count++] = member;
        }
    }
    TNgAcquisitionPool *pool = CreatePool(members, count, error);
    if (!pool) {
        return NG_ERROR;
    }
    for (unsigned poolIndex = 0; poolIndex < 2; ++poolIndex) {
        DestroyPool(previous[poolIndex]);
    }
    for (unsigned memberIndex = 0; memberIndex < count; ++memberIndex) {
        members[memberIndex]->pool = pool;
    }
    return NG_OK;
}

/** Detach after RX; no workspace is freed until all outstanding borrowers return. */
void NgAcquisitionDestroy(TNgAcquisition *acquisition)
{
    TNgAcquisitionPool *pool;
    unsigned subscriptionIndex;
    int remaining = 0;
    if (!acquisition) {
        return;
    }
    NgAtomicSet(&acquisition->stop, 1);
    pool = acquisition->pool;
    if (pool) {
        NgMutexLock(&pool->mutex);
        NgConditionSignal(&pool->work);
        while (acquisition->activeJobs) {
            NgConditionWait(&pool->idle, &pool->mutex, UINT_MAX);
        }
        for (subscriptionIndex = 0; subscriptionIndex < pool->subscriptionCount;
             ++subscriptionIndex) {
            if (pool->subscriptions[subscriptionIndex] == acquisition) {
                pool->subscriptions[subscriptionIndex] = NULL;
            }
            if (pool->subscriptions[subscriptionIndex]) {
                remaining = 1;
            }
        }
        NgMutexUnlock(&pool->mutex);
        if (!remaining) {
            DestroyPool(pool);
        }
    }
    if (acquisition->ownsContext) {
        FreeContexts(acquisition, 0);
    } else {
        FreeContexts(acquisition, 1);
    }
    NgMutexDestroy(&acquisition->mutex);
    free(acquisition->spaces);
    free(acquisition->storage);
    free(acquisition);
}

/** Publish slot masks and wake only when eligibility changes. */
void NgAcquisitionSetAdmission(TNgAcquisition *acquisition, uint32_t mask, int paused)
{
    NgAcquisitionSetAdmissionMask(acquisition, mask, paused);
}

void NgAcquisitionSetAdmissionMask(TNgAcquisition *acquisition, uint64_t mask, int paused)
{
    int changed;
    if (!acquisition) {
        return;
    }
    NgMutexLock(&acquisition->mutex);
    changed = acquisition->excludedMask != mask || acquisition->paused != (paused != 0);
    acquisition->excludedMask = mask;
    acquisition->paused = paused != 0;
    NgMutexUnlock(&acquisition->mutex);
    if (changed) {
        Wake(acquisition->pool);
    }
}

/** Publish adaptive total concurrency and this subscription's free-slot quota. */
void NgAcquisitionSetParallelism(TNgAcquisition *acquisition, unsigned totalLimit, unsigned quota,
                                 unsigned executionLimit)
{
    TNgAcquisitionPool *pool;
    if (!acquisition || !acquisition->pool) {
        return;
    }
    pool = acquisition->pool;
    if (totalLimit > pool->capacity) {
        totalLimit = pool->capacity;
    }
    if (executionLimit > pool->capacity) {
        executionLimit = pool->capacity;
    }
    if ((unsigned)NgAtomicGet(&pool->limit) == totalLimit && acquisition->quota == quota &&
        (unsigned)NgAtomicGet(&pool->executionLimit) == executionLimit) {
        return;
    }
    NgMutexLock(&pool->mutex);
    int changed = (unsigned)NgAtomicGet(&pool->limit) != totalLimit ||
                  acquisition->quota != quota ||
                  (unsigned)NgAtomicGet(&pool->executionLimit) != executionLimit;
    NgAtomicSet(&pool->limit, (int)totalLimit);
    NgAtomicSet(&pool->executionLimit, (int)executionLimit);
    acquisition->quota = quota;
    if (changed) {
        NgConditionSignal(&pool->work);
    }
    NgMutexUnlock(&pool->mutex);
}

/** Credit the shared pool with CPU time for newly processed signal. Linked subscriptions
 * share one pool and one signal clock, so calling this for both is harmless. */
unsigned NgAcquisitionAdvanceSignal(TNgAcquisition *acquisition, uint64_t epoch, uint64_t endSample,
                                    double sampleRate)
{
    TNgAcquisitionPool *pool;
    unsigned execution;
    if (!acquisition || !acquisition->pool || !(sampleRate > 0)) {
        return 0;
    }
    pool = acquisition->pool;
    if (!(pool->cpuBudget > 0)) {
        return 0;
    }
    execution = (unsigned)ceil(pool->cpuBudget);
    if (execution > pool->capacity) {
        execution = pool->capacity;
    }
    NgMutexLock(&pool->mutex);
    double cap = NG_ACQUISITION_CREDIT_SECONDS * pool->cpuBudget;
    if (!pool->budgetActive) {
        pool->budgetActive = 1;
        pool->cpuCredit = cap;
    }
    if (pool->haveSignal && pool->signalEpoch == epoch && endSample > pool->signalSample) {
        int starved = pool->cpuCredit <= 0;
        pool->cpuCredit += (double)(endSample - pool->signalSample) / sampleRate * pool->cpuBudget;
        if (pool->cpuCredit > cap) {
            pool->cpuCredit = cap;
        }
        if (starved && pool->cpuCredit > 0) {
            NgConditionSignal(&pool->work);
        }
    }
    if (!pool->haveSignal || pool->signalEpoch != epoch || endSample > pool->signalSample) {
        pool->signalEpoch = epoch;
        pool->signalSample = endSample;
        pool->haveSignal = 1;
    }
    NgMutexUnlock(&pool->mutex);
    return execution ? execution : 1u;
}

/** Publish display interest without touching search decisions or the owned capture. */
void NgAcquisitionObserve(TNgAcquisition *acquisition, unsigned prn)
{
    if (!acquisition) {
        return;
    }
    NgMutexLock(&acquisition->mutex);
    acquisition->observedPrn = NgSatellitePrnValid(acquisition->constellation, prn) ? prn : 0;
    acquisition->observedUntil = NgMonotonicSeconds() + 2;
    NgMutexUnlock(&acquisition->mutex);
}

/** Scheduling budget affects only cooperative waiting, never acquisition arithmetic. */
void NgAcquisitionSetBudget(TNgAcquisition *acquisition, unsigned yieldMilliseconds)
{
    if (acquisition) {
        NgAtomicSet(&acquisition->yieldMilliseconds,
                    (int)(yieldMilliseconds > 20 ? 20 : yieldMilliseconds));
    }
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
        snapshot->poolExecuting = acquisition->pool->executing;
        snapshot->executionLimit = (unsigned)NgAtomicGet(&acquisition->pool->executionLimit);
        NgMutexUnlock(&acquisition->pool->mutex);
    }
}

void NgAcquisitionReadResults(TNgAcquisition *acquisition,
                              TNgAcquisitionResult results[NG_SATELLITE_MAX_PRN])
{
    if (!acquisition) {
        memset(results, 0, NG_SATELLITE_MAX_PRN * sizeof(*results));
        return;
    }
    NgMutexLock(&acquisition->mutex);
    memcpy(results, acquisition->snapshot.results, sizeof(acquisition->snapshot.results));
    memcpy(results + NG_LEGACY_MAX_PRN, acquisition->highResults, sizeof(acquisition->highResults));
    NgMutexUnlock(&acquisition->mutex);
}

void NgAcquisitionReadSearchSpace(TNgAcquisition *acquisition, unsigned prn,
                                  TNgAcquisitionSearchSpace *space)
{
    memset(space, 0, sizeof(*space));
    if (!acquisition || !acquisition->spaces || prn < acquisition->config.firstPrn ||
        prn > acquisition->config.lastPrn) {
        return;
    }
    NgMutexLock(&acquisition->mutex);
    *space = acquisition->spaces[prn - acquisition->config.firstPrn];
    NgMutexUnlock(&acquisition->mutex);
}
