/** @file test_acquisition.c @brief End-to-end acquisition with independent sample-time oracle. */
#include "ngene/acquisition.h"
#include "ngene/tracking.h"
#include "ngene/dsp/gps_ca.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "acquisition/grid.h"
#include "acquisition/workspace.h"
#include "ngene/dsp/beidou_b1c.h"
#include "acquisition/noise_policy.h"
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static TNgSearchFftMode testFftMode;

/** Deterministic independent uniform noise source. */
static double Noise(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return (double)(*state >> 8) / 16777216.0 - 0.5;
}

/** Native-rate waveform oracle: double trigonometry and direct continuous code indexing. */
static int Scenario(uint32_t rate, int64_t digitalIf, int square, int xorSigns, int signal,
                    int truncated)
{
    TNgAcquisition *acquisition = NULL;
    TNgAcquisitionConfig config;
    TNgAcquisitionSnapshot snapshot;
    TNgChannel channel;
    TNgStreamInfo stream = {0};
    TNgError error = {{0}};
    TNgIqView window = {0};
    TNgChannelState next;
    int8_t chips[1023];
    size_t count = (size_t)(((uint64_t)rate * 56u + 999u) / 1000u);
    TNgIq8 *samples = calloc(count, sizeof(*samples));
    uint32_t random = 521u;
    double doppler = 1234;
    double phase = 123.42;
    size_t k;
    unsigned tries;
    CHECK(samples != NULL);
    CHECK(NgGpsCaGenerate(7, chips, &error) == NG_OK);
    for (k = 0; k < count; ++k) {
        double time = (double)k / rate;
        double codePhase = fmod(phase + time * 1023000.0 * (1 + doppler / 1575420000.0), 1023);
        double carrier = 6.283185307179586 * ((double)digitalIf + doppler) * time + 0.71;
        if (rate == 8183800)
            carrier = -carrier;
        double amplitude =
            signal && (!truncated || time < 0.024) ? 5 * chips[(size_t)codePhase] : 0;
        samples[k].i = (int8_t)lround(amplitude * cos(carrier) + 28 * Noise(&random) + 2.3);
        samples[k].q = (int8_t)lround(amplitude * sin(carrier) + 28 * Noise(&random) - 1.7);
    }
    stream.sampleRateHz = rate;
    stream.digitalIfHz = digitalIf;
    stream.format = NG_FORMAT_CI8;
    stream.spectrumInverted = rate == 8183800;
    NgAcquisitionDefaults(&config);
    config.firstPrn = 7;
    config.lastPrn = 7;
    config.squareCarrier = square;
    config.xorCorrelation = xorSigns;
    config.fftMode = testFftMode;
    CHECK(NgAcquisitionCreate(&config, &stream, &acquisition, &channel, &error) == NG_OK);
    NgAcquisitionObserve(acquisition, 7);
    window.samples = samples;
    window.meta.count = count;
    window.meta.epoch = 3;
    window.meta.firstSample = 1000000;
    CHECK(channel.ops->process(channel.context, &channel.id, channel.state, &window, &next,
                               &error) == NG_OK);
    CHECK(next == NG_CHANNEL_ACQUISITION);
    for (tries = 0; tries < 3000; ++tries) {
        NgAcquisitionRead(acquisition, &snapshot);
        if (snapshot.searches != 0)
            break;
        NgSleepMs(10);
    }
    printf("rate=%u IF=%lld square=%d xor=%d signal=%d truncated=%d searches=%llu "
           "confirmed=%d doppler=%.1f phase=%.4f ratio=%.2f successes=%u %s\n",
           rate, (long long)digitalIf, square, xorSigns, signal, truncated,
           (unsigned long long)snapshot.searches, snapshot.results[6].confirmed,
           snapshot.results[6].dopplerHz, snapshot.results[6].codePhaseChips,
           snapshot.results[6].peakRatio, snapshot.results[6].successes, snapshot.message);
    CHECK(snapshot.searches == 1);
    CHECK(snapshot.results[6].confirmed == (signal && !truncated));
    {
        TNgAcquisitionSearchSpace space;
        double maximum = 0;
        unsigned row;
        unsigned column;
        NgAcquisitionReadSearchSpace(acquisition, 7, &space);
        CHECK(space.valid);
        CHECK(space.fftSize == (rate < 4096000 ? 2048u : 4096u));
        CHECK(space.result.prn == 7 && space.result.epoch == 3);
        CHECK(space.result.firstSample == 1000000);
        CHECK(space.result.confirmed == snapshot.results[6].confirmed);
        CHECK(space.threshold > 0);
        for (row = 0; row < NG_ACQUISITION_DOPPLER_BINS; ++row)
            for (column = 0; column < NG_ACQUISITION_CODE_BINS; ++column) {
                CHECK(isfinite(space.power[row][column]) && space.power[row][column] >= 0);
                if (space.power[row][column] > maximum)
                    maximum = space.power[row][column];
            }
        CHECK(fabs(maximum - space.result.peakRatio) < 1e-5 * (1 + maximum));
        if (signal) {
            unsigned peakColumn = (unsigned)(space.peakCodePhaseChips * NG_ACQUISITION_CODE_BINS / 1023);
            unsigned peakRow = (unsigned)lround((space.peakDopplerHz + 5000) / 667);
            CHECK(fabs(space.power[peakRow][peakColumn] - maximum) < 1e-5 * (1 + maximum));
        }
        NgAcquisitionReadSearchSpace(acquisition, 0, &space);
        CHECK(!space.valid);
        NgAcquisitionReadSearchSpace(acquisition, 6, &space);
        CHECK(!space.valid);
    }
    if (signal && !truncated) {
        CHECK(fabs(snapshot.results[6].dopplerHz - doppler) < 25);
        CHECK(fabs(snapshot.results[6].codePhaseChips - phase) < 0.6);
        CHECK(snapshot.results[6].successes == 3);
        CHECK(snapshot.results[6].firstSample == 1000000);
        CHECK(snapshot.results[6].epoch == 3);
    }
    NgAcquisitionDestroy(acquisition);
    free(samples);
    return 0;
}

