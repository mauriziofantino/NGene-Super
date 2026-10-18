/** @file acquisition_math.c @brief Native IQ legacy compression and calibrated Q14 correlation. */
#include "ngene/dsp/acquisition_math.h"
#include "ngene/dsp/fft.h"
#include "ngene/dsp/gps_ca.h"
#include "ngene/dsp/nco.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int16_t *spectrum;
    double scale;
    double energy;
} TNgAcqReplica;

struct TNgAcqMathPlan {
    uint32_t sampleRateHz;
    uint32_t prnMask;
    size_t size;
    unsigned variants;
    size_t count[2];
    size_t *boundaries[2];
    int8_t chips[32][NG_GPS_CA_LENGTH];
    TNgAcqReplica replica[2][32];
    TNgNcoTable carrier;
    TNgFftPlan *fft;
};

struct TNgAcqMathWorkspace {
    const TNgAcqMathPlan *plan;
    double *values;
    int16_t *input;
    int16_t *spectrum;
    int16_t *correlation;
};

static int ValidView(const TNgIqView *view)
{
    return view != NULL && view->samples != NULL && view->meta.count != 0 &&
        NgSampleBytes(view->format) != 0;
}

static int ComponentMeans(const TNgIqView *view, double *meanI, double *meanQ,
                           TNgError *error)
{
    int64_t sumI = 0, sumQ = 0;
    size_t n;
    if ((uint64_t)view->meta.count > (uint64_t)INT64_MAX / UINT64_C(32768))
        return NgFail(error, "Acquisition frame is too large for exact native component sums");
    for (n = 0; n < view->meta.count; ++n) {
        sumI += NgIqComponent(view, n, 0);
        sumQ += NgIqComponent(view, n, 1);
    }
    *meanI = (double)sumI / (double)view->meta.count;
    *meanQ = (double)sumQ / (double)view->meta.count;
    return NG_OK;
}

/* Retaining 16000 input levels gives the existing Q14 butterflies explicit
 * headroom. Each transform is 1/N; every floating gain is recorded and undone. */
static double Quantize(const double *input, int16_t *output, size_t count)
{
    size_t n;
    double maximum = 0.0, scale;
    for (n = 0; n < count; ++n) {
        double value = fabs(input[n]);
        if (value > maximum) maximum = value;
    }
    if (maximum == 0.0) {
        memset(output, 0, count * sizeof(*output));
        return 0.0;
    }
    scale = 16000.0 / maximum;
    for (n = 0; n < count; ++n) output[n] = (int16_t)lround(input[n] * scale);
    return scale;
}

void NgAcqMathWorkspaceDestroy(TNgAcqMathWorkspace *workspace)
{
    if (workspace == NULL) return;
    free(workspace->values);
    free(workspace->input);
    free(workspace->spectrum);
    free(workspace->correlation);
    free(workspace);
}

int NgAcqMathWorkspaceCreate(const TNgAcqMathPlan *plan, TNgAcqMathWorkspace **out,
                              TNgError *error)
{
    TNgAcqMathWorkspace *workspace;
    if (out == NULL) return NgFail(error, "Missing acquisition workspace destination");
    *out = NULL;
    if (plan == NULL) return NgFail(error, "Missing acquisition math plan");
    workspace = (TNgAcqMathWorkspace *)calloc(1, sizeof(*workspace));
    if (workspace == NULL) return NgFail(error, "Cannot allocate acquisition workspace");
    workspace->plan = plan;
    workspace->values = (double *)calloc(2u * plan->size, sizeof(*workspace->values));
    workspace->input = (int16_t *)calloc(2u * plan->size, sizeof(*workspace->input));
    workspace->spectrum = (int16_t *)calloc(2u * plan->size, sizeof(*workspace->spectrum));
    workspace->correlation = (int16_t *)calloc(2u * plan->size, sizeof(*workspace->correlation));
    if (workspace->values == NULL || workspace->input == NULL ||
        workspace->spectrum == NULL || workspace->correlation == NULL) {
        NgAcqMathWorkspaceDestroy(workspace);
        return NgFail(error, "Cannot allocate acquisition FFT scratch");
    }
    *out = workspace;
    return NG_OK;
}

