/** @file bank.c @brief One receiver slot pool shared by all acquisition strategies. */
#include "tracking/bank.h"
#include "core/channel_dispatch.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>

/** Signal batched per tracking dispatch. Tracking accepts arbitrary window splits, so this
 * changes only how often workers meet at the barrier (250 instead of 1000 times a second)
 * and adds at most this latency to slot admission and PVT. */
#define NG_BANK_BATCH_MS 4u

/** Receiver-owned pool. All allocation/assignment happens outside DSP jobs. */
typedef struct {
    TNgPvtSnapshot pvt;                   /**< Last common-epoch code/Doppler solve. */
    double sampleRate;                    /**< Actual source pairs per second. */
    uint64_t lastPvtSample;               /**< Last periodic solver invocation. */
    TNgTrackingConfig config;             /**< Frozen total capacity and reservations. */
    TNgTracking *tracking[64];            /**< One reusable private workspace per receiver slot. */
    TNgChannel jobs[64];                  /**< Stable identities independent of constellation. */
    TNgTrackingSnapshot slots[64];        /**< Copied observation state. */
    unsigned reservation[64];             /**< 0 shared, 1 GPS, 2 Galileo. */
    TNgChannel captures[2];               /**< Borrowed original acquisition registrations. */
    TNgAcquisition *acquisition[2];       /**< Borrowed workers. */
    TNgAcquisitionResult accepted[2][32]; /**< Consumed seeds. */
    TNgBroadcast broadcast[2][32]; /**< Classic satellite cache survives slot reacquisition. */
    TNgChannelDispatch *dispatch;  /**< One fixed pool for all slots. */
    uint64_t epoch, firstSample, expected; /**< Common-window identity. */
    size_t count;                          /**< Last window sample count. */
    int haveWindow;                        /**< A frame has been processed. */
    unsigned observedChannel; /**< Optional tap interest; receiver-owner writes only. */
    double trackingLoad;      /**< Signal-time EMA of mandatory dispatch wall time / window. */
    uint64_t budgetSamples;   /**< Sample timing only once per ten milliseconds of signal. */
    int catchingUp;           /**< Hysteresis while foreground processing recovers. */
    TNgBlockMeta lastInput;   /**< Deduplicate GPS/Galileo subscriptions before batching. */
    int haveInput;
    unsigned char batch[20000u * 4u * NG_BANK_BATCH_MS]; /**< One batch, allocated before RX. */
    TNgIqView pending;                /**< Owned batch geometry; no ring pointer retained. */
    size_t batchLimit;                /**< NG_BANK_BATCH_MS * ceil(actual sample rate / 1000). */
    size_t order[64];                 /**< Dispatch claim order, costliest slots first. */
} TNgChannelPool;

/** A light signal subscriber points to the same shared pool after linking. */
struct TNgTrackingBank {
    TNgChannelPool *pool; /**< Shared receiver pool. */
    unsigned system;      /**< Subscription index, GPS 0 or Galileo 1. */
    int ownsPool;         /**< Destroy pool only through its owner. */
};

/** Run an exclusive channel workspace on the borrowed common samples. */
static int Track(void *context, const TNgChannelId *id, TNgChannelState state,
                 const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgTrackingSnapshot snapshot;
    int result;
    (void)id;
    (void)state;
    result = NgTrackingProcess(context, window, error);
    NgTrackingRead(context, &snapshot);
    *next = snapshot.active                ? NG_CHANNEL_TRACKING
            : state == NG_CHANNEL_TRACKING ? NG_CHANNEL_HOLDOVER
                                           : state;
    return result;
}

/** Native signed formats retain their ADC levels. */
static const TNgChannelOps trackOps = {
    Track, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL,
    1 /* An ACQUISITION-state slot holds no active tracking: nothing to do. */};

/** Copy worker-owned results after the barrier, preserving stable slot identity. */
static void Refresh(TNgChannelPool *pool)
{
    unsigned k;
    for (k = 0; k < pool->config.channelCount; ++k) {
        NgTrackingRead(pool->tracking[k], &pool->slots[k]);
        if (pool->slots[k].prn && pool->slots[k].constellation >= 1 &&
            pool->slots[k].constellation <= 2) {
            const TNgBroadcast *b = &pool->slots[k].navigation.broadcast;
            if (b->subframe1Received || b->subframe2Received || b->subframe3Received)
                pool->broadcast[pool->slots[k].constellation - 1][pool->slots[k].prn - 1] = *b;
        }
        if (!pool->slots[k].active) {
            memset(&pool->slots[k], 0, sizeof(pool->slots[k]));
            /* The receiver owner releases this assignment after the worker barrier. */
            pool->jobs[k].state = NG_CHANNEL_ACQUISITION;
        }
        pool->slots[k].channelId = k + 1;
        pool->slots[k].reservation = pool->reservation[k];
        pool->slots[k].state = pool->slots[k].active ? 2u : 0u;
        pool->jobs[k].id.satellite = pool->slots[k].prn;
        pool->jobs[k].id.constellation = pool->slots[k].constellation;
    }
}

