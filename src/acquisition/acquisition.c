/** @file acquisition.c @brief NGene GPS acquisition adapted to native complex IQ. */
#include "ngene/acquisition.h"
#include "ngene/dsp/acquisition_math.h"
#include "ngene/dsp/gps_ca.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ACQ_L1_HZ 1575420000.0
#define ACQ_NOISE_DOPPLER_HZ 30000.0
#define ACQ_MAX_GRID 65536u

struct TNgAcquisitionPlan {
    TNgAcquisitionConfig config;
    TNgStreamInfo stream;
    TNgAcqMathPlan *math;
    size_t fftSize;
    size_t frameCapacity;
    size_t sampleBytes;
    size_t dopplerCount;
    size_t delayCount;
    size_t delayStep;
    size_t noiseGuardBins;
    size_t noiseReferenceCount;
    size_t dopplerRefinementCount;
    size_t codeRefinementCount;
    double codeRefinementSpanSamples;
    /** Immutable native-delay to display-cell map for floor/ceil frames. */
    uint8_t *cafColumns[2];
};

struct TNgAcquisition {
    const TNgAcquisitionPlan *plan;
    TNgAcqMathWorkspace *workspace;
    unsigned char *frame;
    double *power;
    double *accumulatedPower;
    TNgAcquisitionResult result;
    TNgAcquisitionCaf caf;
    size_t frameCount;
    size_t frameTarget;
    size_t skipSamples;
    unsigned frameRemainder;
    uint64_t frameFirst;
    uint64_t coarseStartSample;
    uint64_t expectedSample;
    int continuityValid;
    unsigned coarseSums;
    unsigned sumsDone;
    size_t delayIndex;
    size_t refinementIndex;
    double coarseMaximum;
    double coarseScore;
    double coarseThreshold;
    double refinementMaximum;
    double refinementPower;
    double refinementNoise;
    double coarseDoppler;
    double bestRefinementOffset;
    uint64_t originReference;
    double originOffset;
    double codeRateHz;
    double thresholdFactor;
    double confirmationThresholdFactor;
    unsigned thresholdSums;
};

static double Wrap(double value, double period)
{
    double result = fmod(value, period);
    if (result < 0.0) result += period;
    /* Adding a sub-ULP negative remainder to period can round to period. */
    return result >= period ? 0.0 : result;
}

static void ClearCaf(TNgAcquisition *context)
{
    const TNgAcquisitionPlan *plan = context->plan;
    TNgAcquisitionCaf *caf = &context->caf;
    size_t row, column;
    memset(caf, 0, sizeof(*caf));
    caf->prn = context->result.prn;
    caf->fftSize = plan->fftSize;
    caf->nonCoherentSums = context->coarseSums;
    caf->codeBins = plan->fftSize < NG_ACQ_CAF_MAX_BINS ? plan->fftSize : NG_ACQ_CAF_MAX_BINS;
    caf->dopplerBins = plan->dopplerCount < NG_ACQ_CAF_MAX_BINS ?
        plan->dopplerCount : NG_ACQ_CAF_MAX_BINS;
    caf->totalDopplerBins = plan->dopplerCount;
    for (column = 0; column < caf->codeBins; ++column)
        caf->codeDelaySamples[column] = ((double)column + 0.5) *
            (double)plan->stream.sampleRateHz / (1000.0 * (double)caf->codeBins);
    for (row = 0; row < caf->dopplerBins; ++row) {
        size_t lo = (row * plan->dopplerCount + caf->dopplerBins - 1u) / caf->dopplerBins;
        size_t hi = ((row + 1u) * plan->dopplerCount + caf->dopplerBins - 1u) /
            caf->dopplerBins - 1u;
        caf->dopplerHz[row] = -plan->config.dopplerMaxHz +
            ((double)lo + (double)hi) * 0.5 * plan->config.dopplerStepHz;
    }
    for (column = 0; column < NG_ACQ_CAF_MAX_BINS * NG_ACQ_CAF_MAX_BINS; ++column)
        caf->power[column] = -1.0f;
}

static int FinitePositive(double value)
{
    return isfinite(value) && value > 0.0;
}

static size_t GridCount(double span, double step)
{
    return (size_t) floor((2.0 * span / step) + 0.5) + 1u;
}

