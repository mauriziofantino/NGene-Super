/** @file butterfly.h @brief Target ABI for integer FFT SIMD kernels. */
#ifndef NGENE_FFT_BUTTERFLY_H
#define NGENE_FFT_BUTTERFLY_H
#include <stdint.h>
#include <stddef.h>
/**
 * @brief Apply one group of Q14 integer butterflies using the target ABI.
 *
 * @details
 * Internal SIMD boundary: disjoint u/v arrays each contain count interleaved complex
 * pairs, tw contains four signed Q14 coefficients per pair. Computes floor products,
 * sum/difference then /2 scaling with int16 saturation. Natural int16 alignment is sufficient;
 * caller guarantees count >=1, plan-generated Q14 rotations and valid FFT headroom. The
 * four-butterfly paths pack products losslessly (<23230 magnitude), then use exact
 * floor-average operations. No global backup or allocation.
 *
 * @param upper First read/write butterfly half, 2*count int16 values.
 * @param lower Second read/write butterfly half, 2*count int16 values.
 * @param twiddle Read-only Q14 coefficients, 4*count int16 values.
 * @param count Number of complex butterflies; at least one.
 */
void NgFftButterflies(int16_t *upper, int16_t *lower, const int16_t *twiddle, size_t count);
/**
 * @brief Fuse the first three radix-2 stages into an eight-point SIMD codelet.
 * @details Exact W=1/-j rotations and Q14 eighth-root products retain every original
 * product floor and per-stage /2 floor, including +/-16384 endpoints. No allocation,
 * global mutable state or aligned-load requirement.
 * @param data Read/write interleaved int16 workspace containing size complex samples.
 * @param size Complex sample count; a power of two of at least eight.
 */
void NgFftFirstStages(int16_t *data, size_t size);
/**
 * @brief Validate the public Q14 component headroom using SIMD comparisons.
 * @param data Read-only interleaved int16 input with natural alignment.
 * @param size Complex sample count; a power of two of at least eight.
 * @return Nonzero if every component is in [-16384,16384], otherwise zero.
 */
int NgFftInputInRange(const int16_t *data, size_t size);
#ifdef NGENE_FFT_RUNTIME_SSE2
/** @brief Test 32-bit x86 SSE2 availability without executing SSE2 instructions. */
int NgFftSimdAvailable(void);
#endif
#endif
