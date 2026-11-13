/** @file bank.c @brief One receiver slot pool shared by all acquisition strategies. */
#include "tracking/bank.h"
#include "core/channel_dispatch.h"
#include <stdlib.h>
#include <string.h>

/** Receiver-owned pool. All allocation/assignment happens outside DSP jobs. */
typedef struct {
    TNgTrackingConfig config;              /**< Frozen total capacity and reservations. */
    TNgTracking *tracking[64];             /**< One reusable private workspace per receiver slot. */
    TNgChannel jobs[64];                   /**< Stable identities independent of constellation. */
    TNgTrackingSnapshot slots[64];         /**< Copied observation state. */
    unsigned reservation[64];              /**< 0 shared, 1 GPS, 2 Galileo. */
    TNgChannel captures[2];                /**< Borrowed original acquisition registrations. */
    TNgAcquisition *acquisition[2];        /**< Borrowed workers. */
    TNgAcquisitionResult accepted[2][32];  /**< Consumed seeds. */
    TNgChannelDispatch *dispatch;          /**< One fixed pool for all slots. */
    uint64_t epoch, firstSample, expected; /**< Common-window identity. */
    size_t count;                          /**< Last window sample count. */
    unsigned nextSearch;   /**< Alternating admission above sixteen tracked channels. */
    uint64_t completed[2]; /**< Last observed strategy counts. */
    int haveWindow;        /**< A frame has been processed. */
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
    *next = snapshot.active ? NG_CHANNEL_TRACKING : NG_CHANNEL_ACQUISITION;
    return result;
}

/** Native signed formats retain their ADC levels. */
static const TNgChannelOps trackOps = {
    Track, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL};

/** Copy worker-owned results after the barrier, preserving stable slot identity. */
static void Refresh(TNgChannelPool *pool)
{
    unsigned k;
    for (k = 0; k < pool->config.channelCount; ++k) {
        NgTrackingRead(pool->tracking[k], &pool->slots[k]);
        if (!pool->slots[k].active)
            memset(&pool->slots[k], 0, sizeof(pool->slots[k]));
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
         (window->meta.flags & NG_BLOCK_GAP)))
        for (k = 0; k < pool->config.channelCount; ++k)
            pool->jobs[k].state = NG_CHANNEL_ACQUISITION;
    for (k = 0; k < pool->config.channelCount; ++k)
        if (pool->slots[k].active)
            ++active;
    if (active && NgChannelDispatchProcess(pool->dispatch, window, error) != NG_OK)
        return NG_ERROR;
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
                if (!best || seed->completedSeconds < best->completedSeconds) {
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
        pool->accepted[bestSystem][best->prn - 1] = *best;
        mask[bestSystem] |= UINT32_C(1) << (best->prn - 1);
        Refresh(pool);
    }
    active = 0;
    for (k = 0; k < pool->config.channelCount; ++k)
        if (pool->slots[k].active)
            ++active;
    for (system = 0; system < 2; ++system)
        if (searches[system].searches != pool->completed[system]) {
            pool->completed[system] = searches[system].searches;
            pool->nextSearch = 1 - system;
        }
    unsigned owner = searches[0].searching ? 0u : searches[1].searching ? 1u : pool->nextSearch;
    if (FreeSlot(pool, owner + 1, 0) == pool->config.channelCount)
        owner = 1 - owner;
    for (system = 0; system < 2; ++system) {
        unsigned slot = FreeSlot(pool, system + 1, 1);
        /* With at most two sequential strategy workers, Classic's 4/3/2 limits
           are respected through 16 tracking channels. Above 16 serialize admission. */
        int wait = active > 16 && system != owner;
        NgAcquisitionSetAdmission(pool->acquisition[system], mask[system],
                                  slot == pool->config.channelCount || wait);
        if (slot < pool->config.channelCount && searches[system].searching &&
            searches[system].currentPrn &&
            !(mask[system] & (UINT32_C(1) << (searches[system].currentPrn - 1)))) {
            pool->slots[slot].state = 1;
            pool->slots[slot].constellation = system + 1;
            pool->slots[slot].prn = searches[system].currentPrn;
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
    if (Frame(bank->pool, window, error) != NG_OK)
        return NG_ERROR;
    return capture->ops->process(capture->context, id, state, window, next, error);
}

/** Shared stream subscription operations. */
static const TNgChannelOps bankOps = {
    Process, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL};

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
        gps->pool->config.galileoChannels != galileo->pool->config.galileoChannels)
        return NgFail(error, "Both signal subscribers must share the receiver channel allocation");
    gps->pool->captures[1] = galileo->pool->captures[1];
    gps->pool->acquisition[1] = galileo->pool->acquisition[1];
    DestroyPool(galileo->pool);
    galileo->pool = gps->pool;
    galileo->ownsPool = 0;
    return NG_OK;
}
