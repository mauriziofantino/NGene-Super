/** @file ingress.c @brief No duplicate ring implementation or per-sample diagnostics. */
#include "frontend/ingress.h"
#include <string.h>

/** @brief Initialize shared bookkeeping and preallocate native-format slots.
 * Parameter ingress Zero-initialized caller-owned state.
 * Parameter config Validated queue/rate/overflow configuration.
 * Parameter format Native IQ format.
 * Parameter error Optional diagnostic.
 * Result: NG_OK on success; NG_ERROR with no owned resources on failure.
 */
int NgIngressInit(TNgIngress *ingress, const TNgConfig *config, TNgSampleFormat format,
                  TNgError *error)
{
    memset(ingress, 0, sizeof(*ingress));
    if (NgMutexInit(&ingress->mutex) != 0) {
        return NgFail(error, "Cannot initialize ingress counters");
    }
    if (NgRingCreateFormat(config->ingressBlocks, config->windowSamples, format, &ingress->ring,
                           error) != NG_OK) {
        NgMutexDestroy(&ingress->mutex);
        return NG_ERROR;
    }
    ingress->config = *config;
    ingress->format = format == 0 ? NG_FORMAT_CI8 : format;
    return NG_OK;
}

/** @brief Record a transport error independently of queue overflow.
 * Parameter ingress Live state, callable outside or inside its single producer.
 */
void NgIngressTransportError(TNgIngress *ingress)
{
    NgMutexLock(&ingress->mutex);
    if (ingress->published.transportErrors != UINT64_MAX) {
        ++ingress->published.transportErrors;
    }
    NgMutexUnlock(&ingress->mutex);
    NgAtomicSet(&ingress->failure, 1);
    NgRingClose(ingress->ring);
}

/** @brief Record vendor metadata gaps; never label them hardware-only loss.
 * Parameter ingress Live initialized state.
 * Parameter missing Exact timestamp-derived missing pairs, or zero if unknown.
 */
void NgIngressDeviceGap(TNgIngress *ingress, uint64_t missing)
{
    NgMutexLock(&ingress->mutex);
    if (ingress->published.deviceGapEvents != UINT64_MAX) {
        ++ingress->published.deviceGapEvents;
    }
    if (UINT64_MAX - ingress->published.deviceMissingSamples >= missing) {
        ingress->published.deviceMissingSamples += missing;
    }
    NgMutexUnlock(&ingress->mutex);
}

/** @brief Add a timestamp-derived duration to an already reported device gap.
 * Parameter ingress Live initialized source.
 * Parameter missing Missing complex pairs; zero leaves the duration unknown.
 */
void NgIngressDeviceMissing(TNgIngress *ingress, uint64_t missing)
{
    NgMutexLock(&ingress->mutex);
    if (UINT64_MAX - ingress->published.deviceMissingSamples >= missing) {
        ingress->published.deviceMissingSamples += missing;
    }
    NgMutexUnlock(&ingress->mutex);
}

/** @brief Copy a whole vendor transfer in bounded chunks, before its pointer expires.
 * @details Single producer only; assigns GAP metadata for known software drops. No
 * allocation or I/O. Publishes counters once per transfer. Returns NG_FULL only after a
 * stop-policy overflow; drop-newest continues and exposes the exact dropped-pair count.
 * Parameter ingress Live initialized ingress.
 * Parameter transfer Native immutable transfer with caller-supplied monotonic sample indices.
 * Result: NG_OK, NG_FULL on overflow-stop, or NG_ERROR for malformed input/closed ring.
 */
