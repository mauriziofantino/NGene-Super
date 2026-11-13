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
#endif
