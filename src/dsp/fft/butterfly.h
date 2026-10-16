/** @file butterfly.h @brief Target ABI for integer FFT assembly kernels. */
#ifndef NGENE_FFT_BUTTERFLY_H
#define NGENE_FFT_BUTTERFLY_H
#include <stdint.h>
#include <stddef.h>
/**
 * @brief Apply one group of Q14 integer butterflies using the target ABI.
 *
 * @details
 * Internal assembly boundary: disjoint u/v arrays each contain count interleaved complex
 * pairs, tw contains four signed Q14 coefficients per pair. Computes floor products,
 * sum/difference then /2 scaling with int16 saturation. Natural int16 alignment is sufficient;
 * caller guarantees count >=1, plan-generated Q14 rotations and valid FFT headroom. The
 * x86-64 four-butterfly path packs products losslessly (<23230 magnitude), then uses exact
 * floor-average identities; scalar and NEON keep signed32 sums. No global backup or allocation.
 *
 * @param upper First read/write butterfly half, 2*count int16 values.
 * @param lower Second read/write butterfly half, 2*count int16 values.
 * @param twiddle Read-only Q14 coefficients, 4*count int16 values.
 * @param count Number of complex butterflies; at least one.
 */
void NgFftButterflies(int16_t *upper, int16_t *lower, const int16_t *twiddle, size_t count);
/**
 * @brief Fuse the first two radix-2 stages on x86-64 SSE2.
 * @details Processes four bit-reversed complex samples per iteration. Exact W=1 and W=-j
 * rotations replace Q14 products; signed32 sums are floor-scaled after each stage, preserving
 * the scalar contract even at the +/-16384 endpoints. No allocation, globals or aligned loads.
 * @param data Read/write interleaved int16 workspace containing size complex samples.
 * @param size Complex sample count; a power of two of at least eight.
 */
void NgFftFirstStages(int16_t *data, size_t size);
/**
 * @brief Validate the public Q14 component headroom using x86-64 SSE2 comparisons.
 * @param data Read-only interleaved int16 input with natural alignment.
 * @param size Complex sample count; a power of two of at least eight.
 * @return Nonzero if every component is in [-16384,16384], otherwise zero.
 */
int NgFftInputInRange(const int16_t *data, size_t size);
#endif