int NgIngressSubmit(TNgIngress *ingress, const TNgIqView *transfer)
{
    size_t offset = 0;
    int status = NG_OK;
    size_t stride = NgSampleBytes(transfer->format);
    if (transfer->samples == NULL || transfer->meta.count == 0 || stride == 0 ||
        transfer->meta.count > SIZE_MAX / stride ||
        (transfer->format == 0 ? NG_FORMAT_CI8 : transfer->format) != ingress->format ||
        transfer->meta.firstSample > UINT64_MAX - transfer->meta.count ||
        ingress->received > UINT64_MAX - transfer->meta.count ||
        ((transfer->meta.flags & NG_BLOCK_HARDWARE_TIME) != 0 &&
         transfer->meta.hardwareTimestamp > UINT64_MAX - transfer->meta.count)) {
        NgIngressTransportError(ingress);
        return NG_ERROR;
    }
    if ((transfer->meta.flags & NG_BLOCK_GAP) != 0) {
        if (ingress->pendingDeviceGap == 0) {
            ingress->pendingDeviceMissingKnown = transfer->meta.missingKnown;
        } else if (transfer->meta.missingKnown == 0) {
            ingress->pendingDeviceMissingKnown = 0;
        }
        if (UINT64_MAX - ingress->pendingDeviceMissing < transfer->meta.missingBefore) {
            NgIngressTransportError(ingress);
            return NG_ERROR;
        }
        ingress->pendingDeviceGap = 1;
        ingress->pendingDeviceMissing += transfer->meta.missingBefore;
    }
    while (offset < transfer->meta.count) {
        TNgIqView chunk = *transfer;
        int result;
        chunk.samples =
            (const TNgIq8 *)(const void *)((const unsigned char *)(const void *)transfer->samples +
                                           offset * stride);
        chunk.meta.firstSample += offset;
        if ((chunk.meta.flags & NG_BLOCK_HARDWARE_TIME) != 0) {
            chunk.meta.hardwareTimestamp += offset;
        }
        chunk.meta.count = transfer->meta.count - offset;
        if (chunk.meta.count > ingress->config.windowSamples) {
            chunk.meta.count = ingress->config.windowSamples;
        }
        chunk.meta.flags &= ~(unsigned)NG_BLOCK_GAP;
        chunk.meta.missingBefore = 0;
        chunk.meta.missingKnown = 0;
        if (ingress->pendingMissing != 0 || ingress->pendingDeviceGap != 0) {
            chunk.meta.missingKnown =
                ingress->pendingDeviceGap == 0 || ingress->pendingDeviceMissingKnown != 0;
            if (chunk.meta.missingKnown != 0) {
                if (UINT64_MAX - ingress->pendingDeviceMissing < ingress->pendingMissing) {
                    NgIngressTransportError(ingress);
                    status = NG_ERROR;
                    break;
                }
                chunk.meta.missingBefore = ingress->pendingDeviceMissing + ingress->pendingMissing;
            }
            chunk.meta.flags |= NG_BLOCK_GAP;
        }
        result = NgRingPush(ingress->ring, &chunk);
        if (result == NG_OK) {
            ingress->pendingMissing = 0;
            ingress->pendingDeviceMissing = 0;
            ingress->pendingDeviceGap = 0;
        } else if (result == NG_FULL) {
            size_t lost = ingress->config.overflowPolicy == NG_OVERFLOW_STOP
                              ? transfer->meta.count - offset
                              : chunk.meta.count;
            ingress->dropped += lost;
            ingress->pendingMissing += lost;
            if (ingress->config.overflowPolicy == NG_OVERFLOW_STOP) {
                NgAtomicSet(&ingress->failure, 2);
                NgRingClose(ingress->ring);
                status = NG_FULL;
                break;
            }
        } else {
            NgIngressTransportError(ingress);
            status = NG_ERROR;
            break;
        }
        offset += chunk.meta.count;
    }
    ingress->received += transfer->meta.count;
    NgMutexLock(&ingress->mutex);
    ingress->published.receivedSamples = ingress->received;
    ingress->published.droppedSamples = ingress->dropped;
    NgMutexUnlock(&ingress->mutex);
    return status;
}

/** @brief Copy common counters and the existing ring's high-water snapshot.
 * Parameter ingress Live initialized ingress.
 * Parameter stopped Nonzero after RX thread/callbacks have joined.
 * Parameter stats Writable copied result.
 */
void NgIngressSnapshot(TNgIngress *ingress, int stopped, TNgFrontendStats *stats)
{
    NgMutexLock(&ingress->mutex);
    *stats = ingress->published;
    NgMutexUnlock(&ingress->mutex);
    NgRingSnapshot(ingress->ring, ingress->config.sampleRateHz, &stats->ingress);
    stats->ingressAvailable = 1;
    stats->queuedSamples = stats->ingress.pendingSamples;
    stats->discardedOnStop = stopped != 0 ? stats->queuedSamples : 0;
}

/** @brief Free initialized state after both endpoints stop and all leases end.
 * Parameter ingress Initialized state; ring NULL means initialization failed.
 */
void NgIngressDestroy(TNgIngress *ingress)
{
    if (ingress->ring != NULL) {
        NgRingDestroy(ingress->ring);
        ingress->ring = NULL;
        NgMutexDestroy(&ingress->mutex);
    }
}