static int GalileoScenario(uint32_t rate, int64_t digitalIf, int square, int xorSigns, int signal,
                           int truncated, int wide, unsigned fftSize)
{
    TNgAcquisition *acquisition = NULL;
    TNgAcquisitionConfig config;
    TNgAcquisitionSnapshot snapshot;
    TNgChannel channel;
    TNgStreamInfo stream = {0};
    TNgError error = {{0}};
    TNgIqView window = {0};
    TNgChannelState next;
    int8_t chips[4092];
    FILE *codeFile;
    size_t count = (size_t)(((uint64_t)rate * 224u + 999u) / 1000u);
    TNgIq8 *samples = calloc(count, sizeof(*samples));
    int16_t *wideSamples = wide ? calloc(2 * count, sizeof(*wideSamples)) : NULL;
    uint32_t random = 521u;
    double doppler = 1234;
    double phase = 123.42;
    size_t k;
    unsigned tries;
    CHECK(samples != NULL);
    codeFile = fopen(NGENE_TEST_SOURCE_DIR "/tests/data/galileo-e1b-prn7.bin", "rb");
    CHECK(codeFile != NULL);
    CHECK(fread(chips, 1, sizeof(chips), codeFile) == sizeof(chips));
    fclose(codeFile);
    for (k = 0; k < count; ++k) {
        double time = (double)k / rate;
        double codePhase = fmod(phase + time * 1023000.0 * (1 + doppler / 1575420000.0), 4092);
        double carrier = 6.283185307179586 * ((double)digitalIf + doppler) * time + 0.71;
        if (rate == 8183800)
            carrier = -carrier;
        double amplitude =
            signal && (!truncated || time < 0.096) ? 5 * chips[(size_t)codePhase] * ((codePhase - floor(codePhase)) < 0.5 ? 1 : -1) : 0;
        samples[k].i = (int8_t)lround(amplitude * cos(carrier) + 28 * Noise(&random) + 2.3);
        samples[k].q = (int8_t)lround(amplitude * sin(carrier) + 28 * Noise(&random) - 1.7);
    }
    if (wide) {
        CHECK(wideSamples != NULL);
        for (k = 0; k < count; ++k) {
            wideSamples[2 * k] = (int16_t)(samples[k].i * 101);
            wideSamples[2 * k + 1] = (int16_t)(samples[k].q * 101);
        }
    }
    stream.sampleRateHz = rate;
    stream.digitalIfHz = digitalIf;
    stream.format = wide ? NG_FORMAT_CI16 : NG_FORMAT_CI8;
    stream.spectrumInverted = rate == 8183800;
    NgAcquisitionDefaults(&config);
    config.firstPrn = 7;
    config.lastPrn = 7;
    config.squareCarrier = square;
    config.xorCorrelation = xorSigns;
    config.fftMode = testFftMode;
    config.fftSize = fftSize;
    CHECK(NgAcquisitionCreateGalileo(&config, &stream, &acquisition, &channel, &error) == NG_OK);
    NgAcquisitionObserve(acquisition, 7);
    window.samples = wide ? (const TNgIq8 *)wideSamples : samples;
    window.format = stream.format;
    window.meta.count = count;
    window.meta.epoch = 3;
    window.meta.firstSample = 1000000;
    CHECK(channel.ops->process(channel.context, &channel.id, channel.state, &window, &next,
                               &error) == NG_OK);
    CHECK(next == NG_CHANNEL_ACQUISITION);
    for (tries = 0; tries < 3000; ++tries) {
        NgAcquisitionRead(acquisition, &snapshot);
        if (snapshot.searches != 0)
            break;
        NgSleepMs(10);
    }
    printf("rate=%u IF=%lld square=%d xor=%d signal=%d truncated=%d searches=%llu "
           "confirmed=%d doppler=%.1f phase=%.4f ratio=%.2f successes=%u %s\n",
           rate, (long long)digitalIf, square, xorSigns, signal, truncated,
           (unsigned long long)snapshot.searches, snapshot.results[6].confirmed,
           snapshot.results[6].dopplerHz, snapshot.results[6].codePhaseChips,
           snapshot.results[6].peakRatio, snapshot.results[6].successes, snapshot.message);
    CHECK(snapshot.searches == 1);
    CHECK(snapshot.results[6].confirmed == (signal && !truncated));
    {
        TNgAcquisitionSearchSpace space;
        double maximum = 0;
        unsigned row;
        unsigned column;
        NgAcquisitionReadSearchSpace(acquisition, 7, &space);
        CHECK(space.valid && space.dopplerBins == 116 && space.codePeriodChips == 4092);
        CHECK(space.fftSize == (fftSize ? fftSize : (rate < 4096000 ? 8192u : 16384u)));
        CHECK(space.result.confirmed == snapshot.results[6].confirmed);
        for (row = 0; row < space.dopplerBins; ++row) {
            for (column = 0; column < NG_ACQUISITION_CODE_BINS; ++column) {
                CHECK(isfinite(space.power[row][column]) && space.power[row][column] >= 0);
                if (space.power[row][column] > maximum)
                    maximum = space.power[row][column];
            }
        }
        CHECK(fabs(maximum - snapshot.results[6].peakRatio) < 1e-5 * (1 + maximum));
    }
    if (signal && !truncated) {
        CHECK(fabs(snapshot.results[6].dopplerHz - doppler) < 25);
        CHECK(fabs(snapshot.results[6].codePhaseChips - phase) < 0.6);
        CHECK(snapshot.results[6].successes == 3);
        CHECK(snapshot.results[6].firstSample == 1000000);
        CHECK(snapshot.results[6].epoch == 3);
        /* Consume the actual refined/confirmed result after an unaligned processing
           delay. The independent waveform keeps its capture-relative code/carrier
           origin, including data transitions and native inverted/wide formats. */
        TNgTrackingConfig trackingConfig;
        TNgTrackingSnapshot tracked;
        TNgTracking *tracking=NULL;
        size_t followed=0;
        size_t future=count+(size_t)((uint64_t)rate*137u/1000u)+257;
        size_t followCount=(size_t)((uint64_t)rate*160u/1000u);
        NgTrackingDefaults(&trackingConfig);
        CHECK(NgTrackingCreate(&trackingConfig,&stream,2,&tracking,&error)==NG_OK);
        CHECK(NgTrackingSeed(tracking,&snapshot.results[6],&error)==NG_OK);
        while(followed<followCount) {
            size_t take=followCount-followed;
            size_t n;
            if(take>4096)take=4096;
            for(n=0;n<take;++n) {
                double time=(double)(future+followed+n)/rate;
                double unwrapped=phase+time*1023000.0*(1+doppler/1575420000.0);
                double chip=fmod(unwrapped,4092);
                double angle=6.283185307179586*((double)digitalIf+doppler)*time+0.71;
                double amplitude=5*chips[(size_t)chip]*(chip-floor(chip)<0.5?1:-1);
                if(((unsigned long long)floor(unwrapped/4092))%2u)amplitude=-amplitude;
                if(stream.spectrumInverted)angle=-angle;
                int i=(int)lround(amplitude*cos(angle)+28*Noise(&random)+2.3);
                int q=(int)lround(amplitude*sin(angle)+28*Noise(&random)-1.7);
                samples[n].i=(int8_t)i;samples[n].q=(int8_t)q;
                if(wide){wideSamples[2*n]=(int16_t)(i*101);wideSamples[2*n+1]=(int16_t)(q*101);}
            }
            window.samples=wide?(const TNgIq8 *)wideSamples:samples;
            window.meta.firstSample=1000000+future+followed;window.meta.count=take;
            CHECK(NgTrackingProcess(tracking,&window,&error)==NG_OK);
            followed+=take;
        }
        NgTrackingRead(tracking,&tracked);
        CHECK(tracked.active && tracked.mainPeakVerified);
        CHECK(fabs(tracked.dopplerHz-doppler)<5);
        CHECK(hypot(tracked.i[1],tracked.q[1])>hypot(tracked.i[0],tracked.q[0]));
        CHECK(hypot(tracked.i[1],tracked.q[1])>hypot(tracked.i[2],tracked.q[2]));
        printf("Galileo acquisition-to-tracking handoff: Fs=%u native=%s main-peak=%d\n",
               rate,wide?"CI16":"CI8",tracked.mainPeakVerified);
        NgTrackingDestroy(tracking);
    }
    NgAcquisitionDestroy(acquisition);
    free(samples);
    free(wideSamples);
    return 0;
}