/** Prefer this signal's reserved slots, then the shared slots; never borrow another reservation. */
static unsigned FreeSlot(const TNgChannelPool *pool, unsigned system, int forDisplay)
{
    unsigned pass, k;
    for (pass = 0; pass < 2; ++pass)
        for (k = 0; k < pool->config.channelCount; ++k)
            if (!pool->slots[k].active && (!forDisplay || pool->slots[k].state == 0) &&
                pool->reservation[k] == (pass == 0 ? system : 0u))
                return k;
    return pool->config.channelCount;
}

/** Process the common interval once, then admit confirmed results in completion order. */
static int Frame(TNgChannelPool *pool, const TNgIqView *window, TNgError *error)
{
    TNgAcquisitionSnapshot searches[2];
    uint32_t mask[2] = {0, 0};
    unsigned k, system, active = 0;
    if (pool->haveWindow && pool->epoch == window->meta.epoch &&
        pool->firstSample == window->meta.firstSample && pool->count == window->meta.count)
        return NG_OK;
    if (pool->haveWindow &&
        (pool->epoch != window->meta.epoch || pool->expected != window->meta.firstSample ||
         (window->meta.flags & NG_BLOCK_GAP))) {
        /* Active slots see the gap in this window and stop themselves (TRACKING to
         * HOLDOVER); Refresh then returns them to ACQUISITION after the barrier. */
        memset(&pool->pvt, 0, sizeof(pool->pvt));
        pool->lastPvtSample = 0;
    }
    for (k = 0; k < pool->config.channelCount; ++k)
        if (pool->slots[k].active)
            ++active;
    for (k = 0; k < pool->config.channelCount; ++k)
        NgTrackingObserve(pool->tracking[k], pool->observedChannel == k + 1);
    {
        /* Galileo integrations cost more per sample: start them first so that the barrier
         * does not wait for a long job claimed last. */
        unsigned n = 0, pass;
        for (pass = 0; pass < 3; ++pass)
            for (k = 0; k < pool->config.channelCount; ++k) {
                unsigned rank = !pool->slots[k].active ? 2u
                                : pool->slots[k].constellation == 2 ? 0u
                                                                    : 1u;
                if (rank == pass)
                    pool->order[n++] = k;
            }
    }
    {
        int measure;
        double started = 0;
        double duration = (double)window->meta.count / pool->sampleRate;
        pool->budgetSamples += window->meta.count;
        measure = (double)pool->budgetSamples >= pool->sampleRate * 0.01;
        if (measure)
            started = NgMonotonicSeconds();
        if (active && NgChannelDispatchProcess(pool->dispatch, window, error) != NG_OK)
            return NG_ERROR;
        if (measure && duration > 0) {
            double finished = NgMonotonicSeconds();
            double weight = (double)pool->budgetSamples / pool->sampleRate / 0.1;
            if (weight > 1)
                weight = 1;
            if (active)
                pool->trackingLoad +=
                    weight * ((finished - started) / duration - pool->trackingLoad);
            else
                pool->trackingLoad = 0;
            pool->budgetSamples = 0;
        }
    }
    pool->epoch = window->meta.epoch;
    pool->firstSample = window->meta.firstSample;
    pool->count = window->meta.count;
    pool->expected = window->meta.firstSample + window->meta.count;
    pool->haveWindow = 1;
    Refresh(pool);
    for (system = 0; system < 2; ++system)
        NgAcquisitionRead(pool->acquisition[system], &searches[system]);
    for (k = 0; k < pool->config.channelCount; ++k)
        if (pool->slots[k].active)
            mask[pool->slots[k].constellation - 1] |= UINT32_C(1) << (pool->slots[k].prn - 1);
    for (;;) {
        const TNgAcquisitionResult *best = NULL;
        unsigned bestSystem = 0, bestSlot = 0, prn;
        for (system = 0; system < 2; ++system) {
            unsigned slot = FreeSlot(pool, system + 1, 0);
            if (slot == pool->config.channelCount)
                continue;
            for (prn = 1; prn <= 32; ++prn) {
                const TNgAcquisitionResult *seed = &searches[system].results[prn - 1];
                const TNgAcquisitionResult *used = &pool->accepted[system][prn - 1];
                if (!seed->confirmed || seed->epoch != window->meta.epoch ||
                    seed->firstSample > window->meta.firstSample ||
                    (mask[system] & (UINT32_C(1) << (prn - 1))) ||
                    (used->confirmed && used->epoch == seed->epoch &&
                     used->firstSample >= seed->firstSample))
                    continue;
                int preferred =
                    pool->reservation[slot] == 0 && pool->config.autoPriority == system + 1;
                int previousPreferred = best && pool->reservation[bestSlot] == 0 &&
                                        pool->config.autoPriority == bestSystem + 1;
                if (!best || (preferred != previousPreferred
                                  ? preferred
                                  : seed->completedSeconds < best->completedSeconds)) {
                    best = seed;
                    bestSystem = system;
                    bestSlot = slot;
                }
            }
        }
        if (!best)
            break;
        if (NgTrackingSetSignal(pool->tracking[bestSlot], bestSystem + 1, error) != NG_OK ||
            NgTrackingSeed(pool->tracking[bestSlot], best, error) != NG_OK)
            return NG_ERROR;
        pool->jobs[bestSlot].state = NG_CHANNEL_TRACKING;
        NgTrackingRestoreBroadcast(pool->tracking[bestSlot],
                                   &pool->broadcast[bestSystem][best->prn - 1]);
        pool->accepted[bestSystem][best->prn - 1] = *best;
        mask[bestSystem] |= UINT32_C(1) << (best->prn - 1);
        Refresh(pool);
    }
    active = 0;
    for (k = 0; k < pool->config.channelCount; ++k)
        if (pool->slots[k].active)
            ++active;
    for (system = 0; system < 2; ++system) {
        unsigned slot = FreeSlot(pool, system + 1, 1);
        unsigned quota = 0, totalLimit = pool->config.channelCount - active;
        unsigned capacity = searches[system].parallelCapacity;
        unsigned budget = capacity;
        /* Signal-time CPU budget: acquisition receives a fixed share of CPU per second of
         * signal, independent of the number of cores and tracking workers. */
        unsigned budgeted = NgAcquisitionAdvanceSignal(pool->acquisition[system], pool->epoch,
                                                       pool->expected, pool->sampleRate);
        for (k = 0; k < pool->config.channelCount; ++k)
            if (!pool->slots[k].active &&
                (pool->reservation[k] == 0 || pool->reservation[k] == system + 1))
                ++quota;
        if (budgeted)
            budget = budgeted;
        else if (active && capacity) {
            double headroom = 1.0 - pool->trackingLoad / 0.85;
            budget = headroom > 0 ? (unsigned)((double)capacity * headroom) : 0;
            if (headroom > 0 && !budget)
                budget = 1;
            if (totalLimit > budget)
                totalLimit = budget;
        }
        if (pool->catchingUp)
            budget = totalLimit = 0;
        NgAcquisitionSetParallelism(pool->acquisition[system], totalLimit, quota, budget);
        unsigned delay = !active || budgeted         ? 0u
                         : pool->trackingLoad > 0.95 ? 20u
                         : pool->trackingLoad > 0.75 ? 4u
                         : pool->trackingLoad > 0.55 ? 1u
                                                     : 0u;
        NgAcquisitionSetBudget(pool->acquisition[system], delay);
        NgAcquisitionSetAdmission(pool->acquisition[system], mask[system],
                                  slot == pool->config.channelCount);
        for (unsigned lane = 0; lane < NG_ACQUISITION_MAX_PARALLEL; ++lane) {
            unsigned prn = searches[system].currentPrns[lane];
            slot = FreeSlot(pool, system + 1, 1);
            if (!prn || slot == pool->config.channelCount ||
                (mask[system] & (UINT32_C(1) << (prn - 1))))
                continue;
            pool->slots[slot].state = 1;
            pool->slots[slot].constellation = system + 1;
            pool->slots[slot].prn = prn;
        }
    }
    return NG_OK;
}

