/** @file tracking_replica.h @brief Exact double phase and +/-1 tracking replicas. */
#ifndef NG_TRACKING_REPLICA_H
#define NG_TRACKING_REPLICA_H
#include "ngene/types.h"
/** Generate base phases, preserving base + sample*codeRate/sampleRate arithmetic order.
 * All memory is caller-owned and disjoint; count elements, allocation-free. */
void NgTrackingCodePhases(double *phases, size_t count, double base, double codeRate,
                          double sampleRate);
/** Sample GPS or Galileo BOC(1,1), with the Classic-derived double wrap rounding.
 * Phases lie within one primary-code period plus one chip; offsets within +/-16 chips.
 * chips has period signed +/-1 entries for GPS, or 2*period preexpanded BOC half chips. output has count floats. SIMD and scalar
 * paths return identical replicas; no phase quantization or amplitude reduction. */
void NgTrackingReplica(float *output, const double *phases, size_t count, const int8_t *chips,
                       unsigned period, double offset, int boc);
/** Correlate a block with one replica tap through run sums of its cumulative samples.
 * prefix holds count + 1 interleaved (I, Q) double sums, prefix[0..1] = 0. The replica is
 * sample for sample the one of NgTrackingReplica on NgTrackingCodePhases(base, codeRate,
 * sampleRate): chip boundaries are predicted, and checked with that exact arithmetic when
 * within 1e-6 samples. Results equal the per-sample correlation up to double rounding of
 * the prefix differences (relative error near 1e-15 of the block energy). */
void NgTrackingCorrelateRuns(const double *prefix, size_t count, double base, double codeRate,
                             double sampleRate, const int8_t *chips, unsigned period, double offset,
                             int boc, double *i, double *q);
#endif