void NgAcquisitionDefaults(TNgAcquisitionConfig *config)
{
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->gpsPrnMask = UINT32_MAX;
    config->gpsChannels = 4;
    config->fftSize = 4096;
    config->dopplerMaxHz = 5000.0;
    config->dopplerStepHz = 667.0;
    config->codeStepChips = 0.25;
    config->falseAlarmProbability = 1e-9;
    config->nonCoherentSums = 8;
    config->confirmationAttempts = 4;
    config->confirmationSuccesses = 3;
    config->refinementDopplerSpanHz = 667.0;
    config->refinementDopplerStepHz = 10.0;
    config->refinementCodeSpanChips = 1.0;
    config->refinementCodeStepSamples = 1;
}

int NgAcquisitionValidate(const TNgAcquisitionConfig *config, TNgError *error)
{
    if (!config) return NgFail(error, "Missing acquisition configuration");
    if ((config->enabled != 0 && config->enabled != 1) ||
        config->gpsChannels < 1 || config->gpsChannels > 32 ||
        (config->enabled && !config->gpsPrnMask))
        return NgFail(error, "Acquisition requires 1..32 GPS channels and a nonempty PRN mask");
    if (config->fftSize < 8 || config->fftSize > 16384 ||
        (config->fftSize & (config->fftSize - 1u)))
        return NgFail(error, "Acquisition FFT size must be a power of two in 8..16384");
    if (!isfinite(config->dopplerMaxHz) || config->dopplerMaxHz < 0.0 ||
        config->dopplerMaxHz > 100000.0 || !FinitePositive(config->dopplerStepHz) ||
        (2.0 * config->dopplerMaxHz / config->dopplerStepHz) >= ACQ_MAX_GRID)
        return NgFail(error, "Invalid or excessive GPS Doppler search grid");
    if (!FinitePositive(config->codeStepChips) || config->codeStepChips > 1023.0 ||
        1023.0 / config->codeStepChips >= ACQ_MAX_GRID)
        return NgFail(error, "Invalid GPS coarse code-delay step");
    if (!FinitePositive(config->falseAlarmProbability) || config->falseAlarmProbability >= 1.0 ||
        config->falseAlarmProbability < 1e-15)
        return NgFail(error, "Acquisition false-alarm probability must be in [1e-15,1)");
    if (config->nonCoherentSums < 1 || config->nonCoherentSums > 1024)
        return NgFail(error, "Acquisition noncoherent sums must be in 1..1024");
    if (config->confirmation != NG_ACQ_CONFIRM_M_OF_N && config->confirmation != NG_ACQ_CONFIRM_NONE)
        return NgFail(error, "Unsupported acquisition confirmation method");
    if (config->confirmationAttempts < 1 || config->confirmationAttempts > 1024 ||
        config->confirmationSuccesses < 1 ||
        config->confirmationSuccesses > 1024 ||
        (config->confirmation == NG_ACQ_CONFIRM_M_OF_N &&
         config->confirmationSuccesses > config->confirmationAttempts))
        return NgFail(error, "Confirmation requires 1 <= M <= N <= 1024");
    if (!isfinite(config->refinementDopplerSpanHz) || config->refinementDopplerSpanHz < 0.0 ||
        config->refinementDopplerSpanHz > 100000.0 ||
        !FinitePositive(config->refinementDopplerStepHz) ||
        (2.0 * config->refinementDopplerSpanHz / config->refinementDopplerStepHz) >= ACQ_MAX_GRID)
        return NgFail(error, "Invalid or excessive Doppler refinement grid");
    if (!isfinite(config->refinementCodeSpanChips) || config->refinementCodeSpanChips < 0.0 ||
        config->refinementCodeSpanChips > 16.0 || config->refinementCodeStepSamples < 1)
        return NgFail(error, "Invalid GPS native-sample code refinement grid");
    return NG_OK;
}


