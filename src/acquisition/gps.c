/** @file gps.c @brief Classic-derived GPS coarse, Doppler/code refinement and 3-of-4 search.
 * Numerical adaptation: native complex samples, floating PCPS, no legacy FFT gain constants.
 * FalseAlarm's integer-shape gamma survival law and GPS Pfa=1e-9 are retained.
 *
 * @details For each Doppler hypothesis mix the native samples by the negative
 * carrier phase. Correlate each of eight 1-ms periods circularly against the
 * C/A replica on the PCPS grid. With mixed block \f$x_m\f$ and replica \f$c\f$,
 * the coarse map is
 * \f[
 * P_f[\ell]=\sum_{m=0}^{7}\left|
 * \operatorname{IFFT}\!\left(\operatorname{FFT}(x_m)
 *                  \overline{\operatorname{FFT}(c)}\right)[\ell]\right|^2 .
 * \f]
 * The detector estimates noise at a wrong Doppler and normalizes the sum by
 * eight to obtain the mean single-period power. For independent complex
 * Gaussian noise, \f$P_f/\mu\f$ has a unit-scale gamma law of shape eight,
 * where \f$\mu\f$ is that mean. The configured Pfa is the per-cell tail.
 * Refine Doppler, then code phase on the native sample grid. Confirmation
 * tests three of four independent windows and preserves the capture-origin
 * code phase and sample index required by tracking.
 */
#include "acquisition/workspace.h"
#include "dsp/egnos_code.h"
#include "dsp/search_plan.h"
#include "ngene/dsp/gps_ca.h"
#include "acquisition/grid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "noise_policy.h"
#define NG_GPS_SEARCH_MS 56u
#define NG_GPS_SUMS 8u
#define NG_GPS_L1_HZ 1575420000.0

/** Private reusable workspace; one instance per strategy runner. */
typedef struct {
    size_t size;
    size_t nativeCapacity;
    TNgSearchFftMode fftMode;
    TNgSearchFft *fft;
    TNgComplex *baseband;
    int16_t *integerBaseband;
    double integerScale;
    unsigned integerCodePrn;
    TNgSearchFftMode integerCodeMode;
    TNgComplex *signal;
    TNgComplex *carrier;
    TNgComplex *work;
    TNgComplex *codeFft;
    TNgComplex *native;
    float *code;
    double *power;
    int8_t chips[32][NG_GPS_CA_LENGTH];
    uint64_t preparedFirst;
    uint64_t preparedEpoch;
    int prepared;
    double meanI;
    double meanQ;
    TNgAcquisitionSearchSpace space;
} TNgGpsSearch;

/** Worker-only copy, called after a successful search and before its publication. */
void NgAcquisitionGpsSearchSpace(void *context, TNgAcquisitionSearchSpace *space)
{
    *space = ((TNgGpsSearch *)context)->space;
}

