/** @file gps_ca.h @brief GPS L1 C/A replicas and persistent code phase. */
#ifndef NGENE_DSP_GPS_CA_H
#define NGENE_DSP_GPS_CA_H
#include "ngene/types.h"
#define NG_GPS_CA_LENGTH 1023u
#define NG_GPS_CA_RATE_HZ 1023000.0

/** Code phase in Q32 chips, modulo 1023 chips. The state belongs to one consumer. */
typedef struct {
    uint64_t phase;
    uint64_t step;
    uint64_t remainder;
    uint32_t denominator;
    uint32_t wholeStep;
    uint32_t fractionalStep;
} TNgGpsCaNco;

/** Generate PRN 1..32. Chip zero/polarity match legacy NGene GPS_codes.dat (+1 first). */
int NgGpsCaGenerate(unsigned prn, int8_t chips[NG_GPS_CA_LENGTH], TNgError *error);
/** Set the chip phase at the first sample; positive phase advances the local replica. */
int NgGpsCaNcoInit(TNgGpsCaNco *nco, double sampleRateHz, double codeRateHz, double phaseChips,
                   TNgError *error);
/** Emit one chip and advance; no allocation or transcendental operations. */
int NgGpsCaNext(const int8_t chips[NG_GPS_CA_LENGTH], TNgGpsCaNco *nco);
/** Generate sampled chips, preserving fractional code phase across calls. */
void NgGpsCaSample(const int8_t chips[NG_GPS_CA_LENGTH], TNgGpsCaNco *nco, int8_t *output,
                   size_t count);
#endif
