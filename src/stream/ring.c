/** @file ring.c
 * @brief NGene stream implementation.
 */
#include "ngene/ring.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>

/** @brief Preallocated SPSC window ring; occupied slots remain immutable until reader release. */
struct TNgRing {
    /** Owned preallocated complex-pair storage; released by the instance destructor. */
    unsigned char *samples;
    /** Native IQ representation of every slot. */
    TNgSampleFormat format;
    /** Bytes per simultaneous complex pair. */
    size_t stride;
    /** Block-level accepted, consumed, pending and overflow counts. */
    uint64_t accepted;
    /** Pairs whose reader lease was released. */
    uint64_t consumed;
    /** Pairs in occupied slots, including the reader lease. */
    uint64_t pending;
    /** Saturating count of full-ring push refusals. */
    uint64_t overflows;
    /** Owned metadata array, one entry per sample slot. */
    TNgBlockMeta *metadata;
    /** Preallocated sample or slot capacity, according to the owning container. */
    size_t capacity;
    /** Maximum complex pairs held by each slot. */
    size_t blockSamples;
    /** Next producer insertion slot, protected by mutex. */
    size_t head;
    /** Current reader slot, advanced only after lease release. */
    size_t tail;
    /** Number of occupied immutable slots, protected by mutex. */
    size_t used;
    /** Maximum simultaneous occupied slots observed during this run. */
    size_t highWater;
    /** Nonzero while the sole reader owns a borrowed view. */
    int leased;
    /** Nonzero after producer close; queued windows may still be drained. */
    int closed;
    /** Protects shared state; never perform transport or disk I/O while holding it. */
    TNgMutex mutex;
    /** Notifies the sole reader of data availability or close. */
    TNgCondition changed;
};

/**
 * @brief Create a preallocated single-producer/single-reader block ring.
 *
 * @details
 * Allocates all sample slots and metadata before reception. On success the caller owns the
 * ring; stop both endpoints before destroying it. No sample-path allocations are required.
 *
 * Parameter blocks: Number of block slots, at least two.
 * Parameter blockSamples: Maximum complex pairs in one block; positive.
 * Parameter format: Native IQ representation, fixed for all slots.
 * Parameter out: Receives the newly allocated ring; set to NULL on allocation failure.
 * Parameter error: Optional diagnostic destination on failure.
 *
 * Result: NG_OK on success; NG_ERROR for invalid dimensions or allocation/synchronization
 * failure.
 */
int NgRingCreateFormat(size_t blocks, size_t blockSamples, TNgSampleFormat format, TNgRing **out,
                       TNgError *error)
{
    size_t stride = NgSampleBytes(format);
    TNgRing *ring;
    if (out != NULL) {
        *out = NULL;
    }
    if (out == NULL || stride == 0 || blocks < 2 || blockSamples == 0 ||
        blocks > SIZE_MAX / sizeof(TNgBlockMeta) || blockSamples > SIZE_MAX / stride / blocks) {
        return NgFail(error, "Invalid ring geometry");
    }
    ring = calloc(1, sizeof(*ring));
    if (ring == NULL) {
        return NgFail(error, "Cannot allocate ring");
    }
    ring->samples = malloc(blocks * blockSamples * stride);
    ring->metadata = calloc(blocks, sizeof(TNgBlockMeta));
    if (ring->samples == NULL || ring->metadata == NULL) {
        goto memory_error;
    }
    if (NgMutexInit(&ring->mutex) != 0) {
        goto memory_error;
    }
    if (NgConditionInit(&ring->changed) != 0) {
        NgMutexDestroy(&ring->mutex);
        goto memory_error;
    }
    ring->format = format == 0 ? NG_FORMAT_CI8 : format;
    ring->stride = stride;
    ring->capacity = blocks;
    ring->blockSamples = blockSamples;
    *out = ring;
    return NG_OK;
memory_error:
    free(ring->metadata);
    free(ring->samples);
    free(ring);
    return NgFail(error, "Cannot allocate/init ring storage");
}

/**
 * @brief Release the ring and all its preallocated storage.
 *
 * @details
 * The producer and reader must already be stopped and all borrowed views abandoned. This
 * operation does not join their threads. A NULL ring is accepted.
 *
 * Parameter ring: Ring to destroy; becomes invalid after return.
 */
