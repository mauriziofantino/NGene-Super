/** @file test_acquisition.c @brief Independent GPS signal and incremental acquisition contracts. */
#include "ngene/acquisition.h"
#include "ngene/acquisition_receiver.h"
#include "ngene/config.h"
#include "ngene/dsp/acquisition_math.h"
#include "ngene/dsp/gps_ca.h"
#include "ngene/dsp/nco.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression)                                                                         \
    do {                                                                                          \
        if (!(expression)) {                                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression);                 \
            exit(1);                                                                              \
        }                                                                                         \
    } while (0)

#define TEST_PI 3.14159265358979323846264338327950288
#define GPS_CHIPS 1023u

/** The first (BPSK) 1023 bytes of each PRN in original NGene GPS_codes.dat.
 * FNV-1a hashes preserve byte polarity and the complete chip ordering, not only
 * an autocorrelation property shared by incorrect or inverted generators.
 */
static const uint32_t legacyCodeHash[32] = {
    0xa16745e0u, 0xa3e53700u, 0xb41e7fe0u, 0x94c4b1a8u,
    0x90a009d8u, 0x2bab94f0u, 0x3c3d6fa0u, 0x1c526ddcu,
    0x40d270b0u, 0x828b5138u, 0x889f951cu, 0xc927eb5cu,
    0x46334b18u, 0xf0ca333cu, 0xbd496790u, 0xae72c694u,
    0x2a0e4b50u, 0x7ccc4b74u, 0xa49a9920u, 0x88f48a34u,
    0xda04ae30u, 0x24bd08ecu, 0x06211544u, 0xa554a04cu,
    0x038fecd4u, 0x5f73213cu, 0x05f07ebcu, 0x057c3facu,
    0x8a02ba44u, 0x72e697c0u, 0x2960f044u, 0x4311e5b8u
};

/** Test-side ten-stage registers, independently implemented from DSP tables. */
static void ReferenceCode(unsigned prn, int8_t code[GPS_CHIPS])
{
    static const unsigned taps[32][2] = {
        {2, 6}, {3, 7}, {4, 8}, {5, 9}, {1, 9}, {2, 10}, {1, 8}, {2, 9},
        {3, 10}, {2, 3}, {3, 4}, {5, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 10},
        {1, 4}, {2, 5}, {3, 6}, {4, 7}, {5, 8}, {6, 9}, {1, 3}, {4, 6},
        {5, 7}, {6, 8}, {7, 9}, {8, 10}, {1, 6}, {2, 7}, {3, 8}, {4, 9}
    };
    unsigned g1[10], g2[10], n, j;
    CHECK(prn >= 1 && prn <= 32);
    for (j = 0; j < 10; ++j) g1[j] = g2[j] = 1;
    for (n = 0; n < GPS_CHIPS; ++n) {
        unsigned feedback1 = g1[2] ^ g1[9];
        unsigned feedback2 = g2[1] ^ g2[2] ^ g2[5] ^ g2[7] ^ g2[8] ^ g2[9];
        code[n] = (g1[9] ^ g2[taps[prn - 1][0] - 1] ^ g2[taps[prn - 1][1] - 1]) ? 1 : -1;
        for (j = 9; j > 0; --j) {
            g1[j] = g1[j - 1];
            g2[j] = g2[j - 1];
        }
        g1[0] = feedback1;
        g2[0] = feedback2;
    }
}

static uint32_t CodeHash(const int8_t code[GPS_CHIPS])
{
    uint32_t hash = UINT32_C(2166136261);
    unsigned n;
    for (n = 0; n < GPS_CHIPS; ++n) {
        hash ^= (uint8_t)code[n];
        hash *= UINT32_C(16777619);
    }
    return hash;
}

/** Fixed seed and Box-Muller provide reproducible complex AWGN without libc rand. */
static double Uniform(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return ((double)*state + 0.5) / 4294967296.0;
}

typedef struct {
    TNgStreamInfo stream;
    void *samples;
    size_t count;
} TSignal;

/** Generate an independently sampled complex GPS BPSK signal in native ADC units. */
static TSignal SignalCreate(uint32_t rate, TNgSampleFormat format, unsigned prn,
                            double delaySamples, double dopplerHz, int64_t digitalIfHz,
                            double amplitude, double noiseSigma, unsigned milliseconds)
{
    TSignal signal;
    int8_t code[GPS_CHIPS];
    size_t n;
    uint32_t randomState = UINT32_C(0x13579bdf);
    memset(&signal, 0, sizeof(signal));
    ReferenceCode(prn, code);
    signal.stream.sampleRateHz = rate;
    signal.stream.format = format;
    signal.stream.centerFrequencyHz = UINT64_C(1575420000) - digitalIfHz;
    signal.stream.signalFrequencyHz = UINT64_C(1575420000);
    signal.stream.digitalIfHz = digitalIfHz;
    signal.stream.sourceName = "deterministic GPS acquisition fixture";
    signal.stream.clockDomain = "test sample clock";
    signal.count = (size_t)(((uint64_t)rate * milliseconds + 999) / 1000);
    signal.samples = malloc(signal.count * NgSampleBytes(format));
    CHECK(signal.samples != NULL);
    for (n = 0; n < signal.count; ++n) {
        double chip = fmod(((double)n - delaySamples) * 1023000.0 / rate, 1023.0);
        double angle = 2.0 * TEST_PI * (digitalIfHz + dopplerHz) * (double)n / rate + 0.37;
        double radius = sqrt(-2.0 * log(Uniform(&randomState))) * noiseSigma;
        double noisePhase = 2.0 * TEST_PI * Uniform(&randomState);
        double signedAmplitude;
        int i, q, low = format == NG_FORMAT_SC16_Q11 ? -2048 : -128;
        int high = format == NG_FORMAT_SC16_Q11 ? 2047 : 127;
        if (chip < 0.0) chip += 1023.0;
        signedAmplitude = amplitude * code[(unsigned)floor(chip)];
        i = (int)floor(signedAmplitude * cos(angle) + radius * cos(noisePhase) + 0.5);
        q = (int)floor(signedAmplitude * sin(angle) + radius * sin(noisePhase) + 0.5);
        if (i < low) i = low;
        if (i > high) i = high;
        if (q < low) q = low;
        if (q > high) q = high;
        NgIqStore(signal.samples, format, n, 0, i);
        NgIqStore(signal.samples, format, n, 1, q);
    }
    return signal;
}

static void SignalDestroy(TSignal *signal)
{
    free(signal->samples);
    signal->samples = NULL;
}

static TNgAcquisitionConfig TestConfig(unsigned prn)
{
    TNgAcquisitionConfig config;
    NgAcquisitionDefaults(&config);
    config.enabled = 1;
    config.gpsPrnMask = UINT32_C(1) << (prn - 1);
    config.gpsChannels = 1;
    config.fftSize = 1024;
    config.dopplerMaxHz = 1000.0;
    config.dopplerStepHz = 500.0;
    config.codeStepChips = 0.5;
    config.falseAlarmProbability = 1e-5;
    config.nonCoherentSums = 1;
    config.confirmationAttempts = 4;
    config.confirmationSuccesses = 3;
    config.refinementDopplerSpanHz = 500.0;
    config.refinementDopplerStepHz = 50.0;
    config.refinementCodeSpanChips = 1.0;
    config.refinementCodeStepSamples = 1;
    return config;
}

/** Feed actual stream metadata; no call retains the temporary view. */
static void ProcessRange(TNgAcquisition *context, const TSignal *signal, size_t first, size_t count,
                         uint64_t epoch, unsigned flags, TNgAcquisitionResult *result)
{
    TNgIqView view;
    TNgError error = {{0}};
    memset(&view, 0, sizeof(view));
    CHECK(first <= signal->count && count <= signal->count - first);
    view.samples = (const TNgIq8 *)((const unsigned char *)signal->samples +
                                  first * NgSampleBytes(signal->stream.format));
    view.format = signal->stream.format;
    view.meta.epoch = epoch;
    view.meta.firstSample = first;
    view.meta.count = count;
    view.meta.flags = flags;
    CHECK(NgAcquisitionProcess(context, &view, result, &error) == NG_OK);
}

static TNgAcquisitionResult RunChunksWithSums(const TNgAcquisitionPlan *plan, const TSignal *signal,
                                              unsigned prn, const size_t *chunks, size_t chunkCount,
                                              unsigned nonCoherentSums)
{
    TNgAcquisition *context = NULL;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    size_t first = 0, chunk = 0;
    unsigned seenPhases;
    TNgAcquisitionPhase previous;
    CHECK(NgAcquisitionCreate(plan, &context, &error) == NG_OK);
    CHECK(NgAcquisitionAssign(context, prn, nonCoherentSums, &error) == NG_OK);
    NgAcquisitionSnapshot(context, &result);
    previous = result.phase;
    seenPhases = 1u << result.phase;
    while (first < signal->count) {
        size_t count = chunks[chunk++ % chunkCount];
        if (count > signal->count - first) count = signal->count - first;
        ProcessRange(context, signal, first, count, 7, 0, &result);
        seenPhases |= 1u << result.phase;
        if (getenv("NGENE_ACQ_TEST_TRACE") && previous != result.phase)
            fprintf(stderr, "PRN %u Fs %u phase %s Doppler %g delay %g metric %g threshold %g\n",
                    prn, signal->stream.sampleRateHz, NgAcquisitionPhaseName(result.phase),
                    result.dopplerHz, result.codeDelaySamples, result.metric, result.threshold);
        previous = result.phase;
        first += count;
    }
    NgAcquisitionSnapshot(context, &result);
    if (result.phase == NG_ACQ_ACQUIRED && chunkCount == 1 &&
        chunks[0] <= signal->stream.sampleRateHz / 1000) {
        unsigned required = (1u << NG_ACQ_COARSE_NOISE) | (1u << NG_ACQ_COARSE) |
            (1u << NG_ACQ_DOPPLER_REFINEMENT) | (1u << NG_ACQ_CODE_REFINEMENT) |
            (result.confirmation == NG_ACQ_CONFIRM_NONE ? 0 : (1u << NG_ACQ_CONFIRMATION)) | (1u << NG_ACQ_ACQUIRED);
        CHECK((seenPhases & required) == required);
        if (result.confirmation == NG_ACQ_CONFIRM_NONE) CHECK(!(seenPhases & (1u << NG_ACQ_CONFIRMATION)));
    }
    NgAcquisitionDestroy(context);
    return result;
}

