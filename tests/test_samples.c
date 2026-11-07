/** @file test_samples.c @brief Signed sample math, continuous DC and benchmark. */
#include "ngene/dsp/sample_ops.h"
#include "fixtures.h"
#include "ngene/conditioning.h"
#include "ngene/frontend.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static TNgIq8 samples[262145];
static volatile uint64_t benchmarkSink;
static int16_t iq[32770];
static uint64_t p[16385];
static uint64_t q[16385];

/**
 * @brief Verify benchmark behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 */
static void Benchmark(void)
{
    size_t n = 16000;
    size_t j;
    int64_t sum[2];
    uint64_t square[2];
    double t;
    double scalar;
    double simd;
    t = NgMonotonicSeconds();
    for (j = 0; j < 10000; ++j) {
        NgIqMomentsReference(samples, n, sum, square);
        benchmarkSink += square[0];
    }
    scalar = (NgMonotonicSeconds() - t) * 1e6 / 10000;
    t = NgMonotonicSeconds();
    for (j = 0; j < 10000; ++j) {
        NgIqMoments(samples, n, sum, square);
        benchmarkSink += square[0];
    }
    simd = (NgMonotonicSeconds() - t) * 1e6 / 10000;
    printf("moments %zu CI8: reference %.3f us, %s %.3f us, %.2fx\n", n, scalar,
           NgSampleOpsBackend(), simd, scalar / simd);
    t = NgMonotonicSeconds();
    for (j = 0; j < 10000; ++j) {
        NgPowerAccumulateReference(iq, p, n);
    }
    scalar = (NgMonotonicSeconds() - t) * 1e6 / 10000;
    t = NgMonotonicSeconds();
    for (j = 0; j < 10000; ++j) {
        NgPowerAccumulate(iq, q, n);
    }
    simd = (NgMonotonicSeconds() - t) * 1e6 / 10000;
    printf("power %zu CI16: reference %.3f us, %s %.3f us, %.2fx\n", n, scalar,
           NgSampleOpsBackend(), simd, scalar / simd);
}

/**
 * @brief Run the C contract test executable.
 *
 * @details
 * Assertions abort with a nonzero process status on contract failures. This executable is
 * development tooling, not receiver runtime.
 *
 * @param argc Command-line argument count.
 * @param argv Argument vector.
 *
 * @return Zero if every contract assertion passes.
 */
static void TestBlockDc(void)
{
    static const size_t lengths[] = {1, 3, 4, 7, 8, 9, 16, 31, 257, 4097, 262144};
    unsigned char *allocation = malloc(4 * 262144 + 1), *raw;
    int32_t *storage = malloc((2 * 262144 + 2) * sizeof(*storage)), *out;
    TNgSampleFormat format;
    TNgError error = {{0}};
    size_t k, t;
    CHECK(allocation && storage);
    raw = allocation + 1;
    out = storage + 1; /* SIMD-unaligned input/output. */
    for (format = NG_FORMAT_CI8; format <= NG_FORMAT_CI16; format = (TNgSampleFormat)(format + 1)) {
        int limit = format == NG_FORMAT_CI16 ? 32768 : format == NG_FORMAT_SC16_Q11 ? 2048 : 128;
        TNgIqView view = {0};
        view.samples = (const TNgIq8 *)(const void *)raw;
        view.format = format;
        for (k = 0; k < 262144; ++k) {
            NgIqStore(raw, format, k, 0, (int)((k * 271) % (unsigned)(2 * limit)) - limit);
            NgIqStore(raw, format, k, 1, (int)((k * 73 + 17) % (unsigned)(2 * limit)) - limit);
        }
        for (t = 0; t < sizeof(lengths) / sizeof(lengths[0]); ++t) {
            int64_t sum[2] = {0, 0};
            int dc[2], expected[2];
            unsigned c;
            view.meta.count = lengths[t];
            for (k = 0; k < view.meta.count; ++k)
                for (c = 0; c < 2; ++c)
                    sum[c] += NgIqComponent(&view, k, c);
            for (c = 0; c < 2; ++c)
                expected[c] = (int)llround((double)sum[c] / (double)view.meta.count);
            storage[0] = storage[2 * view.meta.count + 1] = 1234567;
            CHECK(NgIqCenterBlock(&view, out, dc, &error) == NG_OK);
            CHECK(dc[0] == expected[0] && dc[1] == expected[1]);
            for (k = 0; k < view.meta.count; ++k)
                for (c = 0; c < 2; ++c)
                    CHECK(out[2 * k + c] == NgIqComponent(&view, k, c) - expected[c]);
            CHECK(storage[0] == 1234567 && storage[2 * view.meta.count + 1] == 1234567);
            dc[0] = limit - 1;
            dc[1] = -limit;
            CHECK(NgIqSubtractDc(&view, out, dc, &error) == NG_OK);
            for (k = 0; k < view.meta.count; ++k)
                for (c = 0; c < 2; ++c)
                    CHECK(out[2 * k + c] == NgIqComponent(&view, k, c) - dc[c]);
            CHECK(storage[0] == 1234567 && storage[2 * view.meta.count + 1] == 1234567);
        }
        /* Extreme mean and an opposite rail: widening must preserve excursions
         * beyond the original ADC container, without invented clipping. */
        for (k = 0; k < 262144; ++k) {
            NgIqStore(raw, format, k, 0, limit - 1);
            NgIqStore(raw, format, k, 1, -limit);
        }
        NgIqStore(raw, format, 0, 0, -limit);
        {
            int dc[2];
            CHECK(NgIqCenterBlock(&view, out, dc, &error) == NG_OK);
            CHECK(dc[0] == limit - 1 && dc[1] == -limit);
            CHECK(out[0] == -2 * limit + 1 && out[1] == 0 && out[2] == 0);
        }
    }
    free(storage);
    free(allocation);
}

