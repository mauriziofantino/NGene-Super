/** @file galileo.c @brief Classic-derived Galileo E1-B coarse, Doppler/code refinement and 3-of-4 search.
 * Numerical adaptation: native complex samples, floating PCPS, no legacy FFT gain constants.
 * FalseAlarm's integer-shape gamma survival law and Galileo E1-B Pfa=1e-9 are retained. */
#include "ngene/acquisition.h"
#include "galileo_codes.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "noise_policy.h"
#define NG_GAL_SEARCH_PERIODS 56u
#define NG_GAL_SUMS 8u
#define NG_GAL_E1_HZ 1575420000.0

/** Private reusable workspace; one instance per strategy runner. */
typedef struct {
    size_t size;
    size_t nativeCapacity;
    TNgSearchFft *fft;
    TNgComplex *baseband;
    TNgComplex *signal;
    TNgComplex *carrier;
    TNgComplex *work;
    TNgComplex *codeFft;
    TNgComplex *native;
    float *code;
    double *power;
    int8_t chips[32][4092u];
    uint64_t preparedFirst;
    uint64_t preparedEpoch;
    int prepared;
    double meanI;
    double meanQ;
    TNgAcquisitionSearchSpace space;
} TNgGalileoSearch;

/** Sample Classic E1-B primary code with its BOC(1,1) replica, no narrowing. */
static void GalileoCode(float *out, size_t count, const int8_t *chips,
                        double rate, double codeRate, double phase)
{
    size_t k;
    for (k = 0; k < count; ++k) {
        double chip = phase + (double)k * codeRate / rate;
        double wrapped = fmod(fmod(chip, 4092.0) + 4092.0, 4092.0);
        out[k] = (float)(chips[(size_t)wrapped] * ((wrapped - floor(wrapped)) < 0.5 ? 1 : -1));
    }
}

/** Copy the last coarse map on its owning worker. */
void NgAcquisitionGalileoSearchSpace(void *context, TNgAcquisitionSearchSpace *space)
{
    *space = ((TNgGalileoSearch *)context)->space;
}

/** Release all strategy-owned resources; accepts NULL and partial construction. */
void NgAcquisitionGalileoFree(void *context)
{
    TNgGalileoSearch *search = context;
    if (search == NULL)
        return;
    NgSearchFftDestroy(search->fft);
    free(search->baseband);
    free(search->signal);
    free(search->carrier);
    free(search->work);
    free(search->codeFft);
    free(search->native);
    free(search->code);
    free(search->power);
    free(search);
}

/** Allocate all replica/FFT/correlation buffers before reception. */
int NgAcquisitionGalileoWorkspace(const TNgStreamInfo *stream, void **out, TNgError *error)
{
    TNgGalileoSearch *search;
    unsigned prn;
    size_t capacity;
    *out = NULL;
    if (stream->sampleRateHz < 2048000 || stream->sampleRateHz > 20000000)
        return NgFail(error, "Galileo E1-B acquisition rate must be 2.048..20 Msps");
    search = calloc(1, sizeof(*search));
    if (search == NULL)
        return NgFail(error, "Galileo E1-B workspace allocation failed");
    search->size = stream->sampleRateHz >= 4096000 ? 16384 : 8192;
    search->nativeCapacity = (stream->sampleRateHz * 4u + 999u) / 1000u;
    capacity = search->nativeCapacity > search->size ? search->nativeCapacity : search->size;
    search->baseband = calloc(search->size * NG_GAL_SEARCH_PERIODS, sizeof(TNgComplex));
    search->signal = calloc(search->size * NG_GAL_SUMS, sizeof(TNgComplex));
    search->carrier = calloc(capacity, sizeof(TNgComplex));
    search->work = calloc(search->size, sizeof(TNgComplex));
    search->codeFft = calloc(32 * search->size, sizeof(TNgComplex));
    search->native = calloc(capacity, sizeof(TNgComplex));
    search->code = calloc(capacity, sizeof(float));
    search->power = calloc(search->size, sizeof(double));
    if (!search->baseband || !search->signal || !search->carrier || !search->work ||
        !search->codeFft || !search->native || !search->code || !search->power ||
        NgSearchFftCreate(search->size, &search->fft, error) != NG_OK) {
        NgAcquisitionGalileoFree(search);
        return NgFail(error, "Galileo E1-B DSP workspace allocation failed");
    }
    for (prn = 1; prn <= 32; ++prn) {
        size_t k;
        TNgComplex *spectrum = search->codeFft + (prn - 1) * search->size;
        for (k = 0; k < 4092; ++k)
            search->chips[prn - 1][k] = (ngGalileoCodes[prn - 1][k / 8] & (1u << (7 - k % 8))) ? -1 : 1;
        GalileoCode(search->code, search->size, search->chips[prn - 1],
                     (double)search->size * 250, 1023000.0, 0);
        for (k = 0; k < search->size; ++k)
            spectrum[k].i = search->code[k];
        NgSearchFftExecute(search->fft, spectrum, 0);
    }
    *out = search;
    return NG_OK;
}

