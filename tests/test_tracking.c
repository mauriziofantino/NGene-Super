/** @file test_tracking.c @brief Deterministic coherent epoch, arithmetic and gap contracts. */
#include "ngene/tracking.h"
#include "ngene/dsp/gps_ca.h"
#include "ngene/dsp/tracking_replica.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/** Generate and track a PRN with a nonzero capture phase and carrier. */
static TNgTrackingSnapshot Run(int square, int xorMode, size_t split, int observed)
{
    TNgTrackingConfig config;
    TNgStreamInfo stream = {0};
    TNgTracking *tracking;
    TNgTrackingSnapshot snapshot;
    TNgAcquisitionResult seed = {0};
    TNgError error = {{0}};
    TNgIq8 samples[20480];
    unsigned char wideSamples[20480 * 4];
    TNgIqView window = {0};
    int8_t chips[1023];
    size_t n;
    NgTrackingDefaults(&config);
    config.tapCount = 9;
    config.squareCarrier = square;
    config.xorCorrelation = xorMode;
    stream.sampleRateHz = 2048000;
    stream.digitalIfHz = 200000;
    CHECK(NgGpsCaGenerate(1, chips, &error) == NG_OK);
    for (n = 0; n < 20480; ++n) {
        double phase = fmod(173.25 + (double)n * 1023000 / 2048000, 1023);
        double angle = 6.2831853071795864769 * (double)n * 200000 / 2048000;
        samples[n].i = (int8_t)lround(50 * chips[(size_t)phase] * cos(angle));
        samples[n].q = (int8_t)lround(50 * chips[(size_t)phase] * sin(angle));
    }
    for (n = 0; n < 20480; ++n) {
        NgIqStore(wideSamples, NG_FORMAT_CI16, n, 0, (int)samples[n].i * 256);
        NgIqStore(wideSamples, NG_FORMAT_CI16, n, 1, (int)samples[n].q * 256);
    }
    CHECK(NgTrackingCreate(&config, &stream, 1, &tracking, &error) == NG_OK);
    seed.prn = 1;
    seed.confirmed = 1;
    seed.codePhaseChips = 173.25;
    seed.epoch = 7;
    CHECK(NgTrackingSeed(tracking, &seed, &error) == NG_OK);
    NgTrackingObserve(tracking, observed);
    for (n = 0; n < 20480;) {
        size_t count = 20480 - n;
        if (count > split)
            count = split;
        window.samples = xorMode ? (const TNgIq8 *)(wideSamples + 4 * n) : samples + n;
        window.format = xorMode ? NG_FORMAT_CI16 : NG_FORMAT_CI8;
        window.meta.epoch = 7;
        window.meta.firstSample = n;
        window.meta.count = count;
        CHECK(NgTrackingProcess(tracking, &window, &error) == NG_OK);
        n += count;
    }
    NgTrackingRead(tracking, &snapshot);
    CHECK(snapshot.active && snapshot.integrations >= 8);
    CHECK(hypot(snapshot.i[snapshot.tapCount / 2], snapshot.q[snapshot.tapCount / 2]) > 40);
    CHECK(hypot(snapshot.i[snapshot.tapCount / 2], snapshot.q[snapshot.tapCount / 2]) > hypot(snapshot.i[snapshot.tapCount / 2 - 1], snapshot.q[snapshot.tapCount / 2 - 1]));
    if (xorMode)
        for (n = 0; n < snapshot.tapCount; n++) {
            snapshot.i[n] /= 256;
            snapshot.q[n] /= 256;
        }
    window.meta.firstSample++;
    CHECK(NgTrackingProcess(tracking, &window, &error) == NG_OK);
    {
        TNgTrackingSnapshot invalid;
        NgTrackingRead(tracking, &invalid);
        CHECK(!invalid.active);
    }
    NgTrackingDestroy(tracking);
    return snapshot;
}

