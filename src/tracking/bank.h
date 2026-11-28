/** @file bank.h @brief Ordered shared-stream acquisition and tracking composition. */
#ifndef NG_TRACKING_BANK_H
#define NG_TRACKING_BANK_H
#include "ngene/tracking.h"
#include "ngene/pvt.h"
/** Receiver-owned composite registration. */
typedef struct TNgTrackingBank TNgTrackingBank;
/** Prepare private workspaces and wrap the borrowed acquisition registration. */
int NgTrackingBankCreate(const TNgTrackingConfig *config, const TNgStreamInfo *stream,
                         TNgAcquisition *acquisition, TNgChannel *channel, TNgTrackingBank **out,
                         TNgError *error);
/** Copy latest telemetry on the receiver thread only. */
void NgTrackingBankRead(TNgTrackingBank *bank, TNgTrackingSnapshot snapshots[32]);
/** Copy the unified receiver slot pool, including automatic reservations. */
void NgTrackingBankReadAll(TNgTrackingBank *bank, TNgTrackingSnapshot snapshots[64]);
/** Copy the last common-epoch broadcast solution on the receiver owner thread. */
void NgTrackingBankReadPvt(TNgTrackingBank *bank, TNgPvtSnapshot *snapshot);
/** Set common-epoch PVT smoothing before receiver processing starts. */
/** Receiver owner publishes optional correlator interest between window dispatches. */
void NgTrackingBankReadEgnos(TNgTrackingBank *bank, TNgTrackingSnapshot *snapshot);
void NgTrackingBankObserve(TNgTrackingBank *bank, unsigned channelId);
/** Receiver-thread headroom for optional diagnostics; no extra clock reads or DSP. */
int NgTrackingBankBackgroundAllowed(TNgTrackingBank *bank);
/** Receiver owner publishes measured ingress pressure; unavailable/file sources never throttle. */
void NgTrackingBankBackpressure(TNgTrackingBank *bank, int available, double occupancyPercent);
/** Release tracking before destroying acquisition; accepts NULL. */
void NgTrackingBankDestroy(TNgTrackingBank *bank);
/** Link constellation admission counters on the receiver thread before starting RX. */
int NgTrackingBankLink(TNgTrackingBank *gps, TNgTrackingBank *galileo, TNgError *error);
#endif