/** Wipe actual digital IF before integrating native pairs onto a rational uniform grid.
 * Fixed box integration matches Classic compression; no component narrowing or FFT clipping.
 * Uses absolute rational boundaries, including SiGe's fractional samples per millisecond. */
static int Prepare(TNgGalileoSearch *search, const TNgAcquisitionCapture *capture,
                   TNgAcquisitionCancel cancel, void *cancelContext)
{
    size_t k;
    size_t outputCount = search->size * NG_GAL_SEARCH_PERIODS;
    double rate = (double)capture->stream.sampleRateHz;
    search->meanI = 0;
    search->meanQ = 0;
    for (k = 0; k < capture->count; ++k) {
        search->meanI += capture->samples[k].i;
        search->meanQ += capture->samples[k].q;
    }
    search->meanI /= (double)capture->count;
    search->meanQ /= (double)capture->count;
    for (k = 0; k < outputCount; ++k) {
        size_t begin =
            (size_t)((uint64_t)k * capture->stream.sampleRateHz / ((uint64_t)search->size * 250u));
        size_t end = (size_t)((uint64_t)(k + 1) * capture->stream.sampleRateHz /
                              ((uint64_t)search->size * 250u));
        size_t j;
        double i = 0;
        double q = 0;
        /* Carrier recurrence is generated in native-ms batches below. */
        if ((k % search->size) == 0 && cancel(cancelContext))
            return NG_END;
        for (j = begin; j < end; ++j) {
            i += capture->samples[j].i - search->meanI;
            q += (capture->samples[j].q - search->meanQ) *
                 (capture->stream.spectrumInverted ? -1 : 1);
        }
        search->baseband[k].i = (float)(i / (double)(end - begin));
        search->baseband[k].q = (float)(q / (double)(end - begin));
    }
    /* IF must be removed before compression. Recompute only when IF is nonzero. */
    if (capture->stream.digitalIfHz != 0) {
        unsigned ms;
        for (ms = 0; ms < NG_GAL_SEARCH_PERIODS; ++ms) {
            size_t begin = (size_t)((uint64_t)ms * 4u * capture->stream.sampleRateHz / 1000u);
            size_t end = (size_t)((uint64_t)(ms + 1) * 4u * capture->stream.sampleRateHz / 1000u);
            size_t count = end - begin;
            size_t j;
            NgSearchCarrier(search->carrier, count, rate, (double)capture->stream.digitalIfHz,
                            (double)begin * (double)capture->stream.digitalIfHz / rate, 0);
            for (j = 0; j < count; ++j) {
                search->native[j].i = (float)(capture->samples[begin + j].i - search->meanI);
                search->native[j].q = (float)((capture->samples[begin + j].q - search->meanQ) *
                                              (capture->stream.spectrumInverted ? -1 : 1));
            }
            NgSearchMix(search->native, search->carrier, count);
            for (j = 0; j < search->size; ++j) {
                size_t first =
                    (size_t)((uint64_t)(ms * search->size + j) * capture->stream.sampleRateHz /
                             ((uint64_t)search->size * 250u)) -
                    begin;
                size_t last =
                    (size_t)((uint64_t)(ms * search->size + j + 1) * capture->stream.sampleRateHz /
                             ((uint64_t)search->size * 250u)) -
                    begin;
                size_t t;
                double i = 0;
                double q = 0;
                for (t = first; t < last; ++t) {
                    i += search->native[t].i;
                    q += search->native[t].q;
                }
                search->baseband[ms * search->size + j].i = (float)(i / (double)(last - first));
                search->baseband[ms * search->size + j].q = (float)(q / (double)(last - first));
            }
            if (cancel(cancelContext))
                return NG_END;
        }
    }
    search->preparedFirst = capture->origin.firstSample;
    search->preparedEpoch = capture->origin.epoch;
    search->prepared = 1;
    return NG_OK;
}

