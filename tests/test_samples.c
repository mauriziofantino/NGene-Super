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
static TNgIq8 output[4096];
static int16_t iq[32770];
static uint64_t p[16385];
static uint64_t q[16385];
static volatile uint64_t benchmarkSink;

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
    {
        TNgConditioner dc;
        TNgError e = {{0}};
        TNgIqView in = {samples, {0}};
        TNgIqView out;
        in.meta.count = n;
        CHECK(NgConditionerDcCreate(n, 16, &dc, &e) == NG_OK);
        t = NgMonotonicSeconds();
        for (j = 0; j < 1000; ++j) {
            in.meta.firstSample = j * n;
            CHECK(dc.process(dc.context, &in, &out, &e) == NG_OK);
            benchmarkSink += (uint8_t)out.samples[0].i;
        }
        printf("continuous DC Q32 %zu pairs: %.3f us/block\n", n,
               (NgMonotonicSeconds() - t) * 1e6 / 1000);
        NgConditionerDcDestroy(&dc);
    }
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
int main(int argc, char **argv)
{
    size_t k;
    size_t n;
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
        TNgConditioner whole;
        TNgConditioner chunks;
        TNgIqView in = {samples, {0}};
        TNgIqView out;
        CHECK(NgConditionerDcCreate(4096, 8, &whole, &e) == NG_OK);
        CHECK(NgConditionerDcCreate(4096, 8, &chunks, &e) == NG_OK);
        for (k = 0; k < 4096; ++k) {
            samples[k].i = 24;
            samples[k].q = 12;
        }
        in.meta.count = 4096;
        CHECK(whole.process(whole.context, &in, &out, &e) == NG_OK);
        memcpy(output, out.samples, sizeof(output));
        for (k = 0; k < 4096; k += 37) {
            in.samples = samples + k;
            in.meta.firstSample = k;
            in.meta.count = 4096 - k < 37 ? 4096 - k : 37;
            CHECK(chunks.process(chunks.context, &in, &out, &e) == NG_OK);
            CHECK(!memcmp(output + k, out.samples, in.meta.count * sizeof(TNgIq8)));
        }
        CHECK(abs(output[4095].i) <= 1 && abs(output[4095].q) <= 1);
        CHECK(samples[4095].i == 24 && samples[4095].q == 12);
        in.samples = samples;
        in.meta.count = 1;
        in.meta.firstSample = 10000;
        in.meta.flags = NG_BLOCK_GAP;
        CHECK(chunks.process(chunks.context, &in, &out, &e) == NG_OK);
        CHECK(out.samples[0].i == 24);
        NgConditionerDcDestroy(&whole);
        NgConditionerDcDestroy(&chunks);
    }
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
    puts("PASS: signed CI8 moments, full-range CI16 power, DC chunk invariance/gap, tuning "
         "metadata");
    return 0;
}