void NgRingDestroy(TNgRing *ring)
{
    if (ring == NULL) {
        return;
    }
    NgConditionDestroy(&ring->changed);
    NgMutexDestroy(&ring->mutex);
    free(ring->metadata);
    free(ring->samples);
    free(ring);
}

/**
 * @brief Copy one complex block into an unused ring slot.
 *
 * @details
 * One producer may call concurrently with the reader. Input storage is borrowed only during
 * the copy. Full rings preserve every unread block; the caller chooses how to handle overflow.
 *
 * Parameter ring: Live ring created by NgRingCreate.
 * Parameter input: Nonempty read-only block, count <= block capacity, with valid sample indices.
 *
 * Result: NG_OK if copied; NG_FULL without a copy if full; NG_ERROR for invalid input or a
 * closed ring.
 */
int NgRingPush(TNgRing *ring, const TNgIqView *input)
{
    int result = NG_OK;
    if (ring == NULL || input == NULL || input->samples == NULL || input->meta.count == 0 ||
        input->meta.count > ring->blockSamples ||
        (input->format == 0 ? NG_FORMAT_CI8 : input->format) != ring->format ||
        UINT64_MAX - input->meta.firstSample < input->meta.count ||
        ((input->meta.flags & NG_BLOCK_HARDWARE_TIME) != 0 &&
         input->meta.hardwareTimestamp > UINT64_MAX - input->meta.count)) {
        return NG_ERROR;
    }
    NgMutexLock(&ring->mutex);
    if (ring->closed != 0 || UINT64_MAX - ring->accepted < input->meta.count) {
        result = NG_ERROR;
    } else if (ring->used == ring->capacity) {
        if (ring->overflows != UINT64_MAX) {
            ++ring->overflows;
        }
        result = NG_FULL;
    } else {
        memcpy(ring->samples + ring->head * ring->blockSamples * ring->stride, input->samples,
               input->meta.count * ring->stride);
        ring->metadata[ring->head] = input->meta;
        ring->head = (ring->head + 1) % ring->capacity;
        ++ring->used;
        ring->accepted += input->meta.count;
        ring->pending += input->meta.count;
        if (ring->used > ring->highWater) {
            ring->highWater = ring->used;
        }
        NgConditionSignal(&ring->changed);
    }
    NgMutexUnlock(&ring->mutex);
    return result;
}

/**
 * @brief Borrow the next unread ring block without copying.
 *
 * @details
 * Only one reader and one outstanding lease are supported. The returned samples remain
 * immutable and valid until NgRingRelease. Closing still permits draining queued blocks.
 *
 * Parameter ring: Live ring.
 * Parameter view: Receives the read-only borrowed sample view on NG_OK.
 * Parameter timeoutMs: Maximum wait in milliseconds; zero polls immediately.
 *
 * Result: NG_OK for a lease; NG_TIMEOUT if empty; NG_END after close and drain; NG_ERROR for
 * invalid arguments or an existing lease.
 */
int NgRingAcquire(TNgRing *ring, TNgIqView *view, unsigned timeoutMs)
{
    int result;
    const double deadline = NgMonotonicSeconds() + (double)timeoutMs * 0.001;
    if (ring == NULL || view == NULL) {
        return NG_ERROR;
    }
    NgMutexLock(&ring->mutex);
    if (ring->leased != 0) {
        NgMutexUnlock(&ring->mutex);
        return NG_ERROR;
    }
    while (ring->used == 0 && ring->closed == 0 && timeoutMs != 0) {
        double remaining = deadline - NgMonotonicSeconds();
        if (remaining <= 0) {
            break;
        }
        NgConditionWait(&ring->changed, &ring->mutex, (unsigned)(remaining * 1000.0) + 1u);
    }
    if (ring->used == 0) {
        result = (ring->closed) != 0 ? NG_END : NG_TIMEOUT;
    } else {
        view->samples =
            (const TNgIq8 *)(const void *)(ring->samples +
                                           ring->tail * ring->blockSamples * ring->stride);
        view->format = ring->format;
        view->meta = ring->metadata[ring->tail];
        ring->leased = 1;
        result = NG_OK;
    }
    NgMutexUnlock(&ring->mutex);
    return result;
}

