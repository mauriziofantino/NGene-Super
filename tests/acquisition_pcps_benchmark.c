/** @file acquisition_pcps_benchmark.c @brief Equal-work Galileo PCPS and strategy measurements. */
#include "ngene/acquisition.h"
#include "acquisition/workspace.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PRN = 7, PERIODS = 8, CAPTURE_PERIODS = 56, WARMUPS = 5, REPEATS = 24 };

static const double tau = 6.283185307179586;
static const double dopplerHz = 1264;
static const uint32_t seed = 20261121u;
static const TNgSearchFftMode modes[] = {NG_SEARCH_FFT_FLOAT, NG_SEARCH_FFT_INTEGER_SCALED,
                                         NG_SEARCH_FFT_INTEGER_PRECISE};
static const char *modeNames[] = {"float", "q14", "int32"};

typedef struct {
    size_t n;
    double inputScale;
    TNgComplex *samples;
    TNgComplex *baseband;
    TNgComplex *carrier;
    TNgComplex *spectrum;
    TNgComplex *work;
    int16_t *integerBaseband;
    float *code;
    double *reference;
    double *power;
    TNgSearchFft *plan;
    void *workspaces[3];
} TPcpsFixture;

static int Fail(const char *message)
{
    fprintf(stderr, "PCPS benchmark: %s\n", message);
    return 1;
}

static double SampleNoise(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return (double)(*state >> 8) / 16777216.0 - 0.5;
}

static int InitFixture(TPcpsFixture *b, size_t n, const int8_t *chips)
{
    uint32_t random = seed;
    uint32_t hash = 2166136261u;
    double meanI = 0, meanQ = 0, maximum = 1;
    size_t count = CAPTURE_PERIODS * n;
    size_t shift = 247 * n / 8192;
    b->n = n;
    b->samples = calloc(count, sizeof(*b->samples));
    b->baseband = calloc(PERIODS * n, sizeof(*b->baseband));
    b->carrier = calloc(n, sizeof(*b->carrier));
    b->spectrum = calloc(n, sizeof(*b->spectrum));
    b->work = calloc(n, sizeof(*b->work));
    b->integerBaseband = calloc(2 * PERIODS * n, sizeof(*b->integerBaseband));
    b->code = calloc(2 * n, sizeof(*b->code));
    b->reference = calloc(n, sizeof(*b->reference));
    b->power = calloc(n, sizeof(*b->power));
    if (!b->samples || !b->baseband || !b->carrier || !b->spectrum || !b->work ||
        !b->integerBaseband || !b->code || !b->reference || !b->power ||
        NgSearchFftCreate(n, &b->plan, NULL) != NG_OK)
        return Fail("fixture allocation failed");

    for (size_t k = 0; k < n; ++k) {
        size_t halfChip = k * 8184 / n;
        b->code[n + k] = b->code[k] = (float)(chips[halfChip / 2] * (halfChip % 2 ? -1 : 1));
    }
    for (size_t k = 0; k < count; ++k) {
        double phase = tau * dopplerHz * (double)k / ((double)n * 250) + 0.71;
        double signal = 12 * b->code[(k + n - shift) % n];
        int i = (int)lround(signal * cos(phase) + 28 * SampleNoise(&random) + 2);
        int q = (int)lround(signal * sin(phase) + 28 * SampleNoise(&random) - 2);
        meanI += i;
        meanQ += q;
        b->samples[k].i = (float)i;
        b->samples[k].q = (float)q;
        hash = (hash ^ (uint8_t)i) * 16777619u;
        hash = (hash ^ (uint8_t)q) * 16777619u;
    }
    meanI /= (double)count;
    meanQ /= (double)count;
    for (size_t k = 0; k < count; ++k) {
        double i = fabs((float)(b->samples[k].i - meanI));
        double q = fabs((float)(b->samples[k].q - meanQ));
        maximum = fmax(maximum, fmax(i, q));
    }
    b->inputScale = 4096 / maximum;
    for (size_t k = 0; k < PERIODS * n; ++k) {
        b->baseband[k].i = (float)(b->samples[k].i - meanI);
        b->baseband[k].q = (float)(b->samples[k].q - meanQ);
        b->integerBaseband[2 * k] = (int16_t)lround(b->baseband[k].i * b->inputScale);
        b->integerBaseband[2 * k + 1] = (int16_t)lround(b->baseband[k].q * b->inputScale);
    }
    printf("fixture,%zu,%u,%u,%zu,%08lx,%.12g\n", n, PRN, seed, count, (unsigned long)hash,
           b->inputScale);
    return 0;
}