void NgAcqMathDestroy(TNgAcqMathPlan *plan)
{
    unsigned v, prn;
    if (plan == NULL) return;
    for (v = 0; v < 2; ++v) {
        for (prn = 0; prn < 32; ++prn) free(plan->replica[v][prn].spectrum);
        free(plan->boundaries[v]);
    }
    NgFftDestroy(plan->fft);
    free(plan);
}

int NgAcqMathCreate(uint32_t sampleRateHz, size_t fftSize, uint32_t prnMask,
                     TNgAcqMathPlan **out, TNgError *error)
{
    TNgAcqMathPlan *plan;
    TNgAcqMathWorkspace *workspace = NULL;
    unsigned v, prn;
    size_t j;
    if (out == NULL) return NgFail(error, "Missing acquisition math plan destination");
    *out = NULL;
    if (sampleRateHz < 8000u || fftSize < 8u || fftSize > 16384u ||
        (fftSize & (fftSize - 1u)) != 0 || fftSize > sampleRateHz / 1000u || prnMask == 0)
        return NgFail(error, "Invalid acquisition sample rate, FFT size or GPS PRN mask");
    plan = (TNgAcqMathPlan *)calloc(1, sizeof(*plan));
    if (plan == NULL) return NgFail(error, "Cannot allocate acquisition math plan");
    plan->sampleRateHz = sampleRateHz;
    plan->prnMask = prnMask;
    plan->size = fftSize;
    plan->count[0] = sampleRateHz / 1000u;
    plan->count[1] = ((uint64_t)sampleRateHz + 999u) / 1000u;
    plan->variants = plan->count[0] == plan->count[1] ? 1u : 2u;
    NgNcoTableInit(&plan->carrier);
    if (NgFftCreate(fftSize, &plan->fft, error) != NG_OK ||
        NgAcqMathWorkspaceCreate(plan, &workspace, error) != NG_OK) goto failed;
    for (prn = 0; prn < 32; ++prn) {
        if ((prnMask & (UINT32_C(1) << prn)) != 0u &&
            NgGpsCaGenerate(prn + 1u, plan->chips[prn], error) != NG_OK) goto failed;
    }
    for (v = 0; v < plan->variants; ++v) {
        plan->boundaries[v] = (size_t *)calloc(fftSize + 1u, sizeof(size_t));
        if (plan->boundaries[v] == NULL) {
            NgFail(error, "Cannot allocate acquisition compression boundaries");
            goto failed;
        }
        /* Exactly the legacy Q16 short/long NCO sequence with initial remainder
         * zero. For radix-two N<=16384 its Q16 ratio is exact. Unlike the legacy
         * char sums, native samples and accumulators are never narrowed. */
        for (j = 0; j <= fftSize; ++j)
            plan->boundaries[v][j] = (size_t)(((uint64_t)j * plan->count[v] +
                                               fftSize - 1u) / fftSize);
        for (prn = 0; prn < 32; ++prn) {
            TNgAcqReplica *replica = &plan->replica[v][prn];
            if ((prnMask & (UINT32_C(1) << prn)) == 0u) continue;
            for (j = 0; j < fftSize; ++j) {
                size_t n;
                int64_t sum = 0;
                for (n = plan->boundaries[v][j]; n < plan->boundaries[v][j + 1u]; ++n) {
                    size_t chip = (size_t)(((uint64_t)n * UINT64_C(1023000) /
                                               sampleRateHz) % NG_GPS_CA_LENGTH);
                    sum += plan->chips[prn][chip];
                }
                workspace->values[2u * j] = (double)sum;
                workspace->values[2u * j + 1u] = 0.0;
                replica->energy += (double)sum * (double)sum;
            }
            replica->scale = Quantize(workspace->values, workspace->input, 2u * fftSize);
            replica->spectrum = (int16_t *)calloc(2u * fftSize, sizeof(int16_t));
            if (replica->spectrum == NULL) {
                NgFail(error, "Cannot allocate cached GPS replica spectrum");
                goto failed;
            }
            if (replica->energy <= 0.0 || replica->scale == 0.0) {
                NgFail(error, "GPS replica disappeared at the selected compression ratio");
                goto failed;
            }
            if (NgFftExecute(plan->fft, fftSize, workspace->input, replica->spectrum, error) != NG_OK)
                goto failed;
        }
    }
    NgAcqMathWorkspaceDestroy(workspace);
    *out = plan;
    return NG_OK;
failed:
    NgAcqMathWorkspaceDestroy(workspace);
    NgAcqMathDestroy(plan);
    return NG_ERROR;
}