/**
 * @brief Consume and relinquish the outstanding borrowed block.
 *
 * @details
 * Call once after processing an acquired view. The sample pointer must not be used afterwards.
 * Calling with no lease has no effect; the ring itself must be valid.
 *
 * Parameter ring: Live ring whose outstanding lease is being released.
 */
void NgRingRelease(TNgRing *ring)
{
    NgMutexLock(&ring->mutex);
    if (ring->leased != 0) {
        ring->consumed += ring->metadata[ring->tail].count;
        ring->pending -= ring->metadata[ring->tail].count;
        ring->tail = (ring->tail + 1) % ring->capacity;
        --ring->used;
        ring->leased = 0;
    }
    NgMutexUnlock(&ring->mutex);
}

/**
 * @brief Reject further pushes and wake the waiting reader.
 *
 * @details
 * May be called by the owner while endpoints are running. Queued blocks remain readable and
 * destruction is still a separate operation.
 *
 * Parameter ring: Live ring to close.
 */
void NgRingClose(TNgRing *ring)
{
    NgMutexLock(&ring->mutex);
    ring->closed = 1;
    NgConditionSignal(&ring->changed);
    NgMutexUnlock(&ring->mutex);
}

/**
 * @brief Read the peak number of occupied block slots.
 *
 * @details
 * Takes the ring mutex. Includes the slot currently leased to the reader.
 *
 * Parameter ring: Live ring.
 *
 * Result: Maximum occupancy since creation, in block slots.
 */
size_t NgRingHighWater(TNgRing *ring)
{
    size_t n;
    NgMutexLock(&ring->mutex);
    n = ring->highWater;
    NgMutexUnlock(&ring->mutex);
    return n;
}

/**
 * @brief Count complex pairs still queued in the ring.
 *
 * @details
 * Takes the ring mutex and reads the O(1) pending counter; includes the current reader lease.
 * Intended for status, not per-sample processing.
 *
 * Parameter ring: Live ring.
 *
 * Result: Queued complex CI8 pairs, including any leased block.
 */
uint64_t NgRingPendingSamples(TNgRing *ring)
{
    uint64_t pending;
    NgMutexLock(&ring->mutex);
    pending = ring->pending;
    NgMutexUnlock(&ring->mutex);
    return pending;
}

/**
 * @brief Create a preallocated single-producer/single-reader block ring.
 *
 * @details
 * Allocates all sample slots and metadata before reception. On success the caller owns the
 * ring; stop both endpoints before destroying it. No sample-path allocations are required.
 *
 * Parameter blocks Number of block slots, at least two.
 * Parameter blockSamples Maximum complex CI8 pairs in one block; positive.
 * Parameter out Receives the newly allocated ring; set to NULL on allocation failure.
 * Parameter error Optional diagnostic destination on failure.
 *
 * Result: NG_OK on success; NG_ERROR for invalid dimensions or allocation/synchronization
 * failure.
 */
int NgRingCreate(size_t blocks, size_t blockSamples, TNgRing **out, TNgError *error)
{
    return NgRingCreateFormat(blocks, blockSamples, NG_FORMAT_CI8, out, error);
}

/** @brief Copy a consistent snapshot under the existing ring mutex.
 * Parameter ring Live ring, or NULL for an all-zero unavailable snapshot.
 * Parameter sampleRateHz Nominal complex pairs/s; zero disables availableMs calculation.
 * Parameter stats Writable result; no sample pointers are returned.
 */
void NgRingSnapshot(TNgRing *ring, uint32_t sampleRateHz, TNgRingStats *stats)
{
    memset(stats, 0, sizeof(*stats));
    if (ring == NULL) {
        return;
    }
    NgMutexLock(&ring->mutex);
    stats->capacityBlocks = ring->capacity;
    stats->blockSamples = ring->blockSamples;
    stats->usedBlocks = ring->used;
    stats->highWaterBlocks = ring->highWater;
    stats->acceptedSamples = ring->accepted;
    stats->consumedSamples = ring->consumed;
    stats->pendingSamples = ring->pending;
    stats->overflowEvents = ring->overflows;
    stats->occupancyPercent = 100.0 * (double)ring->used / (double)ring->capacity;
    if (sampleRateHz != 0) {
        stats->availableMs = 1000.0 * (double)(ring->capacity - ring->used) *
                             (double)ring->blockSamples / (double)sampleRateHz;
    }
    NgMutexUnlock(&ring->mutex);
}