static void FreeFixture(TPcpsFixture *b)
{
    for (unsigned m = 0; m < 3; ++m)
        NgAcquisitionGalileoFree(b->workspaces[m]);
    NgSearchFftDestroy(b->plan);
    free(b->samples);
    free(b->baseband);
    free(b->carrier);
    free(b->spectrum);
    free(b->work);
    free(b->integerBaseband);
    free(b->code);
    free(b->reference);
    free(b->power);
}

static int PrepareCode(TPcpsFixture *b, TNgSearchFftMode mode)
{
    if (mode != NG_SEARCH_FFT_FLOAT)
        return NgSearchIntegerCode(b->plan, b->code, mode, NULL);
    for (size_t k = 0; k < b->n; ++k) {
        b->spectrum[k].i = b->code[k];
        b->spectrum[k].q = 0;
    }
    NgSearchFftExecute(b->plan, b->spectrum, 0);
    return NG_OK;
}

static int Pcps(TPcpsFixture *b, TNgSearchFftMode mode)
{
    size_t n = b->n;
    memset(b->power, 0, n * sizeof(*b->power));
    for (size_t period = 0; period < PERIODS; ++period) {
        if (mode != NG_SEARCH_FFT_FLOAT) {
            if (NgSearchIntegerPcps(b->plan, b->integerBaseband + 2 * n * period, b->carrier,
                                    b->inputScale, mode, b->power, NULL) != NG_OK)
                return NG_ERROR;
            continue;
        }
        memcpy(b->work, b->baseband + n * period, n * sizeof(*b->work));
        NgSearchMix(b->work, b->carrier, n);
        NgSearchFftExecute(b->plan, b->work, 0);
        for (size_t k = 0; k < n; ++k) {
            TNgComplex a = b->work[k], c = b->spectrum[k];
            b->work[k].i = a.i * c.i + a.q * c.q;
            b->work[k].q = a.q * c.i - a.i * c.q;
        }
        NgSearchFftExecute(b->plan, b->work, 1);
        for (size_t k = 0; k < n; ++k) {
            double i = b->work[k].i, q = b->work[k].q;
            b->power[k] += i * i + q * q;
        }
    }
    return NG_OK;
}

/** Direct circular correlation in double, with independent trigonometric carrier. */
static int Reference(TPcpsFixture *b, double frequency)
{
    size_t n = b->n;
    double *i = calloc(n, sizeof(*i)), *q = calloc(n, sizeof(*q));
    if (!i || !q) {
        free(i);
        free(q);
        return Fail("oracle allocation failed");
    }
    memset(b->reference, 0, n * sizeof(*b->reference));
    for (size_t period = 0; period < PERIODS; ++period) {
        for (size_t k = 0; k < n; ++k) {
            double phase = -tau * frequency * (double)k / ((double)n * 250);
            TNgComplex x = b->baseband[period * n + k];
            i[k] = x.i * cos(phase) - x.q * sin(phase);
            q[k] = x.i * sin(phase) + x.q * cos(phase);
        }
        for (size_t lag = 0; lag < n; ++lag) {
            const float *code = b->code + n - lag;
            double real = 0, imag = 0;
            for (size_t k = 0; k < n; ++k) {
                real += i[k] * code[k];
                imag += q[k] * code[k];
            }
            b->reference[lag] += real * real + imag * imag;
        }
    }
    free(i);
    free(q);
    return 0;
}