static int Correlate(const TNgAcqMathPlan *plan, TNgAcqMathWorkspace *workspace,
                     unsigned prn, const TNgIqView *view, double carrierHz,
                     double initialCarrierCycles, size_t sampleOffset,
                     double *power, int centered, TNgError *error)
{
    unsigned variant;
    size_t j, position;
    double inputScale, productScale, normalization;
    double meanI = 0.0, meanQ = 0.0;
    const TNgAcqReplica *replica;
    TNgCarrierNco nco, beginning;
    if (plan == NULL || workspace == NULL || workspace->plan != plan || power == NULL ||
        !ValidView(view) || prn < 1u || prn > 32u ||
        (plan->prnMask & (UINT32_C(1) << (prn - 1u))) == 0u)
        return NgFail(error, "Invalid acquisition correlation input or GPS PRN");
    for (variant = 0; variant < plan->variants; ++variant)
        if (plan->count[variant] == view->meta.count) break;
    if (variant == plan->variants)
        return NgFail(error, "Acquisition FFT requires one floor/ceil millisecond of native samples");
    if (NgNcoInit(&beginning, (double)plan->sampleRateHz, carrierHz,
                   initialCarrierCycles, error) != NG_OK) return NG_ERROR;
    if (centered && ComponentMeans(view, &meanI, &meanQ, error) != NG_OK) return NG_ERROR;
    position = sampleOffset % view->meta.count;
    nco = beginning;
    nco.phase += nco.step * (uint64_t)position;
    for (j = 0; j < plan->size; ++j) {
        size_t n, length = plan->boundaries[variant][j + 1u] - plan->boundaries[variant][j];
        double sumI = 0.0, sumQ = 0.0;
        for (n = 0; n < length; ++n) {
            double i, q;
            int nativeI = NgIqComponent(view, position, 0);
            int nativeQ = NgIqComponent(view, position, 1);
            if (centered)
                NgNcoRotateF64(&plan->carrier, &nco, (double)nativeI - meanI,
                                (double)nativeQ - meanQ, &i, &q);
            else NgNcoRotate(&plan->carrier, &nco, nativeI, nativeQ, &i, &q);
            sumI += i;
            sumQ += q;
            if (++position == view->meta.count) {
                position = 0;
                nco.phase = beginning.phase;
            }
        }
        workspace->values[2u * j] = sumI;
        workspace->values[2u * j + 1u] = sumQ;
    }
    inputScale = Quantize(workspace->values, workspace->input, 2u * plan->size);
    if (inputScale == 0.0) {
        memset(power, 0, plan->size * sizeof(*power));
        return NG_OK;
    }
    if (NgFftExecute(plan->fft, plan->size, workspace->input, workspace->spectrum, error) != NG_OK)
        return NG_ERROR;
    replica = &plan->replica[variant][prn - 1u];
    for (j = 0; j < plan->size; ++j) {
        int64_t i = workspace->spectrum[2u * j];
        int64_t q = workspace->spectrum[2u * j + 1u];
        int64_t ci = replica->spectrum[2u * j];
        int64_t cq = replica->spectrum[2u * j + 1u];
        workspace->values[2u * j] = (double)(i * ci + q * cq);
        workspace->values[2u * j + 1u] = (double)(q * ci - i * cq);
    }
    productScale = Quantize(workspace->values, workspace->input, 2u * plan->size);
    if (productScale == 0.0) {
        memset(power, 0, plan->size * sizeof(*power));
        return NG_OK;
    }
    if (NgFftExecute(plan->fft, plan->size, workspace->input, workspace->correlation, error) != NG_OK)
        return NG_ERROR;
    normalization = ((double)plan->size * (double)plan->size) /
        (inputScale * replica->scale * productScale * replica->energy);
    for (j = 0; j < plan->size; ++j) {
        /* Forward FFT twice = inverse correlation in reverse time. The inverse
         * also conjugates Q; power is invariant but the positive delay is not. */
        size_t reverse = j == 0 ? 0 : plan->size - j;
        double i = (double)workspace->correlation[2u * reverse] * normalization;
        double q = (double)workspace->correlation[2u * reverse + 1u] * normalization;
        power[j] = i * i + q * q;
    }
    return NG_OK;
}