/** Coarse PCPS power over eight four-ms coherent periods. */
static void Coarse(TNgGalileoSearch *search, unsigned prn, double doppler, int square)
{
    unsigned ms;
    size_t k;
    const TNgComplex *code = search->codeFft + (prn - 1) * search->size;
    memset(search->power, 0, search->size * sizeof(*search->power));
    NgSearchCarrier(search->carrier, search->size, (double)search->size * 250, doppler, 0, square);
    for (ms = 0; ms < NG_GAL_SUMS; ++ms) {
        memcpy(search->work, search->baseband + ms * search->size,
               search->size * sizeof(TNgComplex));
        NgSearchMix(search->work, search->carrier, search->size);
        NgSearchFftExecute(search->fft, search->work, 0);
        for (k = 0; k < search->size; ++k) {
            TNgComplex a = search->work[k];
            search->work[k].i = a.i * code[k].i + a.q * code[k].q;
            search->work[k].q = a.q * code[k].i - a.i * code[k].q;
        }
        NgSearchFftExecute(search->fft, search->work, 1);
        for (k = 0; k < search->size; ++k) {
            double i = search->work[k].i;
            double q = search->work[k].q;
            search->power[k] += i * i + q * q;
        }
    }
}

/** Mix 32 ms from the prepared grid for Doppler refinement, phase remains continuous. */
static void MixGrid(TNgGalileoSearch *search, unsigned startMs, double doppler, int square)
{
    unsigned ms;
    for (ms = 0; ms < NG_GAL_SUMS; ++ms) {
        TNgComplex *output = search->signal + ms * search->size;
        memcpy(output, search->baseband + (startMs + ms) * search->size,
               search->size * sizeof(*output));
        NgSearchCarrier(search->carrier, search->size, (double)search->size * 250, doppler,
                        (double)(startMs + ms) * doppler / 250, square);
        NgSearchMix(output, search->carrier, search->size);
    }
}

/** Prompt power on grid, including physical Doppler's code-rate correction. */
static double GridPower(TNgGalileoSearch *search, unsigned prn, unsigned startMs, double doppler,
                        double phase, int xorSigns)
{
    double power = 0;
    double codeRate = 1023000.0 * (1 + doppler / NG_GAL_E1_HZ);
    unsigned ms;
    for (ms = 0; ms < NG_GAL_SUMS; ++ms) {
        double i;
        double q;
        GalileoCode(search->code, search->size, search->chips[prn - 1],
                     (double)search->size * 250, codeRate,
                     phase + (startMs + ms) * codeRate / 250);
        NgSearchCorrelate(search->signal + ms * search->size, search->code, search->size, xorSigns,
                          &i, &q);
        power += i * i + q * q;
    }
    return power;
}