/** Check FFT normalization/phase and exact XOR/product agreement on nonintegral IQ. */
static int Kernels(void)
{
    TNgSearchFft *fft = NULL;
    TNgComplex values[16] = {{0}};
    float code[16];
    TNgError error = {{0}};
    double pi;
    double pq;
    double xi;
    double xq;
    size_t k;
    CHECK(NgSearchFftCreate(16, &fft, &error) == NG_OK);
    {
        int32_t input[32] = {0}, output[32] = {0};
        input[6] = 4096;
        CHECK(NgSearchFftExecuteInt32(fft, input, output, 0, &error) == NG_OK);
        CHECK(output[0] == 4096 && output[1] == 0);
        CHECK(output[16] == -4096 && output[17] == 0);
        CHECK(NgSearchFftExecuteInt32(fft, input, input, 0, &error) == NG_ERROR);
    }
    values[3].i = 0.37f;
    values[3].q = -0.19f;
    NgSearchFftExecute(fft, values, 0);
    for (k = 0; k < 16; ++k) {
        double angle = -6.283185307179586 * (double)(3 * k) / 16;
        CHECK(fabs(values[k].i - (0.37 * cos(angle) + 0.19 * sin(angle))) < 1e-6);
    }
    NgSearchFftExecute(fft, values, 1);
    CHECK(fabs(values[3].i - 0.37) < 1e-6);
    for (unsigned mode = NG_SEARCH_FFT_INTEGER_PRECISE;
         mode <= NG_SEARCH_FFT_INTEGER_SCALED; ++mode) {
        memset(values, 0, sizeof(values));
        values[3].i = 0.37f;
        values[3].q = -0.19f;
        NgSearchFftExecuteMode(fft, values, 0, (TNgSearchFftMode)mode);
        for (k = 0; k < 16; ++k) {
            double angle = -6.283185307179586 * (double)(3 * k) / 16;
            CHECK(fabs(values[k].i - (0.37 * cos(angle) + 0.19 * sin(angle))) <
                  (mode == NG_SEARCH_FFT_INTEGER_PRECISE ? 0.002 : 0.02));
        }
        NgSearchFftExecuteMode(fft, values, 1, (TNgSearchFftMode)mode);
        CHECK(fabs(values[3].i - 0.37) <
              (mode == NG_SEARCH_FFT_INTEGER_PRECISE ? 0.002 : 0.02));
    }
    for (k = 0; k < 16; ++k)
        code[k] = k % 2 ? -1.0f : 1.0f;
    NgSearchCorrelate(values, code, 15, 0, &pi, &pq);
    NgSearchCorrelate(values, code, 15, 1, &xi, &xq);
    CHECK(pi == xi && pq == xq);
    NgSearchFftDestroy(fft);
    return 0;
}

