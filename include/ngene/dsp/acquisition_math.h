/** @file acquisition_math.h @brief Cached legacy GPS compression/FFT acquisition mathematics. */
#ifndef NGENE_DSP_ACQUISITION_MATH_H
#define NGENE_DSP_ACQUISITION_MATH_H
#include "ngene/types.h"

/** Finite-reference nominal per-cell threshold: Gamma(M) cell power > factor times
 * the mean of K independent references, each averaged over M frames. Valid for
 * white Gaussian noise; correlated code delays/interference do not guarantee global Pfa.
 * Bounded setup-time calculation, M=1..1024, K=1..16384, Pfa in [1e-15,1). */
int NgAcqMathThreshold(unsigned sums, size_t references, double pfa, double *factor, TNgError *error);
#define NG_ACQ_NOISE_REFERENCES 32u
typedef struct TNgAcqMathPlan TNgAcqMathPlan;
typedef struct TNgAcqMathWorkspace TNgAcqMathWorkspace;
/** Direct coherent correlation; mean I,Q are native ADC units, power = I^2+Q^2.
 * inputPower is the mean unrotated I^2+Q^2, and count is the integrated sample count. */
typedef struct { double i; double q; double power; double inputPower; size_t count; }
    TNgAcqComplexSum;
/** Create shared immutable carrier table, FFT plan, short/long compression boundaries and
 * PRN spectra. fs is the native complex sample rate. fftSize is a power of two in 8..16384
 * and must not exceed floor(fs/1000). Both floor/ceil one-ms input lengths are cached.
 * prnMask bit (prn-1) selects PRNs. No process-global cache or hidden ownership. */
int NgAcqMathCreate(uint32_t sampleRateHz, size_t fftSize, uint32_t prnMask,
                     TNgAcqMathPlan **out, TNgError *error);
void NgAcqMathDestroy(TNgAcqMathPlan *plan);
/** Create per-consumer scratch. Plan must outlive its workspaces; all execution allocates zero. */
int NgAcqMathWorkspaceCreate(const TNgAcqMathPlan *plan, TNgAcqMathWorkspace **out,
                              TNgError *error);
void NgAcqMathWorkspaceDestroy(TNgAcqMathWorkspace *workspace);
/** Mix native complex samples, sum successive short/long runs directly into fftSize bins,
 * and perform circular matched correlation using the existing normalized integer FFT.
 * view.count must be floor/ceil(fs/1000). Template chip zero is at this view's first sample.
 * Each power[k] estimates squared mean complex ADC amplitude for code lag
 * k * view.count / fftSize samples: signal[n] = code[n-lag]. No zero padding is performed.
 * Scaling is block floating for data, cached template and spectral product separately;
 * the two 1/N transforms are explicitly undone before template-energy normalization.
 * initialCarrierCycles is the physical carrier phase at the first input sample.
 * Plan is immutable, workspace and output are private/disjoint per invocation. */
int NgAcqMathCorrelate(const TNgAcqMathPlan *plan, TNgAcqMathWorkspace *workspace,
                        unsigned prn, const TNgIqView *view, double carrierHz,
                        double initialCarrierCycles, double *power, TNgError *error);
/** As above with a circular input-origin shift for the original NGene fractional
 * compression grids. Carrier phase follows the original physical input indices.
 * Returned lag in the original frame is (k*count/fftSize + sampleOffset) modulo count. */
int NgAcqMathCorrelateOffset(const TNgAcqMathPlan *plan, TNgAcqMathWorkspace *workspace,
                              unsigned prn, const TNgIqView *view, double carrierHz,
                              double initialCarrierCycles, size_t sampleOffset,
                              double *power, TNgError *error);
/** Same circular correlation and delay convention as CorrelateOffset, with local
 * full-frame I/Q component means removed before carrier mixing. Mean removal uses
 * int64 component sums and double residuals, retaining sub-ADC differences.
 * Frontend samples, conditioning configuration and cached code spectra are unchanged. */
int NgAcqMathCorrelateCentered(const TNgAcqMathPlan *plan, TNgAcqMathWorkspace *workspace,
                                unsigned prn, const TNgIqView *view, double carrierHz,
                                double initialCarrierCycles, size_t sampleOffset,
                                double *power, TNgError *error);
/** Full-native-rate prompt correlation, for legacy Doppler/code refinement and confirmation.
 * codePhaseChips is the local replica phase at the first sample (negative delay advances
 * toward a received delayed code); positive carrierHz is removed with exp(-j*phase).
 * Native ADC precision is retained. No allocation or trig occurs on this path. */
int NgAcqMathPrompt(const TNgAcqMathPlan *plan, unsigned prn, const TNgIqView *view,
                     double carrierHz, double initialCarrierCycles, double codePhaseChips,
                     double codeRateHz, TNgAcqComplexSum *result, TNgError *error);
/** Native-rate prompt after local full-frame component-mean removal before mixing.
 * result->inputPower is the mean residual I^2+Q^2. The uncentered Prompt API
 * retains its original semantics and exact native ADC values. */
int NgAcqMathPromptCentered(const TNgAcqMathPlan *plan, unsigned prn,
                             const TNgIqView *view, double carrierHz,
                             double initialCarrierCycles, double codePhaseChips,
                             double codeRateHz, TNgAcqComplexSum *result, TNgError *error);
/** Locally centered prompt plus 32 same-carrier references outside the candidate peak.
 * Full-frame component means are subtracted before mixing, preserving fractional
 * residuals. result->inputPower is the mean residual I^2+Q^2.
 * Reference j advances the local code by exactly 16+31*j chips, j=0..31.
 * All references share one carrier rotation and the prompt's code-phase NCO;
 * no samples, carrier replicas or code replicas are allocated per call.
 * noisePower receives the mean squared complex ADC amplitude of the 32
 * reference correlations, in the same calibrated units as result->power.
 * This is a reference estimate for nominal white-noise CFAR. Correlated,
 * colored or interfered inputs do not imply an exact false-alarm probability.
 * The existing NgAcqMathPrompt path computes no reference correlations. */
int NgAcqMathPromptWithNoise(const TNgAcqMathPlan *plan, unsigned prn,
                              const TNgIqView *view, double carrierHz,
                              double initialCarrierCycles, double codePhaseChips,
                              double codeRateHz, TNgAcqComplexSum *result,
                              double *noisePower, TNgError *error);
#endif