/** Native-rate coherent prompt; carrier and code share exact capture-relative sample origin. */
static double NativePower(TNgGalileoSearch *search, const TNgAcquisitionCapture *capture, unsigned prn,
                          unsigned startMs, double doppler, double phase,
                          const TNgAcquisitionConfig *config, double *noise, double carrierOffset)
{
    double power = 0;
    double rate = (double)capture->stream.sampleRateHz;
    double frequency = (double)capture->stream.digitalIfHz + doppler + carrierOffset;
    double codeRate = 1023000.0 * (1 + doppler / NG_GAL_E1_HZ);
    unsigned ms;
    *noise = 0;
    for (ms = startMs; ms < startMs + NG_GAL_SUMS; ++ms) {
        size_t begin = (size_t)((uint64_t)ms * 4u * capture->stream.sampleRateHz / 1000u);
        size_t end = (size_t)((uint64_t)(ms + 1) * 4u * capture->stream.sampleRateHz / 1000u);
        size_t count = end - begin;
        size_t k;
        double i;
        double q;
        double energy = 0;
        NgSearchCarrier(search->carrier, count, rate, frequency, (double)begin * frequency / rate,
                        config->squareCarrier);
        GalileoCode(search->code, count, search->chips[prn - 1], rate, codeRate,
                     phase + (double)begin * codeRate / rate);
        for (k = 0; k < count; ++k) {
            search->native[k].i = (float)(capture->samples[begin + k].i - search->meanI);
            search->native[k].q = (float)((capture->samples[begin + k].q - search->meanQ) *
                                          (capture->stream.spectrumInverted ? -1 : 1));
        }
        NgSearchMix(search->native, search->carrier, count);
        for (k = 0; k < count; ++k) {
            double ri = search->native[k].i;
            double rq = search->native[k].q;
            energy += ri * ri + rq * rq;
        }
        NgSearchCorrelate(search->native, search->code, count, config->xorCorrelation, &i, &q);
        power += i * i + q * q;
        *noise += energy / NG_GAL_SUMS;
    }
    return power;
}