/** Custom strategy fixture proves raw native precision and cancellation across ring gaps. */
typedef struct {
    TNgAtomicInt entered;
    TNgAtomicInt cancelled;
    TNgAtomicInt release;
} TNgStrategyFixture;

/** Replacement strategy sees owned raw samples; cooperatively abandons stale captures. */
static int FixtureSearch(void *context, const TNgAcquisitionCapture *capture, unsigned prn,
                         const TNgAcquisitionConfig *config, TNgAcquisitionCancel cancel,
                         void *cancelContext, TNgAcquisitionResult *result, TNgError *error)
{
    TNgStrategyFixture *fixture = context;
    (void)config;
    (void)error;
    NgAtomicSet(&fixture->entered, 1);
    while (!NgAtomicGet(&fixture->release) && !cancel(cancelContext))
        NgSleepMs(1);
    if (cancel(cancelContext)) {
        NgAtomicSet(&fixture->cancelled, 1);
        return NG_END;
    }
    result->prn = prn;
    result->firstSample = capture->origin.firstSample;
    result->epoch = capture->origin.epoch;
    result->dopplerHz = capture->samples[0].i;
    result->codePhaseChips = capture->samples[0].q;
    return NG_OK;
}

/** Exercise pluggability, 16-bit amplitude, raw Q convention and discontinuity invalidation. */
static int Ownership(void)
{
    static const TNgAcquisitionStrategy strategy = {FixtureSearch};
    TNgStrategyFixture fixture = {0};
    TNgAcquisition *acquisition = NULL;
    TNgAcquisitionConfig config;
    TNgAcquisitionSnapshot snapshot;
    TNgStreamInfo stream = {0};
    TNgChannel channel;
    TNgChannelState next;
    TNgError error = {{0}};
    TNgIqView window = {0};
    size_t count = 2048 * 56;
    int16_t *samples = calloc(2 * count, sizeof(*samples));
    unsigned wait;
    CHECK(samples != NULL);
    samples[0] = 12345;
    samples[1] = -23456;
    stream.sampleRateHz = 2048000;
    stream.format = NG_FORMAT_CI16;
    stream.spectrumInverted = 1;
    NgAcquisitionDefaults(&config);
    config.firstPrn = 1;
    config.lastPrn = 1;
    CHECK(NgAcquisitionCreateWithStrategy(&config, &stream, &strategy, &fixture, &acquisition,
                                          &channel, &error) == NG_OK);
    window.samples = (const TNgIq8 *)samples;
    window.format = NG_FORMAT_CI16;
    window.meta.count = count;
    window.meta.epoch = 1;
    CHECK(channel.ops->process(channel.context, &channel.id, channel.state, &window, &next,
                               &error) == NG_OK);
    for (wait = 0; wait < 1000 && !NgAtomicGet(&fixture.entered); ++wait)
        NgSleepMs(1);
    CHECK(NgAtomicGet(&fixture.entered));
    window.meta.epoch = 2;
    window.meta.firstSample = count;
    window.meta.flags = NG_BLOCK_GAP;
    CHECK(channel.ops->process(channel.context, &channel.id, channel.state, &window, &next,
                               &error) == NG_OK);
    for (wait = 0; wait < 1000 && !NgAtomicGet(&fixture.cancelled); ++wait)
        NgSleepMs(1);
    CHECK(NgAtomicGet(&fixture.cancelled));
    for (wait = 0; wait < 1000; ++wait) {
        NgAcquisitionRead(acquisition, &snapshot);
        if (!snapshot.searching)
            break;
        NgSleepMs(1);
    }
    CHECK(snapshot.searches == 0 && snapshot.gaps == 1);
    NgAtomicSet(&fixture.release, 1);
    window.meta.flags = 0;
    window.meta.firstSample += count;
    CHECK(channel.ops->process(channel.context, &channel.id, channel.state, &window, &next,
                               &error) == NG_OK);
    /* Overwrite the ring backing store immediately after callback return. */
    memset(samples, 0, 2 * count * sizeof(*samples));
    for (wait = 0; wait < 1000; ++wait) {
        NgAcquisitionRead(acquisition, &snapshot);
        if (snapshot.searches != 0)
            break;
        NgSleepMs(1);
    }
    CHECK(snapshot.searches == 1);
    CHECK(snapshot.results[0].dopplerHz == 12345);
    CHECK(snapshot.results[0].codePhaseChips == -23456);
    CHECK(snapshot.results[0].epoch == 2);
    CHECK(snapshot.results[0].firstSample == 2 * count);
    NgAcquisitionDestroy(acquisition);
    free(samples);
    return 0;
}

