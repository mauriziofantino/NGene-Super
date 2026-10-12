/** @file ingress.h @brief Shared adapter bookkeeping around the existing NGene ring. */
#ifndef NGENE_INGRESS_H
#define NGENE_INGRESS_H
#include "ngene/frontend.h"
#include "platform/platform.h"

/** @brief One producer-owned ingress with copied block publication and loss accounting. */
typedef struct {
    /** Owned existing SPSC ring; occupied leases can never be overwritten. */
    TNgRing *ring;
    /** Source format, nominal rate and overflow policy, copied before RX. */
    TNgConfig config;
    /** Producer-private delivered and software-overflow pair counts. */
    uint64_t received;
    /** Producer-owned software-overflow loss count. */
    uint64_t dropped;
    /** Known software losses preceding the next accepted block. */
    uint64_t pendingMissing;
    /** Vendor gap metadata retained until the next accepted block. */
    uint64_t pendingDeviceMissing;
    /** Whether an unreported vendor gap exists and whether its duration is exact. */
    int pendingDeviceGap;
    /** Nonzero if all pending device-gap durations are exact. */
    int pendingDeviceMissingKnown;
    /** Fixed native representation of the ring. */
    TNgSampleFormat format;
    /** Atomic error: 1 transport/shape failure, 2 software ingress overflow. */
    TNgAtomicInt failure;
    /** Protects transfer-level snapshots, not individual IQ samples. */
    TNgMutex mutex;
    /** Published common counters; device counters are updated explicitly by the adapter. */
    TNgFrontendStats published;
} TNgIngress;

/** @brief Initialize shared bookkeeping and preallocate native-format slots.
 * @param ingress Zero-initialized caller-owned state.
 * @param config Validated queue/rate/overflow configuration.
 * @param format Native IQ format.
 * @param error Optional diagnostic.
 * @return NG_OK on success; NG_ERROR with no owned resources on failure.
 */
int NgIngressInit(TNgIngress *ingress, const TNgConfig *config, TNgSampleFormat format,
                  TNgError *error);
/** @brief Copy a whole vendor transfer in bounded chunks, before its pointer expires.
 * @details Single producer only; assigns GAP metadata for known software drops. No
 * allocation or I/O. Publishes counters once per transfer. Returns NG_FULL only after a
 * stop-policy overflow; drop-newest continues and exposes the exact dropped-pair count.
 * @param ingress Live initialized ingress.
 * @param transfer Native immutable transfer with caller-supplied monotonic sample indices.
 * @return NG_OK, NG_FULL on overflow-stop, or NG_ERROR for malformed input/closed ring.
 */
int NgIngressSubmit(TNgIngress *ingress, const TNgIqView *transfer);
/** @brief Copy common counters and the existing ring's high-water snapshot.
 * @param ingress Live initialized ingress.
 * @param stopped Nonzero after RX thread/callbacks have joined.
 * @param stats Writable copied result.
 */
void NgIngressSnapshot(TNgIngress *ingress, int stopped, TNgFrontendStats *stats);
/** @brief Record a transport error independently of queue overflow.
 * @param ingress Live state, callable outside or inside its single producer.
 */
void NgIngressTransportError(TNgIngress *ingress);
/** @brief Record vendor metadata gaps; never label them hardware-only loss.
 * @param ingress Live initialized state.
 * @param missing Exact timestamp-derived missing pairs, or zero if unknown.
 */
void NgIngressDeviceGap(TNgIngress *ingress, uint64_t missing);
/** @brief Add an exact duration for a device-gap event already reported.
 * @param ingress Live initialized source.
 * @param missing Missing complex pairs; zero leaves the duration unknown.
 */
void NgIngressDeviceMissing(TNgIngress *ingress, uint64_t missing);
/** @brief Free initialized state after both endpoints stop and all leases end.
 * @param ingress Initialized state; ring NULL means initialization failed.
 */
void NgIngressDestroy(TNgIngress *ingress);
#endif
