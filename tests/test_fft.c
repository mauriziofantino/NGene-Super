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

/** @brief Compare public backends at independent natural alignments in guarded storage. */
static void CheckGuardedTransformAtOffsets(TNgFftPlan *plan, size_t n, size_t inputLane,
                                           size_t outputLane)
{
    static int16_t guardedInput[32768 + 16];
    static int16_t guardedOutput[32768 + 16];
    const int16_t canary = 23130;
    const size_t inputOffset =
        inputLane + (size_t)((16u - ((uintptr_t)guardedInput & 15u)) & 15u) / sizeof(int16_t);
    const size_t outputOffset =
        outputLane + (size_t)((16u - ((uintptr_t)guardedOutput & 15u)) & 15u) / sizeof(int16_t);
    const size_t count = 2 * n + 16;
    TNgError error = {{0}};
    size_t i;
    memset(guardedInput, 0x5a, count * sizeof(*guardedInput));
    memset(guardedOutput, 0x5a, count * sizeof(*guardedOutput));
    memcpy(guardedInput + inputOffset, input, 2 * n * sizeof(*input));
    CHECK(((uintptr_t)(guardedInput + inputOffset) & 15u) == inputLane * sizeof(int16_t));
    CHECK(((uintptr_t)(guardedOutput + outputOffset) & 15u) == outputLane * sizeof(int16_t));
    CHECK(NgFftReference(plan, n, input, reference, &error) == NG_OK);
    CHECK(NgFftExecute(plan, n, guardedInput + inputOffset, guardedOutput + outputOffset, &error) ==
          NG_OK);
    CHECK(memcmp(guardedInput + inputOffset, input, 2 * n * sizeof(*input)) == 0);
    for (i = 0; i < 2 * n; ++i) {
        if (guardedOutput[outputOffset + i] != reference[i]) {
            fprintf(stderr, "FFT mismatch: N=%zu component=%zu actual=%d reference=%d\n", n, i,
                    (int)guardedOutput[outputOffset + i], (int)reference[i]);
            CHECK(guardedOutput[outputOffset + i] == reference[i]);
        }
    }
    for (i = 0; i < inputOffset; ++i) {
        CHECK(guardedInput[i] == canary);
    }
    for (i = inputOffset + 2 * n; i < count; ++i) {
        CHECK(guardedInput[i] == canary);
    }
    for (i = 0; i < outputOffset; ++i) {
        CHECK(guardedOutput[i] == canary);
    }
    for (i = outputOffset + 2 * n; i < count; ++i) {
        CHECK(guardedOutput[i] == canary);
    }
}

/** @brief Keep the exhaustive arithmetic corpus deliberately two bytes off vector alignment. */
static void CheckGuardedTransform(TNgFftPlan *plan, size_t n)
{
    CheckGuardedTransformAtOffsets(plan, n, 1, 1);
}

/** @brief Exercise every input/output lane alignment independently on every transform width. */
static void CheckNaturalAlignments(TNgFftPlan *plan)
{
    uint32_t random = 0x41524d32u;
    size_t n;
    for (n = 8; n <= 16384; n *= 2) {
        size_t i;
        size_t inputLane;
        size_t outputLane;
        for (i = 0; i < 2 * n; ++i) {
            random = random * 1664525u + 1013904223u;
            input[i] = (int16_t)((int)(random % 32769u) - 16384);
        }
        for (inputLane = 0; inputLane < 8; ++inputLane) {
            for (outputLane = 0; outputLane < 8; ++outputLane) {
                CheckGuardedTransformAtOffsets(plan, n, inputLane, outputLane);
            }
        }
    }
}

