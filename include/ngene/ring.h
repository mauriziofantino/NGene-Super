/** @file ring.h @brief Preallocated block ring: one producer, one logical reader. */
#ifndef NGENE_RING_H
#define NGENE_RING_H
#include "ngene/types.h"
/** @brief Opaque owned TNgRing handle; release with the matching destroy operation. */
typedef struct TNgRing TNgRing;

/** @brief Consistent block-level integrity counters; leased storage remains occupied. */
typedef struct {
    /** Total preallocated block slots. */
    size_t capacityBlocks;
    /** Maximum complex pairs in each slot. */
    size_t blockSamples;
    /** Occupied slots, including the outstanding reader lease. */
    size_t usedBlocks;
    /** Peak occupied slots since creation; same counter as NgRingHighWater. */
    size_t highWaterBlocks;
    /** Successfully copied complex pairs. */
    uint64_t acceptedSamples;
    /** Pairs whose reader lease was released. */
    uint64_t consumedSamples;
    /** Pending pairs, including the outstanding lease. */
    uint64_t pendingSamples;
    /** Push attempts refused because all slots were occupied. */
    uint64_t overflowEvents;
    /** Percentage of slots occupied, independent of partial block lengths. */
    double occupancyPercent;
    /** Free slots expressed in milliseconds at the specified nominal rate. */
    double availableMs;
} TNgRingStats;

/** @brief Create the existing preallocated ring for a native IQ representation.
 * @param blocks Number of slots, at least two.
 * @param blockSamples Complex-pair capacity per slot, positive.
 * @param format CI8, CU8 or little-endian SC16 Q11.
 * @param out Receives owned ring, NULL on failure.
 * @param error Optional diagnostic destination.
 * @return NG_OK on success, NG_ERROR on invalid geometry or allocation failure.
 */
int NgRingCreateFormat(size_t blocks, size_t blockSamples, TNgSampleFormat format, TNgRing **out,
                       TNgError *error);
/** @brief Copy a consistent snapshot under the existing ring mutex.
 * @param ring Live ring, or NULL for an all-zero unavailable snapshot.
 * @param sampleRateHz Nominal complex pairs/s; zero disables availableMs calculation.
 * @param stats Writable result; no sample pointers are returned.
 */
void NgRingSnapshot(TNgRing *ring, uint32_t sampleRateHz, TNgRingStats *stats);
/**
 * @brief Create a preallocated single-producer/single-reader block ring.
 *
 * @details
 * Allocates all sample slots and metadata before reception. On success the caller owns the
 * ring; stop both endpoints before destroying it. No sample-path allocations are required.
 *
 * @param blocks Number of block slots, at least two.
 * @param blockSamples Maximum complex CI8 pairs in one block; positive.
 * @param out Receives the newly allocated ring; set to NULL on allocation failure.
 * @param error Optional diagnostic destination on failure.
 *
 * @return NG_OK on success; NG_ERROR for invalid dimensions or allocation/synchronization
 * failure.
 */
int NgRingCreate(size_t blocks, size_t blockSamples, TNgRing **out, TNgError *error);
/**
 * @brief Release the ring and all its preallocated storage.
 *
 * @details
 * The producer and reader must already be stopped and all borrowed views abandoned. This
 * operation does not join their threads. A NULL ring is accepted.
 *
 * @param ring Ring to destroy; becomes invalid after return.
 */
void NgRingDestroy(TNgRing *ring);

/**
 * @brief Copy one complex block into an unused ring slot.
 *
 * @details
 * One producer may call concurrently with the reader. Input storage is borrowed only during
 * the copy. Full rings preserve every unread block; the caller chooses how to handle overflow.
 *
 * @param ring Live ring created by NgRingCreate.
 * @param input Nonempty read-only block, count <= block capacity, with valid sample indices.
 *
 * @return NG_OK if copied; NG_FULL without a copy if full; NG_ERROR for invalid input or a
 * closed ring.
 */
int NgRingPush(TNgRing *ring, const TNgIqView *input);
/**
 * @brief Borrow the next unread ring block without copying.
 *
 * @details
 * Only one reader and one outstanding lease are supported. The returned samples remain
 * immutable and valid until NgRingRelease. Closing still permits draining queued blocks.
 *
 * @param ring Live ring.
 * @param view Receives the read-only borrowed sample view on NG_OK.
 * @param timeoutMs Maximum wait in milliseconds; zero polls immediately.
 *
 * @return NG_OK for a lease; NG_TIMEOUT if empty; NG_END after close and drain; NG_ERROR for
 * invalid arguments or an existing lease.
 */
int NgRingAcquire(TNgRing *ring, TNgIqView *view, unsigned timeoutMs);
/**
 * @brief Consume and relinquish the outstanding borrowed block.
 *
 * @details
 * Call once after processing an acquired view. The sample pointer must not be used afterwards.
 * Calling with no lease has no effect; the ring itself must be valid.
 *
 * @param ring Live ring whose outstanding lease is being released.
 */
void NgRingRelease(TNgRing *ring);

/**
 * @brief Reject further pushes and wake the waiting reader.
 *
 * @details
 * May be called by the owner while endpoints are running. Queued blocks remain readable and
 * destruction is still a separate operation.
 *
 * @param ring Live ring to close.
 */
void NgRingClose(TNgRing *ring);
/**
 * @brief Read the peak number of occupied block slots.
 *
 * @details
 * Takes the ring mutex. Includes the slot currently leased to the reader.
 *
 * @param ring Live ring.
 *
 * @return Maximum occupancy since creation, in block slots.
 */
size_t NgRingHighWater(TNgRing *ring);
/**
 * @brief Count complex pairs still queued in the ring.
 *
 * @details
 * Takes the ring mutex and reads the O(1) pending counter; includes the current reader lease.
 * Intended for status, not per-sample processing.
 *
 * @param ring Live ring.
 *
 * @return Queued complex CI8 pairs, including any leased block.
 */
uint64_t NgRingPendingSamples(TNgRing *ring);
#endif