/** Amortize worker barriers for small source windows without changing a single sample. */
static int Accumulate(TNgChannelPool *pool, const TNgIqView *window, TNgError *error)
{
    size_t offset = 0, stride = NgSampleBytes(window->format);
    if (pool->haveInput && pool->lastInput.epoch == window->meta.epoch &&
        pool->lastInput.firstSample == window->meta.firstSample &&
        pool->lastInput.count == window->meta.count)
        return NG_OK;
    int discontinuity =
        (window->meta.flags & NG_BLOCK_GAP) ||
        (pool->haveInput && (pool->lastInput.epoch != window->meta.epoch ||
                             pool->lastInput.firstSample + pool->lastInput.count !=
                                 window->meta.firstSample));
    if (discontinuity && pool->pending.meta.count) {
        /* The batch before a discontinuity is contiguous signal: process it, then restart. */
        if (Frame(pool, &pool->pending, error) != NG_OK)
            return NG_ERROR;
        pool->pending.meta.count = 0;
    }
    pool->lastInput = window->meta;
    pool->haveInput = 1;
    while (offset < window->meta.count) {
        if (!pool->pending.meta.count && window->meta.count - offset >= pool->batchLimit) {
            TNgIqView direct = *window;
            direct.samples = (const TNgIq8 *)(const void *)((const unsigned char *)(const void *)
                                                                window->samples +
                                                            offset * stride);
            direct.meta.firstSample += offset;
            direct.meta.count -= offset;
            if (offset)
                direct.meta.flags &= ~(unsigned)NG_BLOCK_GAP;
            if (direct.meta.flags & NG_BLOCK_HARDWARE_TIME)
                direct.meta.hardwareTimestamp += offset;
            return Frame(pool, &direct, error);
        }
        if (!pool->pending.meta.count) {
            pool->pending = *window;
            pool->pending.samples = (const TNgIq8 *)(const void *)pool->batch;
            pool->pending.meta.firstSample += offset;
            pool->pending.meta.count = 0;
            if (offset)
                pool->pending.meta.flags &= ~(unsigned)NG_BLOCK_GAP;
            if (pool->pending.meta.flags & NG_BLOCK_HARDWARE_TIME)
                pool->pending.meta.hardwareTimestamp += offset;
        }
        size_t take = pool->batchLimit - pool->pending.meta.count;
        if (take > window->meta.count - offset)
            take = window->meta.count - offset;
        memcpy(pool->batch + pool->pending.meta.count * stride,
               (const unsigned char *)(const void *)window->samples + offset * stride,
               take * stride);
        pool->pending.meta.count += take;
        offset += take;
        /* A discontinuity is handled at once, so slots stop on the first gap window. */
        if (pool->pending.meta.count == pool->batchLimit ||
            (window->meta.flags & NG_BLOCK_PARTIAL) || discontinuity) {
            if (Frame(pool, &pool->pending, error) != NG_OK)
                return NG_ERROR;
            pool->pending.meta.count = 0;
        }
    }
    return NG_OK;
}

