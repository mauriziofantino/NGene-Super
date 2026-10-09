/** @file sample_ops.h @brief Integer sample primitives with scalar reference. */
#ifndef NGENE_SAMPLE_OPS_H
#define NGENE_SAMPLE_OPS_H
#include "ngene/types.h"
/**
 * @brief Compute exact sums and sums of squares for signed I and Q.
 *
 * @details
 * Overwrites both output pairs; does not change samples. Count must be <=262144 to respect
 * vector lane bounds. Arrays may be naturally aligned with no extra SIMD alignment; scalar
 * tails cover remainders. Uses SSE2/NEON intrinsics where selected.
 *
 * @param samples Read-only CI8 complex pairs; valid for n pairs.
 * @param n Complex-pair count in 0..262144.
 * @param sum Receives int64 sums: index 0 I, index 1 Q.
 * @param squares Receives uint64 sums of squares: index 0 I, index 1 Q.
 */
void NgIqMoments(const TNgIq8 *samples, size_t n, int64_t sum[2], uint64_t squares[2]);
/**
 * @brief Compute exact sums and sums of squares for signed I and Q.
 *
 * @details
 * Overwrites both output pairs; does not change samples. Count must be <=262144 to respect
 * vector lane bounds. Arrays may be naturally aligned with no extra SIMD alignment; scalar
 * tails cover remainders. Scalar oracle for verifying SIMD results.
 *
 * @param samples Read-only CI8 complex pairs; valid for n pairs.
 * @param n Complex-pair count in 0..262144.
 * @param sum Receives int64 sums: index 0 I, index 1 Q.
 * @param squares Receives uint64 sums of squares: index 0 I, index 1 Q.
 */
void NgIqMomentsReference(const TNgIq8 *samples, size_t n, int64_t sum[2], uint64_t squares[2]);
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
/**
 * @brief Report the compiled sample primitive backend.
 *
 * @details
 * The returned static string may be used in diagnostics; no allocation or runtime ISA probing
 * occurs.
 *
 * @return SSE2-intrinsics, NEON-intrinsics or C-reference.
 */
const char *NgSampleOpsBackend(void);
#endif
