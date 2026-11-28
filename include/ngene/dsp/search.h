/** @file search.h @brief Reusable acquisition FFT, replicas and complex correlations. */
#ifndef NGENE_DSP_SEARCH_H
#define NGENE_DSP_SEARCH_H
#include "ngene/types.h"

/** Interleaved single-precision complex sample in native ADC units. */
typedef struct {
    float i;
    float q;
} TNgComplex;
/** Owned immutable FFT tables, shareable by independent workspaces. */
typedef struct TNgSearchFft TNgSearchFft;

/** FFT arithmetic: float SIMD, unscaled int32 butterflies, or Q14 stage-rescaled SIMD. */
typedef enum {
    NG_SEARCH_FFT_FLOAT,
    NG_SEARCH_FFT_INTEGER_PRECISE,
    NG_SEARCH_FFT_INTEGER_SCALED
} TNgSearchFftMode;

/** Return whether this size is supported without allocating an FFT plan. */
int NgSearchFftSupportsSize(size_t size);
/** Allocate radix-2^2 FFT tables. size is a power of two in 8..16384.
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
/** In-place transform with explicit arithmetic and float-domain output scale.
 * Integer workspaces belong to plan; calls on one plan must be serialized. */
void NgSearchFftExecuteMode(TNgSearchFft *plan, TNgComplex *values, int inverse,
                            TNgSearchFftMode mode);
/** Native signed-int32 FFT. Input/output contain interleaved integer I/Q, each input
 * component must be within +/-16384. Forward output is unscaled; inverse is /N.
 * No floating sample conversion occurs in this API. */
int NgSearchFftExecuteInt32(TNgSearchFft *plan, const int32_t *input, int32_t *output, int inverse,
                            TNgError *error);
/** Prepare an integer code spectrum for the next PCPS calls on this worker-owned plan. */
int NgSearchIntegerCode(TNgSearchFft *plan, const float *code, TNgSearchFftMode mode,
                        TNgError *error);
/** Integer carrier rotation, FFT, spectral product and inverse FFT. The supplied
 * baseband is signed-int16 I/Q, scaled by baseScale relative to native ADC units.
 * The returned power is restored to native float-PCPS units. */
int NgSearchIntegerPcps(TNgSearchFft *plan, const int16_t *baseband, const TNgComplex *carrier,
                        double baseScale, TNgSearchFftMode mode, double *power, TNgError *error);
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
void NgSearchCorrelate(const TNgComplex *values, const float *code, size_t count, int xorSigns,
                       double *i, double *q);
/** Interleaved (I, Q) running double sums of count values; prefix has 2 * (count + 1)
 * entries and prefix[0..1] = 0. */
void NgSearchPrefix(double *prefix, const TNgComplex *values, size_t count);
/** One pass equal to NgSearchCarrier(phaseCycles, square), CI8 to float with the Q sign
 * inverted when requested, NgSearchMix and NgSearchPrefix. Mixed samples are identical to
 * the separate kernels; only the cumulative sums are stored. */
void NgSearchMixPrefixCi8(double *prefix, const TNgIq8 *input, size_t count, double rateHz,
                          double frequencyHz, double phaseCycles, int square, int inverted);
/** Throughput variant of NgSearchMixPrefixCi8: four independent double precision carrier
 * lanes started from exact phases, sums formed four samples at a time. No tables or phase
 * quantization; results differ from the one-lane kernel only by double rounding. */
void NgSearchMixPrefixCi8Lanes(double *prefix, const TNgIq8 *input, size_t count, double rateHz,
                               double frequencyHz, double phaseCycles, int square, int inverted);
/** Experimental byte mixer. Carrier components use signed four-bit levels -7..7;
 * products widen before rounding to signed-byte output. Prefix is restored to native
 * ADC units, with double accumulation. Input and prefix are disjoint, caller owned.
 * No allocation, DC subtraction, or change of NCO phase convention. */
void NgSearchMixPrefixCi8Quantized(double *prefix, const TNgIq8 *input, size_t count, double rateHz,
                                   double frequencyHz, double phaseCycles, int inverted);
/** Byte-carrier modes: 4 input shift, 8 rounded byte output, 16 widened exact product.
 * Integer products are accumulated before conversion back to native ADC units. */
void NgSearchMixPrefixCi8Arithmetic(double *prefix, const TNgIq8 *input, size_t count,
                                    double rateHz, double frequencyHz, double phaseCycles,
                                    int inverted, unsigned mixerBits);
#endif
