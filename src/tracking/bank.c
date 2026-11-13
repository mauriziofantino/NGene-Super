/** @file bank.c @brief Bounded receiver slots, ordered transitions and fixed tracking pool. */
#include "tracking/bank.h"
#include "core/channel_dispatch.h"
#include <stdlib.h>
#include <string.h>

/** Private receiver-owned allocator; workers only touch their own tracking context. */
struct TNgTrackingBank {
    TNgChannel acquisitionChannel;     /**< Borrowed original registration. */
    TNgAcquisition *acquisition;       /**< Borrowed strategy worker. */
    TNgTracking *channels[32];         /**< Per-slot workspaces, never per visible PRN. */
    TNgChannel jobs[64];               /**< Stable engine-style slot registrations. */
    TNgTrackingSnapshot slots[32];     /**< Owner-thread published status. */
    TNgAcquisitionResult accepted[32]; /**< Prevent duplicate capture consumption. */
    TNgChannel *jobViews;              /**< This bank's slice of the shared run-owned jobs. */
    TNgTrackingBank *root;        /**< One owner dispatches both banks before any search copy. */
    unsigned workers;             /**< Total fixed worker budget across constellations. */
    unsigned dispatchCount;       /**< Number of jobs owned by root. */
    TNgChannelDispatch *dispatch; /**< Fixed workers and barrier, allocated before RX. */
    uint64_t expected, epoch;     /**< Owner-thread continuity guard for all jobs. */
    int haveEpoch;                /**< Prior common window accepted. */
    TNgTrackingBank *peer;        /**< Other constellation, accessed by owner thread only. */
    unsigned trackingCount;       /**< Last completed barrier count. */
    unsigned count;               /**< Configured slot capacity. */
    unsigned system;              /**< Constellation. */
};

/** Run one satellite on its private state; engine validates and commits transitions. */
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

/** Native formats are preserved by every per-slot callback. */
static const TNgChannelOps trackOps = {
    Track, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL};

/** Publish coherent tracking status and mark unassigned slots as waiting. */
static void Refresh(TNgTrackingBank *bank)
{
    unsigned k;
    bank->trackingCount = 0;
    for (k = 0; k < bank->count; ++k) {
        NgTrackingRead(bank->channels[k], &bank->slots[k]);
        if (!bank->slots[k].active)
            memset(&bank->slots[k], 0, sizeof(bank->slots[k]));
        bank->slots[k].channelId = (bank->system - 1) * 32 + k + 1;
        bank->slots[k].constellation = bank->system;
        bank->slots[k].state = bank->slots[k].active ? 2u : 0u;
        if (bank->slots[k].active)
            ++bank->trackingCount;
        bank->jobViews[k].id.satellite = bank->slots[k].prn;
    }
}

/** Process all tracking before admitting acquisition, using the same immutable window. */
static int Process(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgTrackingBank *bank = context;
    TNgAcquisitionSnapshot acquisition;
    uint32_t excluded = 0;
    unsigned k, prn, freeSlot;
    int result;
    if (bank == bank->root) {
        if (bank->haveEpoch &&
            (bank->epoch != window->meta.epoch || bank->expected != window->meta.firstSample ||
             (window->meta.flags & NG_BLOCK_GAP))) {
            for (k = 0; k < bank->dispatchCount; ++k)
                bank->jobs[k].state = NG_CHANNEL_ACQUISITION;
        }
        bank->haveEpoch = 1;
        bank->epoch = window->meta.epoch;
        bank->expected = window->meta.firstSample + window->meta.count;
        if (bank->dispatch && (bank->trackingCount || (bank->peer && bank->peer->trackingCount)) &&
            NgChannelDispatchProcess(bank->dispatch, window, error) != NG_OK)
            return NG_ERROR;
    }
    Refresh(bank);
    NgAcquisitionRead(bank->acquisition, &acquisition);
    for (k = 0; k < bank->count; ++k)
        if (bank->slots[k].active)
            excluded |= UINT32_C(1) << (bank->slots[k].prn - 1);
    for (prn = 1; prn <= 32; ++prn) {
        TNgAcquisitionResult *seed = &acquisition.results[prn - 1];
        if (!seed->confirmed || seed->epoch != window->meta.epoch ||
            seed->firstSample > window->meta.firstSample ||
            (excluded & (UINT32_C(1) << (prn - 1))) ||
            (bank->accepted[prn - 1].confirmed && seed->epoch == bank->accepted[prn - 1].epoch &&
             seed->firstSample <= bank->accepted[prn - 1].firstSample))
            continue;
        for (freeSlot = 0; freeSlot < bank->count && bank->slots[freeSlot].active; ++freeSlot) {
        }
        if (freeSlot == bank->count)
            break;
        if (NgTrackingSeed(bank->channels[freeSlot], seed, error) != NG_OK)
            return NG_ERROR;
        /* The seed is applied after this window's barrier. Its first processed
           window will be the next one, so no sample is processed twice. */
        bank->jobViews[freeSlot].state = NG_CHANNEL_TRACKING;
        bank->accepted[prn - 1] = *seed;
        NgTrackingRead(bank->channels[freeSlot], &bank->slots[freeSlot]);
        excluded |= UINT32_C(1) << (prn - 1);
    }
    Refresh(bank);
    for (freeSlot = 0; freeSlot < bank->count && bank->slots[freeSlot].active; ++freeSlot) {
    }
    /* Classic admits 4/3/2/1 acquisitions as tracking load rises. There is
       one sequential search worker per constellation, so <=16 tracking slots
       cannot exceed those budgets. Above 16, reserve the one admission for GPS
       while it has free slots. In-flight decisions finish without modification. */
    int reserveGps = bank->system == 2 && bank->peer &&
                     bank->peer->trackingCount < bank->peer->count &&
                     bank->trackingCount + bank->peer->trackingCount > 16;
    NgAcquisitionSetAdmission(bank->acquisition, excluded, freeSlot == bank->count || reserveGps);
    if (freeSlot < bank->count && acquisition.searching && acquisition.currentPrn &&
        !(excluded & (UINT32_C(1) << (acquisition.currentPrn - 1)))) {
        bank->slots[freeSlot].state = 1;
        bank->slots[freeSlot].prn = acquisition.currentPrn;
    }
    result = bank->acquisitionChannel.ops->process(bank->acquisitionChannel.context, id, state,
                                                   window, next, error);
    return result;
}