int NgAcquisitionPlanCreate(const TNgAcquisitionConfig *config,
                            const TNgStreamInfo *streamInfo,
                            TNgAcquisitionPlan **out, TNgError *error)
{
    TNgAcquisitionPlan *plan;
    size_t minimumFrame;
    double ratio;
    if (!out) return NgFail(error, "Missing acquisition plan destination");
    *out = NULL;
    if (NgAcquisitionValidate(config, error) != NG_OK) return NG_ERROR;
    if (!streamInfo || streamInfo->sampleRateHz < 8000 || !NgSampleBytes(streamInfo->format))
        return NgFail(error, "Acquisition requires supported native complex IQ and Fs >= 8000 Hz");
    minimumFrame = streamInfo->sampleRateHz / 1000u;
    plan = calloc(1, sizeof(*plan));
    if (!plan) return NgFail(error, "Cannot allocate acquisition plan");
    plan->config = *config;
    plan->stream = *streamInfo;
    /* Identity strings are never needed while processing; do not retain borrowed text. */
    plan->stream.sourceName = NULL;
    plan->stream.clockDomain = NULL;
    plan->stream.deviceIdentifier = NULL;
    plan->fftSize = config->fftSize;
    while (plan->fftSize > minimumFrame) plan->fftSize >>= 1u;
    /* Keep the main lobe and its fractional-chip shoulders out of the local
     * background estimate; distances are circular on the FFT delay grid. */
    plan->noiseGuardBins = (2u * plan->fftSize + 1022u) / 1023u;
    plan->noiseReferenceCount = plan->fftSize - 2u * plan->noiseGuardBins - 1u;
    plan->frameCapacity = minimumFrame + (streamInfo->sampleRateHz % 1000u != 0u);
    plan->sampleBytes = NgSampleBytes(streamInfo->format);
    plan->dopplerCount = GridCount(config->dopplerMaxHz, config->dopplerStepHz);
    ratio = 1023.0 / (double) plan->fftSize;
    /* Keep NGene's shifted compression grids. Round their COUNT upward: the
     * legacy floor silently made a requested half-chip step a full chip when
     * N=1024 (1023/1024/.5 is just below two). With rational sample rates that
     * loss can bias a sequential Doppler search as sample/chip phase changes.
     * The standard N=4096, .25-chip configuration still has one grid. */
    plan->delayCount = config->codeStepChips > ratio ? 1u :
        (size_t) ceil(ratio / config->codeStepChips);
    if (!plan->delayCount) plan->delayCount = 1;
    plan->delayStep = (size_t) floor((double) streamInfo->sampleRateHz *
        config->codeStepChips / NG_GPS_CA_RATE_HZ + 0.5);
    if (!plan->delayStep) plan->delayCount = 1;
    {
        unsigned variant, variants = plan->frameCapacity == minimumFrame ? 1u : 2u;
        size_t codeBins = plan->fftSize < NG_ACQ_CAF_MAX_BINS ?
            plan->fftSize : NG_ACQ_CAF_MAX_BINS;
        if (plan->delayCount > SIZE_MAX / plan->fftSize) {
            NgAcquisitionPlanDestroy(plan);
            return NgFail(error, "Acquisition CAF delay map exceeds addressable memory");
        }
        for (variant = 0; variant < variants; ++variant) {
            size_t frameCount = minimumFrame + variant;
            uint64_t periodUnits = (uint64_t)frameCount * (uint64_t)plan->fftSize;
            size_t delay, index;
            plan->cafColumns[variant] = malloc(plan->delayCount * plan->fftSize);
            if (plan->cafColumns[variant] == NULL) {
                NgAcquisitionPlanDestroy(plan);
                return NgFail(error, "Cannot allocate bounded acquisition CAF delay map");
            }
            for (delay = 0; delay < plan->delayCount; ++delay) {
                uint64_t shiftUnits = ((uint64_t)delay * (uint64_t)plan->delayStep %
                    (uint64_t)frameCount) * (uint64_t)plan->fftSize;
                for (index = 0; index < plan->fftSize; ++index) {
                    uint64_t units = (uint64_t)index * (uint64_t)frameCount + shiftUnits;
                    if (units >= periodUnits) units -= periodUnits;
                    plan->cafColumns[variant][delay * plan->fftSize + index] =
                        (uint8_t)(units * (uint64_t)codeBins / periodUnits);
                }
            }
        }
    }
    plan->dopplerRefinementCount = GridCount(config->refinementDopplerSpanHz,
                                            config->refinementDopplerStepHz);
    plan->codeRefinementSpanSamples = floor(config->refinementCodeSpanChips *
        (double) streamInfo->sampleRateHz / NG_GPS_CA_RATE_HZ + 0.5);
    ratio = 2.0 * plan->codeRefinementSpanSamples / (double) config->refinementCodeStepSamples;
    if (ratio >= ACQ_MAX_GRID) {
        NgAcquisitionPlanDestroy(plan);
        return NgFail(error, "Code refinement exceeds the bounded search grid");
    }
    plan->codeRefinementCount = (size_t) floor(ratio + 0.5) + 1u;
    if (NgAcqMathCreate(streamInfo->sampleRateHz, plan->fftSize, config->gpsPrnMask,
                         &plan->math, error) != NG_OK) {
        NgAcquisitionPlanDestroy(plan);
        return NG_ERROR;
    }
    *out = plan;
    return NG_OK;
}