static TNgAcquisitionResult RunChunks(const TNgAcquisitionPlan *plan, const TSignal *signal,
                                      unsigned prn, const size_t *chunks, size_t chunkCount)
{
    return RunChunksWithSums(plan, signal, prn, chunks, chunkCount, 1);
}

static double CircularDistance(double a, double b, double period)
{
    double difference = fabs(a - b);
    difference = fmod(difference, period);
    return difference > period / 2.0 ? period - difference : difference;
}

/** The display matrix must contain measured coarse powers at the right delay
 * cells, rather than a reconstructed peak-shaped image. Compare its first
 * Doppler row against two independent direct DSP calls (both compression shifts).
 */
static void TestMeasuredCafCells(void)
{
    TSignal signal = SignalCreate(2046000, NG_FORMAT_CI8, 3, 509.0, 350.0, 24000,
                                  35.0, 6.0, 20);
    TNgAcquisitionConfig config = TestConfig(3);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisition *context = NULL;
    TNgAcqMathPlan *math = NULL;
    TNgAcqMathWorkspace *workspace = NULL;
    TNgAcquisitionResult result;
    TNgAcquisitionCaf caf;
    TNgError error = {{0}};
    double power[1024];
    float expected[NG_ACQ_CAF_MAX_BINS];
    size_t frame, index, column;
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    CHECK(NgAcquisitionCreate(plan, &context, &error) == NG_OK);
    CHECK(NgAcquisitionAssign(context, 3, 1, &error) == NG_OK);
    CHECK(NgAcqMathCreate(signal.stream.sampleRateHz, 1024, config.gpsPrnMask,
                          &math, &error) == NG_OK);
    CHECK(NgAcqMathWorkspaceCreate(math, &workspace, &error) == NG_OK);
    NgAcquisitionCafSnapshot(context, &caf);
    CHECK(!caf.available && caf.codeBins == 64 && caf.dopplerBins == 5);
    for (column = 0; column < caf.codeBins; ++column) expected[column] = -1.0f;
    ProcessRange(context, &signal, 0, 2046, 7, 0, &result); /* Legacy noise warmup. */
    NgAcquisitionCafSnapshot(context, &caf);
    CHECK(!caf.available && NgAcquisitionCafSequence(context) == 0);
    for (frame = 0; frame < 2; ++frame) {
        TNgIqView view;
        size_t first = (frame + 1u) * 2046u;
        memset(&view, 0, sizeof(view));
        view.samples = (const TNgIq8 *)signal.samples + first;
        view.format = signal.stream.format;
        view.meta.epoch = 7;
        view.meta.firstSample = first;
        view.meta.count = 2046;
        CHECK(NgAcqMathCorrelateCentered(math, workspace, 3, &view,
                NgCarrierFromDoppler(&signal.stream, -1000.0), 0.0, frame,
                power, &error) == NG_OK);
        for (index = 0; index < 1024; ++index) {
            size_t units = index * 2046u + frame * 1024u;
            float measured = (float)power[index];
            if (units >= 2046u * 1024u) units -= 2046u * 1024u;
            column = units * 64u / (2046u * 1024u);
            if (measured > expected[column]) expected[column] = measured;
        }
        ProcessRange(context, &signal, first, 2046, 7, 0, &result);
        NgAcquisitionCafSnapshot(context, &caf);
        CHECK(caf.available && caf.prn == 3 && caf.epoch == 7);
        CHECK(caf.sequence == NgAcquisitionCafSequence(context));
        CHECK(caf.completedDopplerBins == frame);
    }
    CHECK(caf.completedDopplerBins == 1 && !caf.complete);
    CHECK(caf.totalDopplerBins == 5 && caf.nonCoherentSums == 1 && caf.fftSize == 1024);
    CHECK(caf.referenceSample == 4092 && caf.dopplerHz[0] == -1000.0);
    for (column = 0; column < caf.codeBins; ++column) {
        CHECK(fabs(caf.power[column] - expected[column]) <=
              1e-5 + 1e-5 * fabs(expected[column]));
        CHECK(caf.power[caf.codeBins + column] < 0.0f); /* Unmeasured row. */
        CHECK(fabs(caf.codeDelaySamples[column] -
                   ((double)column + 0.5) * 2046.0 / 64.0) < 1e-9);
    }
    CHECK(caf.maxPower > 0.0);
    NgAcqMathWorkspaceDestroy(workspace);
    NgAcqMathDestroy(math);
    NgAcquisitionDestroy(context);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

/** More than 64 actual Doppler hypotheses are max pooled without losing the
 * injected GPS peak; epoch resets remove the previous surface immediately.
 */
static void TestCafDecimationAndReset(void)
{
    TSignal signal = SignalCreate(2046000, NG_FORMAT_CI8, 7, 731.0, 650.0, 0,
                                  55.0, 3.0, 100);
    TNgAcquisitionConfig config = TestConfig(7);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisition *context = NULL;
    TNgAcquisitionResult result;
    TNgAcquisitionCaf caf;
    TNgError error = {{0}};
    size_t row, column, peakRow = 0, peakColumn = 0;
    double maximum = -1.0;
    config.dopplerMaxHz = 4000.0;
    config.dopplerStepHz = 100.0; /* 81 raw Doppler bins into at most 64 rows. */
    config.codeStepChips = 2.0;   /* One compression origin per Doppler bin. */
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    CHECK(NgAcquisitionCreate(plan, &context, &error) == NG_OK);
    CHECK(NgAcquisitionAssign(context, 7, 1, &error) == NG_OK);
    ProcessRange(context, &signal, 0, 82u * 2046u, 7, 0, &result);
    NgAcquisitionCafSnapshot(context, &caf);
    CHECK(caf.available && caf.complete && caf.prn == 7 && caf.epoch == 7);
    CHECK(caf.dopplerBins == 64 && caf.codeBins == 64);
    CHECK(caf.completedDopplerBins == 81 && caf.totalDopplerBins == 81);
    for (row = 0; row < caf.dopplerBins; ++row)
        for (column = 0; column < caf.codeBins; ++column) {
            double value = caf.power[row * caf.codeBins + column];
            CHECK(value >= 0.0 && isfinite(value));
            if (value > maximum) {
                maximum = value;
                peakRow = row;
                peakColumn = column;
            }
        }
    CHECK(maximum == caf.maxPower);
    CHECK(fabs(caf.dopplerHz[peakRow] - 650.0) <= 150.0);
    CHECK(CircularDistance(caf.codeDelaySamples[peakColumn], 731.0, 2046.0) <= 20.0);
    {
        TNgAcquisitionPhase phase = result.phase;
        double metric = result.metric, threshold = result.threshold;
        NgAcquisitionSetCafEnabled(context, 0);
        NgAcquisitionCafSnapshot(context, &caf);
        CHECK(!caf.available && NgAcquisitionCafSequence(context) == 0);
        CHECK(NgAcquisitionAssign(context, 7, 1, &error) == NG_OK);
        ProcessRange(context, &signal, 0, 82u * 2046u, 9, 0, &result);
        NgAcquisitionCafSnapshot(context, &caf);
        CHECK(!caf.available && NgAcquisitionCafSequence(context) == 0);
        CHECK(result.phase == phase && result.metric == metric && result.threshold == threshold);
        NgAcquisitionSetCafEnabled(context, 1);
        CHECK(NgAcquisitionAssign(context, 7, 1, &error) == NG_OK);
        ProcessRange(context, &signal, 0, 2u * 2046u, 10, 0, &result);
        NgAcquisitionCafSnapshot(context, &caf);
        CHECK(caf.available && caf.sequence != 0);
    }
    NgAcquisitionReset(context);
    NgAcquisitionCafSnapshot(context, &caf);
    CHECK(!caf.available && !caf.complete && caf.power[0] < 0.0f);
    ProcessRange(context, &signal, 0, 2046, 8, NG_BLOCK_GAP, &result);
    NgAcquisitionCafSnapshot(context, &caf);
    CHECK(!caf.available);
    ProcessRange(context, &signal, 2046, 2046, 8, 0, &result);
    NgAcquisitionCafSnapshot(context, &caf);
    CHECK(caf.available && caf.epoch == 8 && !caf.complete);
    NgAcquisitionDestroy(context);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

static void CheckEstimate(const TNgAcquisitionResult *result, const TSignal *signal,
                          unsigned prn, double delaySamples, double dopplerHz)
{
    double period = signal->stream.sampleRateHz / 1000.0;
    double expectedDelay = fmod(delaySamples - (double)result->referenceSample, period);
    if (expectedDelay < 0.0) expectedDelay += period;
    if (result->phase != NG_ACQ_ACQUIRED) {
        fprintf(stderr, "Acquisition PRN %u failed: phase=%s metric=%g threshold=%g hits=%u/%u\n",
                prn, NgAcquisitionPhaseName(result->phase), result->metric, result->threshold,
                result->confirmationHits, result->confirmationTrials);
    }
    CHECK(result->phase == NG_ACQ_ACQUIRED);
    CHECK(result->prn == prn);
    CHECK(result->epoch == 7);
    CHECK(isfinite(result->metric) && isfinite(result->threshold));
    CHECK(result->metric > result->threshold && result->threshold > 0.0);
    if (result->confirmation == NG_ACQ_CONFIRM_NONE)
        CHECK(result->confirmationHits == 0 && result->confirmationTrials == 0);
    else CHECK(result->confirmationHits >= 3 && result->confirmationTrials <= 4);
    if (fabs(result->dopplerHz - dopplerHz) > 100.0 ||
        CircularDistance(result->codeDelaySamples, expectedDelay, period) > 2.5)
        fprintf(stderr, "PRN %u estimate: Doppler %g expected %g, delay %g expected %g, ref=%llu\n",
                prn, result->dopplerHz, dopplerHz, result->codeDelaySamples, expectedDelay,
                (unsigned long long)result->referenceSample);
    CHECK(fabs(result->dopplerHz - dopplerHz) <= 100.0);
    CHECK(fabs(result->carrierHz - (signal->stream.digitalIfHz + result->dopplerHz)) < 1e-9);
    CHECK(CircularDistance(result->codeDelaySamples, expectedDelay, period) <= 2.5);
    CHECK(result->codePhaseChips >= 0.0 && result->codePhaseChips < 1023.0);
    CHECK(CircularDistance(result->codePhaseChips,
                           fmod(1023.0 - expectedDelay * 1023000.0 / signal->stream.sampleRateHz,
                                1023.0), 1023.0) <= 1.3);
}

/** Arbitrary scheduler chunks must not change one sample of algorithm history. */
static void TestChunkInvariance(void)
{
    static const size_t milliseconds[] = {2046};
    static const size_t fiveMilliseconds[] = {10230};
    static const size_t arbitrary[] = {1, 17, 2045, 33, 8184, 99, 507};
    TSignal signal = SignalCreate(2046000, NG_FORMAT_CI8, 7, 731.0, 650.0, 0, 55.0, 3.0, 80);
    TNgAcquisitionConfig config = TestConfig(7);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisitionResult a, b, c;
    TNgError error = {{0}};
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    a = RunChunks(plan, &signal, 7, milliseconds, 1);
    b = RunChunks(plan, &signal, 7, fiveMilliseconds, 1);
    c = RunChunks(plan, &signal, 7, arbitrary, sizeof(arbitrary) / sizeof(arbitrary[0]));
    CheckEstimate(&a, &signal, 7, 731.0, 650.0);
    CHECK(a.phase == b.phase && a.phase == c.phase);
    CHECK(a.referenceSample == b.referenceSample && a.referenceSample == c.referenceSample);
    CHECK(a.sequence == b.sequence && a.sequence == c.sequence);
    CHECK(a.codeDelaySamples == b.codeDelaySamples && a.codeDelaySamples == c.codeDelaySamples);
    CHECK(a.dopplerHz == b.dopplerHz && a.dopplerHz == c.dopplerHz);
    CHECK(a.metric == b.metric && a.metric == c.metric);
    CHECK(a.confirmationHits == b.confirmationHits && a.confirmationHits == c.confirmationHits);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

static void TestNativeFormatAndFrequency(TNgSampleFormat format, double doppler, int64_t digitalIf)
{
    static const size_t chunks[] = {2046};
    double gain = format == NG_FORMAT_SC16_Q11 ? 4.0 : 1.0;
    TSignal signal = SignalCreate(2046000, format, 19, 1301.0, doppler, digitalIf,
                                  50.0 * gain, 3.0 * gain, 80);
    TNgAcquisitionConfig config = TestConfig(19);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    result = RunChunks(plan, &signal, 19, chunks, 1);
    CheckEstimate(&result, &signal, 19, 1301.0, doppler);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

static void TestRationalMillisecond(void)
{
    static const size_t chunks[] = {3001, 77, 1999, 1, 6900};
    TSignal signal = SignalCreate(2046501, NG_FORMAT_CI8, 3, 2043.5, -450.0, -14000,
                                  58.0, 2.0, 80);
    TNgAcquisitionConfig config = TestConfig(3);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    /* With 1024 bins, a 0.5-chip requested grid requires two fractional
     * compression origins. Truncating their count to one biases the winning
     * Doppler at alternating floor/ceil frame lengths. */
    config.refinementDopplerSpanHz = 250.0;
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    result = RunChunks(plan, &signal, 3, chunks, sizeof(chunks) / sizeof(chunks[0]));
    CheckEstimate(&result, &signal, 3, 2043.5, -450.0);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

/** Original-rate compression exercises sixteen-sample chip runs and both frame lengths. */
static void TestHighRateCompression(void)
{
    static const size_t chunks[] = {8192, 41, 65536, 1023};
    TSignal signal = SignalCreate(16367600, NG_FORMAT_CI8, 22, 11137.0, 650.0, 1100000,
                                  35.0, 16.0, 100);
    TNgAcquisitionConfig config = TestConfig(22);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    config.fftSize = 4096;
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    result = RunChunks(plan, &signal, 22, chunks, sizeof(chunks) / sizeof(chunks[0]));
    CheckEstimate(&result, &signal, 22, 11137.0, 650.0);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

static void TestWeakSignalNoncoherent(void)
{
    static const size_t chunks[] = {2046};
    TSignal signal = SignalCreate(2046000, NG_FORMAT_CI8, 9, 1141.0, -650.0, 0,
                                  4.0, 16.0, 120);
    TNgAcquisitionConfig config = TestConfig(9);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    config.nonCoherentSums = 2;
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    result = RunChunksWithSums(plan, &signal, 9, chunks, 1, 2);
    CheckEstimate(&result, &signal, 9, 1141.0, -650.0);
    CHECK(result.nonCoherentSums == 2);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

/* A GPS-like power level exercises sensitivity rather than only strong
 * synthetic peaks. For complex AWGN, C/N0 = A^2 Fs / (2 sigma^2). */
static void TestReceiverLevelSensitivity(void)
{
    static const size_t chunks[] = {3200};
    TSignal signal = SignalCreate(3200000, NG_FORMAT_CU8, 6, 997.0, 350.0, 0,
                                  1.04, 13.0, 450);
    TNgAcquisitionConfig config = TestConfig(6);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    config.nonCoherentSums = 8;
    config.fftSize = 4096; /* Effective 2048 at 3.2 Msps, as in a live profile. */
    config.codeStepChips = 0.25;
    config.falseAlarmProbability = 1e-9;
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    result = RunChunksWithSums(plan, &signal, 6, chunks, 1, 8);
    if (result.phase != NG_ACQ_ACQUIRED)
        fprintf(stderr, "Sensitivity: phase=%s metric=%g threshold=%g\n",
                NgAcquisitionPhaseName(result.phase), result.metric, result.threshold);
    CHECK(result.phase == NG_ACQ_ACQUIRED);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

static void TestNoFalseAcquisition(void)
{
    static const size_t chunks[] = {2046};
    unsigned caseIndex;
    for (caseIndex = 0; caseIndex < 2; ++caseIndex) {
        TSignal signal = SignalCreate(2046000, NG_FORMAT_CI8, 12, 411.0, 500.0, 0,
                                      caseIndex ? 45.0 : 0.0, caseIndex ? 5.0 : 18.0, 80);
        TNgAcquisitionConfig config = TestConfig(13);
        TNgAcquisitionPlan *plan = NULL;
        TNgAcquisitionResult result;
        TNgError error = {{0}};
        CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
        result = RunChunks(plan, &signal, 13, chunks, 1);
        CHECK(result.phase == NG_ACQ_NOT_FOUND);
        CHECK(result.confirmationHits < 3);
        NgAcquisitionPlanDestroy(plan);
        SignalDestroy(&signal);
    }
}

static void TestDiscontinuityAndReset(void)
{
    TSignal signal = SignalCreate(2046000, NG_FORMAT_CI8, 5, 113.0, 500.0, 0, 55.0, 3.0, 80);
    TNgAcquisitionConfig config = TestConfig(5);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisition *context = NULL;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    uint64_t previousResets;
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    CHECK(NgAcquisitionCreate(plan, &context, &error) == NG_OK);
    CHECK(NgAcquisitionAssign(context, 5, 1, &error) == NG_OK);
    ProcessRange(context, &signal, 0, signal.count - 20, 7, 0, &result);
    CHECK(result.phase == NG_ACQ_ACQUIRED);
    previousResets = result.resetCount;
    ProcessRange(context, &signal, signal.count - 20, 20, 8, NG_BLOCK_GAP, &result);
    CHECK(result.phase != NG_ACQ_ACQUIRED);
    CHECK(result.resetCount > previousResets);
    CHECK(result.epoch == 8);
    CHECK(result.confirmationHits == 0 && result.confirmationTrials == 0);
    CHECK(result.metric == 0.0);
    previousResets = result.resetCount;
    NgAcquisitionReset(context);
    NgAcquisitionSnapshot(context, &result);
    CHECK(result.phase != NG_ACQ_ACQUIRED);
    CHECK(result.resetCount > previousResets);
    CHECK(result.prn == 5);
    CHECK(NgAcquisitionAssign(context, 0, 1, &error) == NG_ERROR);
    CHECK(NgAcquisitionAssign(context, 33, 1, &error) == NG_ERROR);
    CHECK(NgAcquisitionAssign(context, 5, 0, &error) == NG_ERROR);
    CHECK(NgAcquisitionAssign(context, 5, config.nonCoherentSums + 1, &error) == NG_ERROR);
    NgAcquisitionDestroy(context);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

static void TestInvalidConfiguration(void)
{
    TNgAcquisitionConfig base = TestConfig(1), invalid;
    TNgAcquisitionPlan *plan = NULL;
    TNgStreamInfo stream;
    TNgError error = {{0}};
    memset(&stream, 0, sizeof(stream));
    stream.sampleRateHz = 2046000;
    stream.format = NG_FORMAT_CI8;
    CHECK(NgAcquisitionValidate(&base, &error) == NG_OK);
    invalid = base; invalid.fftSize = 1023;
    CHECK(NgAcquisitionValidate(&invalid, &error) == NG_ERROR);
    invalid = base; invalid.dopplerStepHz = 0;
    CHECK(NgAcquisitionValidate(&invalid, &error) == NG_ERROR);
    invalid = base; invalid.dopplerMaxHz = NAN;
    CHECK(NgAcquisitionValidate(&invalid, &error) == NG_ERROR);
    invalid = base; invalid.falseAlarmProbability = 1;
    CHECK(NgAcquisitionValidate(&invalid, &error) == NG_ERROR);
    invalid = base; invalid.confirmationSuccesses = invalid.confirmationAttempts + 1;
    CHECK(NgAcquisitionValidate(&invalid, &error) == NG_ERROR);
    invalid = base; invalid.nonCoherentSums = 0;
    CHECK(NgAcquisitionValidate(&invalid, &error) == NG_ERROR);
    invalid = base; invalid.refinementDopplerStepHz = 0;
    CHECK(NgAcquisitionValidate(&invalid, &error) == NG_ERROR);
    invalid = base; invalid.refinementCodeStepSamples = 0;
    CHECK(NgAcquisitionValidate(&invalid, &error) == NG_ERROR);
    stream.sampleRateHz = 0;
    CHECK(NgAcquisitionPlanCreate(&base, &stream, &plan, &error) == NG_ERROR);
    CHECK(plan == NULL);
    NgAcquisitionDestroy(NULL);
    NgAcquisitionPlanDestroy(NULL);
}

static void TestGpsReplica(void)
{
    int8_t code[GPS_CHIPS], reference[GPS_CHIPS], whole[20000], split[20000];
    TNgGpsCaNco a, b;
    TNgError error = {{0}};
    unsigned prn;
    size_t n;
    for (prn = 1; prn <= 32; ++prn) {
        CHECK(NgGpsCaGenerate(prn, code, &error) == NG_OK);
        ReferenceCode(prn, reference);
        CHECK(CodeHash(code) == legacyCodeHash[prn - 1]);
        CHECK(memcmp(code, reference, sizeof(code)) == 0);
    }
    CHECK(NgGpsCaGenerate(0, code, &error) == NG_ERROR);
    CHECK(NgGpsCaGenerate(33, code, &error) == NG_ERROR);
    CHECK(NgGpsCaGenerate(7, code, &error) == NG_OK);
    CHECK(NgGpsCaNcoInit(&a, 2046001, 1023000, 0.37, &error) == NG_OK);
    b = a;
    NgGpsCaSample(code, &a, whole, sizeof(whole));
    NgGpsCaSample(code, &b, split, 17);
    NgGpsCaSample(code, &b, split + 17, 8191);
    NgGpsCaSample(code, &b, split + 8208, sizeof(split) - 8208);
    CHECK(memcmp(whole, split, sizeof(whole)) == 0);
    CHECK(a.phase == b.phase);
    for (n = 0; n < sizeof(whole); ++n) {
        unsigned chip = (unsigned)floor(fmod(0.37 + n * 1023000.0 / 2046001.0, 1023.0));
        CHECK(whole[n] == code[chip]);
    }
    /* At 3 MHz several chip boundaries land exactly on sample boundaries.
     * Integer rational arithmetic supplies an oracle without floating rounding. */
    CHECK(NgGpsCaNcoInit(&a, 3000000, 1023000, 0, &error) == NG_OK);
    for (n = 0; n < 100000; ++n) {
        unsigned chip = (unsigned)(((uint64_t)n * UINT64_C(1023000) / 3000000) % GPS_CHIPS);
        CHECK(NgGpsCaNext(code, &a) == code[chip]);
    }
    CHECK(NgGpsCaNcoInit(&a, 0, 1023000, 0, &error) == NG_ERROR);
}

/** Sign, phase and widened arithmetic, including the signed-eight-bit minimum. */
static void TestCarrierNco(void)
{
    TNgNcoTable *table = malloc(sizeof(*table));
    static const double frequencies[] = {-330001.25, -500.0, 0.0, 10000.125};
    TNgCarrierNco nco;
    TNgError error = {{0}};
    size_t frequency, n;
    CHECK(table != NULL);
    NgNcoTableInit(table);
    for (frequency = 0; frequency < sizeof(frequencies) / sizeof(frequencies[0]); ++frequency) {
        CHECK(NgNcoInit(&nco, 2046001, frequencies[frequency], 0.123, &error) == NG_OK);
        for (n = 0; n < 2000; ++n) {
            int i = n % 2 ? -128 : -2048;
            int q = n % 3 ? 127 : 2047;
            double phase = 2.0 * TEST_PI * fmod(0.123 + n * frequencies[frequency] / 2046001, 1.0);
            double expectedI = i * cos(phase) + q * sin(phase);
            double expectedQ = q * cos(phase) - i * sin(phase);
            double actualI, actualQ;
            NgNcoRotate(table, &nco, i, q, &actualI, &actualQ);
            CHECK(fabs(expectedI - actualI) < 0.002);
            CHECK(fabs(expectedQ - actualQ) < 0.002);
        }
    }
    CHECK(NgNcoInit(&nco, 0, 1000, 0, &error) == NG_ERROR);
    free(table);
}

/** O(N^2) circular correlation oracle does not use the FFT or DUT resampler. */
static void TestFftCorrelationOracle(void)
{
    enum { COUNT = 1024, DELAY = 731 };
    int8_t code[GPS_CHIPS];
    TNgIq8 input[COUNT];
    double actual[COUNT], expected[COUNT], peak = 0.0, maximumError = 0.0;
    TNgIqView view;
    TNgAcqMathPlan *plan = NULL;
    TNgAcqMathWorkspace *workspace = NULL;
    TNgError error = {{0}};
    size_t n, lag, peakIndex = 0;
    ReferenceCode(11, code);
    for (n = 0; n < COUNT; ++n) {
        int chip = code[((n + COUNT - DELAY) % COUNT) * GPS_CHIPS / COUNT];
        input[n].i = (int8_t)(37 * chip + (int)(n % 7) - 3);
        input[n].q = (int8_t)(-21 * chip + (int)(n % 5) - 2);
    }
    memset(&view, 0, sizeof(view));
    view.samples = input;
    view.format = NG_FORMAT_CI8;
    view.meta.count = COUNT;
    CHECK(NgAcqMathCreate(1024000, COUNT, UINT32_C(1) << 10, &plan, &error) == NG_OK);
    CHECK(NgAcqMathWorkspaceCreate(plan, &workspace, &error) == NG_OK);
    CHECK(NgAcqMathCorrelate(plan, workspace, 11, &view, 0, 0, actual, &error) == NG_OK);
    for (lag = 0; lag < COUNT; ++lag) {
        double sumI = 0.0, sumQ = 0.0;
        for (n = 0; n < COUNT; ++n) {
            int chip = code[((n + COUNT - lag) % COUNT) * GPS_CHIPS / COUNT];
            sumI += input[n].i * chip;
            sumQ += input[n].q * chip;
        }
        expected[lag] = (sumI * sumI + sumQ * sumQ) / ((double)COUNT * COUNT);
        if (actual[lag] > peak) {
            peak = actual[lag];
            peakIndex = lag;
        }
        if (fabs(actual[lag] - expected[lag]) > maximumError)
            maximumError = fabs(actual[lag] - expected[lag]);
    }
    CHECK(peakIndex == DELAY);
    CHECK(fabs(actual[DELAY] - expected[DELAY]) < expected[DELAY] * 0.04);
    CHECK(maximumError < expected[DELAY] * 0.04);
    /* Reuse exactly the same cached PRN spectrum at a single-ADC-level signal.
     * A fixed-scale chain can erase this peak after two normalized FFTs. */
    for (n = 0; n < COUNT; ++n) {
        input[n].i = code[((n + COUNT - DELAY) % COUNT) * GPS_CHIPS / COUNT];
        input[n].q = 0;
    }
    CHECK(NgAcqMathCorrelate(plan, workspace, 11, &view, 0, 0, actual, &error) == NG_OK);
    CHECK(fabs(actual[DELAY] - 1.0) < 0.04);
    peakIndex = 0;
    for (n = 1; n < COUNT; ++n) if (actual[n] > actual[peakIndex]) peakIndex = n;
    CHECK(peakIndex == DELAY);
    for (n = 0; n < COUNT; ++n) {
        input[n].i = (int8_t)(1 + code[((n + COUNT - DELAY) % COUNT) * GPS_CHIPS / COUNT]);
        input[n].q = -3;
    }
    CHECK(NgAcqMathCorrelateCentered(plan, workspace, 11, &view, 0, 0, 0,
                                      actual, &error) == NG_OK);
    peakIndex = 0;
    for (n = 1; n < COUNT; ++n) if (actual[n] > actual[peakIndex]) peakIndex = n;
    CHECK(peakIndex == DELAY && fabs(actual[DELAY] - 1.0) < 0.04);
    for (n = 0; n < COUNT; ++n) {
        input[n].i = 1;
        input[n].q = -3;
    }
    CHECK(NgAcqMathCorrelateCentered(plan, workspace, 11, &view, 0, 0, 0,
                                      actual, &error) == NG_OK);
    for (n = 0; n < COUNT; ++n) CHECK(actual[n] == 0.0);
    /* Offset compression wraps sample storage, but carrier phase must still
     * follow each sample's physical index, including the wrapped section. */
    for (n = 0; n < COUNT; ++n) {
        int chip = code[((n + COUNT - DELAY) % COUNT) * GPS_CHIPS / COUNT];
        double phase = 2.0 * TEST_PI * 137125.0 * n / 1024000.0 + 0.37;
        input[n].i = (int8_t)floor(60.0 * chip * cos(phase) + 0.5);
        input[n].q = (int8_t)floor(60.0 * chip * sin(phase) + 0.5);
    }
    CHECK(NgAcqMathCorrelateOffset(plan, workspace, 11, &view, 137125,
                                   0.37 / (2.0 * TEST_PI), 111, actual, &error) == NG_OK);
    peakIndex = 0;
    for (n = 1; n < COUNT; ++n) if (actual[n] > actual[peakIndex]) peakIndex = n;
    CHECK(peakIndex == DELAY - 111);
    CHECK(fabs(actual[peakIndex] - 3600.0) < 3600.0 * 0.04);
    CHECK(NgAcqMathCorrelate(plan, workspace, 12, &view, 0, 0, actual, &error) == NG_ERROR);
    NgAcqMathWorkspaceDestroy(workspace);
    NgAcqMathDestroy(plan);
}

/** SC16 sub-CI8 differences must survive native-rate refinement/confirmation. */
static void TestNativePromptPrecision(void)
{
    enum { COUNT = 1024 };
    unsigned char storage[COUNT * 4];
    TNgIqView view;
    TNgAcqMathPlan *plan = NULL;
    TNgAcqComplexSum result;
    TNgError error = {{0}};
    size_t n;
    memset(&view, 0, sizeof(view));
    view.samples = (const TNgIq8 *)storage;
    view.format = NG_FORMAT_SC16_Q11;
    view.meta.count = COUNT;
    for (n = 0; n < COUNT; ++n) {
        NgIqStore(storage, view.format, n, 0, -128);
        NgIqStore(storage, view.format, n, 1, 129);
    }
    CHECK(NgAcqMathCreate(1024000, COUNT, 1, &plan, &error) == NG_OK);
    CHECK(NgAcqMathPrompt(plan, 1, &view, 0, 0, 0.25, 1.0, &result, &error) == NG_OK);
    CHECK(fabs(result.i + 128.0) < 1e-9);
    CHECK(fabs(result.q - 129.0) < 1e-9);
    CHECK(fabs(result.power - (128.0 * 128.0 + 129.0 * 129.0)) < 1e-6);
    CHECK(result.count == COUNT);
    CHECK(NgAcqMathPromptCentered(plan, 1, &view, 0, 0, 0.25, 1.0,
                                   &result, &error) == NG_OK);
    CHECK(result.i == 0.0 && result.q == 0.0 && result.power == 0.0);
    CHECK(result.inputPower == 0.0);
    NgAcqMathDestroy(plan);
}

/** Independent same-carrier, centred prompt and 32 off-peak correlation oracle. */
static void TestCenteredNoiseOracle(void)
{
    enum { COUNT = 1024 };
    unsigned char storage[COUNT * 4], original[COUNT * 4];
    int iLevels[COUNT], qLevels[COUNT];
    int8_t code[GPS_CHIPS];
    TNgIqView view;
    TNgAcqMathPlan *plan = NULL;
    TNgAcqComplexSum result, plain;
    TNgError error = {{0}};
    double meanI = 0.0, meanQ = 0.0, energy = 0.0, noise = 0.0;
    double actualNoise, expectedI = 0.0, expectedQ = 0.0;
    double referenceI[32] = {0}, referenceQ[32] = {0};
    size_t n;
    unsigned reference;
    ReferenceCode(6, code);
    memset(&view, 0, sizeof(view));
    view.samples = (const TNgIq8 *)storage;
    view.format = NG_FORMAT_SC16_Q11;
    view.meta.count = COUNT;
    CHECK(NG_ACQ_NOISE_REFERENCES == 32);
    for (n = 0; n < COUNT; ++n) {
        unsigned chip = (unsigned)floor(fmod(0.37 + n * 1023.0 / 1024.0, 1023.0));
        double angle = 2.0 * TEST_PI * (0.123 + 137125.0 * n / 1024000.0);
        iLevels[n] = 1200 + (int)floor(5.0 * code[chip] * cos(angle) + 0.5) + (int)(n % 11) - 5;
        qLevels[n] = -900 + (int)floor(5.0 * code[chip] * sin(angle) + 0.5) + (int)(n % 7) - 3;
        NgIqStore(storage, view.format, n, 0, iLevels[n]);
        NgIqStore(storage, view.format, n, 1, qLevels[n]);
        meanI += iLevels[n];
        meanQ += qLevels[n];
    }
    meanI /= COUNT;
    meanQ /= COUNT;
    memcpy(original, storage, sizeof(original));
    for (n = 0; n < COUNT; ++n) {
        unsigned chip = (unsigned)floor(fmod(0.37 + n * 1023.0 / 1024.0, 1023.0));
        double angle = 2.0 * TEST_PI * (0.123 + 137125.0 * n / 1024000.0);
        double centeredI = iLevels[n] - meanI, centeredQ = qLevels[n] - meanQ;
        double rotatedI = centeredI * cos(angle) + centeredQ * sin(angle);
        double rotatedQ = centeredQ * cos(angle) - centeredI * sin(angle);
        energy += centeredI * centeredI + centeredQ * centeredQ;
        expectedI += rotatedI * code[chip];
        expectedQ += rotatedQ * code[chip];
        for (reference = 0; reference < 32; ++reference) {
            unsigned referenceChip = (chip + 16 + 31 * reference) % GPS_CHIPS;
            referenceI[reference] += rotatedI * code[referenceChip];
            referenceQ[reference] += rotatedQ * code[referenceChip];
        }
    }
    expectedI /= COUNT;
    expectedQ /= COUNT;
    energy /= COUNT;
    for (reference = 0; reference < 32; ++reference)
        noise += (referenceI[reference] * referenceI[reference] +
                  referenceQ[reference] * referenceQ[reference]) / ((double)COUNT * COUNT * 32);
    CHECK(NgAcqMathCreate(1024000, COUNT, UINT32_C(1) << 5, &plan, &error) == NG_OK);
    CHECK(NgAcqMathPromptWithNoise(plan, 6, &view, 137125, 0.123, 0.37, 1023000,
                                    &result, &actualNoise, &error) == NG_OK);
    CHECK(fabs(result.i - expectedI) < 2e-5);
    CHECK(fabs(result.q - expectedQ) < 2e-5);
    CHECK(fabs(result.power - (expectedI * expectedI + expectedQ * expectedQ)) < 0.001);
    CHECK(fabs(result.inputPower - energy) < 1e-6);
    CHECK(fabs(actualNoise - noise) < 1e-5 + 1e-5 * noise);
    CHECK(actualNoise > 0.0 && result.count == COUNT);
    CHECK(NgAcqMathPromptCentered(plan, 6, &view, 137125, 0.123, 0.37, 1023000,
                                   &plain, &error) == NG_OK);
    CHECK(result.i == plain.i && result.q == plain.q && result.power == plain.power);
    CHECK(memcmp(original, storage, sizeof(original)) == 0);
    NgAcqMathDestroy(plan);
}

static int ParseProfile(const char *text, TNgConfig *config)
{
    TNgError error = {{0}};
    FILE *file = tmpfile();
    int result;
    CHECK(file != NULL);
    CHECK(fwrite(text, 1, strlen(text), file) == strlen(text));
    rewind(file);
    result = NgConfigRead(file, config, &error);
    CHECK(fclose(file) == 0);
    return result;
}

static void TestConfigurationRoundTrip(void)
{
    TNgConfig config, loaded;
    TNgError error = {{0}};
    FILE *file = tmpfile();
    CHECK(file != NULL);
    CHECK(ParseProfile("[frontend]\ntype=file\nsample_rate_hz=2046000\n", &loaded) == NG_OK);
    CHECK(loaded.acquisition.enabled == 0);
    CHECK(loaded.acquisition.fftSize == 4096);
    NgConfigDefaults(&config);
    config.sampleRateHz = 2046000;
    config.acquisition = TestConfig(31);
    config.acquisition.gpsPrnMask |= UINT32_C(1) << 3;
    config.acquisition.gpsChannels = 2;
    config.acquisition.nonCoherentSums = 7;
    config.acquisition.codeStepChips = 0.25;
    config.acquisition.falseAlarmProbability = 1e-7;
    config.acquisition.refinementDopplerSpanHz = 337.5;
    config.acquisition.refinementDopplerStepHz = 12.5;
    config.acquisition.refinementCodeSpanChips = 0.75;
    config.acquisition.refinementCodeStepSamples = 2;
    CHECK(NgConfigWrite(file, &config, &error) == NG_OK);
    rewind(file);
    CHECK(NgConfigRead(file, &loaded, &error) == NG_OK);
    CHECK(fclose(file) == 0);
    CHECK(loaded.acquisition.enabled == config.acquisition.enabled);
    CHECK(loaded.acquisition.gpsPrnMask == config.acquisition.gpsPrnMask);
    CHECK(loaded.acquisition.gpsChannels == config.acquisition.gpsChannels);
    CHECK(loaded.acquisition.fftSize == config.acquisition.fftSize);
    CHECK(loaded.acquisition.dopplerMaxHz == config.acquisition.dopplerMaxHz);
    CHECK(loaded.acquisition.dopplerStepHz == config.acquisition.dopplerStepHz);
    CHECK(loaded.acquisition.codeStepChips == config.acquisition.codeStepChips);
    CHECK(loaded.acquisition.falseAlarmProbability == config.acquisition.falseAlarmProbability);
    CHECK(loaded.acquisition.nonCoherentSums == config.acquisition.nonCoherentSums);
    CHECK(loaded.acquisition.confirmationAttempts == config.acquisition.confirmationAttempts);
    CHECK(loaded.acquisition.confirmationSuccesses == config.acquisition.confirmationSuccesses);
    CHECK(loaded.acquisition.refinementDopplerSpanHz == config.acquisition.refinementDopplerSpanHz);
    CHECK(loaded.acquisition.refinementDopplerStepHz == config.acquisition.refinementDopplerStepHz);
    CHECK(loaded.acquisition.refinementCodeSpanChips == config.acquisition.refinementCodeSpanChips);
    CHECK(loaded.acquisition.refinementCodeStepSamples == config.acquisition.refinementCodeStepSamples);
    CHECK(ParseProfile("[acquisition]\nunknown=1\n", &loaded) == NG_ERROR);
    CHECK(ParseProfile("[acquisition]\nenabled=true\nenabled=false\n", &loaded) == NG_ERROR);
    CHECK(ParseProfile("[acquisition]\ngps_prn_mask=-1\n", &loaded) == NG_ERROR);
    CHECK(ParseProfile("[acquisition]\ngps_channels=33\n", &loaded) == NG_ERROR);
    CHECK(ParseProfile("[acquisition]\nfft_size=2047\n", &loaded) == NG_ERROR);
    CHECK(ParseProfile("[acquisition]\nfalse_alarm_probability=nan\n", &loaded) == NG_ERROR);
    CHECK(ParseProfile("[acquisition]\nconfirmation_successes=5\nconfirmation_attempts=4\n",
                        &loaded) == NG_ERROR);
    CHECK(ParseProfile("[frontend]\ntype=file\nsample_rate_hz=100000\n"
                        "[acquisition]\nenabled=true\n", &loaded) == NG_ERROR);
}

/** Exercise actual kernel-facing callbacks, including independent PRN allocation. */
static void TestReceiverAdapter(void)
{
    TSignal signal = SignalCreate(2046000, NG_FORMAT_SC16_Q11, 3, 509.0, 350.0, 12000,
                                  220.0, 12.0, 100);
    TNgAcquisitionConfig config = TestConfig(3);
    TNgAcquisitionReceiver *receiver = NULL;
    TNgAcquisitionReceiverStatus status;
    TNgAcquisitionCaf caf;
    TNgChannel *channels;
    TNgError error = {{0}};
    size_t count = 99, first, slot;
    config.enabled = 0;
    CHECK(NgAcquisitionReceiverCreate(&config, &signal.stream, &receiver, &error) == NG_OK);
    CHECK(receiver == NULL);
    CHECK(NgAcquisitionReceiverChannels(receiver, &count) == NULL && count == 0);
    NgAcquisitionReceiverRead(receiver, &status);
    CHECK(status.enabled == 0 && status.channelCount == 0 && status.acquiredPrnMask == 0);
    NgAcquisitionReceiverReadCaf(receiver, 3, &caf);
    CHECK(!caf.available);
    NgAcquisitionReceiverDestroy(receiver);
    config.enabled = 1;
    config.gpsPrnMask = (UINT32_C(1) << 0) | (UINT32_C(1) << 2) | (UINT32_C(1) << 4);
    config.gpsChannels = 2;
    CHECK(NgAcquisitionReceiverCreate(&config, &signal.stream, &receiver, &error) == NG_OK);
    channels = NgAcquisitionReceiverChannels(receiver, &count);
    NgAcquisitionReceiverCafInterestUntil(receiver, 1e20);
    CHECK(channels != NULL && count == 2);
    CHECK(NgAcquisitionReceiverExecution(receiver).workerCount == 2);
    for (slot = 0; slot < count; ++slot) {
        CHECK(channels[slot].ops != NULL && channels[slot].ops->process != NULL);
        CHECK((channels[slot].ops->nativeFormats & (1u << NG_FORMAT_SC16_Q11)) != 0);
        CHECK(channels[slot].state == NG_CHANNEL_ACQUISITION);
    }
    for (first = 0; first < signal.count; first += 2046) {
        TNgIqView view;
        memset(&view, 0, sizeof(view));
        view.samples = (const TNgIq8 *)((const unsigned char *)signal.samples + first * 4);
        view.format = signal.stream.format;
        view.meta.epoch = 7;
        view.meta.firstSample = first;
        view.meta.count = signal.count - first < 2046 ? signal.count - first : 2046;
        for (slot = 0; slot < count; ++slot) {
            TNgChannelState next = channels[slot].state;
            CHECK(channels[slot].ops->process(channels[slot].context, &channels[slot].id,
                                             channels[slot].state, &view, &next, &error) == NG_OK);
            CHECK(NgChannelTransitionValid(channels[slot].state, next));
            channels[slot].state = next;
        }
        if (first == 0) {
            unsigned assigned = 0;
            NgAcquisitionReceiverRead(receiver, &status);
            for (slot = 0; slot < 32; ++slot) if (status.satellites[slot].prn) ++assigned;
            CHECK(assigned == 2); /* Two slots cannot accidentally search the same PRN. */
            CHECK(channels[0].id.satellite != channels[1].id.satellite);
            for (slot = 0; slot < count; ++slot) {
                unsigned assignedPrn = channels[slot].id.satellite;
                CHECK(assignedPrn >= 1 && assignedPrn <= 32);
                CHECK((config.gpsPrnMask & (UINT32_C(1) << (assignedPrn - 1))) != 0);
                CHECK(status.satellites[assignedPrn - 1].prn == assignedPrn);
                CHECK(status.satellites[assignedPrn - 1].processedSamples == view.meta.count);
            }
        }
    }
    NgAcquisitionReceiverRead(receiver, &status);
    CHECK(status.enabled && status.channelCount == 2);
    CHECK(status.acquiredPrnMask == (UINT32_C(1) << 2));
    CHECK((status.attemptedPrnMask & ~config.gpsPrnMask) == 0);
    CheckEstimate(&status.satellites[2], &signal, 3, 509.0, 350.0);
    NgAcquisitionReceiverReadCaf(receiver, 3, &caf);
    CHECK(caf.available && caf.prn == 3 && caf.complete && caf.epoch == 7);
    for (slot = 0; slot < 32; ++slot)
        CHECK(status.satellites[slot].prn == 0 || status.satellites[slot].prn == slot + 1);
    /* A marker with the same epoch/start as the last data window is still a
     * distinct event. It must invalidate every confirmed result immediately. */
    {
        TNgIqView marker;
        memset(&marker, 0, sizeof(marker));
        marker.format = signal.stream.format;
        marker.meta.epoch = 7;
        marker.meta.firstSample = ((signal.count - 1) / 2046) * 2046;
        marker.meta.flags = NG_BLOCK_GAP;
        for (slot = 0; slot < count; ++slot) {
            TNgChannel *channel = &channels[slot];
            TNgChannelState next = channel->state;
            CHECK(channel->ops->process(channel->context, &channel->id, channel->state,
                                         &marker, &next, &error) == NG_OK);
        }
        NgAcquisitionReceiverRead(receiver, &status);
        CHECK(status.acquiredPrnMask == 0 && status.resets == 1);
        CHECK(status.satellites[2].phase != NG_ACQ_ACQUIRED);
        NgAcquisitionReceiverReadCaf(receiver, 3, &caf);
        CHECK(!caf.available);
    }
    {
        TNgIqView view;
        memset(&view, 0, sizeof(view));
        view.samples = signal.samples;
        view.format = signal.stream.format;
        view.meta.epoch = 8;
        view.meta.flags = NG_BLOCK_GAP;
        view.meta.count = 20;
        for (slot = count; slot > 0; --slot) {
            TNgChannel *channel = &channels[slot - 1];
            TNgChannelState next = channel->state;
            CHECK(channel->ops->process(channel->context, &channel->id, channel->state,
                                         &view, &next, &error) == NG_OK);
        }
        NgAcquisitionReceiverRead(receiver, &status);
        CHECK(status.acquiredPrnMask == 0 && status.resets == 2);
        CHECK(status.satellites[2].phase != NG_ACQ_ACQUIRED);
    }
    NgAcquisitionReceiverDestroy(receiver);
    SignalDestroy(&signal);
}

/** A sparse mask must advance integration at the PRN-cycle boundary, including PRN 32. */
static void TestAdapterAdaptiveSparseMask(unsigned prn)
{
    TSignal signal = SignalCreate(2046000, NG_FORMAT_CI8, 12, 411.0, 500.0, 0,
                                  0.0, 18.0, 120);
    TNgAcquisitionConfig config = TestConfig(prn);
    TNgAcquisitionReceiver *receiver = NULL;
    TNgAcquisitionReceiverStatus status;
    TNgChannel *channel;
    TNgError error = {{0}};
    size_t count, first;
    unsigned attemptsSeen = 0;
    uint64_t attemptStart = UINT64_MAX;
    config.nonCoherentSums = 3;
    config.falseAlarmProbability = 1e-9;
    CHECK(NgAcquisitionReceiverCreate(&config, &signal.stream, &receiver, &error) == NG_OK);
    channel = NgAcquisitionReceiverChannels(receiver, &count);
    CHECK(channel != NULL && count == 1);
    for (first = 0; first < signal.count && attemptsSeen < 3; first += 2046) {
        TNgIqView view;
        TNgChannelState next = channel->state;
        const TNgAcquisitionResult *result;
        memset(&view, 0, sizeof(view));
        view.samples = (const TNgIq8 *)signal.samples + first;
        view.format = signal.stream.format;
        view.meta.epoch = 7;
        view.meta.firstSample = first;
        view.meta.count = signal.count - first < 2046 ? signal.count - first : 2046;
        CHECK(channel->ops->process(channel->context, &channel->id, channel->state,
                                    &view, &next, &error) == NG_OK);
        CHECK(channel->id.satellite == prn);
        NgAcquisitionReceiverRead(receiver, &status);
        CHECK(status.acquiredPrnMask == 0);
        result = &status.satellites[prn - 1];
        CHECK(result->prn == prn);
        if (result->firstSample != attemptStart) {
            attemptStart = result->firstSample;
            ++attemptsSeen;
            CHECK(result->nonCoherentSums == attemptsSeen);
            CHECK(result->processedSamples == view.meta.count);
        }
        channel->state = next;
    }
    CHECK(attemptsSeen == 3);
    NgAcquisitionReceiverDestroy(receiver);
    SignalDestroy(&signal);
}

typedef struct {
    const TSignal *signal;
    size_t cursor;
    size_t acquires;
    size_t releases;
    size_t observedWindows;
    unsigned starts;
    unsigned stops;
    int leased;
    int observedWide;
} TScheduledSource;

static int ScheduledSourceStart(void *context, TNgError *error)
{
    TScheduledSource *source = context;
    (void)error;
    CHECK(!source->leased && source->starts == 0);
    source->cursor = 0;
    ++source->starts;
    return NG_OK;
}

/** Unequal native frontend leases deliberately cross kernel and one-ms boundaries. */
static int ScheduledSourceAcquire(void *context, TNgIqView *view, unsigned timeoutMs,
                                   TNgError *error)
{
    static const size_t lengths[] = {137, 2046, 4096, 33, 3001};
    TScheduledSource *source = context;
    size_t count = lengths[source->acquires % (sizeof(lengths) / sizeof(lengths[0]))];
    (void)timeoutMs;
    (void)error;
    CHECK(!source->leased);
    if (source->cursor == source->signal->count) return NG_END;
    if (count > source->signal->count - source->cursor)
        count = source->signal->count - source->cursor;
    memset(view, 0, sizeof(*view));
    view->format = source->signal->stream.format;
    view->samples = (const TNgIq8 *)((const unsigned char *)source->signal->samples +
                                   source->cursor * NgSampleBytes(view->format));
    view->meta.epoch = 7;
    view->meta.firstSample = source->cursor;
    view->meta.count = count;
    source->cursor += count;
    source->leased = 1;
    ++source->acquires;
    return NG_OK;
}

static void ScheduledSourceRelease(void *context)
{
    TScheduledSource *source = context;
    CHECK(source->leased);
    source->leased = 0;
    ++source->releases;
}

static int ScheduledSourceStop(void *context, TNgError *error)
{
    TScheduledSource *source = context;
    (void)error;
    CHECK(!source->leased);
    ++source->stops;
    return NG_OK;
}

/** Verify that the real kernel ring/conditioner path retains every SC16 source byte. */
static int ScheduledObserveNative(void *context, const TNgIqView *view, TNgError *error)
{
    TScheduledSource *source = context;
    const unsigned char *expected;
    size_t n;
    (void)error;
    CHECK(view->format == NG_FORMAT_SC16_Q11 && view->meta.epoch == 7);
    CHECK(view->meta.firstSample <= source->signal->count);
    CHECK(view->meta.count <= source->signal->count - view->meta.firstSample);
    expected = (const unsigned char *)source->signal->samples + view->meta.firstSample * 4;
    CHECK(memcmp(view->samples, expected, view->meta.count * 4) == 0);
    for (n = 0; n < view->meta.count && !source->observedWide; ++n) {
        int i = NgIqComponent(view, n, 0), q = NgIqComponent(view, n, 1);
        if (i < -128 || i > 127 || q < -128 || q > 127) source->observedWide = 1;
    }
    ++source->observedWindows;
    return NG_OK;
}

/** Actual native-format kernel dispatch with two acquisition workers and a shared plan. */
static void TestScheduledNativeAcquisition(void)
{
    static const TNgFrontendOps sourceOps = {
        ScheduledSourceStart, ScheduledSourceAcquire, ScheduledSourceRelease,
        ScheduledSourceStop, NULL, NULL
    };
    TSignal signal = SignalCreate(2046000, NG_FORMAT_SC16_Q11, 3, 509.0, 350.0, 12000,
                                  220.0, 12.0, 120);
    TNgAcquisitionReceiver *receiver = NULL;
    TNgAcquisitionReceiverStatus status;
    TNgReceiverStats stats;
    TNgReceiverExecution execution;
    TNgReceiverHooks hooks;
    TNgConditioner conditioner = NgConditionerBypass();
    TNgConfig config;
    TNgFrontend frontend;
    TNgChannel *channels;
    TScheduledSource source;
    TNgError error = {{0}};
    size_t count;
    int result;
    memset(&source, 0, sizeof(source));
    memset(&frontend, 0, sizeof(frontend));
    memset(&hooks, 0, sizeof(hooks));
    source.signal = &signal;
    frontend.info = signal.stream;
    frontend.context = &source;
    frontend.ops = &sourceOps;
    hooks.context = &source;
    hooks.block = ScheduledObserveNative;
    NgConfigDefaults(&config);
    config.sampleRateHz = signal.stream.sampleRateHz;
    config.centerFrequencyHz = signal.stream.centerFrequencyHz;
    config.signalFrequencyHz = signal.stream.signalFrequencyHz;
    config.replayFormat = signal.stream.format;
    config.windowSamples = 4096;
    config.runSeconds = 0;
    config.recordingEnabled = 0;
    config.acquisition = TestConfig(3);
    config.acquisition.gpsPrnMask = (UINT32_C(1) << 0) | (UINT32_C(1) << 2) | (UINT32_C(1) << 4);
    config.acquisition.gpsChannels = 2;
    CHECK(NgAcquisitionReceiverCreate(&config.acquisition, &frontend.info, &receiver, &error) == NG_OK);
    channels = NgAcquisitionReceiverChannels(receiver, &count);
    execution = NgAcquisitionReceiverExecution(receiver);
    CHECK(count == 2 && execution.workerCount == 2);
    /* Legacy CI8-only algorithms must still be rejected before reception starts. */
    {
        const TNgChannelOps *nativeOps = channels[0].ops;
        TNgChannelOps legacyOps = *nativeOps;
        legacyOps.nativeFormats = 0;
        channels[0].ops = &legacyOps;
        CHECK(NgReceiverRunScheduled(&config, &frontend, &conditioner, channels, count,
                                     NULL, NULL, &stats, &execution, &hooks, &error) == NG_ERROR);
        CHECK(source.starts == 0 && source.acquires == 0 && source.stops == 0);
        channels[0].ops = nativeOps;
    }
    result = NgReceiverRunScheduled(&config, &frontend, &conditioner, channels, count,
                                     NULL, NULL, &stats, &execution, &hooks, &error);
    if (result != NG_OK) fprintf(stderr, "Scheduled acquisition: %s\n", error.message);
    CHECK(result == NG_OK);
    CHECK(source.starts == 1 && source.stops == 1 && !source.leased);
    CHECK(source.acquires == source.releases && source.cursor == signal.count);
    CHECK(stats.processedSamples == signal.count && stats.gapEvents == 0);
    CHECK(stats.discardedPartialSamples == 0);
    CHECK(stats.windows == (signal.count + config.windowSamples - 1) / config.windowSamples);
    CHECK(source.observedWindows == stats.windows && source.observedWide);
    NgAcquisitionReceiverRead(receiver, &status);
    CHECK(status.acquiredPrnMask == (UINT32_C(1) << 2));
    CHECK(channels[0].id.satellite != channels[1].id.satellite);
    CHECK(channels[0].id.satellite >= 1 && channels[0].id.satellite <= 32);
    CHECK(channels[1].id.satellite >= 1 && channels[1].id.satellite <= 32);
    CheckEstimate(&status.satellites[2], &signal, 3, 509.0, 350.0);
    NgAcquisitionReceiverDestroy(receiver);
    SignalDestroy(&signal);
}

/** No-antenna RTL-SDR-like CU8 sources, including the half-ADC-level centre bias. */
static TSignal Cu8DisturbanceCreate(unsigned mode, unsigned milliseconds)
{
    TSignal signal;
    uint32_t state = UINT32_C(0x13579bdf);
    double filteredI = 0.0, filteredQ = 0.0;
    size_t n;
    memset(&signal, 0, sizeof(signal));
    signal.stream.sampleRateHz = 3200000;
    signal.stream.centerFrequencyHz = UINT64_C(1575420000);
    signal.stream.signalFrequencyHz = UINT64_C(1575420000);
    signal.stream.format = NG_FORMAT_CU8;
    signal.stream.sourceName = "independent CU8 disturbance fixture";
    signal.stream.clockDomain = "test sample clock";
    signal.count = (size_t)milliseconds * 3200;
    signal.samples = malloc(signal.count * 2);
    CHECK(signal.samples != NULL);
    for (n = 0; n < signal.count; ++n) {
        double sigma = mode == 3 ? 1.0 : 0.2;
        double radius = sigma * sqrt(-2.0 * log(Uniform(&state)));
        double angle = 2.0 * TEST_PI * Uniform(&state);
        double i = radius * cos(angle), q = radius * sin(angle);
        if (mode == 0) {
            /* Quantizing around 127.5 yields mostly ADC bytes 127 and 128. */
            i -= 0.5;
            q -= 0.5;
        } else if (mode == 1) {
            i = q = 1.0;
        } else if (mode == 2) {
            double phase = 2.0 * TEST_PI * 3407.0 * n / 3200000.0;
            i += 3.0 * cos(phase);
            q += 3.0 * sin(phase);
        } else if (mode == 4) {
            /* Narrow low-pass noise remains random, without GPS modulation. */
            filteredI = 0.995 * filteredI + 0.0998749217771909 * i;
            filteredQ = 0.995 * filteredQ + 0.0998749217771909 * q;
            i = filteredI - 0.5;
            q = filteredQ - 0.5;
        }
        NgIqStore(signal.samples, NG_FORMAT_CU8, n, 0, (int)floor(i + 0.5));
        NgIqStore(signal.samples, NG_FORMAT_CU8, n, 1, (int)floor(q + 0.5));
    }
    return signal;
}

/** A stationary spectral projection is not an acquired spreading code. */
static void TestCu8NoAntennaRejection(void)
{
    static const char *names[] = {"half-level DC bias", "constant I/Q", "CW tone",
                                  "white quantized noise", "low-pass biased noise"};
    unsigned mode;
    for (mode = 0; mode < sizeof(names) / sizeof(names[0]); ++mode) {
        TSignal signal = Cu8DisturbanceCreate(mode, 3000);
        TNgAcquisitionConfig config;
        TNgAcquisitionReceiver *receiver = NULL;
        TNgAcquisitionReceiverStatus status;
        TNgChannel *channel;
        TNgError error = {{0}};
        size_t count, first;
        NgAcquisitionDefaults(&config);
        config.enabled = 1;
        config.gpsPrnMask = UINT32_C(1) << 5;
        config.gpsChannels = 1;
        config.fftSize = 1024;
        config.falseAlarmProbability = 1e-9;
        CHECK(NgAcquisitionReceiverCreate(&config, &signal.stream, &receiver, &error) == NG_OK);
        channel = NgAcquisitionReceiverChannels(receiver, &count);
        CHECK(channel != NULL && count == 1);
        memset(&status, 0, sizeof(status));
        for (first = 0; first < signal.count; first += 3200) {
            TNgIqView view;
            TNgChannelState next = channel->state;
            memset(&view, 0, sizeof(view));
            view.samples = (const TNgIq8 *)((const unsigned char *)signal.samples + first * 2);
            view.format = NG_FORMAT_CU8;
            view.meta.epoch = 7;
            view.meta.firstSample = first;
            view.meta.count = 3200;
            CHECK(channel->ops->process(channel->context, &channel->id, channel->state,
                                        &view, &next, &error) == NG_OK);
            channel->state = next;
            NgAcquisitionReceiverRead(receiver, &status);
            if (status.acquiredPrnMask != 0) break;
        }
        if (status.acquiredPrnMask != 0) {
            const TNgAcquisitionResult *result = &status.satellites[5];
            fprintf(stderr, "False PRN 6 on %s: fd=%g delay=%g metric=%.12g threshold=%.12g\n",
                    names[mode], result->dopplerHz, result->codeDelaySamples,
                    result->metric, result->threshold);
        }
        CHECK(status.acquiredPrnMask == 0);
        NgAcquisitionReceiverDestroy(receiver);
        SignalDestroy(&signal);
    }
}

/** Rejecting the ADC-centre bias must preserve a real weak GPS signal underneath. */
static void TestWeakGpsUnderCu8Dc(void)
{
    static const size_t chunks[] = {3200};
    TSignal signal = SignalCreate(3200000, NG_FORMAT_CU8, 6, 997.0, 350.0, 0,
                                  0.5, 2.0, 200);
    TNgAcquisitionConfig config = TestConfig(6);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisitionResult result;
    TNgIqView whole;
    TNgError error = {{0}};
    size_t n;
    memset(&whole, 0, sizeof(whole));
    whole.samples = signal.samples;
    whole.format = NG_FORMAT_CU8;
    whole.meta.count = signal.count;
    for (n = 0; n < signal.count; ++n) {
        int i = NgIqComponent(&whole, n, 0), q = NgIqComponent(&whole, n, 1);
        NgIqStore(signal.samples, NG_FORMAT_CU8, n, 0, i + 12);
        NgIqStore(signal.samples, NG_FORMAT_CU8, n, 1, q - 9);
    }
    config.nonCoherentSums = 2;
    config.falseAlarmProbability = 1e-9;
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    result = RunChunksWithSums(plan, &signal, 6, chunks, 1, 2);
    CheckEstimate(&result, &signal, 6, 997.0, 350.0);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
}

/** Closed-form tails independent of the production negative-binomial recurrence. */
static void TestFiniteReferenceThreshold(void)
{
    static const size_t references[] = {5, 32, 1017};
    static const double probabilities[] = {0.1, 1e-5, 1e-9, 1e-15};
    size_t k, p;
    TNgError error = {{0}};
    double factor;
    for (k = 0; k < 3; ++k) for (p = 0; p < 4; ++p) {
        double count = (double)references[k], probability = probabilities[p];
        double exact = count * expm1(-log(probability) / count);
        CHECK(NgAcqMathThreshold(1, references[k], probability, &factor, &error) == NG_OK);
        CHECK(fabs(factor - exact) < 1e-12 * exact);
        CHECK(NgAcqMathThreshold(2, references[k], probability, &factor, &error) == NG_OK);
        count *= 2;
        exact = exp(-count * log1p(factor / count)) * (1 + count * factor / (count + factor));
        CHECK(fabs(exact / probability - 1) < 1e-11);
        CHECK(NgAcqMathThreshold(1024, references[k], probability, &factor, &error) == NG_OK);
        CHECK(isfinite(factor) && factor > 1024);
    }
    CHECK(NgAcqMathThreshold(0, 32, 1e-9, &factor, &error) == NG_ERROR);
    CHECK(NgAcqMathThreshold(1, 0, 1e-9, &factor, &error) == NG_ERROR);
    CHECK(NgAcqMathThreshold(1, 32, NAN, &factor, &error) == NG_ERROR);
}

static void TestOptionalConfirmation(void)
{
    static const size_t chunks[] = {2046};
    TSignal signal = SignalCreate(2046000, NG_FORMAT_CI8, 3, 509, 350, 24000, 35, 6, 100);
    TNgAcquisitionConfig config = TestConfig(3);
    TNgAcquisitionPlan *plan = NULL;
    TNgAcquisitionResult confirmed, unconfirmed;
    TNgError error = {{0}};
    TNgConfig loaded;
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    confirmed = RunChunks(plan, &signal, 3, chunks, 1);
    NgAcquisitionPlanDestroy(plan);
    config.confirmation = NG_ACQ_CONFIRM_NONE;
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    unconfirmed = RunChunks(plan, &signal, 3, chunks, 1);
    CheckEstimate(&confirmed, &signal, 3, 509, 350);
    CheckEstimate(&unconfirmed, &signal, 3, 509, 350);
    CHECK(unconfirmed.processedSamples < confirmed.processedSamples);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
    signal = SignalCreate(2046000, NG_FORMAT_CI8, 3, 509, 350, 24000, 0, 18, 100);
    CHECK(NgAcquisitionPlanCreate(&config, &signal.stream, &plan, &error) == NG_OK);
    unconfirmed = RunChunks(plan, &signal, 3, chunks, 1);
    CHECK(unconfirmed.phase == NG_ACQ_NOT_FOUND && unconfirmed.confirmationTrials == 0);
    NgAcquisitionPlanDestroy(plan);
    SignalDestroy(&signal);
    CHECK(ParseProfile("[acquisition]\nconfirmation=none\nconfirmation_successes=5\nconfirmation_attempts=4\n", &loaded) == NG_OK);
    CHECK(loaded.acquisition.confirmation == NG_ACQ_CONFIRM_NONE);
    {
        FILE *file = tmpfile();
        CHECK(file != NULL && NgConfigWrite(file, &loaded, &error) == NG_OK);
        rewind(file);
        CHECK(NgConfigRead(file, &loaded, &error) == NG_OK);
        CHECK(loaded.acquisition.confirmation == NG_ACQ_CONFIRM_NONE);
        fclose(file);
    }
    CHECK(ParseProfile("[acquisition]\nconfirmation=tong\n", &loaded) == NG_ERROR);
    CHECK(ParseProfile("[acquisition]\ndetector=bayesian\n", &loaded) == NG_ERROR);
    CHECK(ParseProfile("[acquisition]\nsearch_method=serial\n", &loaded) == NG_ERROR);
}

/** Admission changes restart suspended contexts and return their PRNs for later search. */
static void TestTrackingLoadAdmission(void)
{
    TSignal signal = SignalCreate(2046000, NG_FORMAT_CI8, 3, 509, 350, 0, 35, 6, 12);
    TNgAcquisitionConfig config = TestConfig(3);
    TNgAcquisitionReceiver *receiver = NULL;
    TNgAcquisitionReceiverStatus status;
    TNgError error = {{0}};
    size_t count, step, slot;
    TNgChannel *channels;
    config.gpsPrnMask = 15;
    config.gpsChannels = 4;
    CHECK(NgAcquisitionReceiverCreate(&config, &signal.stream, &receiver, &error) == NG_OK);
    channels = NgAcquisitionReceiverChannels(receiver, &count);
    CHECK(count == 4);
    for (step = 0; step < 3; ++step) {
        TNgIqView view = {0};
        unsigned active = 0;
        view.samples = (const TNgIq8 *)signal.samples + step * 2046;
        view.format = NG_FORMAT_CI8;
        view.meta.firstSample = step * 2046;
        view.meta.count = 2046;
        for (slot = 0; slot < count; ++slot)
            channels[slot].ops->setTrackingLoad(channels[slot].context, step == 1 ? 3 : 0);
        for (slot = 0; slot < count; ++slot) {
            TNgChannelState next = channels[slot].state;
            CHECK(channels[slot].ops->process(channels[slot].context, &channels[slot].id,
                  channels[slot].state, &view, &next, &error) == NG_OK);
            if (channels[slot].id.satellite) ++active;
        }
        NgAcquisitionReceiverRead(receiver, &status);
        CHECK(status.searchLimit == (step == 1 ? 1u : 4u));
        CHECK(active == status.searchLimit);
        CHECK(status.resets == 0); /* Intentional suspension is not a stream discontinuity. */
        if (step == 2) for (slot = 1; slot < count; ++slot) {
            TNgAcquisitionResult *r = &status.satellites[channels[slot].id.satellite - 1];
            CHECK(r->firstSample == 4092 && r->processedSamples == 2046);
        }
    }
    CHECK(status.deferredWindows == 3);
    NgAcquisitionReceiverDestroy(receiver);
    SignalDestroy(&signal);
}

int main(void)
{
    unsigned prn;
    int8_t reference[GPS_CHIPS];
    for (prn = 1; prn <= 32; ++prn) {
        ReferenceCode(prn, reference);
        CHECK(CodeHash(reference) == legacyCodeHash[prn - 1]);
    }
    TestGpsReplica();
    TestFiniteReferenceThreshold();
    TestOptionalConfirmation();
    TestTrackingLoadAdmission();
    TestCarrierNco();
    TestFftCorrelationOracle();
    TestNativePromptPrecision();
    TestCenteredNoiseOracle();
    TestConfigurationRoundTrip();
    TestInvalidConfiguration();
    TestChunkInvariance();
    TestMeasuredCafCells();
    TestCafDecimationAndReset();
    TestNativeFormatAndFrequency(NG_FORMAT_CI8, -650.0, 0);
    TestNativeFormatAndFrequency(NG_FORMAT_CU8, 350.0, 24000);
    TestNativeFormatAndFrequency(NG_FORMAT_SC16_Q11, -350.0, -16000);
    TestRationalMillisecond();
    TestHighRateCompression();
    TestWeakSignalNoncoherent();
    TestReceiverLevelSensitivity();
    TestNoFalseAcquisition();
    TestDiscontinuityAndReset();
    TestReceiverAdapter();
    TestAdapterAdaptiveSparseMask(7);
    TestAdapterAdaptiveSparseMask(32);
    TestScheduledNativeAcquisition();
    TestCu8NoAntennaRejection();
    TestWeakGpsUnderCu8Dc();
    puts("GPS acquisition independent-signal and incremental API contracts passed");
    return 0;
}
