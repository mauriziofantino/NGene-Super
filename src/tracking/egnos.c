/** @file egnos.c @brief EGNOS L1 augmentation over the receiver's shared sample timeline. */
#include "tracking/egnos.h"
#include "acquisition/workspace.h"
#include "navigation/sbas_stream.h"
#include <stdlib.h>
#include <string.h>

/** Receiver-thread GEO state and private search worker for one auxiliary channel. */
struct TNgEgnos {
    TNgAcquisition *search;  /**< Private GEO search runner. */
    void *workspace;         /**< Owned PCPS workspace for GEO replicas. */
    TNgChannel capture;      /**< Borrowed registration from the search runner. */
    TNgTracking *tracker;    /**< Owned C/A tracking instance. */
    TNgSbasStream decoder;   /**< Frame and convolutional decoder state. */
    TNgSbasCorrections bank; /**< Corrections from the current GEO assignment. */
    double blockUntil[2];    /**< Per-GEO end of MT0 exclusion in signal seconds. */
    double rate;             /**< Native common sample rate in hertz. */
    uint64_t epoch;          /**< Current continuity generation. */
    uint64_t expected;       /**< Exclusive end of the last processed window. */
    uint64_t used[2];        /**< Last assigned capture origin per GEO. */
    unsigned usedMask;       /**< Indicates which previous origins are meaningful. */
    unsigned active;         /**< One-based search index of the assigned GEO, or zero. */
    int haveInput;           /**< Set after the first observed window. */
};

/** Preserve the GPS PCPS stages with the private GEO replica workspace. */
static int Search(void *workspace, const TNgAcquisitionCapture *capture, unsigned slot,
                  const TNgAcquisitionConfig *config, TNgAcquisitionCancel cancel,
                  void *cancelContext, TNgAcquisitionResult *result, TNgError *error)
{
    return ngGpsAcquisitionStrategy.search(workspace, capture, slot, config, cancel, cancelContext,
                                           result, error);
}

static const TNgAcquisitionStrategy egnosSearch = {Search};

/** Push prompts with sample-based reception age and remember any MT0 exclusion. */
static void Prompt(void *context, double value, uint64_t sample)
{
    TNgEgnos *egnos = context;
    NgSbasPrompt(&egnos->decoder, &egnos->bank, value, (double)sample / egnos->rate);
    egnos->blockUntil[egnos->active - 1] = egnos->bank.blockedUntil;
}

/** Allocate the independent GEO search and tracking state before reception. */
int NgEgnosCreate(const TNgStreamInfo *stream, const TNgTrackingConfig *config, TNgEgnos **out,
                  TNgError *error)
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
    track.squareCarrier = config->squareCarrier;
    track.fastCarrier = config->fastCarrier;
    track.mixerBits = config->mixerBits;
    track.xorCorrelation = config->xorCorrelation;
    track.spacingChips = config->egnosSpacingChips;
    track.cn0CutoffDbHz = config->cn0CutoffDbHz;
    track.tapCount = config->tapCount;
    track.fllDecisionUpdates = config->fllDecisionUpdates;
    track.fllDecisionIntervalMs = config->fllDecisionIntervalMs;
    track.fllTransitionHz = config->fllTransitionHz;
    memcpy(track.loops[0], config->loops[NG_TRACKING_EGNOS_SIGNAL - 1], sizeof(track.loops[0]));
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

/** Stop the auxiliary worker before releasing its workspace. */
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

/** Apply the common continuity contract before searching or updating the current GEO. */
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
        for (unsigned geoIndex = 0; geoIndex < 2; ++geoIndex) {
            const TNgAcquisitionResult *candidate = &results[geoIndex];
            if (!candidate->confirmed || candidate->epoch != egnos->epoch ||
                candidate->firstSample > view->meta.firstSample ||
                (double)(view->meta.firstSample - candidate->firstSample) > egnos->rate ||
                ((egnos->usedMask & (1u << geoIndex)) &&
                 candidate->firstSample <= egnos->used[geoIndex]) ||
                (double)view->meta.firstSample / egnos->rate < egnos->blockUntil[geoIndex]) {
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

/** Forward diagnostic tap interest to the private tracker. */
void NgEgnosObserve(TNgEgnos *egnos, int observed)
{
    if (egnos) {
        NgTrackingObserve(egnos->tracker, observed);
    }
}

/** Snapshot the auxiliary slot, including search state before a PRN is assigned. */
void NgEgnosRead(const TNgEgnos *egnos, TNgTrackingSnapshot *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    if (!egnos) {
        return;
    }
    if (egnos->active) {
        NgTrackingRead(egnos->tracker, snapshot);
        snapshot->navigation.validSubframes = egnos->bank.frameCount;
    }
    snapshot->channelId = NG_TRACKING_EGNOS_CHANNEL;
    snapshot->constellation = NG_TRACKING_EGNOS_SIGNAL;
    snapshot->prn = NgEgnosPrn(egnos);
    snapshot->state = egnos->active ? NG_CHANNEL_TRACKING : NG_CHANNEL_ACQUISITION;
}