/** Every signal observes one common frame, followed by its own bounded capture copy. */
static int Process(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgTrackingBank *bank = context;
    TNgChannel *capture = &bank->pool->captures[bank->system];
    if (Accumulate(bank->pool, window, error) != NG_OK)
        return NG_ERROR;
    if (bank->pool->haveWindow &&
        (bank->pool->pvt.epoch != bank->pool->epoch || !bank->pool->lastPvtSample ||
         bank->pool->expected < bank->pool->lastPvtSample ||
         (double)(bank->pool->expected - bank->pool->lastPvtSample) >= bank->pool->sampleRate)) {
        NgPvtCompute(bank->pool->slots, bank->pool->epoch, bank->pool->expected,
                     bank->pool->sampleRate, &bank->pool->pvt);
        bank->pool->lastPvtSample = bank->pool->expected;
    }
    return capture->ops->process(capture->context, id, state, window, next, error);
}

/** Shared stream subscription operations. */
static const TNgChannelOps bankOps = {
    Process, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL, 0};

/** Join the pool before releasing any private state. */
static void DestroyPool(TNgChannelPool *pool)
{
    unsigned k;
    if (!pool)
        return;
    NgChannelDispatchDestroy(pool->dispatch);
    for (k = 0; k < pool->config.channelCount; ++k)
        NgTrackingDestroy(pool->tracking[k]);
    free(pool);
}

