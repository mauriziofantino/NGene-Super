/** @file acquisition_receiver.c @brief Adapt replaceable GPS processing to common kernel windows. */
#include "ngene/acquisition_receiver.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    struct TNgAcquisitionReceiver *owner;
    TNgAcquisition *algorithm;
    uint64_t generation;
    unsigned index;
    unsigned prn;
    int needsAssignment;
} TNgAcquisitionSlot;

struct TNgAcquisitionReceiver {
    TNgAcquisitionConfig config;
    TNgAcquisitionPlan *plan;
    TNgChannel channels[32];
    TNgAcquisitionSlot slots[32];
    TNgAcquisitionReceiverStatus status;
    TNgMutex mutex;
    unsigned cursor;
    uint32_t reserved;
    uint64_t generation;
    uint64_t epoch;
    uint64_t expectedSample;
    uint64_t lastFirst;
    size_t lastCount;
    unsigned lastFlags;
    uint64_t lastMissing;
    int haveWindow;
};

/** Allocation policy is separate from the acquisition algorithm. Never assign one PRN to
 * concurrent slots; confirmed results keep their slot ready for a future tracking module. */
static unsigned SelectPrn(TNgAcquisitionReceiver *receiver, unsigned *sums)
{
    unsigned n;
    if ((receiver->config.gpsPrnMask & ~(receiver->reserved | receiver->status.acquiredPrnMask)) == 0) return 0;
    for (n = 0; n < 32; ++n) {
        unsigned candidateSums = receiver->status.nonCoherentSums;
        unsigned index = receiver->cursor++;
        uint32_t bit;
        if (receiver->cursor == 32) {
            receiver->cursor = 0;
            ++receiver->status.cycles;
            if (receiver->status.nonCoherentSums < receiver->config.nonCoherentSums) {
                ++receiver->status.nonCoherentSums;
            }
        }
        bit = UINT32_C(1) << index;
        if ((receiver->config.gpsPrnMask & bit) != 0 &&
            ((receiver->reserved | receiver->status.acquiredPrnMask) & bit) == 0) {
            receiver->reserved |= bit;
            receiver->status.attemptedPrnMask |= bit;
            *sums = candidateSums;
            return index + 1;
        }
    }
    return 0;
}

static int Process(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgAcquisitionSlot *slot = context;
    TNgAcquisitionReceiver *receiver = slot->owner;
    TNgAcquisitionResult result;
    unsigned assign = 0;
    unsigned sums = 1;
    int reset = 0;
    unsigned rank = 0, previous;
    (void)id;
    *next = state;
    NgMutexLock(&receiver->mutex);
    /* The barrier guarantees the previous window has finished. Apply a discontinuity once
     * for the bank; each callback resets only its own workspace after releasing the mutex. */
    if (!receiver->haveWindow || receiver->epoch != window->meta.epoch ||
        receiver->lastFirst != window->meta.firstSample ||
        receiver->lastCount != window->meta.count || receiver->lastFlags != window->meta.flags ||
        receiver->lastMissing != window->meta.missingBefore) {
        if (receiver->haveWindow &&
            (receiver->epoch != window->meta.epoch ||
             receiver->expectedSample != window->meta.firstSample ||
             (window->meta.flags & NG_BLOCK_GAP) != 0 || window->meta.missingBefore != 0)) {
            ++receiver->generation;
            ++receiver->status.resets;
            receiver->reserved = 0;
            receiver->status.acquiredPrnMask = 0;
            receiver->status.attemptedPrnMask = 0;
            receiver->status.nonCoherentSums = 1;
            receiver->cursor = 0;
            memset(receiver->status.satellites, 0, sizeof(receiver->status.satellites));
        }
        receiver->haveWindow = 1;
        receiver->epoch = window->meta.epoch;
        receiver->lastFirst = window->meta.firstSample;
        receiver->lastCount = window->meta.count;
        receiver->lastFlags = window->meta.flags;
        receiver->lastMissing = window->meta.missingBefore;
        receiver->expectedSample = window->meta.firstSample + window->meta.count;
    }
    if (slot->generation != receiver->generation) {
        slot->generation = receiver->generation;
        slot->needsAssignment = 1;
        slot->prn = 0;
        reset = 1;
    }
    /* Pause complete search contexts, never puncture an admitted coherent/noncoherent
     * dwell. A paused unfinished PRN is returned to the allocator and starts fresh.
     * Acquired results stay reserved; they are not counted as tracking channels. */
    for (previous = 0; previous < slot->index; ++previous) {
        unsigned prn = receiver->slots[previous].prn;
        if (!prn || !(receiver->status.acquiredPrnMask & (UINT32_C(1) << (prn - 1)))) ++rank;
    }
    if (rank >= receiver->status.searchLimit &&
        !(slot->prn && (receiver->status.acquiredPrnMask & (UINT32_C(1) << (slot->prn - 1))))) {
        if (slot->prn) {
            receiver->reserved &= ~(UINT32_C(1) << (slot->prn - 1));
            memset(&receiver->status.satellites[slot->prn - 1], 0, sizeof(result));
            slot->prn = 0;
            receiver->channels[slot->index].id.satellite = 0;
            reset = 1;
        }
        slot->needsAssignment = 1;
        ++receiver->status.deferredWindows;
        NgMutexUnlock(&receiver->mutex);
        if (reset) NgAcquisitionReset(slot->algorithm);
        return NG_OK;
    }
    if (slot->needsAssignment) {
        assign = SelectPrn(receiver, &sums);
        slot->prn = assign;
        receiver->channels[slot->index].id.satellite = assign;
        slot->needsAssignment = assign == 0;
    }
    NgMutexUnlock(&receiver->mutex);
    if (reset) NgAcquisitionReset(slot->algorithm);
    if (assign && NgAcquisitionAssign(slot->algorithm, assign, sums, error) != NG_OK) return NG_ERROR;
    if (slot->prn == 0) return NG_OK;
    if (NgAcquisitionProcess(slot->algorithm, window, &result, error) != NG_OK) return NG_ERROR;
    NgMutexLock(&receiver->mutex);
    receiver->status.satellites[slot->prn - 1] = result;
    if (result.phase == NG_ACQ_ACQUIRED) {
        receiver->status.acquiredPrnMask |= UINT32_C(1) << (slot->prn - 1);
    } else if (result.phase == NG_ACQ_NOT_FOUND) {
        receiver->reserved &= ~(UINT32_C(1) << (slot->prn - 1));
        slot->needsAssignment = 1;
    }
    NgMutexUnlock(&receiver->mutex);
    return NG_OK;
}

