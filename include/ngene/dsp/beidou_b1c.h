/** @file beidou_b1c.h @brief BeiDou B1C primary codes and ideal baseband replicas. */
#ifndef NGENE_DSP_BEIDOU_B1C_H
#define NGENE_DSP_BEIDOU_B1C_H

#include "ngene/dsp/search.h"

#define NG_BEIDOU_B1C_LENGTH 10230u
#define NG_BEIDOU_B1C_RATE_HZ 1023000u
#define NG_BEIDOU_B1C_PERIOD_MS 10u
#define NG_BEIDOU_B1C_SUBCHIPS 12u

typedef enum {
    NG_BEIDOU_B1C_DATA,
    NG_BEIDOU_B1C_PILOT
} TNgBeidouB1cComponent;

/** Generate one primary period for PRN 1..63, starting at chip zero.
 * ICD bit 0 maps to +1 and bit 1 to -1. Output is untouched on error. */
int NgBeidouB1cGenerate(unsigned prn, TNgBeidouB1cComponent component,
                        int8_t chips[NG_BEIDOU_B1C_LENGTH], TNgError *error);

/** Ideal ICD Eq. 4-11 baseband with data symbol +1 and primary codes only.
 * phase is the number of twelfths of a chip from chip zero. Intervals include their
 * left boundary, and phase wraps after one primary period. Outputs have powers
 * 1/4 (data) and 3/4 (pilot). Each input array must contain one generated period. */
float NgBeidouB1cDataAt(const int8_t chips[NG_BEIDOU_B1C_LENGTH], uint64_t phase);
TNgComplex NgBeidouB1cPilotAt(const int8_t chips[NG_BEIDOU_B1C_LENGTH], uint64_t phase);

#endif