/** Check the common noise law, configurable Pfa and measured integration scaling. */
static int NoisePolicy(void)
{
    TNgComplex values[8000];
    TNgComplex carrier[8000];
    float replica[8000];
    uint32_t random = 3451;
    double measured[2] = {0};
    unsigned duration;
    CHECK(AcquisitionCoarseNoiseDoppler() > 5005);
    CHECK(fabs(AcquisitionThreshold(1, 1e-9) + log(1e-9)) < 1e-10);
    CHECK(fabs(AcquisitionThreshold(1, 1e-100) + log(1e-100)) < 1e-9);
    CHECK(AcquisitionThreshold(8, 1e-9) > AcquisitionThreshold(8, 1e-6));
    for (duration = 0; duration < 2; ++duration) {
        size_t count = duration ? 8000 : 2000;
        unsigned trial;
        double signalI;
        double signalQ;
        size_t k;
        NgSearchCarrier(carrier, count, 2000000, AcquisitionNoiseOffset(0), 0, 0);
        for (k = 0; k < count; ++k) {
            replica[k] = 1;
            values[k].i = 1;
            values[k].q = 0;
        }
        NgSearchMix(values, carrier, count);
        NgSearchCorrelate(values, replica, count, 0, &signalI, &signalQ);
        CHECK(hypot(signalI, signalQ) < 0.001); /* wrong Doppler cancels coherent signal */
        for (trial = 0; trial < 512; ++trial) {
            double i;
            double q;
            for (k = 0; k < count; ++k) {
                values[k].i = (float)Noise(&random);
                values[k].q = (float)Noise(&random);
            }
            NgSearchMix(values, carrier, count);
            NgSearchCorrelate(values, replica, count, 0, &i, &q);
            measured[duration] += (i * i + q * q) / 512;
        }
    }
    CHECK(measured[1] / measured[0] > 3.3 && measured[1] / measured[0] < 4.7);
    return 0;
}

