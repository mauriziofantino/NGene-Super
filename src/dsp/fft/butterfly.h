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
 * caller guarantees count >=1 and valid FFT headroom. No global register backup or allocation.
 *
 * @param upper First read/write butterfly half, 2*count int16 values.
 * @param lower Second read/write butterfly half, 2*count int16 values.
 * @param twiddle Read-only Q14 coefficients, 4*count int16 values.
 * @param count Number of complex butterflies; at least one.
 */
void NgFftButterflies(int16_t *upper, int16_t *lower, const int16_t *twiddle, size_t count);
#endif