/** Independent long waveform exercises loop polarity and Galileo BOC replicas. */
static void Convergence(unsigned system, int inverted, int offsetSign, double codeError)
{
    TNgTrackingConfig config;
    TNgStreamInfo stream = {0};
    TNgTracking *tracking;
    TNgTrackingSnapshot snapshot;
    TNgAcquisitionResult seed = {0};
    TNgError error = {{0}};
    TNgIq8 samples[4096];
    TNgIqView window = {0};
    int8_t chips[4092];
    size_t first, n;
    unsigned period = system == 1 ? 1023u : 4092u;
    double codeRate = 1023000.0 * (1.0 + 350.0 / 1575420000.0);
    NgTrackingDefaults(&config);
    stream.sampleRateHz = 2048000;
    stream.digitalIfHz = -210000;
    stream.spectrumInverted = inverted;
    if (system == 1)
        CHECK(NgGpsCaGenerate(7, chips, &error) == NG_OK);
    else {
        FILE *file = fopen(NGENE_TEST_SOURCE_DIR "/tests/data/galileo-e1b-prn7.bin", "rb");
        CHECK(file);
        CHECK(fread(chips, 1, 4092, file) == 4092);
        fclose(file);
        /* Exercise signal defaults; the previous shared half-chip setting was wrong for BOC. */
    }
    CHECK(NgTrackingCreate(&config, &stream, system, &tracking, &error) == NG_OK);
    seed.prn = 7;
    seed.confirmed = 1;
    seed.epoch = 9;
    seed.dopplerHz = 350.0 + 2.0 * offsetSign;
    seed.codePhaseChips = 173.25 + codeError;
    CHECK(NgTrackingSeed(tracking, &seed, &error) == NG_OK);
    for (first = 0; first < 2048000; first += 4096) {
        for (n = 0; n < 4096; n++) {
            double phase = fmod(173.25 + (double)(first + n) * codeRate / 2048000, period);
            int sign = chips[(size_t)phase];
            if (((first + n) / (system == 1 ? 40960u : 8192u)) % 2u)
                sign = -sign;
            double angle = 6.2831853071795864769 * (double)(first + n) * (-209650.0) / 2048000;
            if (system == 2 && phase - floor(phase) >= 0.5)
                sign = -sign;
            samples[n].i = (int8_t)lround(50 * sign * cos(angle));
            samples[n].q = (int8_t)lround(50 * sign * sin(angle) * (inverted ? -1 : 1));
        }
        window.samples = samples;
        window.meta.count = 4096;
        window.meta.epoch = 9;
        window.meta.firstSample = first;
        CHECK(NgTrackingProcess(tracking, &window, &error) == NG_OK);
    }
    NgTrackingRead(tracking, &snapshot);
    fprintf(stderr, "system %u inverted %d sign %d Doppler %.6f DLL %.6f PLL %d\n", system,
            inverted, offsetSign, snapshot.dopplerHz, snapshot.dllError, snapshot.pll);
    CHECK(snapshot.active && snapshot.pll);
    CHECK(fabs(snapshot.dopplerHz - 350) < 0.5);
    CHECK(fabs(snapshot.dllError) < 0.05);
    if(system==2) {
        unsigned middle=snapshot.tapCount/2;
        if(fabs(codeError)>0.3) CHECK(!snapshot.mainPeakVerified);
        else {
            CHECK(snapshot.mainPeakVerified);
            CHECK(hypot(snapshot.i[middle],snapshot.q[middle])>hypot(snapshot.i[middle-1],snapshot.q[middle-1]));
            CHECK(hypot(snapshot.i[middle],snapshot.q[middle])>hypot(snapshot.i[middle+1],snapshot.q[middle+1]));
        }
    }
    NgTrackingDestroy(tracking);
}

/** Reproducible complex white noise, independent of receiver kernels. */
static double Gaussian(uint32_t *random)
{
    *random = *random * 1664525u + 1013904223u;
    double u = ((double)(*random >> 8) + 0.5) / 16777216.0;
    *random = *random * 1664525u + 1013904223u;
    double v = ((double)(*random >> 8) + 0.5) / 16777216.0;
    return sqrt(-2 * log(u)) * cos(6.2831853071795864769 * v);
}

