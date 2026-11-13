/** @file bank.c @brief Ordered acquisition-to-tracking handoff without independent ring cursors. */
#include "tracking/bank.h"
#include <stdlib.h>
#include <string.h>

/** Receiver-owned state; presentation receives copies only. */
struct TNgTrackingBank {
    TNgChannel acquisitionChannel;     /**< Borrowed original registration. */
    TNgAcquisition *acquisition;       /**< Borrowed worker. */
    TNgTracking *channels[32];         /**< Private per-PRN workspaces. */
    TNgAcquisitionResult accepted[32]; /**< Prevent stale seed reuse. */
};

/** Process a borrowed common window and consume confirmed results on this thread. */
static int Process(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgTrackingBank *bank = context;
    TNgAcquisitionSnapshot acquisition;
    unsigned k;
    int result = bank->acquisitionChannel.ops->process(bank->acquisitionChannel.context, id, state,
                                                       window, next, error);
    if (result != NG_OK)
        return result;
    NgAcquisitionRead(bank->acquisition, &acquisition);
    for (k = 0; k < 32; ++k) {
        TNgTrackingSnapshot snapshot;
        TNgAcquisitionResult *seed = &acquisition.results[k];
        NgTrackingRead(bank->channels[k], &snapshot);
        if (!snapshot.active && seed->confirmed && seed->epoch == window->meta.epoch &&
            seed->firstSample <= window->meta.firstSample &&
            (!bank->accepted[k].confirmed || seed->epoch != bank->accepted[k].epoch ||
             seed->firstSample > bank->accepted[k].firstSample)) {
            if (NgTrackingSeed(bank->channels[k], seed, error) != NG_OK)
                return NG_ERROR;
            bank->accepted[k] = *seed;
        }
        if (NgTrackingProcess(bank->channels[k], window, error) != NG_OK)
            return NG_ERROR;
    }
    return NG_OK;
}

/** Composite native format support. */
static const TNgChannelOps bankOps = {
    Process, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL};

/** Allocate before reception and retain only the original borrowed worker. */
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
        return NgFail(error, "Cannot allocate tracking bank");
    bank->acquisitionChannel = *channel;
    bank->acquisition = acquisition;
    for (k = 0; k < 32; ++k) {
        if (NgTrackingCreate(config, stream, channel->id.constellation, &bank->channels[k],
                             error) != NG_OK) {
            NgTrackingBankDestroy(bank);
            return NG_ERROR;
        }
    }
    channel->ops = &bankOps;
    channel->context = bank;
    *out = bank;
    return NG_OK;
}

/** Copy owned scalar values, never pointers. */
void NgTrackingBankRead(TNgTrackingBank *bank, TNgTrackingSnapshot snapshots[32])
{
    unsigned k;
    memset(snapshots, 0, sizeof(*snapshots) * 32);
    if (bank)
        for (k = 0; k < 32; ++k)
            NgTrackingRead(bank->channels[k], &snapshots[k]);
}

/** Safe for partially allocated banks. */
void NgTrackingBankDestroy(TNgTrackingBank *bank)
{
    unsigned k;
    if (!bank)
        return;
    for (k = 0; k < 32; ++k)
        NgTrackingDestroy(bank->channels[k]);
    free(bank);
}
