/** @file egnos.h
 * @brief Auxiliary GEO receiver on the shared sample timeline.
 *
 * The receiver thread owns the tracker, decoder and correction bank. The
 * private acquisition worker processes its own captured samples. Only the
 * receiver thread consumes its completed results and changes assignments.
 */
#ifndef NG_TRACKING_EGNOS_H
#define NG_TRACKING_EGNOS_H
#include "ngene/tracking.h"
#include "ngene/sbas.h"

/** Opaque state for the optional EGNOS L1 channel. */
typedef struct TNgEgnos TNgEgnos;

/** @brief Allocate the GEO tracker and its private acquisition worker.
 * @param[in] stream Native sample format, rate and IF.
 * @param[in] config Per-run profile providing loop and correlator settings.
 * @param[out] out Caller-owned instance on success, NULL on failure.
 * @param[out] error Diagnostic on failure.
 * @return NG_OK on success or NG_ERROR otherwise.
 */
int NgEgnosCreate(const TNgStreamInfo *stream, const TNgTrackingConfig *config, TNgEgnos **out,
                  TNgError *error);

/** @brief Process one unique window before the next PVT computation.
 * @param[in,out] egnos Receiver-owned channel, or NULL to skip processing.
 * @param[in] view Native samples borrowed for this call.
 * @param[in] searchAllowed Nonzero to admit acquisition background work.
 * @param[out] error Diagnostic on failure.
 * @return NG_OK on success or NG_ERROR if search or tracking processing fails.
 * A sample discontinuity invalidates GEO tracking, decoder state and corrections.
 */
int NgEgnosProcess(TNgEgnos *egnos, const TNgIqView *view, int searchAllowed, TNgError *error);

/** @brief Stop the worker and free the instance.
 * @param[in] egnos Caller-owned channel, or NULL.
 */
void NgEgnosDestroy(TNgEgnos *egnos);

/** @brief Borrow the GEO correction bank during the solver call.
 * @param[in] egnos Receiver-owned channel, or NULL.
 * @return Bank owned by the instance, or NULL. The caller must not retain the
 * pointer beyond the receiver-thread processing step.
 */
const TNgSbasCorrections *NgEgnosCorrections(const TNgEgnos *egnos);

/** @brief Publish diagnostic tap interest for the auxiliary tracker.
 * @param[in,out] egnos Receiver-owned channel, or NULL.
 * @param[in] observed Nonzero to request the configured extra correlators.
 */
void NgEgnosObserve(TNgEgnos *egnos, int observed);

/** @brief Copy the current GEO tracking and framing status.
 * @param[in] egnos Receiver-owned channel, or NULL.
 * @param[out] snapshot Caller-owned destination, zeroed when the channel is disabled.
 */
void NgEgnosRead(const TNgEgnos *egnos, TNgTrackingSnapshot *snapshot);

/** @brief Report the actual GEO satellite identifier.
 * @param[in] egnos Receiver-owned channel, or NULL.
 * @return Tracked SBAS PRN, or zero while idle or searching.
 */
unsigned NgEgnosPrn(const TNgEgnos *egnos);

/** @brief Assign a GEO search result and route prompts to the SBAS decoder.
 * @param[in,out] tracking Unassigned private C/A tracker.
 * @param[in] result Confirmed acquisition using the local GEO search index.
 * @param[in] prompt Receiver-thread callback for signed 1-ms prompt I in native
 * ADC units per sample and its exclusive end sample.
 * @param[in] context Borrowed callback context, valid for the assignment.
 * @param[out] error Diagnostic on failure.
 * @return NG_OK on success or NG_ERROR on invalid handoff.
 */
int NgTrackingSeedEgnos(TNgTracking *tracking, const TNgAcquisitionResult *result,
                        void (*prompt)(void *context, double value, uint64_t sample), void *context,
                        TNgError *error);
#endif
