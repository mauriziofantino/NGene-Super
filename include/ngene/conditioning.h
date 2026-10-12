/** @file conditioning.h @brief Synchronous stage before the central ring. */
#ifndef NGENE_CONDITIONING_H
#define NGENE_CONDITIONING_H
#include "ngene/types.h"

/** @brief Synchronous rate-preserving conditioning stage before the shared ring. */
typedef struct {
    /** Opaque implementation or callback state; ownership follows the enclosing API. */
    void *context;
    /** Borrowed immutable conditioning module name. */
    const char *name;
    /** @brief Apply one synchronous CI8 conditioning stage before the shared ring.
     * @details Output may alias input and is borrowed until the next call. Never retain
     * input. Preserve count, sample rate, indices and epoch; rate-changing stages require a
     * future explicit grid/delay contract. One receiver thread owns the conditioner state.
     * @param context Private conditioner state, or NULL for stateless bypass.
     * @param input Read-only borrowed frontend samples.
     * @param output Receives borrowed conditioned samples and preserved metadata.
     * @param error Optional processing error destination.
     * @return NG_OK on success; NG_ERROR to stop the run on processing failure.
     */
    int (*process)(void *context, const TNgIqView *input, TNgIqView *output, TNgError *error);
} TNgConditioner;

/**
 * @brief Return a stateless conditioner which preserves the input view.
 *
 * @details
 * No allocation or destroy is necessary. The process callback aliases the input samples and
 * metadata; their original lifetime still applies.
 *
 * @return A rate-preserving CI8 conditioner value.
 */
TNgConditioner NgConditionerBypass(void);
/**
 * @brief Allocate a continuous fixed-point I/Q DC estimator.
 *
 * @details
 * Creates private output storage. Its process callback preserves count/rate/indices and
 * saturates corrected components to CI8; state persists across blocks and resets on gaps,
 * index discontinuities or a new epoch. Use NgConditionerDcDestroy after the engine stops.
 *
 * @param capacity Maximum complex pairs accepted per process call; positive.
 * @param shift Estimator exponent 4..24: alpha = 2^-shift per sample.
 * @param out Receives the owned conditioner instance on success.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for invalid bounds or allocation failure.
 */
int NgConditionerDcCreate(size_t capacity, unsigned shift, TNgConditioner *out, TNgError *error);
/** @brief Allocate the continuous DC remover for a native integer IQ format.
 * @details No block-wise mean reset. CU8 is recentered only for arithmetic and stored
 * as CU8 again; SC16 Q11 retains 12-bit ADC resolution and saturates to [-2048,2047].
 * @param capacity Maximum complex pairs per call, positive.
 * @param shift Continuous estimator exponent in 4..24.
 * @param format Native IQ representation.
 * @param out Receives owned conditioner.
 * @param error Optional diagnostic.
 * @return NG_OK on success, NG_ERROR on invalid format/capacity/allocation.
 */
int NgConditionerDcCreateFormat(size_t capacity, unsigned shift, TNgSampleFormat format,
                                TNgConditioner *out, TNgError *error);
/**
 * @brief Release storage owned by the continuous DC conditioner.
 *
 * @details
 * Stop processing first. A NULL pointer or a conditioner of another implementation is ignored;
 * a destroyed DC instance is replaced with a bypass value.
 *
 * @param conditioner Conditioner created by NgConditionerDcCreate.
 */
void NgConditionerDcDestroy(TNgConditioner *conditioner);
#endif
