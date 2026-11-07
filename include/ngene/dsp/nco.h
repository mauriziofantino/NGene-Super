/** @file nco.h @brief Persistent carrier NCO with interpolated Q30 lookup table. */
#ifndef NGENE_DSP_NCO_H
#define NGENE_DSP_NCO_H
#include "ngene/types.h"
#define NG_NCO_TABLE_SIZE 4096u

/** Immutable after initialization; may be shared between independent NCO states. */
typedef struct {
    int32_t cosine[NG_NCO_TABLE_SIZE + 1u];
    int32_t sine[NG_NCO_TABLE_SIZE + 1u];
} TNgNcoTable;

/** Unsigned modulo-one-cycle Q64 phase and signed-frequency modulo increment. */
typedef struct {
    uint64_t phase;
    uint64_t step;
} TNgCarrierNco;

/** Cold initialization only; this operation evaluates sine and cosine. */
void NgNcoTableInit(TNgNcoTable *table);
/** Configure frequency (digital IF + Doppler), and phase in cycles at the first sample. */
int NgNcoInit(TNgCarrierNco *nco, double sampleRateHz, double frequencyHz, double phaseCycles,
              TNgError *error);
/** Rotate I+jQ by exp(-j*phase), then advance. All products widen before arithmetic.
 * Output is in native ADC units; SC16 values are never narrowed to CI8. */
void NgNcoRotate(const TNgNcoTable *table, TNgCarrierNco *nco, int i, int q, double *outI,
                 double *outQ);
/** Rotate fractional native ADC components without narrowing. Used after local
 * block-mean removal; shares exactly the integer path's phase table and advance. */
void NgNcoRotateF64(const TNgNcoTable *table, TNgCarrierNco *nco, double i, double q, double *outI,
                    double *outQ);
#endif
