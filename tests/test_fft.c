/** @file test_fft.c @brief Assembly/reference equivalence and independent DFT checks. */
#include "ngene/dsp/fft.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static int16_t input[32768];
static int16_t actual[32768];
static int16_t reference[32768];

typedef struct {
    TNgFftPlan *plan;
    const int16_t *input;
    const int16_t *expected;
    int ok;
} TNgParallelCase;

/**
 * @brief Verify parallel fft behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 */
static void ParallelFft(void *context)
{
    TNgParallelCase *c = context;
    int16_t out[2048];
    TNgError error = {{0}};
    unsigned repeat;
    c->ok = 1;
    for (repeat = 0; repeat < 100; ++repeat) {
        if (NgFftExecute(c->plan, 1024, c->input, out, &error) != NG_OK ||
            memcmp(out, c->expected, sizeof(out)) != 0) {
            c->ok = 0;
            return;
        }
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
    TNgFftPlan *p;
    TNgError e = {{0}};
    size_t n;
    size_t i;
    size_t k;
    uint32_t random = 17;
    CHECK(NgFftCreate(16384, &p, &e) == NG_OK);
    for (n = 8; n <= 16384; n *= 2) {
        unsigned trial;
        for (trial = 0; trial < 7; ++trial) {
            for (i = 0; i < 2 * n; ++i) {
                random = random * 1664525u + 1013904223u;
                if (trial == 0) {
                    input[i] = 0;
                } else if (trial == 1) {
                    input[i] = (i % 2) != 0 ? -16384 : 16384;
                } else if (trial == 2) {
                    input[i] = i == 0 ? 8192 : 0;
                } else if (trial == 3) {
                    input[i] = (int16_t)((int)(random % 32769) - 16384);
                } else if (trial == 4) {
                    /* Independent I/Q rails exercise the maximum allowed complex magnitude. */
                    input[i] = (random & 0x80000000u) != 0 ? 16384 : -16384;
                } else if (trial == 5) {
                    /* Odd positive/negative values catch signed floor-average rounding errors. */
                    static const int16_t odd[] = {-16383, -1, 1, 16383};
                    input[i] = odd[(random >> 24) & 3u];
                } else {
                    /* Rotating corners drive legal intermediate components beyond 16384. */
                    double phase = 6.283185307179586 * 3 * (double)(i / 2) / (double)n;
                    double component = (i % 2) != 0 ? sin(phase) : cos(phase);
                    input[i] = component >= 0 ? 16384 : -16384;
                }
            }
            CHECK(NgFftExecute(p, n, input, actual, &e) == NG_OK);
            CHECK(NgFftReference(p, n, input, reference, &e) == NG_OK);
            CHECK(memcmp(actual, reference, 2 * n * sizeof(int16_t)) == 0);
        }
        /* Positive complex tone: frequency sign, natural ordering and /N gain. */
        for (i = 0; i < n; ++i) {
            double a = 6.283185307179586 * 3 * (double)i / (double)n;
            input[2 * i] = (int16_t)lround(8000 * cos(a));
            input[2 * i + 1] = (int16_t)lround(8000 * sin(a));
        }
        CHECK(NgFftExecute(p, n, input, actual, &e) == NG_OK);
        CHECK(abs(actual[6] - 8000) < 20 && abs(actual[7]) < 20);
        if (n <= 64) {
            for (k = 0; k < n; ++k) {
                double re = 0;
                double im = 0;
                for (i = 0; i < n; ++i) {
                    double a = -6.283185307179586 * (double)(i * k) / (double)n;
                    re += input[2 * i] * cos(a) - input[2 * i + 1] * sin(a);
                    im += input[2 * i] * sin(a) + input[2 * i + 1] * cos(a);
                }
                CHECK(fabs(actual[2 * k] - re / (double)n) < 10);
                CHECK(fabs(actual[2 * k + 1] - im / (double)n) < 10);
            }
        }
    }
    {
        TNgThread threads[2];
        TNgParallelCase cases[2];
        unsigned j;
        for (i = 0; i < 2048; ++i) {
            input[i] = (int16_t)((int)(i % 16384) - 8192);
        }
        CHECK(NgFftReference(p, 1024, input, reference, &e) == NG_OK);
        for (j = 0; j < 2; ++j) {
            cases[j].plan = p;
            cases[j].input = input;
            cases[j].expected = reference;
            cases[j].ok = 0;
            CHECK(NgThreadStart(&threads[j], ParallelFft, &cases[j]) == 0);
        }
        for (j = 0; j < 2; ++j) {
            NgThreadJoin(threads[j]);
            CHECK(cases[j].ok);
        }
    }
    {
        int16_t guarded[36];
        for (i = 0; i < 36; ++i) {
            guarded[i] = 12345;
        }
        CHECK(NgFftExecute(p, 16, input, guarded + 1, &e) == NG_OK);
        CHECK(guarded[0] == 12345 && guarded[33] == 12345);
        CHECK(NgFftReference(p, 16, input, reference, &e) == NG_OK);
        CHECK(memcmp(guarded + 1, reference, 32 * sizeof(int16_t)) == 0);
        CHECK(NgFftExecute(p, 16, input + 1, guarded + 1, &e) == NG_OK);
        CHECK(NgFftReference(p, 16, input + 1, reference, &e) == NG_OK);
        CHECK(memcmp(guarded + 1, reference, 32 * sizeof(int16_t)) == 0);
        CHECK(guarded[0] == 12345 && guarded[33] == 12345);
        CHECK(NgFftExecute(p, 16, input, input + 2, &e) == NG_ERROR);
    }
    input[0] = 32767;
    CHECK(NgFftExecute(p, 8, input, actual, &e) == NG_ERROR);
    CHECK(NgFftExecute(p, 12, input, actual, &e) == NG_ERROR);
    {
        /* Invalid components in every SIMD lane and the final vector must be rejected. */
        size_t positions[] = {0, 1, 2, 3, 4, 5, 6, 7, 32767};
        size_t position;
        memset(input, 0, sizeof(input));
        for (position = 0; position < sizeof(positions) / sizeof(positions[0]); ++position) {
            input[positions[position]] = -16385;
            CHECK(NgFftExecute(p, 16384, input, actual, &e) == NG_ERROR);
            input[positions[position]] = 16385;
            CHECK(NgFftExecute(p, 16384, input, actual, &e) == NG_ERROR);
            input[positions[position]] = 0;
        }
    }
    if (argc > 1 && strcmp(argv[1], "--benchmark") == 0) {
        for (n = 1024; n <= 16384; n *= 4) {
            double t;
            double assembly;
            double scalar;
            unsigned repeat = 300;
            memset(input, 0, sizeof(input));
            input[0] = 8000;
            t = NgMonotonicSeconds();
            for (i = 0; i < repeat; ++i) {
                CHECK(NgFftExecute(p, n, input, actual, &e) == NG_OK);
            }
            assembly = (NgMonotonicSeconds() - t) * 1e6 / repeat;
            t = NgMonotonicSeconds();
            for (i = 0; i < repeat; ++i) {
                CHECK(NgFftReference(p, n, input, reference, &e) == NG_OK);
            }
            scalar = (NgMonotonicSeconds() - t) * 1e6 / repeat;
            printf("N=%zu assembly=%.2f us reference=%.2f us ratio=%.2f\n", n, assembly, scalar,
                   scalar / assembly);
        }
    }
    printf("PASS: %s, every size 8..16384, bit-exact scalar and DFT checks\n", NgFftBackend());
    NgFftDestroy(p);
    return 0;
}
