/** @file egnos_code.h @brief Internal GEO L1 C/A replicas. */
#ifndef NG_EGNOS_CODE_H
#define NG_EGNOS_CODE_H
#include "ngene/dsp/gps_ca.h"

/** Generate the primary C/A code for slot zero (121) or one (136). */
void NgEgnosCode(unsigned slot, int8_t chips[NG_GPS_CA_LENGTH]);
#endif