static void SetTrackingLoad(void *context, size_t trackingChannels)
{
    TNgAcquisitionReceiver *receiver = ((TNgAcquisitionSlot *)context)->owner;
    size_t divisor;
    NgMutexLock(&receiver->mutex);
    receiver->status.trackingChannels = trackingChannels;
    divisor = trackingChannels < 32 ? trackingChannels + 1 : 33;
    receiver->status.searchLimit = (unsigned)((receiver->status.channelCount + divisor - 1) / divisor);
    NgMutexUnlock(&receiver->mutex);
}

static const TNgChannelOps operations = {Process,
    (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CU8) | (1u << NG_FORMAT_SC16_Q11), SetTrackingLoad};

int NgAcquisitionReceiverCreate(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                               TNgAcquisitionReceiver **out, TNgError *error)
{
    TNgAcquisitionReceiver *receiver;
    unsigned i, available = 0;
    if (out == NULL || config == NULL || stream == NULL) return NgFail(error, "Missing acquisition adapter configuration");
    *out = NULL;
    if (NgAcquisitionValidate(config, error) != NG_OK) return NG_ERROR;
    if (!config->enabled) return NG_OK;
    receiver = calloc(1, sizeof(*receiver));
    if (receiver == NULL) return NgFail(error, "Cannot allocate GPS receiver adapter");
    if (NgMutexInit(&receiver->mutex) != 0) {
        free(receiver);
        return NgFail(error, "Cannot initialize GPS receiver adapter mutex");
    }
    receiver->config = *config;
    receiver->generation = 1;
    receiver->status.enabled = 1;
    receiver->status.nonCoherentSums = 1;
    receiver->status.fftSize = config->fftSize;
    while (receiver->status.fftSize > stream->sampleRateHz / 1000u) receiver->status.fftSize /= 2;
    for (i = 0; i < 32; ++i) if ((config->gpsPrnMask & (UINT32_C(1) << i)) != 0) ++available;
    receiver->status.channelCount = config->gpsChannels < available ? config->gpsChannels : available;
    receiver->status.searchLimit = receiver->status.channelCount;
    if (NgAcquisitionPlanCreate(config, stream, &receiver->plan, error) != NG_OK) {
        NgAcquisitionReceiverDestroy(receiver);
        return NG_ERROR;
    }
    for (i = 0; i < receiver->status.channelCount; ++i) {
        TNgAcquisitionSlot *slot = &receiver->slots[i];
        slot->owner = receiver;
        slot->index = i;
        slot->needsAssignment = 1;
        if (NgAcquisitionCreate(receiver->plan, &slot->algorithm, error) != NG_OK) {
            NgAcquisitionReceiverDestroy(receiver);
            return NG_ERROR;
        }
        receiver->channels[i].id.instanceId = i + 1;
        receiver->channels[i].id.constellation = 1; /* GPS */
        receiver->channels[i].id.signal = 1;        /* L1 C/A */
        receiver->channels[i].state = NG_CHANNEL_ACQUISITION;
        receiver->channels[i].ops = &operations;
        receiver->channels[i].context = slot;
    }
    *out = receiver;
    return NG_OK;
}

void NgAcquisitionReceiverDestroy(TNgAcquisitionReceiver *receiver)
{
    unsigned i;
    if (receiver == NULL) return;
    for (i = 0; i < 32; ++i) NgAcquisitionDestroy(receiver->slots[i].algorithm);
    NgAcquisitionPlanDestroy(receiver->plan);
    NgMutexDestroy(&receiver->mutex);
    free(receiver);
}

TNgChannel *NgAcquisitionReceiverChannels(TNgAcquisitionReceiver *receiver, size_t *count)
{
    if (count != NULL) *count = receiver != NULL ? receiver->status.channelCount : 0;
    return receiver != NULL ? receiver->channels : NULL;
}

TNgReceiverExecution NgAcquisitionReceiverExecution(const TNgAcquisitionReceiver *receiver)
{
    TNgReceiverExecution execution = {1};
    if (receiver != NULL && receiver->status.channelCount > 1) {
        execution.workerCount = receiver->status.channelCount < 4 ? receiver->status.channelCount : 4;
    }
    return execution;
}

void NgAcquisitionReceiverRead(TNgAcquisitionReceiver *receiver, TNgAcquisitionReceiverStatus *status)
{
    if (status == NULL) return;
    if (receiver == NULL) { memset(status, 0, sizeof(*status)); return; }
    NgMutexLock(&receiver->mutex);
    *status = receiver->status;
    NgMutexUnlock(&receiver->mutex);
}
