/** @file statistics.h @brief Exact integer sample statistics with scalar reference. */
#ifndef NGENE_DSP_STATISTICS_H
#define NGENE_DSP_STATISTICS_H
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
#endif
