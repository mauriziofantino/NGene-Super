/** @file acquisition.h @brief Incremental GPS L1 C/A acquisition boundary. */
#ifndef NGENE_ACQUISITION_H
#define NGENE_ACQUISITION_H
#include "ngene/types.h"

typedef enum { NG_ACQ_CONFIRM_M_OF_N, NG_ACQ_CONFIRM_NONE } TNgAcquisitionConfirmation;

/** GPS acquisition parameters, independent of engine scheduling and input chunks. */
typedef struct {
    int enabled;
    uint32_t gpsPrnMask;
    unsigned gpsChannels;
    size_t fftSize;
    double dopplerMaxHz;
    double dopplerStepHz;
    double codeStepChips;
    /** Nominal per-cell Pfa for the white-noise reference model. Repeated searches,
     * correlated code shifts or interference do not have an exact global Pfa. */
    double falseAlarmProbability;
    unsigned nonCoherentSums;
    TNgAcquisitionConfirmation confirmation;
    unsigned confirmationAttempts;
    unsigned confirmationSuccesses;
    double refinementDopplerSpanHz;
    double refinementDopplerStepHz;
    double refinementCodeSpanChips;
    unsigned refinementCodeStepSamples;
} TNgAcquisitionConfig;

typedef enum {
    NG_ACQ_IDLE,
    NG_ACQ_COARSE_NOISE,
    NG_ACQ_COARSE,
    NG_ACQ_DOPPLER_REFINEMENT,
    NG_ACQ_CODE_REFINEMENT,
    NG_ACQ_CONFIRMATION,
    NG_ACQ_ACQUIRED,
    NG_ACQ_NOT_FOUND
} TNgAcquisitionPhase;

/** Copied result; no borrowed input pointers or private DSP state escape.
 * Delays and phases refer to referenceSample in the specified continuity epoch.
 * codeDelaySamples is the nonnegative delay to the next code origin, modulo
 * the estimated code period (approximately Fs/1000 samples); codePhaseChips is
 * the code phase at referenceSample, in [0,1023), advancing at codeRateHz.
 * carrierHz = stream digital IF + physical satellite dopplerHz.
 * metric and threshold are powers in the same calibrated ADC units.
 */
typedef struct {
    unsigned prn;
    TNgAcquisitionPhase phase;
    uint64_t sequence;
    uint64_t epoch;
    uint64_t firstSample;
    uint64_t referenceSample;
    uint64_t processedSamples;
    double codeDelaySamples;
    double dopplerHz;
    double carrierHz;
    double codeRateHz;
    double codePhaseChips;
    double carrierPhaseRad;
    int carrierPhaseValid;
    double metric;
    double threshold;
    unsigned nonCoherentSums;
    unsigned confirmationTrials;
    unsigned confirmationHits;
    /** Configured policy; NONE means a refined coarse detection, without confirmation. */
    TNgAcquisitionConfirmation confirmation;
    size_t dopplerIndex;
    size_t dopplerCount;
    uint64_t resetCount;
} TNgAcquisitionResult;

/** Bounded display copy of actual coarse-search powers. Each published cell is
 * the maximum measured noncoherent power in its Doppler/delay tile; -1 means
 * that tile has not been measured. Doppler hypotheses are searched on successive
 * input frames, so a partial surface is not an instantaneous RF snapshot.
 * Axis entries give tile centers (not interpolated peak coordinates).
 */
#define NG_ACQ_CAF_MAX_BINS 64u
typedef struct {
    int available;
    unsigned prn;
    uint64_t sequence;
    uint64_t epoch;
    uint64_t referenceSample;
    unsigned nonCoherentSums;
    size_t fftSize;
    size_t codeBins;
    size_t dopplerBins;
    size_t completedDopplerBins;
    size_t totalDopplerBins;
    int complete;
    double maxPower;
    double codeDelaySamples[NG_ACQ_CAF_MAX_BINS];
    double dopplerHz[NG_ACQ_CAF_MAX_BINS];
    float power[NG_ACQ_CAF_MAX_BINS * NG_ACQ_CAF_MAX_BINS];
} TNgAcquisitionCaf;

typedef struct TNgAcquisitionPlan TNgAcquisitionPlan;
typedef struct TNgAcquisition TNgAcquisition;

void NgAcquisitionDefaults(TNgAcquisitionConfig *config);
int NgAcquisitionValidate(const TNgAcquisitionConfig *config, TNgError *error);
/** Build immutable PRN replicas and search grids before processing samples. */
int NgAcquisitionPlanCreate(const TNgAcquisitionConfig *config,
                            const TNgStreamInfo *streamInfo,
                            TNgAcquisitionPlan **plan, TNgError *error);
/** All contexts borrowing the plan must have been destroyed first. */
void NgAcquisitionPlanDestroy(TNgAcquisitionPlan *plan);
/** Each context has its own preallocated scratch/history and one assigned PRN.
 * Separate contexts may process concurrently using one immutable plan.
 */
int NgAcquisitionCreate(const TNgAcquisitionPlan *plan,
                        TNgAcquisition **context, TNgError *error);
void NgAcquisitionDestroy(TNgAcquisition *context);
/** Start/restart a search; current sums is selected by the receiver's adaptive
 * search policy, between one and config.nonCoherentSums inclusive.
 */
int NgAcquisitionAssign(TNgAcquisition *context, unsigned prn,
                        unsigned currentNonCoherentSums, TNgError *error);
/** Consume arbitrary contiguous input chunks. Each complete millisecond advances
 * NGene's coarse FFT, Doppler refinement, code refinement or M-of-N state.
 * Borrowed samples are copied only into a bounded, preallocated native frame;
 * no input pointer survives this call. NG_OK includes an unfinished result.
 * A gap/epoch change invalidates all earlier coherent history and estimates.
 * Acquisition removes each private native frame's I/Q means before mixing;
 * the frontend stream and its selected conditioner are not modified.
 */
int NgAcquisitionProcess(TNgAcquisition *context, const TNgIqView *input,
                         TNgAcquisitionResult *result, TNgError *error);
void NgAcquisitionSnapshot(const TNgAcquisition *context,
                           TNgAcquisitionResult *result);
/** Owner-thread copy; call only when this context is not concurrently processing. */
void NgAcquisitionCafSnapshot(const TNgAcquisition *context, TNgAcquisitionCaf *caf);
/** Owner-thread display switch. Disabling skips CAF max pooling without changing
 * coarse correlation or detector decisions. */
void NgAcquisitionSetCafEnabled(TNgAcquisition *context, int enabled);
/** Cheap change indicator for copying a display CAF after a completed coarse dwell. */
uint64_t NgAcquisitionCafSequence(const TNgAcquisition *context);
/** Clear a search and its partial frame while preserving the assigned PRN. */
void NgAcquisitionReset(TNgAcquisition *context);
const char *NgAcquisitionPhaseName(TNgAcquisitionPhase phase);

#endif