/** Prepare one pool; Link merges signal subscribers before reception starts. */
int NgTrackingBankCreate(const TNgTrackingConfig *config, const TNgStreamInfo *stream,
                         TNgAcquisition *acquisition, TNgChannel *channel, TNgTrackingBank **out,
                         TNgError *error)
{
    TNgTrackingBank *bank;
    TNgChannelPool *pool;
    unsigned k;
    *out = NULL;
    if (!acquisition)
        return NG_OK;
    if (NgTrackingValidate(config, error) != NG_OK)
        return NG_ERROR;
    bank = calloc(1, sizeof(*bank));
    pool = calloc(1, sizeof(*pool));
    if (!bank || !pool) {
        free(bank);
        free(pool);
        return NgFail(error, "Cannot allocate receiver slots");
    }
    bank->pool = pool;
    bank->ownsPool = 1;
    bank->system = channel->id.constellation - 1;
    pool->config = *config;
    pool->sampleRate = stream->sampleRateHz;
    pool->batchLimit = (size_t)((stream->sampleRateHz + 999u) / 1000u) * NG_BANK_BATCH_MS;
    pool->captures[bank->system] = *channel;
    pool->acquisition[bank->system] = acquisition;
    for (k = 0; k < config->channelCount; ++k) {
        pool->reservation[k] = k < config->gpsChannels                             ? 1u
                               : k < config->gpsChannels + config->galileoChannels ? 2u
                                                                                   : 0u;
        if (NgTrackingCreate(config, stream, 1, &pool->tracking[k], error) != NG_OK) {
            NgTrackingBankDestroy(bank);
            return NG_ERROR;
        }
        pool->jobs[k].ops = &trackOps;
        pool->jobs[k].context = pool->tracking[k];
        pool->jobs[k].state = NG_CHANNEL_ACQUISITION;
        pool->jobs[k].id.instanceId = k + 1;
    }
    if (NgChannelDispatchCreate(pool->jobs, config->channelCount, config->workers, &pool->dispatch,
                                error) != NG_OK) {
        NgTrackingBankDestroy(bank);
        return NG_ERROR;
    }
    for (k = 0; k < config->channelCount; ++k)
        pool->order[k] = k;
    NgChannelDispatchSetOrder(pool->dispatch, pool->order);
    Refresh(pool);
    channel->ops = &bankOps;
    channel->context = bank;
    *out = bank;
    return NG_OK;
}

/** Copy the first half for older internal consumers. */
void NgTrackingBankRead(TNgTrackingBank *bank, TNgTrackingSnapshot snapshots[32])
{
    memset(snapshots, 0, sizeof(*snapshots) * 32);
    if (bank)
        memcpy(snapshots, bank->pool->slots, sizeof(*snapshots) * 32);
}

/** Publish one unified array, independent of signal subscriber order. */
void NgTrackingBankReadAll(TNgTrackingBank *bank, TNgTrackingSnapshot snapshots[64])
{
    memset(snapshots, 0, sizeof(*snapshots) * 64);
    if (bank)
        memcpy(snapshots, bank->pool->slots, sizeof(*snapshots) * 64);
}

/** PVT snapshots contain values only and share the receiver-thread publication barrier. */
void NgTrackingBankObserve(TNgTrackingBank *bank, unsigned channelId)
{
    if (bank)
        bank->pool->observedChannel = channelId;
}

int NgTrackingBankBackgroundAllowed(TNgTrackingBank *bank)
{
    return !bank || (!bank->pool->catchingUp && bank->pool->trackingLoad < 0.85);
}

void NgTrackingBankBackpressure(TNgTrackingBank *bank, int available, double occupancyPercent)
{
    if (!bank)
        return;
    if (!available)
        bank->pool->catchingUp = 0;
    else if (occupancyPercent >= 75)
        bank->pool->catchingUp = 1;
    else if (occupancyPercent <= 50)
        bank->pool->catchingUp = 0;
}

/** PVT snapshots contain values only and share the receiver-thread publication barrier. */
void NgTrackingBankReadPvt(TNgTrackingBank *bank, TNgPvtSnapshot *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    if (bank)
        *snapshot = bank->pool->pvt;
}

/** Only the owner frees shared slots; non-owner subscribers retain no resources. */
void NgTrackingBankDestroy(TNgTrackingBank *bank)
{
    if (!bank)
        return;
    if (bank->ownsPool)
        DestroyPool(bank->pool);
    free(bank);
}

/** Merge both strategies into the same total slot capacity and fixed worker budget. */
int NgTrackingBankLink(TNgTrackingBank *gps, TNgTrackingBank *galileo, TNgError *error)
{
    if (!gps || !galileo || gps->pool == galileo->pool)
        return NG_OK;
    if (gps->pool->config.channelCount != galileo->pool->config.channelCount ||
        gps->pool->config.gpsChannels != galileo->pool->config.gpsChannels ||
        gps->pool->config.galileoChannels != galileo->pool->config.galileoChannels ||
        gps->pool->config.autoPriority != galileo->pool->config.autoPriority)
        return NgFail(error, "Both signal subscribers must share the receiver channel allocation");
    gps->pool->captures[1] = galileo->pool->captures[1];
    gps->pool->acquisition[1] = galileo->pool->acquisition[1];
    DestroyPool(galileo->pool);
    galileo->pool = gps->pool;
    galileo->ownsPool = 0;
    return NgAcquisitionLink(gps->pool->acquisition[0], gps->pool->acquisition[1], error);
}