/** Verify the native-complex moment C/N0 against a known independent noise level. */
static void Cn0Reference(void)
{
    TNgTrackingConfig config;
    TNgStreamInfo stream = {0};
    TNgTracking *tracking;
    TNgTrackingSnapshot snapshot;
    TNgAcquisitionResult seed = {0};
    TNgIq8 samples[2048];
    TNgIqView view = {0};
    TNgError error = {{0}};
    int8_t chips[1023];
    uint32_t random = 913;
    unsigned ms, n;
    stream.sampleRateHz = 2048000;
    NgTrackingDefaults(&config);
    CHECK(NgTrackingCreate(&config, &stream, 1, &tracking, &error) == NG_OK);
    CHECK(NgGpsCaGenerate(1, chips, &error) == NG_OK);
    seed.confirmed = 1;
    seed.prn = 1;
    seed.codePhaseChips = 0.25;
    CHECK(NgTrackingSeed(tracking, &seed, &error) == NG_OK);
    view.samples = samples;
    view.meta.count = 2048;
    for (ms = 0; ms < 1500; ++ms) {
        for (n = 0; n < 2048; ++n) {
            double phase = fmod(0.25 + (double)n * 1023000 / 2048000, 1023);
            samples[n].i = (int8_t)lround((double)chips[(size_t)phase] + 8 * Gaussian(&random));
            samples[n].q = (int8_t)lround(8 * Gaussian(&random));
        }
        view.meta.firstSample = (uint64_t)ms * 2048;
        CHECK(NgTrackingProcess(tracking, &view, &error) == NG_OK);
    }
    NgTrackingRead(tracking, &snapshot);
    double reference = 10 * log10(2048000.0 / (2 * 8 * 8));
    fprintf(stderr, "C/N0 estimated %.3f, reference %.3f dB-Hz\n", snapshot.cn0DbHz, reference);
    CHECK(snapshot.cn0Valid && fabs(snapshot.cn0DbHz - reference) < 3);
    NgTrackingDestroy(tracking);
}

/** Compare SIMD replicas to the original double-fmod oracle at boundaries and tails. */
static void ReplicaParity(void)
{
    double phases[2049];
    float replica[2049];
    int8_t primary[4092], expanded[8184];
    unsigned system, tap, k, trial;
    const size_t counts[] = {1, 2, 3, 37, 2049};
    for (k = 0; k < 4092; ++k) {
        primary[k] = (k % 7) < 3 ? 1 : -1;
        expanded[2 * k] = primary[k];
        expanded[2 * k + 1] = (int8_t)-primary[k];
    }
    for (system = 1; system <= 2; ++system) {
        unsigned period = system == 1 ? 1023u : 4092u;
        for (trial = 0; trial < 5; ++trial) {
            size_t count = counts[trial], n;
            NgTrackingCodePhases(phases, count, 0.123456, 1023000.0 + 0.123, 16000000.0);
            for (n = 0; n < count; n++)
                CHECK(phases[n] == 0.123456 + (double)n * (1023000.0 + 0.123) / 16000000.0);
            if (count >= 3) {
                phases[count - 3] = nextafter((double)period, 0);
                phases[count - 2] = (double)period;
                phases[count - 1] = nextafter((double)period, INFINITY);
            }
            for (tap = 0; tap < 33; ++tap) {
                double offset = ((double)tap - 16) * 0.125;
                NgTrackingReplica(replica, phases, count, system == 1 ? primary : expanded, period,
                                  offset, system == 2);
                for (n = 0; n < count; ++n) {
                    double wrapped = fmod(fmod(phases[n] - offset, period) + period, period);
                    int sign = primary[(size_t)wrapped];
                    if (system == 2 && wrapped - floor(wrapped) >= 0.5)
                        sign = -sign;
                    CHECK(replica[n] == (float)sign);
                }
            }
        }
    }
}

/** Compare arbitrary window splits and independent sign-XOR selection. */
int main(void)
{
    TNgTrackingSnapshot a = Run(0, 0, 20480, 1), b = Run(0, 1, 37, 1), c = Run(1, 1, 103, 1),
                        d = Run(1, 0, 211, 1);
    TNgTrackingConfig config;
    TNgError error = {{0}};
    unsigned k;
    TNgTrackingSnapshot hidden = Run(0, 0, 20480, 0);
    CHECK(hidden.tapCount == 3 && hidden.dopplerHz == a.dopplerHz && hidden.codeRateHz == a.codeRateHz);
    for (k = 0; k < 3; ++k) CHECK(hidden.i[k] == a.i[k + 3] && hidden.q[k] == a.q[k + 3]);
    CHECK(c.endSample == d.endSample);
    for (k = 0; k < c.tapCount; k++)
        CHECK(fabs(c.i[k] - d.i[k]) < 0.002);
    CHECK(a.endSample == b.endSample);
    for (k = 0; k < a.tapCount; k++)
        CHECK(fabs(a.i[k] - b.i[k]) < 0.002);
    NgTrackingDefaults(&config);
    config.tapCount = 4;
    CHECK(NgTrackingValidate(&config, &error) == NG_ERROR);
    Cn0Reference();
    ReplicaParity();
    Convergence(1, 0, 1, 0.05);
    Convergence(1, 1, -1, -0.05);
    Convergence(2, 0, 1, 0.05);
    Convergence(2, 1, -1, -0.05);
    Convergence(2, 0, 1, 0.5);
    Convergence(2, 1, -1, -0.5);
    puts("tracking contracts passed");
    return 0;
}