static int BenchmarkFft(void)
{
    TNgSearchFft *plan = NULL;
    TNgError error = {{0}};
    TNgComplex *values = calloc(16384, sizeof(*values));
    volatile double checksum = 0;
    CHECK(values != NULL && NgSearchFftCreate(16384, &plan, &error) == NG_OK);
    for (unsigned mode = 0; mode <= NG_SEARCH_FFT_INTEGER_SCALED; ++mode) {
        double start = NgMonotonicSeconds();
        for (unsigned repeat = 0; repeat < 100; ++repeat) {
            for (size_t k = 0; k < 16384; ++k) {
                values[k].i = (float)((int)(k * 37 % 251) - 125);
                values[k].q = (float)((int)(k * 71 % 251) - 125);
            }
            NgSearchFftExecuteMode(plan, values, 0, (TNgSearchFftMode)mode);
            checksum += values[repeat].i;
        }
        printf("FFT 16384 mode %u: %.3f us/transform\n", mode,
               (NgMonotonicSeconds() - start) * 10000.0);
    }
    printf("FFT checksum %.3f\n", checksum);
    NgSearchFftDestroy(plan);
    free(values);
    return 0;
}

int BenchmarkIntegerPcps(int measure);

/** Cover grid geometry for current strategies and candidate dimensions without a B1C receiver. */
static int GridGeometryContract(void)
{
    static const struct {
        const char *signal;
        unsigned periodMs;
        uint32_t rate;
        size_t size;
        int valid;
    } cases[] = {
        {"GPS L1 C/A", 1, 1023999, 1024, 0},
        {"GPS L1 C/A", 1, 1024000, 1024, 1},
        {"GPS L1 C/A", 1, 2047999, 2048, 0},
        {"GPS L1 C/A", 1, 2048000, 2048, 1},
        {"GPS L1 C/A", 1, 4095999, 4096, 0},
        {"GPS L1 C/A", 1, 4096000, 4096, 1},
        {"Galileo E1-B", 4, 1023999, 4096, 0},
        {"Galileo E1-B", 4, 1024000, 4096, 1},
        {"Galileo E1-B", 4, 2047999, 8192, 0},
        {"Galileo E1-B", 4, 2048000, 8192, 1},
        {"Galileo E1-B", 4, 4095999, 16384, 0},
        {"Galileo E1-B", 4, 4096000, 16384, 1},
        {"BeiDou B1C", 10, 1638399, 16384, 0},
        {"BeiDou B1C", 10, 1638400, 16384, 1},
        {"BeiDou B1C", 10, 2048000, 16384, 1},
        {"BeiDou B1C", 10, 8183800, 16384, 1},
        {"BeiDou B1C", 10, 2048000, 20480, 0},
        {"BeiDou B1C", 10, 8183800, 32768, 0},
        {"BeiDou B1C", 10, 8183800, 81838, 0},
        {"BeiDou B1C", 10, 8183800, 0, 0},
        {"BeiDou B1C", 0,  8183800, 16384, 0},
    };
    for (size_t k = 0; k < sizeof(cases) / sizeof(cases[0]); ++k) {
        TNgError error = {{0}};
        CHECK(NgAcquisitionValidateGrid(cases[k].signal, cases[k].rate, cases[k].periodMs,
                                        cases[k].size, &error) ==
              (cases[k].valid ? NG_OK : NG_ERROR));
        if (!cases[k].valid)
            CHECK(strstr(error.message, cases[k].signal) != NULL);
    }
    return 0;
}