int main(int argc, char **argv)
{
    size_t k;
    size_t n;
    TestBlockDc();
    int64_t sum[2];
    int64_t refsum[2];
    uint64_t sq[2];
    uint64_t refsq[2];
    TNgError e = {{0}};
    for (k = 0; k < 262145; ++k) {
        samples[k].i = (int8_t)((int)(k % 256) - 128);
        samples[k].q = (int8_t)(127 - (int)((k * 7) % 256));
    }
    for (n = 0; n < 4097; ++n) {
        NgIqMoments(samples + 1, n, sum, sq);
        NgIqMomentsReference(samples + 1, n, refsum, refsq);
        CHECK(!memcmp(sum, refsum, sizeof(sum)) && !memcmp(sq, refsq, sizeof(sq)));
    }
    NgIqMoments(samples, 262144, sum, sq);
    NgIqMomentsReference(samples, 262144, refsum, refsq);
    CHECK(!memcmp(sum, refsum, sizeof(sum)) && !memcmp(sq, refsq, sizeof(sq)));
    CHECK(sum[0] == -131072); /* Uniform all 256 codes has mean -0.5, not +127.5. */
    for (k = 0; k < 32770; ++k) {
        iq[k] = (int16_t)((int)((k * 271) % 65536) - 32768);
    }
    iq[0] = iq[1] = INT16_MIN;
    NgPowerAccumulate(iq, p, 16385);
    NgPowerAccumulateReference(iq, q, 16385);
    CHECK(p[0] == UINT64_C(2147483648));
    CHECK(!memcmp(p, q, sizeof(p)));
    {
        TNgConfig c;
        TNgFrontend f;
        NgConfigDefaults(&c);
        strcpy(c.replayPath, "ngene_test_tuning.cs8");
        CHECK(NgTestWriteTone(c.replayPath, 8, c.sampleRateHz) == NG_OK);
        c.centerFrequencyHz = 1576420000;
        c.signalFrequencyHz = 1575420000;
        CHECK(NgFrontendOpen(&c, &f, &e) == NG_OK);
        CHECK(f.info.digitalIfHz == -1000000);
        CHECK(NgCarrierFromDoppler(&f.info, 2500) == -997500);
        CHECK(NgDopplerFromCarrier(&f.info, -997500) == 2500);
        NgFrontendDestroy(&f);
        CHECK(remove(c.replayPath) == 0);
    }
    if (argc > 1 && strcmp(argv[1], "--benchmark") == 0) {
        Benchmark();
    }
    puts("PASS: signed CI8 moments, full-range CI16 power, tuning "
         "metadata");
    return 0;
}
