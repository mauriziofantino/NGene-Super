/** @file fft.h @brief NGene integer radix-2 FFT, derived from the legacy Q14 design. */
#ifndef NGENE_DSP_FFT_H
#define NGENE_DSP_FFT_H
#include "ngene/types.h"
/** @brief Opaque owned TNgFftPlan handle; release with the matching destroy operation. */
typedef struct TNgFftPlan TNgFftPlan;
/**
 * @brief Create immutable bit-reversal and Q14 twiddle tables.
 *
 * @details
 * Table generation may use floating point before reception. Execution uses integer butterfly
 * operations only. Concurrent calls may share the plan with disjoint workspaces; destroy only
 * after all calls finish.
 *
 * @param maximumSize Largest supported transform, a power of two in 8..16384.
 * @param out Receives caller-owned plan, or NULL on creation failure.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for dimensions or allocation failure.
 */
int NgFftCreate(size_t maximumSize, TNgFftPlan **out, TNgError *error);
/**
 * @brief Free the immutable integer FFT plan.
 *
 * @details
 * All execution calls using this plan must have returned. A NULL plan is accepted.
 *
 * @param plan Plan to destroy; invalid after return.
 */
void NgFftDestroy(TNgFftPlan *plan);
/**
 * @brief Execute the forward complex integer FFT.
 *
 * @details
 * Forward exp(-j*2*pi*k*n/N), natural-order output scaled by 1/N. Components must be in
 * [-16384,16384]; Q14 twiddles and arithmetic-floor scaling are used. Arrays must be disjoint,
 * have 2*N int16 elements and natural int16 alignment. Execution allocates no memory. Uses the
 * selected architecture SIMD backend, or explicitly configured reference. On 32-bit x86,
 * plans fall back to the scalar oracle when the CPU lacks SSE2. ARM32 NEON builds require
 * NEON-capable hardware; select the reference build for older ARM CPUs.
 *
 * @param plan Immutable plan created by NgFftCreate.
 * @param size Transform size, power of two in 8..plan maximum.
 * @param input Borrowed interleaved I,Q input; not modified.
 * @param output Caller-owned interleaved output/workspace, disjoint from input.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for invalid plan, size, overlap or input range.
 */
int NgFftExecute(const TNgFftPlan *plan, size_t size, const int16_t *input, int16_t *output,
                 TNgError *error);
/**
 * @brief Execute the bit-exact scalar reference FFT.
 *
 * @details
 * Forward exp(-j*2*pi*k*n/N), natural-order output scaled by 1/N. Components must be in
 * [-16384,16384]; Q14 twiddles and arithmetic-floor scaling are used. Arrays must be disjoint,
 * have 2*N int16 elements and natural int16 alignment. Execution allocates no memory. Uses
 * scalar butterflies for validation against the architecture backend.
 *
 * @param plan Immutable plan created by NgFftCreate.
 * @param size Transform size, power of two in 8..plan maximum.
 * @param input Borrowed interleaved I,Q input; not modified.
 * @param output Caller-owned interleaved output/workspace, disjoint from input.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for invalid plan, size, overlap or input range.
 */
int NgFftReference(const TNgFftPlan *plan, size_t size, const int16_t *input, int16_t *output,
                   TNgError *error);
/**
 * @brief Report the available Q14 FFT backend.
 *
 * @details
 * The returned string has static storage; do not free it. On 32-bit x86 it reflects the
 * CPU's SSE2 capability. Experimental x86-64 builds also check runtime AVX2 support.
 *
 * @return Static name of the available AVX2/SSE2, SSE2, NEON or C-reference backend.
 */
const char *NgFftBackend(void);
#endif
