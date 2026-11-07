/** @file acquisition.c @brief Bounded ring subscriber and cancellable acquisition worker. */
#include "ngene/acquisition.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
/** Default strategy workspace implemented separately from scheduling/ownership. */
int NgAcquisitionGpsWorkspace(const TNgStreamInfo *stream, void **out, TNgError *error);
/** Release the default workspace after its worker has joined. */
void NgAcquisitionGpsFree(void *context);
/** Default GPS operations, immutable for the process lifetime. */
extern const TNgAcquisitionStrategy ngGpsAcquisitionStrategy;

/** Owned capture handoff; slot is published only through release/acquire atomics. */
struct TNgAcquisition {
    TNgAcquisitionConfig config;
    TNgAcquisitionCapture capture;
    TNgComplex *storage;
    size_t filled;
    size_t capacity;
    uint64_t expectedSample;
    uint64_t epoch;
    int haveEpoch;
    TNgAtomicInt slot; /**< 0 producer-owned, 1 worker-owned (including ready). */
    TNgAtomicInt stop;
    TNgAtomicInt generation;
    int searchGeneration;
    int captureGeneration; /**< Producer's generation frozen before slot publication. */
    TNgThread worker;
    TNgMutex mutex; /**< Protects snapshot only, never the capture computation. */
    TNgAcquisitionSnapshot snapshot;
    uint64_t skipped;
    uint64_t gaps;
    const TNgAcquisitionStrategy *strategy;
    void *context;
    int ownsContext;
};

/** Default first functional GPS configuration. */
void NgAcquisitionDefaults(TNgAcquisitionConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->enabled = 1;
    config->firstPrn = 1;
    config->lastPrn = 32;
}

/** Reject malformed module options before worker creation. */
int NgAcquisitionValidate(const TNgAcquisitionConfig *config, TNgError *error)
{
    if (config->firstPrn < 1 || config->lastPrn > 32 || config->lastPrn < config->firstPrn ||
        (config->enabled != 0 && config->enabled != 1) ||
        (config->squareCarrier != 0 && config->squareCarrier != 1) ||
        (config->xorCorrelation != 0 && config->xorCorrelation != 1))
        return NgFail(error, "Invalid GPS acquisition PRN range or mode");
    return NG_OK;
}

/** Detect shutdown or a continuity generation changed by the producer. */
static int Cancelled(void *context)
{
    TNgAcquisition *acquisition = context;
    return NgAtomicGet(&acquisition->stop) != 0 ||
           NgAtomicGet(&acquisition->generation) != acquisition->searchGeneration;
}

/** Worker consumes one immutable capture and publishes one result after each PRN. */
static void SearchWorker(void *context)
{
    TNgAcquisition *acquisition = context;
    while (NgAtomicGet(&acquisition->stop) == 0) {
        unsigned prn;
        if (NgAtomicGet(&acquisition->slot) == 0) {
            NgSleepMs(2);
            continue;
        }
        acquisition->searchGeneration = acquisition->captureGeneration;
        NgMutexLock(&acquisition->mutex);
        acquisition->snapshot.searching = 1;
        NgMutexUnlock(&acquisition->mutex);
        for (prn = acquisition->config.firstPrn; prn <= acquisition->config.lastPrn; ++prn) {
            TNgAcquisitionResult result = {0};
            TNgError error = {{0}};
            int status;
            if (Cancelled(acquisition))
                break;
            NgMutexLock(&acquisition->mutex);
            acquisition->snapshot.currentPrn = prn;
            NgMutexUnlock(&acquisition->mutex);
            status = acquisition->strategy->search(acquisition->context, &acquisition->capture, prn,
                                                   &acquisition->config, Cancelled, acquisition,
                                                   &result, &error);
            NgMutexLock(&acquisition->mutex);
            if (!Cancelled(acquisition)) {
                acquisition->snapshot.results[prn - 1] = result;
                acquisition->snapshot.searches++;
                if (status == NG_ERROR)
                    snprintf(acquisition->snapshot.message, sizeof(acquisition->snapshot.message),
                             "%s", error.message);
            }
            NgMutexUnlock(&acquisition->mutex);
            if (status == NG_ERROR)
                break;
        }
        NgMutexLock(&acquisition->mutex);
        acquisition->snapshot.searching = 0;
        acquisition->snapshot.currentPrn = 0;
        NgAtomicSet(&acquisition->slot, 0);
        NgMutexUnlock(&acquisition->mutex);
    }
}