/** Release all strategy-owned resources; accepts NULL and partial construction. */
void NgAcquisitionGpsFree(void *context)
{
    TNgGpsSearch *search = context;
    if (search == NULL) {
        return;
    }
    NgSearchFftDestroy(search->fft);
    free(search->baseband);
    free(search->integerBaseband);
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
int NgAcquisitionGpsWorkspace(const TNgStreamInfo *stream, TNgSearchFftMode mode, void **out,
                              TNgError *error)
{
    TNgGpsSearch *search;
    unsigned prn;
    size_t capacity;
    *out = NULL;
    if (stream == NULL || stream->sampleRateHz < 2048000 || stream->sampleRateHz > 20000000) {
        return NgFail(error, "GPS acquisition rate must be 2.048..20 Msps");
    }
    size_t size = stream->sampleRateHz >= 4096000 ? 4096 : 2048;
    if (NgAcquisitionValidateGrid("GPS L1 C/A", stream->sampleRateHz, 1, size, error) != NG_OK) {
        return NG_ERROR;
    }
    search = calloc(1, sizeof(*search));
    if (search == NULL) {
        return NgFail(error, "GPS workspace allocation failed");
    }
    search->size = size;
    search->fftMode = mode;
    search->nativeCapacity = (stream->sampleRateHz + 999u) / 1000u;
    capacity = search->nativeCapacity > search->size ? search->nativeCapacity : search->size;
    search->baseband = calloc(search->size * NG_GPS_SEARCH_MS, sizeof(TNgComplex));
    if (mode != NG_SEARCH_FFT_FLOAT) {
        search->integerBaseband = calloc(2 * search->size * NG_GPS_SEARCH_MS, sizeof(int16_t));
    }
    search->signal = calloc(search->size * NG_GPS_SUMS, sizeof(TNgComplex));
    search->carrier = calloc(capacity, sizeof(TNgComplex));
    if (mode == NG_SEARCH_FFT_FLOAT) {
        search->work = calloc(search->size, sizeof(TNgComplex));
        search->codeFft = calloc(32 * search->size, sizeof(TNgComplex));
    }
    search->native = calloc(capacity, sizeof(TNgComplex));
    search->code = calloc(capacity, sizeof(float));
    search->power = calloc(search->size, sizeof(double));
    if (!search->baseband || !search->signal || !search->carrier || !search->native ||
        !search->code || !search->power ||
        (mode == NG_SEARCH_FFT_FLOAT ? (!search->work || !search->codeFft)
                                     : !search->integerBaseband) ||
        NgSearchFftCreateForMode(search->size, mode, &search->fft, error) != NG_OK) {
        NgAcquisitionGpsFree(search);
        return NgFail(error, "GPS DSP workspace allocation failed");
    }
    for (prn = 1; prn <= 32; ++prn) {
        size_t k;
        (void)NgGpsCaGenerate(prn, search->chips[prn - 1], NULL);
        if (mode != NG_SEARCH_FFT_FLOAT) {
            continue;
        }
        TNgComplex *spectrum = search->codeFft + (prn - 1) * search->size;
        NgSearchCode(search->code, search->size, search->chips[prn - 1],
                     (double)search->size * 1000, NG_GPS_CA_RATE_HZ, 0);
        for (k = 0; k < search->size; ++k) {
            spectrum[k].i = search->code[k];
        }
        NgSearchFftExecute(search->fft, spectrum, 0);
    }
    *out = search;
    return NG_OK;
}

/** Install GEO L1 C/A replicas in a private two-PRN search workspace. */
int NgAcquisitionEgnosWorkspace(const TNgStreamInfo *stream, void **out, TNgError *error)
{
    if (NgAcquisitionGpsWorkspace(stream, NG_SEARCH_FFT_FLOAT, out, error) != NG_OK) {
        return NG_ERROR;
    }
    TNgGpsSearch *search = *out;
    for (unsigned slot = 0; slot < 2; ++slot) {
        NgEgnosCode(slot, search->chips[slot]);
        TNgComplex *spectrum = search->codeFft + slot * search->size;
        NgSearchCode(search->code, search->size, search->chips[slot], (double)search->size * 1000,
                     NG_GPS_CA_RATE_HZ, 0);
        for (size_t k = 0; k < search->size; ++k) {
            spectrum[k].i = search->code[k];
            spectrum[k].q = 0;
        }
        NgSearchFftExecute(search->fft, spectrum, 0);
    }
    return NG_OK;
}

/** Wipe actual digital IF before integrating native pairs onto a rational uniform grid.
 * Fixed box integration matches Classic compression; no component narrowing or FFT clipping.
 * Uses absolute rational boundaries, including SiGe's fractional samples per millisecond. */
static int Prepare(TNgGpsSearch *search, const TNgAcquisitionCapture *capture,
                   TNgAcquisitionCancel cancel, void *cancelContext)
{
    size_t k;
    size_t outputCount = search->size * NG_GPS_SEARCH_MS;
    double rate = (double)capture->stream.sampleRateHz;
    search->prepared = 0;
    search->meanI = 0;
    search->meanQ = 0;
    for (k = 0; k < capture->count; ++k) {
        search->meanI += capture->samples[k].i;
        search->meanQ += capture->samples[k].q;
    }
    search->meanI /= (double)capture->count;
    search->meanQ /= (double)capture->count;
    if (capture->stream.digitalIfHz == 0) {
        for (k = 0; k < outputCount; ++k) {
            size_t begin = (size_t)((uint64_t)k * capture->stream.sampleRateHz /
                                    ((uint64_t)search->size * 1000u));
            size_t end = (size_t)((uint64_t)(k + 1) * capture->stream.sampleRateHz /
                                  ((uint64_t)search->size * 1000u));
            size_t j;
            double i = 0;
            double q = 0;
            if ((k % search->size) == 0 && cancel(cancelContext)) {
                return NG_END;
            }
            for (j = begin; j < end; ++j) {
                i += capture->samples[j].i - search->meanI;
                q += (capture->samples[j].q - search->meanQ) *
                     (capture->stream.spectrumInverted ? -1 : 1);
            }
            search->baseband[k].i = (float)(i / (double)(end - begin));
            search->baseband[k].q = (float)(q / (double)(end - begin));
        }
    } else {
        unsigned ms;
        if (cancel(cancelContext)) {
            return NG_END;
        }
        for (ms = 0; ms < NG_GPS_SEARCH_MS; ++ms) {
            size_t begin = (size_t)((uint64_t)ms * capture->stream.sampleRateHz / 1000u);
            size_t end = (size_t)((uint64_t)(ms + 1) * capture->stream.sampleRateHz / 1000u);
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
                             ((uint64_t)search->size * 1000u)) -
                    begin;
                size_t last =
                    (size_t)((uint64_t)(ms * search->size + j + 1) * capture->stream.sampleRateHz /
                             ((uint64_t)search->size * 1000u)) -
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
            if (cancel(cancelContext)) {
                return NG_END;
            }
        }
    }
    if (search->fftMode != NG_SEARCH_FFT_FLOAT) {
        double maximum = 0;
        for (k = 0; k < outputCount; ++k) {
            maximum = fmax(maximum, fabs(search->baseband[k].i));
            maximum = fmax(maximum, fabs(search->baseband[k].q));
        }
        search->integerScale = 4096.0 / fmax(maximum, 1.0);
        for (k = 0; k < outputCount; ++k) {
            search->integerBaseband[2 * k] =
                (int16_t)lround(search->baseband[k].i * search->integerScale);
            search->integerBaseband[2 * k + 1] =
                (int16_t)lround(search->baseband[k].q * search->integerScale);
        }
    }
    search->integerCodePrn = 0;
    search->preparedFirst = capture->origin.firstSample;
    search->preparedEpoch = capture->origin.epoch;
    search->prepared = 1;
    return NG_OK;
}