static int NativeGridContract(void)
{
    static const struct {
        uint32_t rate;
        unsigned fftSize;
        int valid;
    } galileo[] = {
        {2048000, 4096, 1}, {2048000, 8192, 1}, {2048000, 16384, 0},
        {4095999, 16384, 0}, {4096000, 16384, 1}, {4096001, 16384, 1},
        {2048000, 0, 1}, {4095999, 0, 1}, {4096000, 0, 1},
    };
    TNgAcquisitionConfig config;
    TNgStreamInfo stream = {0};
    TNgAcquisition *search;
    TNgChannel channel;
    TNgError error = {{0}};

    NgAcquisitionDefaults(&config);
    config.firstPrn = config.lastPrn = 7;
    for (size_t k = 0; k < sizeof(galileo) / sizeof(galileo[0]); ++k) {
        stream.sampleRateHz = galileo[k].rate;
        stream.format = k % 2 == 0 ? NG_FORMAT_CI8 : NG_FORMAT_CI16;
        stream.digitalIfHz = k % 2 == 0 ? 0 : 38500;
        config.fftSize = galileo[k].fftSize;
        int status = NgAcquisitionCreateGalileo(&config, &stream, &search, &channel, &error);
        if (galileo[k].valid) {
            CHECK(status == NG_OK && search != NULL);
            NgAcquisitionDestroy(search);
        } else {
            CHECK(status == NG_ERROR && search == NULL);
            CHECK(strstr(error.message, "Galileo E1-B") != NULL);
            CHECK(strstr(error.message, "16384") != NULL);
            CHECK(strstr(error.message, "4096000") != NULL);
        }
    }
    /* Exercise both compression branches on compatible manually sized grids. */
    CHECK(GalileoScenario(2048000, -38500, 0, 1, 1, 0, 1, 8192) == 0);
    CHECK(GalileoScenario(2048000, 0, 0, 0, 0, 0, 0, 4096) == 0);
    CHECK(GalileoScenario(4096000, 0, 0, 0, 1, 0, 0, 16384) == 0);
    return 0;
}

static int NeverCancel(void *context)
{
    (void)context;
    return 0;
}

