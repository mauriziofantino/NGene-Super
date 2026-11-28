/* Direct int32 FFT qualification. Keeps setup, disk I/O and timing separate. */
#include "dsp/search_plan.h"
#include "ngene/dsp/fft.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SIZE 16384
#define Q14_LIMIT 16384
#define SEED UINT32_C(20261121)
#define VECTOR_COUNT 8
#define TAU 6.28318530717958647692

static int32_t input[2 * MAX_SIZE];
static int32_t output[2 * MAX_SIZE];

static void MakeVector(size_t n, unsigned vector)
{
    uint32_t random = SEED + vector;
    for (size_t k = 0; k < 2 * n; ++k) {
        switch (vector) {
        case 0:
            input[k] = 0;
            break;
        case 1:
            input[k] = k == 2 * (n / 3) ? Q14_LIMIT : 0;
            break;
        case 2: {
            double phase = TAU * 17.375 * (double)(k / 2) / (double)n;
            input[k] = (int32_t)lround(8192 * ((k & 1) ? sin(phase) : cos(phase)));
            break;
        }
        case 3:
            input[k] = (k & 2) ? -Q14_LIMIT : Q14_LIMIT;
            break;
        case 4:
            input[k] = -Q14_LIMIT;
            break;
        case 5:
            input[k] = (int32_t)(k % 17) - 8;
            break;
        default:
            random = random * UINT32_C(1664525) + UINT32_C(1013904223);
            input[k] = (int32_t)((random >> 8) % (2 * Q14_LIMIT + 1)) - Q14_LIMIT;
            break;
        }
    }
}

static void Execute(TNgSearchFft *plan, int inverse)
{
    TNgError error = {{0}};
    if (NgSearchFftExecuteInt32(plan, input, output, inverse, &error) != NG_OK) {
        fprintf(stderr, "%s\n", error.message);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv)
{
    FILE *result = NULL;
    const int plansOnly = argc == 2 && strcmp(argv[1], "--plans") == 0;
    if (argc > 2)
        return EXIT_FAILURE;
    if (argc == 2 && !plansOnly && !(result = fopen(argv[1], "wb")))
        return EXIT_FAILURE;
    for (size_t n = NG_FFT_MIN_SIZE; n <= MAX_SIZE; n *= 2) {
        TNgError error = {{0}};
        TNgSearchFft *plan = NULL;
        if (plansOnly) {
            double elapsed = 0;
            for (unsigned trial = 0; trial < 32; ++trial) {
                const double start = NgMonotonicSeconds();
                if (NgSearchFftCreateForMode(n, NG_SEARCH_FFT_INTEGER_PRECISE, &plan, &error) !=
                    NG_OK) {
                    fprintf(stderr, "%s\n", error.message);
                    return EXIT_FAILURE;
                }
                elapsed += NgMonotonicSeconds() - start;
                NgSearchFftDestroy(plan);
            }
            printf("%zu,%.9g\n", n, elapsed / 32);
            continue;
        }
        if (NgSearchFftCreateForMode(n, NG_SEARCH_FFT_INTEGER_PRECISE, &plan, &error) != NG_OK) {
            fprintf(stderr, "%s\n", error.message);
            return EXIT_FAILURE;
        }
        for (int inverse = 0; inverse <= 1; ++inverse) {
            if (result) {
                for (unsigned vector = 0; vector < VECTOR_COUNT; ++vector) {
                    MakeVector(n, vector);
                    Execute(plan, inverse);
                    if (fwrite(output, sizeof(*output), 2 * n, result) != 2 * n)
                        return EXIT_FAILURE;
                }
            } else {
                const size_t trials = n < 1024 ? 512 : 524288 / n;
                MakeVector(n, VECTOR_COUNT - 1);
                for (unsigned k = 0; k < 5; ++k)
                    Execute(plan, inverse);
                double start = NgMonotonicSeconds();
                for (size_t k = 0; k < trials; ++k)
                    Execute(plan, inverse);
                printf("%zu,%d,%zu,%.9g\n", n, inverse, trials,
                       (NgMonotonicSeconds() - start) / (double)trials);
            }
        }
        NgSearchFftDestroy(plan);
    }
    return result && fclose(result) != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