void NgAcquisitionPlanDestroy(TNgAcquisitionPlan *plan)
{
    if (!plan) return;
    NgAcqMathDestroy(plan->math);
    free(plan->cafColumns[0]);
    free(plan->cafColumns[1]);
    free(plan);
}

int NgAcquisitionCreate(const TNgAcquisitionPlan *plan,
                        TNgAcquisition **out, TNgError *error)
{
    TNgAcquisition *context;
    if (!out) return NgFail(error, "Missing acquisition context destination");
    *out = NULL;
    if (!plan) return NgFail(error, "Missing acquisition plan");
    context = calloc(1, sizeof(*context));
    if (!context) return NgFail(error, "Cannot allocate acquisition context");
    context->plan = plan;
    context->frame = malloc(plan->frameCapacity * plan->sampleBytes);
    context->power = calloc(plan->fftSize, sizeof(double));
    context->accumulatedPower = calloc(plan->fftSize, sizeof(double));
    if (!context->frame || !context->power || !context->accumulatedPower ||
        NgAcqMathWorkspaceCreate(plan->math, &context->workspace, error) != NG_OK) {
        NgAcquisitionDestroy(context);
        return NgFail(error, "Cannot allocate acquisition workspace");
    }
    context->result.phase = NG_ACQ_IDLE;
    context->codeRateHz = NG_GPS_CA_RATE_HZ;
    *out = context;
    return NG_OK;
}

void NgAcquisitionDestroy(TNgAcquisition *context)
{
    if (!context) return;
    NgAcqMathWorkspaceDestroy(context->workspace);
    free(context->frame);
    free(context->power);
    free(context->accumulatedPower);
    free(context);
}

static void ClearSearch(TNgAcquisition *context, int countReset)
{
    unsigned prn = context->result.prn;
    uint64_t sequence = context->result.sequence;
    uint64_t resets = context->result.resetCount;
    memset(&context->result, 0, sizeof(context->result));
    context->result.prn = prn;
    context->result.phase = prn ? NG_ACQ_COARSE_NOISE : NG_ACQ_IDLE;
    context->result.sequence = sequence + (countReset != 0);
    context->result.resetCount = resets + (countReset != 0);
    context->result.nonCoherentSums = context->coarseSums;
    context->result.confirmation = context->plan->config.confirmation;
    context->result.dopplerCount = context->plan->dopplerCount;
    context->result.carrierHz = (double) context->plan->stream.digitalIfHz;
    context->result.codeRateHz = NG_GPS_CA_RATE_HZ;
    context->frameCount = 0;
    context->frameTarget = 0;
    context->skipSamples = 0;
    context->frameRemainder = 0;
    context->coarseStartSample = 0;
    context->continuityValid = 0;
    context->sumsDone = 0;
    context->delayIndex = 0;
    context->refinementIndex = 0;
    context->coarseMaximum = -1.0;
    context->coarseScore = -1.0;
    context->coarseThreshold = 0.0;
    context->refinementMaximum = -1.0;
    context->refinementPower = 0.0;
    context->refinementNoise = 0.0;
    context->coarseDoppler = 0.0;
    context->bestRefinementOffset = 0.0;
    context->originReference = 0;
    context->originOffset = 0.0;
    context->codeRateHz = NG_GPS_CA_RATE_HZ;
    memset(context->accumulatedPower, 0, context->plan->fftSize * sizeof(double));
    ClearCaf(context);
}

int NgAcquisitionAssign(TNgAcquisition *context, unsigned prn,
                        unsigned currentNonCoherentSums, TNgError *error)
{
    if (!context || prn < 1 || prn > 32 ||
        !(context->plan->config.gpsPrnMask & (UINT32_C(1) << (prn - 1u))))
        return NgFail(error, "Assigned GPS PRN is not present in the acquisition plan");
    if (!currentNonCoherentSums || currentNonCoherentSums > context->plan->config.nonCoherentSums)
        return NgFail(error, "Assigned noncoherent sums exceed the acquisition policy");
    context->result.prn = prn;
    context->coarseSums = currentNonCoherentSums;
    if (context->thresholdSums != currentNonCoherentSums) {
        if (NgAcqMathThreshold(currentNonCoherentSums, context->plan->noiseReferenceCount,
                context->plan->config.falseAlarmProbability, &context->thresholdFactor, error) != NG_OK) return NG_ERROR;
        context->confirmationThresholdFactor = 0.0;
        if (context->plan->config.confirmation != NG_ACQ_CONFIRM_NONE &&
            NgAcqMathThreshold(currentNonCoherentSums, NG_ACQ_NOISE_REFERENCES,
                context->plan->config.falseAlarmProbability, &context->confirmationThresholdFactor, error) != NG_OK) return NG_ERROR;
        context->thresholdSums = currentNonCoherentSums;
    }
    ClearSearch(context, 0);
    return NG_OK;
}