/** Coarse PCPS power over eight one-ms coherent periods. */
static int Coarse(TNgGpsSearch *search, unsigned prn, double doppler, int square,
                  TNgSearchFftMode fftMode)
{
    unsigned ms;
    size_t k;
    if (fftMode != search->fftMode) {
        return NG_ERROR;
    }
    const TNgComplex *code =
        fftMode == NG_SEARCH_FFT_FLOAT ? search->codeFft + (prn - 1) * search->size : NULL;
    memset(search->power, 0, search->size * sizeof(*search->power));
    if (fftMode != NG_SEARCH_FFT_FLOAT &&
        (search->integerCodePrn != prn || search->integerCodeMode != fftMode)) {
        NgSearchCode(search->code, search->size, search->chips[prn - 1],
                     (double)search->size * 1000, NG_GPS_CA_RATE_HZ, 0);
        if (NgSearchIntegerCode(search->fft, search->code, fftMode, NULL) != NG_OK) {
            return NG_ERROR;
        }
        search->integerCodePrn = prn;
        search->integerCodeMode = fftMode;
    }
    NgSearchCarrier(search->carrier, search->size, (double)search->size * 1000, doppler, 0, square);
    for (ms = 0; ms < NG_GPS_SUMS; ++ms) {
        if (fftMode != NG_SEARCH_FFT_FLOAT) {
            if (NgSearchIntegerPcps(search->fft, search->integerBaseband + 2 * ms * search->size,
                                    search->carrier, search->integerScale, fftMode, search->power,
                                    NULL) != NG_OK) {
                return NG_ERROR;
            }
            continue;
        }
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
    return NG_OK;
}

/** Mix eight ms from the prepared grid for Doppler refinement, phase remains continuous. */
static void MixGrid(TNgGpsSearch *search, unsigned startMs, double doppler, int square)
{
    unsigned ms;
    for (ms = 0; ms < NG_GPS_SUMS; ++ms) {
        TNgComplex *output = search->signal + ms * search->size;
        memcpy(output, search->baseband + (startMs + ms) * search->size,
               search->size * sizeof(*output));
        NgSearchCarrier(search->carrier, search->size, (double)search->size * 1000, doppler,
                        (double)(startMs + ms) * doppler / 1000, square);
        NgSearchMix(output, search->carrier, search->size);
    }
}

/** Prompt power on grid, including physical Doppler's code-rate correction. */
static double GridPower(TNgGpsSearch *search, unsigned prn, unsigned startMs, double doppler,
                        double phase, int xorSigns)
{
    double power = 0;
    double codeRate = NG_GPS_CA_RATE_HZ * (1 + doppler / NG_GPS_L1_HZ);
    unsigned ms;
    for (ms = 0; ms < NG_GPS_SUMS; ++ms) {
        double i;
        double q;
        NgSearchCode(search->code, search->size, search->chips[prn - 1],
                     (double)search->size * 1000, codeRate,
                     phase + (startMs + ms) * codeRate / 1000);
        NgSearchCorrelate(search->signal + ms * search->size, search->code, search->size, xorSigns,
                          &i, &q);
        power += i * i + q * q;
    }
    return power;
}

/** Native-rate coherent prompt; carrier and code share exact capture-relative sample origin. */
static double NativePower(TNgGpsSearch *search, const TNgAcquisitionCapture *capture, unsigned prn,
                          unsigned startMs, double doppler, double phase,
                          const TNgAcquisitionConfig *config, double *noise, double carrierOffset)
{
    double power = 0;
    double rate = (double)capture->stream.sampleRateHz;
    double frequency = (double)capture->stream.digitalIfHz + doppler + carrierOffset;
    double codeRate = NG_GPS_CA_RATE_HZ * (1 + doppler / NG_GPS_L1_HZ);
    unsigned ms;
    *noise = 0;
    for (ms = startMs; ms < startMs + NG_GPS_SUMS; ++ms) {
        size_t begin = (size_t)((uint64_t)ms * capture->stream.sampleRateHz / 1000u);
        size_t end = (size_t)((uint64_t)(ms + 1) * capture->stream.sampleRateHz / 1000u);
        size_t count = end - begin;
        size_t k;
        double i;
        double q;
        double energy = 0;
        NgSearchCarrier(search->carrier, count, rate, frequency, (double)begin * frequency / rate,
                        config->squareCarrier);
        NgSearchCode(search->code, count, search->chips[prn - 1], rate, codeRate,
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
        *noise += energy / NG_GPS_SUMS;
    }
    return power;
}

/** Search one GPS satellite; fresh data for each stage, no tracking state fabrication. */
static int Search(void *context, const TNgAcquisitionCapture *capture, unsigned prn,
                  const TNgAcquisitionConfig *config, TNgAcquisitionCancel cancel,
                  void *cancelContext, TNgAcquisitionResult *result, TNgError *error)
{
    TNgGpsSearch *search = context;
    double noise = 0;
    double threshold;
    double best = 0;
    double doppler = 0;
    double phase = 0;
    double factor = AcquisitionThreshold(NG_GPS_SUMS, config->falseAlarmProbability);
    size_t k;
    unsigned bin;
    (void)error;
    memset(result, 0, sizeof(*result));
    if (capture->displayPrn == prn) {
        memset(&search->space, 0, sizeof(search->space));
    } else {
        search->space.valid = 0;
    }
    result->prn = prn;
    result->epoch = capture->origin.epoch;
    result->firstSample = capture->origin.firstSample;
    if (!search->prepared || search->preparedFirst != capture->origin.firstSample ||
        search->preparedEpoch != capture->origin.epoch) {
        if (Prepare(search, capture, cancel, cancelContext) != NG_OK) {
            return NG_END;
        }
    }
    /* Classic's fake-Doppler population estimates the complex correlation variance.
     * The sum's mean is 8 * (2*variance); the gamma threshold uses single-period power. */
    if (Coarse(search, prn, AcquisitionCoarseNoiseDoppler(), config->squareCarrier,
               config->fftMode) != NG_OK) {
        return NgFail(error, "GPS integer coarse FFT failed");
    }
    for (k = 0; k < search->size; ++k) {
        noise += search->power[k];
    }
    noise /= (double)(search->size * NG_GPS_SUMS);
    threshold = noise * factor;
    if (!(threshold > 0)) {
        return NG_OK;
    }
    /* Same 667 Hz grid as Classic; explicitly cover both search edges. */
    for (bin = 0; bin <= 15; ++bin) {
        double frequency = bin == 15 ? 5000 : -5000 + (double)bin * 667;
        if (cancel(cancelContext)) {
            return NG_END;
        }
        if (Coarse(search, prn, frequency, config->squareCarrier, config->fftMode) != NG_OK) {
            return NgFail(error, "GPS integer coarse FFT failed");
        }
        for (k = 0; k < search->size; ++k) {
            if (capture->displayPrn == prn) {
                size_t column =
                    ((search->size - k) % search->size) * NG_ACQUISITION_CODE_BINS / search->size;
                float value = (float)(search->power[k] / threshold);
                if (value > search->space.power[bin][column]) {
                    search->space.power[bin][column] = value;
                }
            }
            if (search->power[k] > best) {
                best = search->power[k];
                doppler = frequency;
                phase = -(double)k * NG_GPS_CA_LENGTH / (double)search->size;
            }
        }
    }
    result->peakRatio = best / threshold;
    result->dopplerHz = doppler;
    result->codePhaseChips = fmod(phase + NG_GPS_CA_LENGTH, NG_GPS_CA_LENGTH);
    search->space.dopplerBins = 16;
    search->space.codePeriodChips = 1023;
    search->space.valid = capture->displayPrn == prn;
    search->space.fftSize = (unsigned)search->size;
    search->space.threshold = threshold;
    search->space.peakDopplerHz = doppler;
    search->space.peakCodePhaseChips = result->codePhaseChips;
    if (best <= threshold) {
        return NG_OK;
    }
    /* Doppler refinement: +/-667 Hz, 10 Hz step, eight noncoherent periods. */
    {
        double coarseDoppler = doppler;
        best = 0;
        for (bin = 0; bin <= 134; ++bin) {
            double frequency = coarseDoppler - 667 + (bin == 134 ? 1334 : (double)bin * 10);
            double power;
            if (cancel(cancelContext)) {
                return NG_END;
            }
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
        int radius = (int)ceil((double)capture->stream.sampleRateHz / NG_GPS_CA_RATE_HZ);
        double coarsePhase = phase;
        best = 0;
        for (delta = -radius; delta <= radius; ++delta) {
            double test = coarsePhase +
                          (double)delta * NG_GPS_CA_RATE_HZ / (double)capture->stream.sampleRateHz;
            double power;
            if (cancel(cancelContext)) {
                return NG_END;
            }
            power = NativePower(search, capture, prn, 16, doppler, test, config, &noise, 0);
            if (power > best) {
                best = power;
                phase = test;
            }
        }
    }
    result->dopplerHz = doppler;
    result->codePhaseChips =
        fmod(fmod(phase, NG_GPS_CA_LENGTH) + NG_GPS_CA_LENGTH, NG_GPS_CA_LENGTH);
    /* Four fresh, disjoint intervals. Noise probes retain the candidate replica
     * and code rate, but rotate at deliberately wrong carriers outside the search.
     * Integer-kHz offsets reject coherent one-ms candidate prompts. No ADC narrowing,
     * off-code estimator, historical threshold, or coarse-gain fallback is used. */
    for (bin = 0; bin < 4; ++bin) {
        double power;
        double referenceNoise = 0;
        unsigned reference;
        unsigned startMs = 24 + bin * NG_GPS_SUMS;
        if (cancel(cancelContext)) {
            return NG_END;
        }
        power = NativePower(search, capture, prn, startMs, doppler, phase, config, &noise, 0);
        for (reference = 0; reference < 8; ++reference) {
            double offset = AcquisitionNoiseOffset(reference);
            if (cancel(cancelContext)) {
                return NG_END;
            }
            referenceNoise +=
                NativePower(search, capture, prn, startMs, doppler, phase, config, &noise, offset);
        }
        referenceNoise /= (double)(8 * NG_GPS_SUMS);
        if (referenceNoise > 0 && power > referenceNoise * factor) {
            result->successes++;
        }
        if (result->successes >= 3) {
            result->confirmed = 1;
            break;
        }
        if (result->successes + 3 - bin < 3) {
            break;
        }
    }
    return NG_OK;
}

/** Public strategy interface; no receiver/controller dependency. */
/** Build a confirmed satellite's archive after publishing its seed to tracking. */
int NgAcquisitionGpsDisplay(void *context, unsigned prn, const TNgAcquisitionConfig *config,
                            TNgAcquisitionCancel cancel, void *cancelContext)
{
    TNgGpsSearch *search = context;
    unsigned bin;
    size_t k;
    memset(search->space.power, 0, sizeof(search->space.power));
    search->space.valid = 0;
    if (!(search->space.threshold > 0)) {
        return NG_END;
    }
    for (bin = 0; bin < 16; ++bin) {
        double frequency = bin == 15 ? 5000 : -5000 + (double)bin * 667;
        if (cancel(cancelContext)) {
            return NG_END;
        }
        if (Coarse(search, prn, frequency, config->squareCarrier, config->fftMode) != NG_OK) {
            return NG_END;
        }
        for (k = 0; k < search->size; ++k) {
            size_t column =
                ((search->size - k) % search->size) * NG_ACQUISITION_CODE_BINS / search->size;
            float value = (float)(search->power[k] / search->space.threshold);
            if (value > search->space.power[bin][column]) {
                search->space.power[bin][column] = value;
            }
        }
    }
    search->space.valid = 1;
    return NG_OK;
}

const TNgAcquisitionStrategy ngGpsAcquisitionStrategy = {Search};