/** Copy common-ring samples into the producer-owned slot; never retain the borrowed view. */
static int Process(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgAcquisition *acquisition = context;
    size_t k;
    size_t take;
    (void)id;
    (void)error;
    *next = state;
    if (!acquisition->haveEpoch || window->meta.epoch != acquisition->epoch ||
        window->meta.firstSample != acquisition->expectedSample ||
        (window->meta.flags & NG_BLOCK_GAP) != 0) {
        if (acquisition->haveEpoch)
            acquisition->gaps++;
        int generation = NgAtomicGet(&acquisition->generation);
        NgAtomicSet(&acquisition->generation, generation == INT_MAX ? 0 : generation + 1);
        acquisition->filled = 0;
        acquisition->epoch = window->meta.epoch;
        acquisition->haveEpoch = 1;
    }
    acquisition->expectedSample = window->meta.firstSample + window->meta.count;
    if (NgAtomicGet(&acquisition->slot) != 0) {
        acquisition->skipped += window->meta.count;
    } else {
        if (acquisition->filled == 0)
            acquisition->capture.origin = window->meta;
        take = acquisition->capacity - acquisition->filled;
        if (take > window->meta.count)
            take = window->meta.count;
        for (k = 0; k < take; ++k) {
            acquisition->storage[acquisition->filled + k].i = (float)NgIqComponent(window, k, 0);
            acquisition->storage[acquisition->filled + k].q = (float)NgIqComponent(window, k, 1);
        }
        acquisition->filled += take;
        if (acquisition->filled == acquisition->capacity) {
            acquisition->capture.count = acquisition->capacity;
            acquisition->capture.origin.count = acquisition->capacity;
            acquisition->captureGeneration = NgAtomicGet(&acquisition->generation);
            acquisition->filled = 0;
            acquisition->skipped += window->meta.count - take;
            NgAtomicSet(&acquisition->slot, 1);
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
    Process, (1u << NG_FORMAT_CI8) | (1u << NG_FORMAT_CI16) | (1u << NG_FORMAT_SC16_Q11), NULL};

/** Construct a strategy-independent worker and channel registration. */
int NgAcquisitionCreateWithStrategy(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                                    const TNgAcquisitionStrategy *strategy, void *context,
                                    TNgAcquisition **out, TNgChannel *channel, TNgError *error)
{
    TNgAcquisition *acquisition;
    *out = NULL;
    memset(channel, 0, sizeof(*channel));
    if (NgAcquisitionValidate(config, error) != NG_OK)
        return NG_ERROR;
    if (!config->enabled)
        return NG_OK;
    if (stream->sampleRateHz < 2048000 || stream->sampleRateHz > 20000000 || strategy == NULL ||
        strategy->search == NULL)
        return NgFail(error, "Acquisition supports native rates 2.048..20 Msps");
    acquisition = calloc(1, sizeof(*acquisition));
    if (acquisition == NULL)
        return NgFail(error, "Acquisition allocation failed");
    acquisition->capacity = (size_t)(((uint64_t)stream->sampleRateHz * 56u + 999u) / 1000u);
    acquisition->storage = calloc(acquisition->capacity, sizeof(*acquisition->storage));
    if (acquisition->storage == NULL || NgMutexInit(&acquisition->mutex) != 0) {
        free(acquisition->storage);
        free(acquisition);
        return NgFail(error, "Acquisition buffer/mutex allocation failed");
    }
    acquisition->config = *config;
    acquisition->capture.samples = acquisition->storage;
    acquisition->capture.stream = *stream;
    acquisition->strategy = strategy;
    acquisition->context = context;
    acquisition->snapshot.enabled = 1;
    if (NgThreadStart(&acquisition->worker, SearchWorker, acquisition) != 0) {
        NgMutexDestroy(&acquisition->mutex);
        free(acquisition->storage);
        free(acquisition);
        return NgFail(error, "Cannot start acquisition worker");
    }
    channel->ops = &acquisitionOps;
    channel->context = acquisition;
    channel->state = NG_CHANNEL_ACQUISITION;
    channel->id.constellation = 1;
    channel->id.signal = 1;
    *out = acquisition;
    return NG_OK;
}

/** Install the default GPS strategy while keeping its workspace private. */
int NgAcquisitionCreate(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                        TNgAcquisition **out, TNgChannel *channel, TNgError *error)
{
    void *workspace = NULL;
    int status;
    *out = NULL;
    if (NgAcquisitionValidate(config, error) != NG_OK)
        return NG_ERROR;
    if (config->enabled && NgAcquisitionGpsWorkspace(stream, &workspace, error) != NG_OK)
        return NG_ERROR;
    status = NgAcquisitionCreateWithStrategy(config, stream, &ngGpsAcquisitionStrategy, workspace,
                                             out, channel, error);
    if (status != NG_OK || *out == NULL)
        NgAcquisitionGpsFree(workspace);
    else
        (*out)->ownsContext = 1;
    return status;
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
    NgMutexUnlock(&acquisition->mutex);
}

/** Join before releasing captures and strategy workspace. */
void NgAcquisitionDestroy(TNgAcquisition *acquisition)
{
    if (acquisition == NULL)
        return;
    NgAtomicSet(&acquisition->stop, 1);
    NgThreadJoin(acquisition->worker);
    if (acquisition->ownsContext)
        NgAcquisitionGpsFree(acquisition->context);
    NgMutexDestroy(&acquisition->mutex);
    free(acquisition->storage);
    free(acquisition);
}