int NgAcqMathCorrelateOffset(const TNgAcqMathPlan *plan, TNgAcqMathWorkspace *workspace,
                              unsigned prn, const TNgIqView *view, double carrierHz,
                              double initialCarrierCycles, size_t sampleOffset,
                              double *power, TNgError *error)
{
    return Correlate(plan, workspace, prn, view, carrierHz, initialCarrierCycles,
                      sampleOffset, power, 0, error);
}

int NgAcqMathCorrelateCentered(const TNgAcqMathPlan *plan, TNgAcqMathWorkspace *workspace,
                                unsigned prn, const TNgIqView *view, double carrierHz,
                                double initialCarrierCycles, size_t sampleOffset,
                                double *power, TNgError *error)
{
    return Correlate(plan, workspace, prn, view, carrierHz, initialCarrierCycles,
                      sampleOffset, power, 1, error);
}

int NgAcqMathCorrelate(const TNgAcqMathPlan *plan, TNgAcqMathWorkspace *workspace,
                        unsigned prn, const TNgIqView *view, double carrierHz,
                        double initialCarrierCycles, double *power, TNgError *error)
{
    return NgAcqMathCorrelateOffset(plan, workspace, prn, view, carrierHz,
                                    initialCarrierCycles, 0, power, error);
}

static int Prompt(const TNgAcqMathPlan *plan, unsigned prn, const TNgIqView *view,
                  double carrierHz, double initialCarrierCycles, double codePhaseChips,
                  double codeRateHz, TNgAcqComplexSum *result, double *noisePower,
                  int centered, TNgError *error)
{
    TNgCarrierNco carrier;
    TNgGpsCaNco code;
    double referenceI[NG_ACQ_NOISE_REFERENCES], referenceQ[NG_ACQ_NOISE_REFERENCES];
    size_t n;
    double sumI = 0.0, sumQ = 0.0, energy = 0.0;
    double meanI = 0.0, meanQ = 0.0;
    if (plan == NULL || result == NULL || !ValidView(view) || prn < 1u || prn > 32u ||
        (plan->prnMask & (UINT32_C(1) << (prn - 1u))) == 0u)
        return NgFail(error, "Invalid acquisition prompt input or GPS PRN");
    if (NgNcoInit(&carrier, (double)plan->sampleRateHz, carrierHz,
                   initialCarrierCycles, error) != NG_OK ||
        NgGpsCaNcoInit(&code, (double)plan->sampleRateHz, codeRateHz,
                        codePhaseChips, error) != NG_OK) return NG_ERROR;
    if (centered && ComponentMeans(view, &meanI, &meanQ, error) != NG_OK) return NG_ERROR;
    if (noisePower != NULL) {
        memset(referenceI, 0, sizeof(referenceI));
        memset(referenceQ, 0, sizeof(referenceQ));
    }
    for (n = 0; n < view->meta.count; ++n) {
        int i = NgIqComponent(view, n, 0), q = NgIqComponent(view, n, 1);
        size_t chipIndex = (size_t)(code.phase >> 32);
        int chip = NgGpsCaNext(plan->chips[prn - 1u], &code);
        double rotatedI, rotatedQ;
        double residualI = (double)i - meanI, residualQ = (double)q - meanQ;
        if (centered)
            NgNcoRotateF64(&plan->carrier, &carrier, residualI, residualQ, &rotatedI, &rotatedQ);
        else NgNcoRotate(&plan->carrier, &carrier, i, q, &rotatedI, &rotatedQ);
        sumI += rotatedI * chip;
        sumQ += rotatedQ * chip;
        energy += residualI * residualI + residualQ * residualQ;
        if (noisePower != NULL) {
            unsigned reference;
            /* Integer chip shifts preserve the exact fractional code phase,
             * including rational-clock samples exactly on chip boundaries. */
            size_t index = chipIndex + 16u;
            if (index >= NG_GPS_CA_LENGTH) index -= NG_GPS_CA_LENGTH;
            for (reference = 0; reference < NG_ACQ_NOISE_REFERENCES; ++reference) {
                int referenceChip = plan->chips[prn - 1u][index];
                referenceI[reference] += rotatedI * referenceChip;
                referenceQ[reference] += rotatedQ * referenceChip;
                index += 31u;
                if (index >= NG_GPS_CA_LENGTH) index -= NG_GPS_CA_LENGTH;
            }
        }
    }
    result->count = view->meta.count;
    result->i = sumI / (double)view->meta.count;
    result->q = sumQ / (double)view->meta.count;
    result->power = result->i * result->i + result->q * result->q;
    result->inputPower = energy / (double)view->meta.count;
    if (noisePower != NULL) {
        unsigned reference;
        double power = 0.0;
        for (reference = 0; reference < NG_ACQ_NOISE_REFERENCES; ++reference)
            power += referenceI[reference] * referenceI[reference] +
                     referenceQ[reference] * referenceQ[reference];
        *noisePower = power / ((double)NG_ACQ_NOISE_REFERENCES *
                               (double)view->meta.count * (double)view->meta.count);
    }
    return NG_OK;
}