/** @brief Validate signed input bounds in every first/last SIMD lane, including int16 extrema. */
static void CheckInvalidVectorLanes(TNgFftPlan *plan)
{
    static const int16_t invalid[] = {-32768, -16385, 16385, 32767};
    TNgError error = {{0}};
    size_t n;
    memset(input, 0, sizeof(input));
    for (n = 8; n <= 16384; n *= 2) {
        size_t lane;
        size_t laneCount = 2 * n < 32 ? 2 * n : 32;
        for (lane = 0; lane < laneCount; ++lane) {
            size_t side;
            for (side = 0; side < 2; ++side) {
                size_t position = side == 0 ? lane : 2 * n - 1 - lane;
                size_t value;
                for (value = 0; value < sizeof(invalid) / sizeof(invalid[0]); ++value) {
                    input[position] = invalid[value];
                    CHECK(NgFftExecute(plan, n, input, actual, &error) == NG_ERROR);
                    CHECK(NgFftReference(plan, n, input, reference, &error) == NG_ERROR);
                }
                input[position] = 0;
            }
        }
    }
}

/** @brief Exhaust the eight-point codelet's ternary rails/odd corpus and its rotated inputs. */
static void CheckEightPointCorpus(TNgFftPlan *plan)
{
    static const int16_t pairs[3][2] = {{-16384, -16384}, {16384, -16383}, {1, -1}};
    unsigned rotation;
    unsigned code;
    for (rotation = 0; rotation < 2; ++rotation) {
        for (code = 0; code < 6561;
             ++code) { /* All 3^8 assignments of a complex pair per sample. */
            unsigned digits = code;
            size_t i;
            for (i = 0; i < 8; ++i) {
                int16_t re = pairs[digits % 3u][0];
                int16_t im = pairs[digits % 3u][1];
                unsigned quarter;
                digits /= 3;
                for (quarter = 0; quarter < rotation * (unsigned)(i & 3u); ++quarter) {
                    int16_t previousRe = re;
                    re = (int16_t)-im;
                    im = previousRe;
                }
                input[2 * i] = re;
                input[2 * i + 1] = im;
            }
            CheckGuardedTransform(plan, 8);
        }
    }
}

/** @brief Cover every plan/subsize pair, odd floors, full headroom and block boundaries. */
static void CheckPlanSubsizes(void)
{
    uint32_t random = 0x14f17e5u;
    size_t maximum;
    for (maximum = 8; maximum <= 16384; maximum *= 2) {
        TNgFftPlan *plan;
        TNgError error = {{0}};
        size_t n;
        CHECK(NgFftCreate(maximum, &plan, &error) == NG_OK);
        for (n = 8; n <= maximum; n *= 2) {
            unsigned trial;
            for (trial = 0; trial < 6; ++trial) {
                size_t i;
                for (i = 0; i < 2 * n; ++i) {
                    int value;
                    random = random * 1664525u + 1013904223u;
                    value = (int)(random % 32769u) - 16384;
                    if (trial == 1) {
                        value = 2 * (int)((random >> 8) & 16383u) - 16383;
                    } else if (trial == 2) {
                        value = (random & 0x80000000u) != 0 ? 16384 : -16384;
                    } else if (trial == 3) {
                        value = 16384 - (int)((random >> 8) & 3u);
                        if ((random & 0x80000000u) != 0) {
                            value = -value;
                        }
                    } else if (trial == 4) {
                        /* A nonzero tail detects reversed-index truncation and omitted groups. */
                        value = i / 2 == n - 1 ? value : 0;
                    } else if (trial == 5) {
                        size_t sample = i / 2;
                        /* Include both sides of 4/8-point and prospective cache-block borders. */
                        value = sample % 4 == 3 || sample % 4 == 0 || sample == n / 2 - 1 ||
                                        sample == n / 2 || sample == n / 2 + 1
                                    ? value
                                    : 0;
                    }
                    input[i] = (int16_t)value;
                }
                CheckGuardedTransform(plan, n);
            }
        }
        NgFftDestroy(plan);
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
    CheckEightPointCorpus(p);
    CheckPlanSubsizes();
    CheckNaturalAlignments(p);
    CheckInvalidVectorLanes(p);
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
    printf("PASS: %s, pointer_bits=%u, every size 8..16384, bit-exact scalar and DFT checks\n",
           NgFftBackend(), (unsigned)(sizeof(void *) * 8));
    NgFftDestroy(p);
    return 0;
}
