/** @file test_acquisition.c @brief End-to-end acquisition with independent sample-time oracle. */
#include "ngene/acquisition.h"
#include "ngene/dsp/gps_ca.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

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
    CHECK(NgAcquisitionCreate(&config, &stream, &acquisition, &channel, &error) == NG_OK);
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
    values[3].i = 0.37f;
    values[3].q = -0.19f;
    NgSearchFftExecute(fft, values, 0);
    for (k = 0; k < 16; ++k) {
        double angle = -6.283185307179586 * (double)(3 * k) / 16;
        CHECK(fabs(values[k].i - (0.37 * cos(angle) + 0.19 * sin(angle))) < 1e-6);
    }
    NgSearchFftExecute(fft, values, 1);
    CHECK(fabs(values[3].i - 0.37) < 1e-6);
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

/** Run RF formats/rates, sinusoidal/square mixing, noise and independent confirmation cases. */
int main(void)
{
    CHECK(Kernels() == 0);
    CHECK(Ownership() == 0);
    CHECK(Scenario(2400000, 0, 0, 0, 1, 0) == 0);
    CHECK(Scenario(16000000, 0, 0, 1, 1, 0) == 0);
    CHECK(Scenario(8183800, 38500, 0, 0, 1, 0) == 0);
    CHECK(Scenario(8183800, 38500, 1, 1, 1, 0) == 0);
    CHECK(Scenario(2400000, 0, 1, 1, 1, 0) == 0);
    CHECK(Scenario(2400000, 0, 0, 0, 0, 0) == 0);
    CHECK(Scenario(2400000, 0, 0, 0, 1, 1) == 0);
    return 0;
}