int NgAcqMathPrompt(const TNgAcqMathPlan *plan, unsigned prn, const TNgIqView *view,
                     double carrierHz, double initialCarrierCycles, double codePhaseChips,
                     double codeRateHz, TNgAcqComplexSum *result, TNgError *error)
{
    return Prompt(plan, prn, view, carrierHz, initialCarrierCycles, codePhaseChips,
                   codeRateHz, result, NULL, 0, error);
}

int NgAcqMathPromptCentered(const TNgAcqMathPlan *plan, unsigned prn,
                             const TNgIqView *view, double carrierHz,
                             double initialCarrierCycles, double codePhaseChips,
                             double codeRateHz, TNgAcqComplexSum *result, TNgError *error)
{
    return Prompt(plan, prn, view, carrierHz, initialCarrierCycles, codePhaseChips,
                   codeRateHz, result, NULL, 1, error);
}

int NgAcqMathPromptWithNoise(const TNgAcqMathPlan *plan, unsigned prn,
                              const TNgIqView *view, double carrierHz,
                              double initialCarrierCycles, double codePhaseChips,
                              double codeRateHz, TNgAcqComplexSum *result,
                              double *noisePower, TNgError *error)
{
    if (noisePower == NULL) return NgFail(error, "Missing acquisition reference noise destination");
    *noisePower = 0.0;
    return Prompt(plan, prn, view, carrierHz, initialCarrierCycles, codePhaseChips,
                   codeRateHz, result, noisePower, 1, error);
}

/* Tail of a gamma cell divided by an independent reference mean. Under the
 * white-noise model, S ~ Gamma(M,theta), R ~ Gamma(K*M,theta) and
 * P(S > x*R/(K*M)) is a finite negative-binomial sum. Unlike the known-noise
 * gamma threshold, this includes uncertainty in the estimated reference mean.
 * Real code-shift references can be correlated; Pfa remains a nominal cell
 * probability, not a promise for arbitrary interference or repeated searches. */
static double ReferenceSurvival(unsigned shape, size_t references, double x)
{
    unsigned k;
    double logTerm, maximum = -DBL_MAX, sum = 0.0;
    double referenceShape = (double) references * (double) shape;
    double logProbability, logNormalization;
    if (x <= 0.0) return 1.0;
    logProbability = log(x / (referenceShape + x));
    logNormalization = -referenceShape * log1p(x / referenceShape);
    logTerm = 0.0;
    for (k = 0; k < shape; ++k) {
        if (k) logTerm += log(referenceShape + (double) k - 1.0) - log((double) k) + logProbability;
        if (logTerm > maximum) maximum = logTerm;
    }
    logTerm = 0.0;
    for (k = 0; k < shape; ++k) {
        if (k) logTerm += log(referenceShape + (double) k - 1.0) - log((double) k) + logProbability;
        sum += exp(logTerm - maximum);
    }
    return exp(maximum + logNormalization) * sum;
}

int NgAcqMathThreshold(unsigned sums, size_t references, double pfa, double *factor, TNgError *error)
{
    double low = 0.0, high = (double) sums;
    unsigned iteration;
    if (!factor || sums < 1 || sums > 1024 || references < 1 || references > 16384 ||
        !isfinite(pfa) || pfa < 1e-15 || pfa >= 1.0)
        return NgFail(error, "Invalid finite-reference acquisition threshold parameters");
    while (ReferenceSurvival(sums, references, high) > pfa && high < 1e12) high *= 2.0;
    for (iteration = 0; iteration < 72; ++iteration) {
        double middle = (low + high) * 0.5;
        if (ReferenceSurvival(sums, references, middle) > pfa) low = middle;
        else high = middle;
    }
    *factor = (low + high) * 0.5;
    return NG_OK;
}