void NgAcquisitionReset(TNgAcquisition *context)
{
    if (context) ClearSearch(context, 1);
}

void NgAcquisitionSnapshot(const TNgAcquisition *context, TNgAcquisitionResult *result)
{
    if (context && result) *result = context->result;
}

void NgAcquisitionCafSnapshot(const TNgAcquisition *context, TNgAcquisitionCaf *caf)
{
    if (context && caf) *caf = context->caf;
}

uint64_t NgAcquisitionCafSequence(const TNgAcquisition *context)
{
    return context ? context->caf.sequence : 0;
}

const char *NgAcquisitionPhaseName(TNgAcquisitionPhase phase)
{
    switch (phase) {
    case NG_ACQ_IDLE: return "idle";
    case NG_ACQ_COARSE_NOISE: return "coarse_noise";
    case NG_ACQ_COARSE: return "coarse";
    case NG_ACQ_DOPPLER_REFINEMENT: return "doppler_refinement";
    case NG_ACQ_CODE_REFINEMENT: return "code_refinement";
    case NG_ACQ_CONFIRMATION: return "confirmation";
    case NG_ACQ_ACQUIRED: return "acquired";
    case NG_ACQ_NOT_FOUND: return "not_found";
    default: return "invalid";
    }
}

static double CodePhase(const TNgAcquisition *context, uint64_t sample)
{
    double elapsed;
    if (sample >= context->originReference)
        elapsed = (double) (sample - context->originReference);
    else elapsed = -(double) (context->originReference - sample);
    return Wrap((elapsed - context->originOffset) * context->codeRateHz /
        (double) context->plan->stream.sampleRateHz, 1023.0);
}

static void UpdateReference(TNgAcquisition *context, uint64_t sample)
{
    TNgAcquisitionResult *result = &context->result;
    result->referenceSample = sample;
    result->codeRateHz = context->codeRateHz;
    if (context->coarseMaximum >= 0.0) {
        result->codePhaseChips = CodePhase(context, sample);
        result->codeDelaySamples = Wrap(-result->codePhaseChips, 1023.0) *
            (double) context->plan->stream.sampleRateHz / context->codeRateHz;
    }
}

static void StartRefinement(TNgAcquisition *context, TNgAcquisitionPhase phase)
{
    context->result.phase = phase;
    context->result.nonCoherentSums = context->plan->config.nonCoherentSums;
    context->sumsDone = 0;
    context->refinementIndex = 0;
    context->refinementMaximum = -1.0;
    context->refinementPower = 0.0;
    context->refinementNoise = 0.0;
    context->bestRefinementOffset = 0.0;
}

