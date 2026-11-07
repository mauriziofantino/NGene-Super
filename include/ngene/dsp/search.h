/** @file search.h @brief Reusable acquisition FFT, replicas and complex correlations. */
#ifndef NGENE_DSP_SEARCH_H
#define NGENE_DSP_SEARCH_H
#include "ngene/types.h"
/** Interleaved single-precision complex sample in native ADC units. */
typedef struct { float i; float q; } TNgComplex;
/** Owned immutable FFT tables, shareable by independent workspaces. */
typedef struct TNgSearchFft TNgSearchFft;
/** Allocate radix-2 FFT tables. size is a power of two in 8..16384.
 * @param size Complex transform length.
 * @param out Owned plan, NULL on failure.
 * @param error Optional diagnostic.
 * @return NG_OK or NG_ERROR. */
int NgSearchFftCreate(size_t size, TNgSearchFft **out, TNgError *error);
/** Release plan after all executions complete; accepts NULL. */
void NgSearchFftDestroy(TNgSearchFft *plan);
/** In-place transform, no allocation. Forward is unscaled exp(-j*w); inverse includes 1/N.
 * Workspace has plan size complex elements and is exclusive to the caller. */
void NgSearchFftExecute(const TNgSearchFft *plan, TNgComplex *values, int inverse);
/** Generate exp(-j*phase) carrier using a recurrent oscillator, no per-sample trig.
 * phaseCycles references sample zero. square selects +/-1 cosine and sine.
 * output contains count caller-owned samples; rate/frequency must be finite and rate positive. */
void NgSearchCarrier(TNgComplex *output, size_t count, double rateHz, double frequencyHz,
                     double phaseCycles, int square);
/** Generate sampled +/-1 code with a fixed-point phase accumulator and SIMD stores.
 * phaseChips references sample zero; chips contains 1023 +/-1 entries. */
void NgSearchCode(float *output, size_t count, const int8_t *chips, double rateHz,
                  double codeRateHz, double phaseChips);
/** In-place complex product with carrier; uses architecture SIMD when available. */
void NgSearchMix(TNgComplex *values, const TNgComplex *carrier, size_t count);
/** Complex prompt correlation. xorSigns selects exact IEEE sign-bit XOR with +/-1 code;
 * zero selects SIMD multiplication. Both preserve input amplitude. No sign quantization.
 * Returns double accumulators to preserve coherent-sum precision. */
void NgSearchCorrelate(const TNgComplex *values, const float *code, size_t count,
                       int xorSigns, double *i, double *q);
#endif
