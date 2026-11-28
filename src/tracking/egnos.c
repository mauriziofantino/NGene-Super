/** @file egnos.c @brief EGNOS L1 augmentation over the receiver's shared sample timeline. */
#include "tracking/egnos.h"
#include "acquisition/workspace.h"
#include "navigation/sbas_stream.h"
#include <stdlib.h>
#include <string.h>

struct TNgEgnos {
    TNgAcquisition *search;
    void *workspace;
    TNgChannel capture;
    TNgTracking *tracker;
    TNgSbasStream decoder;
    TNgSbasCorrections bank;
    double blockUntil[2];
    double rate;
    uint64_t epoch, expected;
    uint64_t used[2];
    unsigned usedMask, active;
    int haveInput;
};

/** Preserve the GPS PCPS stages with the private GEO replica workspace. */
static int Search(void *workspace, const TNgAcquisitionCapture *capture, unsigned slot,
                  const TNgAcquisitionConfig *config, TNgAcquisitionCancel cancel,
                  void *cancelContext, TNgAcquisitionResult *result, TNgError *error)
{
    return ngGpsAcquisitionStrategy.search(workspace, capture, slot, config, cancel,
                                           cancelContext, result, error);
}

static const TNgAcquisitionStrategy egnosSearch = {Search};

/** Push prompts with sample-based reception age and remember any MT0 exclusion. */
static void Prompt(void *context, double value, uint64_t sample)
{
    TNgEgnos *egnos = context;
    NgSbasPrompt(&egnos->decoder, &egnos->bank, value, (double)sample / egnos->rate);
    egnos->blockUntil[egnos->active - 1] = egnos->bank.blockedUntil;
}

int NgEgnosCreate(const TNgStreamInfo *stream, TNgEgnos **out, TNgError *error)
{
    *out = NULL;
    TNgEgnos *egnos = calloc(1, sizeof(*egnos));
    if (!egnos) {
        return NgFail(error, "Cannot allocate EGNOS receiver");
    }
    TNgAcquisitionConfig search;
    TNgTrackingConfig track;
    NgAcquisitionDefaults(&search);
    search.firstPrn = 1;
    search.lastPrn = 2;
    search.cpuBudget = 0.25;
    NgTrackingDefaults(&track);
    egnos->rate = stream->sampleRateHz;
    if (NgAcquisitionEgnosWorkspace(stream, &egnos->workspace, error) != NG_OK ||
        NgAcquisitionCreateWithStrategy(&search, stream, &egnosSearch, egnos->workspace,
                                        &egnos->search, &egnos->capture, error) != NG_OK ||
        NgTrackingCreate(&track, stream, NG_CONSTELLATION_GPS, &egnos->tracker, error) != NG_OK) {
        NgEgnosDestroy(egnos);
        return NG_ERROR;
    }
    *out = egnos;
    return NG_OK;
}

void NgEgnosDestroy(TNgEgnos *egnos)
{
    if (egnos) {
        NgAcquisitionDestroy(egnos->search);
        NgAcquisitionGpsFree(egnos->workspace);
        NgTrackingDestroy(egnos->tracker);
        free(egnos);
    }
}

/** Discard all correction issues and symbol state when an assignment ends. */
static void ResetAssignment(TNgEgnos *egnos)
{
    egnos->active = 0;
    memset(&egnos->bank, 0, sizeof(egnos->bank));
    memset(&egnos->decoder, 0, sizeof(egnos->decoder));
}

int NgEgnosProcess(TNgEgnos *egnos, const TNgIqView *view, int searchAllowed, TNgError *error)
{
    if (!egnos) {
        return NG_OK;
    }
    const int gap = (view->meta.flags & NG_BLOCK_GAP) ||
        (egnos->haveInput && (egnos->epoch != view->meta.epoch ||
                             egnos->expected != view->meta.firstSample));
    if (gap) {
        ResetAssignment(egnos);
        egnos->usedMask = 0;
    }
    egnos->haveInput = 1;
    egnos->epoch = view->meta.epoch;
    egnos->expected = view->meta.firstSample + view->meta.count;
    (void)NgAcquisitionAdvanceSignal(egnos->search, egnos->epoch, egnos->expected, egnos->rate);

    if (!egnos->active && !gap) {
        TNgAcquisitionResult results[NG_SATELLITE_MAX_PRN];
        NgAcquisitionReadResults(egnos->search, results);
        const TNgAcquisitionResult *chosen = NULL;
        for (unsigned k = 0; k < 2; ++k) {
            const TNgAcquisitionResult *candidate = &results[k];
            if (!candidate->confirmed || candidate->epoch != egnos->epoch ||
                candidate->firstSample > view->meta.firstSample ||
                (double)(view->meta.firstSample - candidate->firstSample) > egnos->rate ||
                ((egnos->usedMask & (1u << k)) && candidate->firstSample <= egnos->used[k]) ||
                (double)view->meta.firstSample / egnos->rate < egnos->blockUntil[k]) {
                continue;
            }
            if (!chosen || candidate->completedSeconds < chosen->completedSeconds) {
                chosen = candidate;
            }
        }
        if (chosen) {
            if (NgTrackingSeedEgnos(egnos->tracker, chosen, Prompt, egnos, error) != NG_OK) {
                return NG_ERROR;
            }
            egnos->active = chosen->prn;
            egnos->used[chosen->prn - 1] = chosen->firstSample;
            egnos->usedMask |= 1u << (chosen->prn - 1);
        }
    }
    if (egnos->active) {
        if (NgTrackingProcess(egnos->tracker, view, error) != NG_OK) {
            return NG_ERROR;
        }
        TNgTrackingSnapshot snapshot;
        NgTrackingRead(egnos->tracker, &snapshot);
        const double now = (double)egnos->expected / egnos->rate;
        if (!snapshot.active || now < egnos->bank.blockedUntil ||
            (now - (double)snapshot.trackingStartSample / egnos->rate > 8 &&
             (!egnos->bank.frameCount || now - egnos->bank.lastFrame > 8))) {
            ResetAssignment(egnos);
        }
    }
    NgAcquisitionSetParallelism(egnos->search, 1, 1, searchAllowed ? 1u : 0u);
    NgAcquisitionSetAdmission(egnos->search, 0, egnos->active || !searchAllowed);
    TNgChannelState next = NG_CHANNEL_ACQUISITION;
    return egnos->capture.ops->process(egnos->capture.context, &egnos->capture.id,
                                      NG_CHANNEL_ACQUISITION, view, &next, error);
}

const TNgSbasCorrections *NgEgnosCorrections(const TNgEgnos *egnos)
{
    return egnos ? &egnos->bank : NULL;
}

unsigned NgEgnosPrn(const TNgEgnos *egnos)
{
    return !egnos || !egnos->active ? 0 : egnos->active == 1 ? 121u : 136u;
}

/** Snapshot the auxiliary slot, including search state before a PRN is assigned. */
void NgEgnosRead(const TNgEgnos *egnos, TNgTrackingSnapshot *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    if (!egnos)
        return;
    if (egnos->active) {
        NgTrackingRead(egnos->tracker, snapshot);
        snapshot->navigation.validSubframes = egnos->bank.frameCount;
    }
    snapshot->channelId = NG_TRACKING_EGNOS_CHANNEL;
    snapshot->constellation = NG_TRACKING_EGNOS_SIGNAL;
    snapshot->prn = NgEgnosPrn(egnos);
    snapshot->state = egnos->active ? NG_CHANNEL_TRACKING : NG_CHANNEL_ACQUISITION;
}