static int CoarseFrame(TNgAcquisition *context, const TNgIqView *view, TNgError *error)
{
    const TNgAcquisitionPlan *plan = context->plan;
    TNgAcquisitionResult *result = &context->result;
    size_t index;
    double doppler;
    if (result->phase == NG_ACQ_COARSE_NOISE) {
        double mean = 0.0;
        if (NgAcqMathCorrelateCentered(plan->math, context->workspace, result->prn, view,
                NgCarrierFromDoppler(&plan->stream, ACQ_NOISE_DOPPLER_HZ), 0.0,
                0, context->power, error) != NG_OK) return NG_ERROR;
        for (index = 0; index < plan->fftSize; ++index) mean += context->power[index];
        result->threshold = mean / (double) plan->fftSize * context->thresholdFactor;
        result->phase = NG_ACQ_COARSE;
        return NG_OK;
    }
    doppler = -plan->config.dopplerMaxHz + (double) result->dopplerIndex * plan->config.dopplerStepHz;
    if (context->sumsDone == 0) context->coarseStartSample = view->meta.firstSample;
    if (NgAcqMathCorrelateCentered(plan->math, context->workspace, result->prn, view,
            NgCarrierFromDoppler(&plan->stream, doppler), 0.0,
            context->delayIndex * plan->delayStep, context->power, error) != NG_OK)
        return NG_ERROR;
    for (index = 0; index < plan->fftSize; ++index)
        context->accumulatedPower[index] += context->power[index];
    ++context->sumsDone;
    if (context->sumsDone < context->coarseSums) return NG_OK;
    {
        TNgAcquisitionCaf *caf = &context->caf;
        size_t row = result->dopplerIndex * caf->dopplerBins / plan->dopplerCount;
        unsigned variant = view->meta.count == plan->frameCapacity &&
            plan->frameCapacity != plan->stream.sampleRateHz / 1000u ? 1u : 0u;
        const uint8_t *columns = plan->cafColumns[variant] + context->delayIndex * plan->fftSize;
        for (index = 0; index < plan->fftSize; ++index) {
            size_t cell = row * caf->codeBins + columns[index];
            float power;
            if (!isfinite(context->accumulatedPower[index]) || context->accumulatedPower[index] < 0.0)
                continue;
            power = (float)context->accumulatedPower[index];
            if (caf->power[cell] < power) caf->power[cell] = power;
            if ((double)caf->power[cell] > caf->maxPower) caf->maxPower = caf->power[cell];
        }
        caf->available = 1;
        caf->epoch = view->meta.epoch;
        caf->referenceSample = context->coarseStartSample;
        caf->sequence = result->sequence + 1u;
    }
    {
        size_t peakIndex = 0;
        double peak, background = 0.0, threshold, score;
        for (index = 1; index < plan->fftSize; ++index)
            if (context->accumulatedPower[index] > context->accumulatedPower[peakIndex]) peakIndex = index;
        peak = context->accumulatedPower[peakIndex];
        for (index = 0; index < plan->fftSize; ++index) {
            size_t distance = index > peakIndex ? index - peakIndex : peakIndex - index;
            if (distance > plan->fftSize / 2u) distance = plan->fftSize - distance;
            if (distance > plan->noiseGuardBins) background += context->accumulatedPower[index];
        }
        background /= (double) plan->noiseReferenceCount * (double) context->coarseSums;
        threshold = background * context->thresholdFactor;
        score = threshold > 0.0 ? peak / threshold : peak > 0.0 ? DBL_MAX : 0.0;
        /* Different carrier bins can see different colored-noise/spur levels.
         * Select a statistically prominent peak, rather than the largest ADC
         * power against a background measured only at the fake +30 kHz bin. */
        if (score > context->coarseScore) {
            context->coarseScore = score;
            context->coarseMaximum = peak;
            context->coarseThreshold = threshold;
            context->coarseDoppler = doppler;
            context->originReference = view->meta.firstSample;
            context->originOffset = Wrap((double) peakIndex * (double) view->meta.count /
                (double) plan->fftSize + (double) (context->delayIndex * plan->delayStep),
                (double) view->meta.count);
            result->dopplerHz = doppler;
            result->carrierHz = NgCarrierFromDoppler(&plan->stream, doppler);
            result->metric = context->coarseMaximum;
            result->threshold = threshold;
        }
    }
    for (index = 0; index < plan->fftSize; ++index) {
        context->accumulatedPower[index] = 0.0;
    }
    context->sumsDone = 0;
    if (++context->delayIndex >= plan->delayCount) {
        context->delayIndex = 0;
        ++result->dopplerIndex;
        context->caf.completedDopplerBins = result->dopplerIndex;
    }
    if (result->dopplerIndex < plan->dopplerCount) return NG_OK;
    context->caf.complete = 1;
    if (context->coarseMaximum > result->threshold && context->coarseMaximum > 0.0) {
        context->codeRateHz = NG_GPS_CA_RATE_HZ * (1.0 + result->dopplerHz / ACQ_L1_HZ);
        StartRefinement(context, NG_ACQ_DOPPLER_REFINEMENT);
    } else result->phase = NG_ACQ_NOT_FOUND;
    return NG_OK;
}