static int VerifyMaps(TPcpsFixture *b, double frequency)
{
    const double limits[] = {0.0001, 0.05, 0.002};
    size_t expectedPeak = 0;
    double referenceNorm = 0, referenceSum = 0;
    if (Reference(b, frequency))
        return 1;
    for (size_t k = 0; k < b->n; ++k) {
        if (b->reference[k] > b->reference[expectedPeak])
            expectedPeak = k;
        referenceSum += b->reference[k];
        referenceNorm += b->reference[k] * b->reference[k];
    }
    if (frequency == dopplerHz && expectedPeak != 247 * b->n / 8192)
        return Fail("oracle peak differs from injected lag");
    NgSearchCarrier(b->carrier, b->n, (double)b->n * 250.0, frequency, 0, 0);
    for (size_t m = 0; m < sizeof(modes) / sizeof(modes[0]); ++m) {
        size_t peak = 0;
        double errorNorm = 0, maxError = 0, sum = 0;
        if (PrepareCode(b, modes[m]) != NG_OK || Pcps(b, modes[m]) != NG_OK)
            return Fail("PCPS computation failed");
        for (size_t k = 0; k < b->n; ++k) {
            double error = b->power[k] - b->reference[k];
            if (!isfinite(b->power[k]) || b->power[k] < 0)
                return Fail("invalid map power");
            if (b->power[k] > b->power[peak])
                peak = k;
            sum += b->power[k];
            errorNorm += error * error;
            maxError = fmax(maxError, fabs(error));
        }
        double relativeL2 = sqrt(errorNorm / referenceNorm);
        double maxErrorOverPeak = maxError / b->reference[expectedPeak];
        int withinTolerance =
            relativeL2 <= limits[m] && maxErrorOverPeak <= limits[m] && peak == expectedPeak;
        printf("map,%zu,%s,%.0f,%zu,%zu,%.12g,%.12g,%.9g,%.9g,%.9g,%.9g,%s\n", b->n, modeNames[m],
               frequency, expectedPeak, peak, b->reference[expectedPeak], b->power[peak],
               sqrt(b->power[expectedPeak] / b->reference[expectedPeak]), sum / referenceSum,
               relativeL2, maxErrorOverPeak, withinTolerance ? "within" : "outside");
        /* Experimental tolerance misses are evidence. Still fail the test for
           a broken reference path, gross scaling errors or a lost signal peak. */
        if ((modes[m] == NG_SEARCH_FFT_FLOAT && !withinTolerance) || relativeL2 > 0.5 ||
            maxErrorOverPeak > 0.5 || (frequency == dopplerHz && peak != expectedPeak))
            return Fail("map failed the reference contract");
    }
    return 0;
}

/** IF=0: cold Prepare calls the predicate once per period, then Search calls it
 * at the first coarse-row boundary after the wrong-Doppler noise measurement.
 * With an already prepared capture only that last check runs. */
static int CancelAtCoarse(void *context)
{
    unsigned *checks = context;
    return --*checks == 0;
}

static int NeverCancel(void *context)
{
    (void)context;
    return 0;
}