/** Composite registration retains native formats and borrowed window ownership. */
static const TNgChannelOps bankOps = {
    Process, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL};

/** Allocate only configured slots and a bounded worker pool before starting the frontend. */
int NgTrackingBankCreate(const TNgTrackingConfig *config, const TNgStreamInfo *stream,
                         TNgAcquisition *acquisition, TNgChannel *channel, TNgTrackingBank **out,
                         TNgError *error)
{
    unsigned k;
    TNgTrackingBank *bank;
    *out = NULL;
    if (!config->enabled || !acquisition)
        return NG_OK;
    bank = calloc(1, sizeof(*bank));
    if (!bank)
        return NgFail(error, "Cannot allocate channel slots");
    bank->root = bank;
    bank->jobViews = bank->jobs;
    bank->workers = config->workers;
    bank->system = channel->id.constellation;
    bank->count = bank->system == 1 ? config->gpsChannels : config->galileoChannels;
    bank->acquisitionChannel = *channel;
    bank->acquisition = acquisition;
    for (k = 0; k < bank->count; ++k) {
        if (NgTrackingCreate(config, stream, bank->system, &bank->channels[k], error) != NG_OK) {
            NgTrackingBankDestroy(bank);
            return NG_ERROR;
        }
        bank->jobViews[k].ops = &trackOps;
        bank->jobViews[k].context = bank->channels[k];
        bank->jobViews[k].state = NG_CHANNEL_ACQUISITION;
        bank->jobViews[k].id.instanceId = (bank->system - 1) * 32 + k + 1;
        bank->jobViews[k].id.constellation = bank->system;
        bank->jobViews[k].id.signal = 1;
    }
    bank->dispatchCount = bank->count;
    if (NgChannelDispatchCreate(bank->jobs, bank->count, config->workers, &bank->dispatch, error) !=
        NG_OK) {
        NgTrackingBankDestroy(bank);
        return NG_ERROR;
    }
    Refresh(bank);
    NgAcquisitionSetAdmission(acquisition, 0, bank->count == 0);
    channel->ops = &bankOps;
    channel->context = bank;
    *out = bank;
    return NG_OK;
}

/** Copy bounded slot status; zero channelId identifies unused array entries. */
void NgTrackingBankRead(TNgTrackingBank *bank, TNgTrackingSnapshot snapshots[32])
{
    memset(snapshots, 0, sizeof(*snapshots) * 32);
    if (bank)
        memcpy(snapshots, bank->slots, sizeof(*snapshots) * bank->count);
}

/** Join every job before freeing any private workspace. */
void NgTrackingBankDestroy(TNgTrackingBank *bank)
{
    unsigned k;
    if (!bank)
        return;
    NgChannelDispatchDestroy(bank->dispatch);
    for (k = 0; k < bank->count; ++k)
        NgTrackingDestroy(bank->channels[k]);
    free(bank);
}

/** Share one fixed job pool across enabled signals; receiver calls GPS before Galileo. */
int NgTrackingBankLink(TNgTrackingBank *gps, TNgTrackingBank *galileo, TNgError *error)
{
    if (!gps || !galileo)
        return NG_OK;
    gps->peer = galileo;
    galileo->peer = gps;
    NgChannelDispatchDestroy(gps->dispatch);
    gps->dispatch = NULL;
    NgChannelDispatchDestroy(galileo->dispatch);
    galileo->dispatch = NULL;
    memcpy(gps->jobs + gps->count, galileo->jobs, sizeof(*gps->jobs) * galileo->count);
    galileo->jobViews = gps->jobs + gps->count;
    galileo->root = gps;
    gps->dispatchCount = gps->count + galileo->count;
    return NgChannelDispatchCreate(gps->jobs, gps->dispatchCount, gps->workers, &gps->dispatch,
                                   error);
}