static int RefineFrame(TNgAcquisition *context, const TNgIqView *view, TNgError *error)
{
    const TNgAcquisitionPlan *plan = context->plan;
    TNgAcquisitionResult *result = &context->result;
    TNgAcqComplexSum prompt;
    TNgAcquisitionPhase phase = result->phase;
    double offset, carrier = result->carrierHz;
    double codePhase = CodePhase(context, view->meta.firstSample);
    size_t count;
    if (phase == NG_ACQ_DOPPLER_REFINEMENT) {
        offset = -plan->config.refinementDopplerSpanHz +
            (double) context->refinementIndex * plan->config.refinementDopplerStepHz;
        carrier = NgCarrierFromDoppler(&plan->stream, context->coarseDoppler + offset);
        count = plan->dopplerRefinementCount;
    } else {
        offset = -plan->codeRefinementSpanSamples +
            (double) context->refinementIndex * (double) plan->config.refinementCodeStepSamples;
        codePhase -= offset * context->codeRateHz / (double) plan->stream.sampleRateHz;
        count = plan->codeRefinementCount;
    }
    if (NgAcqMathPromptCentered(plan->math, result->prn, view, carrier, 0.0,
            codePhase, context->codeRateHz, &prompt, error) != NG_OK) return NG_ERROR;
    context->refinementPower += prompt.power;
    context->refinementNoise += prompt.inputPower / (double) prompt.count;
    if (++context->sumsDone < plan->config.nonCoherentSums) return NG_OK;
    if (context->refinementPower > context->refinementMaximum) {
        context->refinementMaximum = context->refinementPower;
        context->bestRefinementOffset = offset;
    }
    context->sumsDone = 0;
    context->refinementPower = 0.0;
    context->refinementNoise = 0.0;
    if (++context->refinementIndex < count) return NG_OK;
    /* Refinement maximizes native prompt power, not the compressed FFT statistic.
     * Keep the coarse decision metric/threshold together until a fresh confirmation. */
    if (phase == NG_ACQ_DOPPLER_REFINEMENT) {
        result->dopplerHz = context->coarseDoppler + context->bestRefinementOffset;
        result->carrierHz = NgCarrierFromDoppler(&plan->stream, result->dopplerHz);
        context->codeRateHz = NG_GPS_CA_RATE_HZ * (1.0 + result->dopplerHz / ACQ_L1_HZ);
        StartRefinement(context, NG_ACQ_CODE_REFINEMENT);
    } else {
        context->originOffset += context->bestRefinementOffset;
        result->phase = plan->config.confirmation == NG_ACQ_CONFIRM_NONE ?
            NG_ACQ_ACQUIRED : NG_ACQ_CONFIRMATION;
        result->metric = context->coarseMaximum;
        result->threshold = context->coarseThreshold;
        result->nonCoherentSums = context->coarseSums;
        context->refinementIndex = 0;
        context->refinementPower = 0.0;
        context->refinementNoise = 0.0;
    }
    return NG_OK;
}

static int ConfirmationFrame(TNgAcquisition *context, const TNgIqView *view, TNgError *error)
{
    const TNgAcquisitionPlan *plan = context->plan;
    TNgAcquisitionResult *result = &context->result;
    TNgAcqComplexSum prompt;
    double noisePower;
    if (NgAcqMathPromptWithNoise(plan->math, result->prn, view, result->carrierHz, 0.0,
            CodePhase(context, view->meta.firstSample), context->codeRateHz,
            &prompt, &noisePower, error) != NG_OK) return NG_ERROR;
    context->refinementPower += prompt.power;
    /* Estimate the background after the same carrier/code filtering as the
     * candidate. Raw input energy/N is not a valid floor for DC or CW leakage.
     * Independent future frames, rather than refinement's winning samples,
     * supply both candidate and 32 off-peak control correlations. */
    context->refinementNoise += noisePower;
    if (++context->sumsDone < context->coarseSums) return NG_OK;
    result->metric = context->refinementPower;
    result->threshold = context->confirmationThresholdFactor * context->refinementNoise /
        (double) context->coarseSums;
    ++result->confirmationTrials;
    if (result->metric > result->threshold && result->metric > 0.0)
        ++result->confirmationHits;
    context->sumsDone = 0;
    context->refinementPower = 0.0;
    context->refinementNoise = 0.0;
    if (result->confirmationHits >= plan->config.confirmationSuccesses) {
        result->phase = NG_ACQ_ACQUIRED;
        /* GPS data polarity leaves a pi ambiguity. Tracking owns the coherent
         * carrier phase estimate; acquisition does not claim one here. */
        result->carrierPhaseValid = 0;
    } else if (result->confirmationTrials >= plan->config.confirmationAttempts ||
        plan->config.confirmationAttempts - result->confirmationTrials <
        plan->config.confirmationSuccesses - result->confirmationHits)
        result->phase = NG_ACQ_NOT_FOUND;
    return NG_OK;
}

