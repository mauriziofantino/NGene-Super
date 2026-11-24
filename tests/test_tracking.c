/** @file test_tracking.c @brief Deterministic coherent epoch, arithmetic and gap contracts. */
#include "ngene/tracking.h"
#include "ngene/dsp/gps_ca.h"
#include "ngene/dsp/tracking_replica.h"
#include "ngene/dsp/search.h"
#include <string.h>
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
    CHECK(hypot(snapshot.i[snapshot.tapCount / 2], snapshot.q[snapshot.tapCount / 2]) >
          hypot(snapshot.i[snapshot.tapCount / 2 - 1], snapshot.q[snapshot.tapCount / 2 - 1]));
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
static void Convergence(unsigned system, int inverted, int offsetSign, double codeError,
                        unsigned seconds, double codeDrift)
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
    double codeRate = 1023000.0 * (1.0 + 350.0 / 1575420000.0) + codeDrift;
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
    for (first = 0; first < 2048000u * seconds; first += 4096) {
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
    if (seconds > 1) {
        CHECK(fabs(snapshot.codeRateHz - codeRate) < 0.1);
        CHECK(hypot(snapshot.i[snapshot.tapCount / 2], snapshot.q[snapshot.tapCount / 2]) > 35);
    }
    if (system == 2) {
        unsigned middle = snapshot.tapCount / 2;
        if (fabs(codeError) > 0.3)
            CHECK(!snapshot.mainPeakVerified);
        else {
            CHECK(snapshot.mainPeakVerified);
            CHECK(hypot(snapshot.i[middle], snapshot.q[middle]) >
                  hypot(snapshot.i[middle - 1], snapshot.q[middle - 1]));
            CHECK(hypot(snapshot.i[middle], snapshot.q[middle]) >
                  hypot(snapshot.i[middle + 1], snapshot.q[middle + 1]));
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
    for (ms = 0; ms < 30000; ++ms) {
        for (n = 0; n < 2048; ++n) {
            double phase = fmod(0.25 + (double)n * 1023000 / 2048000, 1023);
            samples[n].i = (int8_t)lround((double)chips[(size_t)phase] * ((ms / 20) % 2 ? -1 : 1) + 8 * Gaussian(&random));
            samples[n].q = (int8_t)lround(8 * Gaussian(&random));
        }
        view.meta.firstSample = (uint64_t)ms * 2048;
        CHECK(NgTrackingProcess(tracking, &view, &error) == NG_OK);
    }
    NgTrackingRead(tracking, &snapshot);
    double reference = 10 * log10(2048000.0 / (2 * 8 * 8));
    fprintf(stderr, "C/N0 estimated %.3f, reference %.3f dB-Hz\n", snapshot.cn0DbHz, reference);
    CHECK(snapshot.cn0Valid && fabs(snapshot.cn0DbHz - reference) < 3);
    /* A previously coherent channel must release its slot after Classic's
       one-second relock timeout, rather than keep a stale PLL flag forever. */
    for (; ms < 31500; ++ms) {
        for (n = 0; n < 2048; ++n) {
            samples[n].i = (int8_t)lround(8 * Gaussian(&random));
            samples[n].q = (int8_t)lround(8 * Gaussian(&random));
        }
        view.meta.firstSample = (uint64_t)ms * 2048;
        CHECK(NgTrackingProcess(tracking, &view, &error) == NG_OK);
    }
    NgTrackingRead(tracking, &snapshot);
    CHECK(!snapshot.active && !snapshot.navigation.towValid);
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

/** Deterministic uniform value in [0, 1). */
static double Uniform(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return (double)(*state >> 8) / 16777216.0;
}

/** The fused one-pass front end must produce exactly the separate kernels' mixed samples. */
static void MixPrefixParity(void)
{
    static TNgIq8 input[2049];
    static TNgComplex carrier[2049], values[2049];
    static double expected[2 * 2050], actual[2 * 2050];
    const size_t counts[] = {1, 2, 3, 1023, 1024, 1025, 2049};
    uint32_t state = 77;
    unsigned c, mode;
    size_t n;
    for (n = 0; n < 2049; ++n) {
        input[n].i = (int8_t)(Uniform(&state) * 255 - 127);
        input[n].q = (int8_t)(Uniform(&state) * 255 - 127);
    }
    for (c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c)
        for (mode = 0; mode < 4; ++mode) {
            int square = mode & 1, inverted = (mode >> 1) & 1;
            size_t count = counts[c];
            double frequency = 4130.0 + 977.0 * c - 3000.0 * mode, phase = 0.37 + 0.11 * c;
            NgSearchCarrier(carrier, count, 16368000.0, frequency, phase, square);
            for (n = 0; n < count; ++n) {
                values[n].i = (float)input[n].i;
                values[n].q = (float)input[n].q * (inverted ? -1.0f : 1.0f);
            }
            NgSearchMix(values, carrier, count);
            NgSearchPrefix(expected, values, count);
            NgSearchMixPrefixCi8(actual, input, count, 16368000.0, frequency, phase, square,
                                 inverted);
            CHECK(memcmp(expected, actual, sizeof(double) * 2 * (count + 1)) == 0);
        }
}

/** Byte SIMD products must agree with an independent widened scalar rotation. */
static void QuantizedMixContract(void)
{
    TNgIq8 input[65];
    double prefix[132], sumI, sumQ, ci, cq, di, dq;
    size_t k;
    int inverted;
    unsigned mode;
    for (k = 0; k < 65; ++k) {
        input[k].i = (int8_t)(k == 0 ? -128 : (int)(k * 71) % 256 - 128);
        input[k].q = (int8_t)(k == 1 ? -128 : (int)(k * 43) % 256 - 128);
    }
    for (mode = 4; mode <= 16; mode *= 2)
    for (inverted = 0; inverted < 2; ++inverted) {
        NgSearchMixPrefixCi8Arithmetic(prefix, input, 65, 8183800, 38500, 0.37,
                                       inverted, mode);
        ci = cos(-6.28318530717958647692 * 0.37);
        cq = sin(-6.28318530717958647692 * 0.37);
        di = cos(-6.28318530717958647692 * 38500 / 8183800);
        dq = sin(-6.28318530717958647692 * 38500 / 8183800);
        sumI = sumQ = 0;
        for (k = 0; k < 65; ++k) {
            int c = (int)lround(7 * ci), s = (int)lround(7 * cq);
            int i = input[k].i, q = input[k].q;
            int real = i * c - q * s, imag = i * s + q * c;
            double next = ci * di - cq * dq;
            if (mode == 4) {
                i = i >= 0 ? i / 16 : -1 - ((-1 - i) / 16);
                q = q >= 0 ? q / 16 : -1 - ((-1 - q) / 16);
            }
            q *= inverted ? -1 : 1;
            real = i * c - q * s;
            imag = i * s + q * c;
            if (mode == 8) {
                real = real < 0 ? -((-real + 8) / 16) : (real + 8) / 16;
                imag = imag < 0 ? -((-imag + 8) / 16) : (imag + 8) / 16;
            }
            sumI += (double)real * (mode == 16 ? 1.0 / 7.0 : 16.0 / 7.0);
            sumQ += (double)imag * (mode == 16 ? 1.0 / 7.0 : 16.0 / 7.0);
            CHECK(prefix[2 * k + 2] == sumI && prefix[2 * k + 3] == sumQ);
            cq = ci * dq + cq * di;
            ci = next;
        }
    }
}

/** Opt-in four-lane carrier: same mixed samples up to float rounding of a double carrier
 * that differs only by double rounding; the square carrier may flip only at zero crossings. */
static void LanesContract(void)
{
    static TNgIq8 input[2049];
    static double one[2 * 2050], four[2 * 2050];
    const size_t counts[] = {1, 3, 4, 5, 1023, 1024, 1027, 2049};
    uint32_t state = 91;
    unsigned c, mode;
    size_t n;
    for (n = 0; n < 2049; ++n) {
        input[n].i = (int8_t)(Uniform(&state) * 255 - 127);
        input[n].q = (int8_t)(Uniform(&state) * 255 - 127);
    }
    for (c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c)
        for (mode = 0; mode < 4; ++mode) {
            int square = mode & 1, inverted = (mode >> 1) & 1;
            size_t count = counts[c], flips = 0;
            double frequency = -4321.5 + 1234.0 * c + 777.0 * mode, phase = 0.61 - 0.07 * c;
            NgSearchMixPrefixCi8(one, input, count, 16368000.0, frequency, phase, square, inverted);
            NgSearchMixPrefixCi8Lanes(four, input, count, 16368000.0, frequency, phase, square,
                                      inverted);
            for (n = 1; n <= count; ++n) {
                double di = (one[2 * n] - one[2 * n - 2]) - (four[2 * n] - four[2 * n - 2]);
                double dq = (one[2 * n + 1] - one[2 * n - 1]) - (four[2 * n + 1] - four[2 * n - 1]);
                if (fabs(di) > 1e-4 || fabs(dq) > 1e-4)
                    ++flips;
            }
            CHECK(square ? flips <= 1 : flips == 0);
        }
}

/** Chip-run correlation against the per-sample replica oracle. Runs must follow the exact
 * sample-to-chip assignment (a misplaced sample changes a sum by a whole sample); sums may
 * differ only by double rounding, bounded here by 1e-12 of the summed magnitudes. */
static void RunCorrelationContract(void)
{
    static TNgComplex values[2048];
    static double phases[2048], prefix[2 * 2049];
    static float replica[2048];
    static int8_t primary[4092], expanded[8184];
    const double rates[] = {2048000.0, 4092000.0, 16000000.0, 16368000.0, 20000000.0};
    const size_t counts[] = {1, 2, 3, 37, 2048};
    uint32_t state = 4242;
    unsigned trial, k;
    for (k = 0; k < 4092; ++k) {
        primary[k] = Uniform(&state) < 0.5 ? 1 : -1;
        expanded[2 * k] = primary[k];
        expanded[2 * k + 1] = (int8_t)-primary[k];
    }
    for (k = 0; k < 2048; ++k) {
        values[k].i = (float)((Uniform(&state) - 0.5) * 200.0);
        values[k].q = (float)((Uniform(&state) - 0.5) * 200.0);
    }
    NgSearchPrefix(prefix, values, 2048);
    for (trial = 0; trial < 4000; ++trial) {
        int boc = (int)(trial & 1u);
        unsigned period = boc ? 4092u : 1023u;
        double rate = rates[(trial / 2) % 5];
        size_t count = counts[(trial / 10) % 5], n;
        /* Every fourth trial puts chip boundaries exactly on samples. */
        int exact = (trial / 50) % 4 == 0;
        double codeRate = exact ? rate / 16.0
                                : 1023000.0 + (Uniform(&state) - 0.5) * 6000.0;
        double span = (double)count * codeRate / rate;
        double base = exact ? (double)(trial % 7) : Uniform(&state) * ((double)period - span);
        double offset = exact ? ((double)(trial % 9) - 4.0) * 0.0625
                              : (Uniform(&state) - 0.5) * 4.0;
        double i, q, ri, rq, magnitudeI = 0, magnitudeQ = 0;
        if (exact && span > period - base)
            base = 0;
        NgTrackingCodePhases(phases, count, base, codeRate, rate);
        NgTrackingReplica(replica, phases, count, boc ? expanded : primary, period, offset, boc);
        NgSearchCorrelate(values, replica, count, 0, &ri, &rq);
        NgTrackingCorrelateRuns(prefix, count, base, codeRate, rate, boc ? expanded : primary,
                                period, offset, boc, &i, &q);
        for (n = 0; n < count; ++n) {
            magnitudeI += fabs(values[n].i);
            magnitudeQ += fabs(values[n].q);
        }
        CHECK(fabs(i - ri) <= 1e-12 * magnitudeI && fabs(q - rq) <= 1e-12 * magnitudeQ);
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
    CHECK(hidden.tapCount == 3 && hidden.dopplerHz == a.dopplerHz &&
          hidden.codeRateHz == a.codeRateHz);
    for (k = 0; k < 3; ++k)
        CHECK(hidden.i[k] == a.i[k + 3] && hidden.q[k] == a.q[k + 3]);
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
    MixPrefixParity();
    QuantizedMixContract();
    RunCorrelationContract();
    LanesContract();
    Convergence(1, 0, 1, 0.05, 1, 0);
    Convergence(1, 1, -1, -0.05, 1, 0);
    Convergence(2, 0, 1, 0.05, 1, 0);
    Convergence(2, 1, -1, -0.05, 1, 0);
    Convergence(2, 0, 1, 0.5, 1, 0);
    Convergence(2, 1, -1, -0.5, 1, 0);
    Convergence(1, 0, 1, 0.12, 40, -0.5);
    Convergence(2, 0, 1, 0.05, 40, -0.25);
    puts("tracking contracts passed");
    return 0;
}
