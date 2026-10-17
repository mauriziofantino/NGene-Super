/** @file power.h @brief Exact complex integer power with scalar reference. */
#ifndef NGENE_DSP_POWER_H
#define NGENE_DSP_POWER_H
#include "ngene/types.h"
/**
 * @brief Add exact complex integer power to uint64 accumulators.
 *
 * @details
 * For each pair adds I^2+Q^2 to the existing output. Handles both components -32768 without
 * signed32 overflow. The caller must prevent uint64 accumulator overflow and provide disjoint
 * valid arrays. SIMD operations use unsigned widening and scalar tails.
 *
 * @param iq Read-only interleaved signed int16 components, 2*n elements.
 * @param power Read/write uint64 accumulators, n elements; initialize before first
 * accumulation.
 * @param n Number of complex pairs.
 */
void NgPowerAccumulate(const int16_t *iq, uint64_t *power, size_t n);
/**
 * @brief Add exact complex integer power to uint64 accumulators.
 *
 * @details
 * For each pair adds I^2+Q^2 to the existing output. Handles both components -32768 without
 * signed32 overflow. The caller must prevent uint64 accumulator overflow and provide disjoint
 * valid arrays. Scalar oracle for verifying SIMD results.
 *
 * @param iq Read-only interleaved signed int16 components, 2*n elements.
 * @param power Read/write uint64 accumulators, n elements; initialize before first
 * accumulation.
 * @param n Number of complex pairs.
 */
void NgPowerAccumulateReference(const int16_t *iq, uint64_t *power, size_t n);
#endif