static int ProcessFrame(TNgAcquisition *context, TNgError *error)
{
    TNgIqView frame;
    TNgAcquisitionPhase previousPhase = context->result.phase;
    int status;
    memset(&frame, 0, sizeof(frame));
    frame.samples = (const TNgIq8 *) context->frame;
    frame.format = context->plan->stream.format;
    frame.meta.firstSample = context->frameFirst;
    frame.meta.epoch = context->result.epoch;
    frame.meta.count = context->frameTarget;
    if (context->result.phase == NG_ACQ_COARSE_NOISE || context->result.phase == NG_ACQ_COARSE)
        status = CoarseFrame(context, &frame, error);
    else if (context->result.phase == NG_ACQ_DOPPLER_REFINEMENT ||
             context->result.phase == NG_ACQ_CODE_REFINEMENT)
        status = RefineFrame(context, &frame, error);
    else status = ConfirmationFrame(context, &frame, error);
    if (status != NG_OK) return status;
    ++context->result.sequence;
    UpdateReference(context, context->frameFirst + (uint64_t) context->frameTarget);
    /* Original NGene advances its channel cursor to the detected code origin.
     * Do the same privately; the kernel can keep distributing common windows.
     * This also avoids systematically straddling navigation-bit boundaries in
     * the following one-code-period coherent integrations. */
    if ((previousPhase == NG_ACQ_COARSE &&
         context->result.phase == NG_ACQ_DOPPLER_REFINEMENT) ||
        (previousPhase == NG_ACQ_CODE_REFINEMENT &&
         context->result.phase == NG_ACQ_CONFIRMATION))
        context->skipSamples = (size_t) floor(context->result.codeDelaySamples + 0.5);
    return NG_OK;
}

int NgAcquisitionProcess(TNgAcquisition *context, const TNgIqView *input,
                         TNgAcquisitionResult *result, TNgError *error)
{
    size_t offset = 0;
    size_t bytes;
    if (!context || !input || (!input->samples && input->meta.count))
        return NgFail(error, "Missing acquisition context or IQ input");
    bytes = NgSampleBytes(input->format);
    if (!bytes || bytes != context->plan->sampleBytes ||
        ((input->format ? input->format : NG_FORMAT_CI8) !=
         (context->plan->stream.format ? context->plan->stream.format : NG_FORMAT_CI8)))
        return NgFail(error, "Acquisition IQ format differs from its immutable stream plan");
    if (input->meta.count > UINT64_MAX - input->meta.firstSample ||
        input->meta.count > SIZE_MAX / bytes)
        return NgFail(error, "Acquisition input index or byte count overflows");
    if (context->result.phase == NG_ACQ_IDLE) {
        NgAcquisitionSnapshot(context, result);
        return NG_OK;
    }
    if (context->continuityValid && (input->meta.epoch != context->result.epoch ||
        input->meta.firstSample != context->expectedSample ||
        (input->meta.flags & NG_BLOCK_GAP) || input->meta.missingBefore))
        ClearSearch(context, 1);
    if (!input->meta.count) {
        if (!context->continuityValid) {
            context->result.epoch = input->meta.epoch;
            context->result.firstSample = input->meta.firstSample;
            context->result.referenceSample = input->meta.firstSample;
        }
        NgAcquisitionSnapshot(context, result);
        return NG_OK;
    }
    if (!context->continuityValid) {
        context->result.epoch = input->meta.epoch;
        context->result.firstSample = input->meta.firstSample;
        context->result.referenceSample = input->meta.firstSample;
        context->continuityValid = 1;
    }
    context->expectedSample = input->meta.firstSample + (uint64_t) input->meta.count;
    while (offset < input->meta.count && context->result.phase != NG_ACQ_ACQUIRED &&
           context->result.phase != NG_ACQ_NOT_FOUND) {
        size_t take;
        if (context->skipSamples) {
            take = context->skipSamples;
            if (take > input->meta.count - offset) take = input->meta.count - offset;
            context->skipSamples -= take;
            context->result.processedSamples += (uint64_t) take;
            offset += take;
            continue;
        }
        if (!context->frameTarget) {
            context->frameFirst = input->meta.firstSample + (uint64_t) offset;
            context->frameTarget = context->plan->stream.sampleRateHz / 1000u;
            context->frameRemainder += context->plan->stream.sampleRateHz % 1000u;
            if (context->frameRemainder >= 1000u) {
                ++context->frameTarget;
                context->frameRemainder -= 1000u;
            }
        }
        take = context->frameTarget - context->frameCount;
        if (take > input->meta.count - offset) take = input->meta.count - offset;
        memcpy(context->frame + context->frameCount * bytes,
            (const unsigned char *) input->samples + offset * bytes, take * bytes);
        context->frameCount += take;
        context->result.processedSamples += (uint64_t) take;
        offset += take;
        if (context->frameCount == context->frameTarget) {
            if (ProcessFrame(context, error) != NG_OK) return NG_ERROR;
            context->frameCount = 0;
            context->frameTarget = 0;
        }
    }
    NgAcquisitionSnapshot(context, result);
    return NG_OK;
}