/** Search one Galileo E1-B satellite; fresh data for each stage, no tracking state fabrication. */
static int Search(void *context, const TNgAcquisitionCapture *capture, unsigned prn,
                  const TNgAcquisitionConfig *config, TNgAcquisitionCancel cancel,
                  void *cancelContext, TNgAcquisitionResult *result, TNgError *error)
{
    TNgGalileoSearch *search = context;
    double noise = 0;
    double threshold;
    double best = 0;
    double doppler = 0;
    double phase = 0;
    double factor = AcquisitionThreshold(NG_GAL_SUMS, config->falseAlarmProbability);
    size_t k;
    unsigned bin;
    (void)error;
    memset(result, 0, sizeof(*result));
    memset(&search->space, 0, sizeof(search->space));
    result->prn = prn;
    result->epoch = capture->origin.epoch;
    result->firstSample = capture->origin.firstSample;
    if (!search->prepared || search->preparedFirst != capture->origin.firstSample ||
        search->preparedEpoch != capture->origin.epoch) {
        if (Prepare(search, capture, cancel, cancelContext) != NG_OK)
            return NG_END;
    }
    /* Classic's fake-Doppler population estimates the complex correlation variance.
     * The sum's mean is 8 * (2*variance); the gamma threshold uses single-period power. */
    Coarse(search, prn, AcquisitionCoarseNoiseDoppler(), config->squareCarrier);
    for (k = 0; k < search->size; ++k)
        noise += search->power[k];
    noise /= (double)(search->size * NG_GAL_SUMS);
    threshold = noise * factor;
    if (!(threshold > 0))
        return NG_OK;
    /* Classic Galileo's 87-Hz grid: -5000 through +5005 Hz (116 trials). */
    for (bin = 0; bin <= 115; ++bin) {
        double frequency = -5000 + (double)bin * 87;
        if (cancel(cancelContext))
            return NG_END;
        Coarse(search, prn, frequency, config->squareCarrier);
        for (k = 0; k < search->size; ++k) {
            size_t column = ((search->size - k) % search->size) * NG_ACQUISITION_CODE_BINS / search->size;
            float value = (float)(search->power[k] / threshold);
            if (value > search->space.power[bin][column])
                search->space.power[bin][column] = value;
            if (search->power[k] > best) {
                best = search->power[k];
                doppler = frequency;
                phase = -(double)k * 4092u / (double)search->size;
            }
        }
    }
    result->peakRatio = best / threshold;
    result->dopplerHz = doppler;
    result->codePhaseChips = fmod(phase + 4092u, 4092u);
    search->space.valid = 1;
    search->space.dopplerBins = 116;
    search->space.codePeriodChips = 4092;
    search->space.fftSize = (unsigned)search->size;
    search->space.threshold = threshold;
    search->space.peakDopplerHz = doppler;
    search->space.peakCodePhaseChips = result->codePhaseChips;
    if (best <= threshold)
        return NG_OK;
    /* Doppler refinement: +/-667 Hz, 10 Hz step, eight noncoherent periods. */
    {
        double coarseDoppler = doppler;
        best = 0;
        for (bin = 0; bin <= 134; ++bin) {
            double frequency = coarseDoppler - 667 + (bin == 134 ? 1334 : (double)bin * 10);
            double power;
            if (cancel(cancelContext))
                return NG_END;
            if (config->squareCarrier) {
                /* Squared oscillators do not compose across IF and Doppler. Refine
                 * with the complete native-rate carrier, as used in confirmation. */
                power = NativePower(search, capture, prn, 8, frequency, phase, config, &noise, 0);
            } else {
                MixGrid(search, 8, frequency, 0);
                power = GridPower(search, prn, 8, frequency, phase, config->xorCorrelation);
            }
            if (power > best) {
                best = power;
                doppler = frequency;
            }
        }
    }
    /* Code refinement: +/-1 chip, one NATIVE sample step, independently of PCPS rate. */
    {
        int delta;
        int radius = (int)ceil((double)capture->stream.sampleRateHz / 1023000.0);
        double coarsePhase = phase;
        best = 0;
        for (delta = -radius; delta <= radius; ++delta) {
            double test = coarsePhase +
                          (double)delta * 1023000.0 / (double)capture->stream.sampleRateHz;
            double power;
            if (cancel(cancelContext))
                return NG_END;
            power = NativePower(search, capture, prn, 16, doppler, test, config, &noise, 0);
            if (power > best) {
                best = power;
                phase = test;
            }
        }
    }
    result->dopplerHz = doppler;
    result->codePhaseChips =
        fmod(fmod(phase, 4092u) + 4092u, 4092u);
    /* Four fresh, disjoint intervals. Noise probes retain the candidate replica
     * and code rate, but rotate at deliberately wrong carriers outside the search.
     * Integer-kHz offsets reject coherent four-ms candidate prompts. No ADC narrowing,
     * off-code estimator, historical threshold, or coarse-gain fallback is used. */
    for (bin = 0; bin < 4; ++bin) {
        double power;
        double referenceNoise = 0;
        unsigned reference;
        unsigned startMs = 24 + bin * NG_GAL_SUMS;
        if (cancel(cancelContext))
            return NG_END;
        power = NativePower(search, capture, prn, startMs, doppler, phase, config, &noise, 0);
        for (reference = 0; reference < 8; ++reference) {
            double offset = AcquisitionNoiseOffset(reference);
            if (cancel(cancelContext))
                return NG_END;
            referenceNoise += NativePower(search, capture, prn, startMs, doppler, phase, config,
                                    &noise, offset);
        }
        referenceNoise /= (double)(8 * NG_GAL_SUMS);
        if (referenceNoise > 0 && power > referenceNoise * factor)
            result->successes++;
        if (result->successes >= 3) {
            result->confirmed = 1;
            break;
        }
        if (result->successes + 3 - bin < 3)
            break;
    }
    return NG_OK;
}

/** Public strategy interface; no receiver/controller dependency. */
const TNgAcquisitionStrategy ngGalileoAcquisitionStrategy = {Search};