static int Measure(TPcpsFixture *b)
{
    TNgAcquisitionCapture capture = {0};
    TNgAcquisitionConfig config;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    capture.samples = b->samples;
    capture.count = CAPTURE_PERIODS * b->n;
    capture.stream.sampleRateHz = (uint32_t)(b->n * 250);
    capture.stream.format = NG_FORMAT_CI8;
    NgAcquisitionDefaults(&config);
    config.firstPrn = config.lastPrn = PRN;
    for (unsigned m = 0; m < 3; ++m)
        if (NgAcquisitionGalileoWorkspace(&capture.stream, (unsigned)b->n, modes[m],
                                          &b->workspaces[m], &error) != NG_OK)
            return Fail(error.message);
    NgSearchCarrier(b->carrier, b->n, (double)b->n * 250.0, dopplerHz, 0, 0);

    for (unsigned trial = 0; trial < WARMUPS + REPEATS; ++trial) {
        for (unsigned position = 0; position < 3; ++position) {
            /* All six mode orders, equally represented in timed trials. */
            unsigned order = (trial + 6 - WARMUPS) % 6;
            unsigned m = (order / 2 + (order % 2 ? 3 - position : position)) % 3;
            void *workspace = b->workspaces[m];
            unsigned checks = CAPTURE_PERIODS + 1;
            double start = NgMonotonicSeconds();
            if (PrepareCode(b, modes[m]) != NG_OK)
                return Fail("code preparation failed");
            double codeTime = NgMonotonicSeconds() - start;
            start = NgMonotonicSeconds();
            if (Pcps(b, modes[m]) != NG_OK)
                return Fail("timed PCPS failed");
            double pcpsTime = NgMonotonicSeconds() - start;

            config.fftMode = modes[m];
            capture.origin.epoch++;
            start = NgMonotonicSeconds();
            int rc = ngGalileoAcquisitionStrategy.search(workspace, &capture, PRN, &config,
                                                         CancelAtCoarse, &checks, &result, &error);
            double cold = NgMonotonicSeconds() - start;
            if (rc != NG_END || checks != 0)
                return Fail("cold search did not stop at the first Doppler row");
            checks = 1;
            start = NgMonotonicSeconds();
            rc = ngGalileoAcquisitionStrategy.search(workspace, &capture, PRN, &config,
                                                     CancelAtCoarse, &checks, &result, &error);
            double warm = NgMonotonicSeconds() - start;
            if (rc != NG_END || checks != 0)
                return Fail("warm search did not stop at the first Doppler row");

            capture.origin.epoch++;
            start = NgMonotonicSeconds();
            rc = ngGalileoAcquisitionStrategy.search(workspace, &capture, PRN, &config, NeverCancel,
                                                     NULL, &result, &error);
            double full = NgMonotonicSeconds() - start;
            double expectedPhase = 4092 * (1 - 247.0 / 8192);
            if (rc != NG_OK || result.prn != PRN || !result.confirmed || result.successes != 3 ||
                fabs(result.dopplerHz - dopplerHz) > 25 ||
                fabs(result.codePhaseChips - expectedPhase) > 0.6)
                return Fail("full strategy did not reproduce the injected satellite");
            if (trial >= WARMUPS)
                printf("time,%zu,%u,%u,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.3f,%.6f,%u\n", b->n,
                       trial - WARMUPS + 1, position + 1, modeNames[m], codeTime, pcpsTime, cold,
                       warm, full, result.dopplerHz, result.codePhaseChips, result.successes);
        }
    }
    return 0;
}

int BenchmarkIntegerPcps(int measure)
{
    int8_t chips[4092];
    FILE *file = fopen(NGENE_TEST_SOURCE_DIR "/tests/data/galileo-e1b-prn7.bin", "rb");
    if (!file)
        return Fail("cannot open PRN 7 fixture");
    size_t length = fread(chips, 1, sizeof(chips), file);
    fclose(file);
    if (length != sizeof(chips))
        return Fail("incomplete PRN 7 fixture");

    puts("fixture,n,prn,seed,capture_pairs,ci8_fnv1a,integer_input_scale");
    puts("map,n,mode,frequency_hz,reference_lag,lag,reference_peak,power_peak,"
         "amplitude_ratio_at_reference_lag,total_power_ratio,relative_l2,max_error_over_peak,"
         "exploratory_tolerance");
    puts("time,n,trial,order,mode,code_s,pcps_8periods_s,cold_prefix_s,warm_prefix_s,"
         "full_search_s,doppler_hz,code_phase_chips,successes");
    const size_t sizes[] = {8192, 16384};
    TPcpsFixture fixtures[sizeof(sizes) / sizeof(sizes[0])] = {0};
    size_t count = measure ? sizeof(sizes) / sizeof(sizes[0]) : 1;
    int rc = 0;
    for (size_t k = 0; k < count && !rc; ++k)
        rc = InitFixture(&fixtures[k], sizes[k], chips) || VerifyMaps(&fixtures[k], dopplerHz) ||
             VerifyMaps(&fixtures[k], 22000);
    for (size_t k = 0; k < count && measure && !rc; ++k)
        rc = Measure(&fixtures[k]);
    for (size_t k = 0; k < count; ++k)
        FreeFixture(&fixtures[k]);
    return rc;
}
