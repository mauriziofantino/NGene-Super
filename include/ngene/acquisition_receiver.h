/** @file acquisition_receiver.h @brief Kernel adapter and bounded GPS PRN allocation. */
#ifndef NGENE_ACQUISITION_RECEIVER_H
#define NGENE_ACQUISITION_RECEIVER_H
#include "ngene/acquisition.h"
#include "ngene/channel.h"
#include "ngene/receiver.h"

/** Run-owned GPS module registrations, private workspaces and shared immutable plan. */
typedef struct TNgAcquisitionReceiver TNgAcquisitionReceiver;

/** Owned snapshot. No sample, workspace or plan pointers cross the interface. */
typedef struct {
    int enabled;
    unsigned channelCount;
    unsigned searchLimit;
    size_t trackingChannels;
    uint64_t deferredWindows;
    unsigned nonCoherentSums;
    size_t fftSize;
    uint32_t attemptedPrnMask;
    uint32_t acquiredPrnMask;
    uint64_t cycles;
    uint64_t resets;
    TNgAcquisitionResult satellites[32];
} TNgAcquisitionReceiverStatus;

/** Create registrations before frontend start. Disabled configuration returns NULL without
 * allocating plans, scratch or threads. Stream metadata must describe the actual opened source.
 * Caller owns the handle and destroys it after the engine/workers have returned. */
int NgAcquisitionReceiverCreate(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                               TNgAcquisitionReceiver **out, TNgError *error);
void NgAcquisitionReceiverDestroy(TNgAcquisitionReceiver *receiver);
/** Borrow run-owned registrations until destroy; do not mutate while the engine is running. */
TNgChannel *NgAcquisitionReceiverChannels(TNgAcquisitionReceiver *receiver, size_t *count);
/** At most four OS-scheduled workers, bounded by registered channels. */
TNgReceiverExecution NgAcquisitionReceiverExecution(const TNgAcquisitionReceiver *receiver);
/** Thread-safe snapshot; NULL receiver produces disabled/empty status. */
void NgAcquisitionReceiverRead(TNgAcquisitionReceiver *receiver,
                             TNgAcquisitionReceiverStatus *status);
#endif