/** Exercise the operational B1C search at the HackRF reference sample rate. */
static int BeidouScenario(void)
{
    const unsigned rate = 16367600, prn = 63;
    const double phase = 3241.37, doppler = 1013.0;
    const double codeRate = 1023000 * (1 + doppler / 1575420000.0);
    const size_t count = (size_t)rate * 57 / 100;
    TNgComplex *samples = calloc(count, sizeof(*samples));
    int8_t chips[NG_BEIDOU_B1C_LENGTH];
    TNgAcquisitionCapture capture = {0};
    TNgAcquisitionConfig config;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    uint32_t random = 331u;
    void *workspace = NULL;
    CHECK(samples);
    CHECK(NgBeidouB1cGenerate(prn, NG_BEIDOU_B1C_DATA, chips, &error) == NG_OK);
    for (size_t n = 0; n < count; ++n) {
        double time = (double)n / rate;
        double code = phase + time * codeRate;
        double carrier = 6.283185307179586 * doppler * time + 0.37;
        float replica = NgBeidouB1cDataAt(chips, (uint64_t)(code * NG_BEIDOU_B1C_SUBCHIPS));
        double symbol = (uint64_t)(code / NG_BEIDOU_B1C_LENGTH) % 2 ? -1 : 1;
        samples[n].i = (float)(5 * symbol * replica * cos(carrier) + 20 * Noise(&random));
        samples[n].q = (float)(5 * symbol * replica * sin(carrier) + 20 * Noise(&random));
    }
    capture.count = count;
    capture.samples = samples;
    capture.stream.sampleRateHz = rate;
    capture.origin.epoch = 9;
    capture.origin.firstSample = 8173921;
    NgAcquisitionDefaults(&config);
    CHECK(NgAcquisitionBeidouWorkspace(&capture.stream, &workspace, &error) == NG_OK);
    CHECK(ngBeidouAcquisitionStrategy.search(workspace, &capture, prn, &config,
          NeverCancel, NULL, &result, &error) == NG_OK);
    CHECK(result.confirmed && result.prn == prn && result.epoch == 9 &&
          result.firstSample == capture.origin.firstSample);
    CHECK(fabs(result.codePhaseChips - phase) < 0.15);
    if (fabs(result.dopplerHz - doppler) > 7)
        fprintf(stderr, "B1C recovered %.1f Hz for %.1f Hz\n", result.dopplerHz, doppler);
    CHECK(fabs(result.dopplerHz - doppler) <= 7);
    NgAcquisitionGalileoFree(workspace);
    free(samples);
    return 0;
}

/** Run RF formats/rates, sinusoidal/square mixing, noise and independent confirmation cases. */
int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--verify-beidou") == 0)
        return BeidouScenario();
    if (argc == 2 && strcmp(argv[1], "--benchmark") == 0)
        return BenchmarkFft();
    if (argc == 2 && strcmp(argv[1], "--benchmark-pcps") == 0)
        return BenchmarkIntegerPcps(1);
    if (argc == 2 && strcmp(argv[1], "--verify-pcps") == 0)
        return BenchmarkIntegerPcps(0);
    if (argc == 2 && strcmp(argv[1], "--verify-grid") == 0) {
        CHECK(GridGeometryContract() == 0);
        return NativeGridContract();
    }
    CHECK(BeidouScenario() == 0);
    CHECK(Kernels() == 0);
    CHECK(NoisePolicy() == 0);
    CHECK(GalileoScenario(4096000, 0, 0, 1, 1, 0, 1, 0) == 0);
    CHECK(GalileoScenario(4096000, 0, 0, 0, 1, 0, 0, 0) == 0);
    CHECK(GalileoScenario(2400000, 0, 0, 1, 1, 0, 0, 0) == 0);
    CHECK(GalileoScenario(8183800, 38500, 0, 1, 1, 0, 0, 0) == 0);
    CHECK(GalileoScenario(4096000, 0, 0, 0, 0, 0, 0, 0) == 0);
    CHECK(GalileoScenario(4096000, 0, 0, 0, 1, 1, 0, 0) == 0);
    CHECK(Ownership() == 0);
    CHECK(Scenario(2400000, 0, 0, 0, 1, 0) == 0);
    CHECK(Scenario(16000000, 0, 0, 1, 1, 0) == 0);
    CHECK(Scenario(8183800, 38500, 0, 0, 1, 0) == 0);
    CHECK(Scenario(8183800, 38500, 1, 1, 1, 0) == 0);
    CHECK(Scenario(2400000, 0, 1, 1, 1, 0) == 0);
    CHECK(Scenario(2400000, 0, 0, 0, 0, 0) == 0);
    CHECK(Scenario(2400000, 0, 0, 0, 1, 1) == 0);
    testFftMode = NG_SEARCH_FFT_INTEGER_PRECISE;
    CHECK(Scenario(2400000, 0, 0, 0, 1, 0) == 0);
    CHECK(GalileoScenario(4096000, 0, 0, 0, 1, 0, 0, 0) == 0);
    testFftMode = NG_SEARCH_FFT_INTEGER_SCALED;
    CHECK(Scenario(2400000, 0, 0, 0, 1, 0) == 0);
    CHECK(GalileoScenario(4096000, 0, 0, 0, 1, 0, 0, 0) == 0);
    return 0;
}
