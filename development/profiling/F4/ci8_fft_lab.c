/* Offline CI8 FFT arithmetic comparison. Output errors use ADC units / N. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static double Seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    return (double)clock() / CLOCKS_PER_SEC;
#endif
}

#define MAX_N 16384
#define TAU 6.28318530717958647692

#ifdef CI8_LAB_AVX2
#include "dsp/x86/avx2_available.h"
void Ci8WideQ15StageAvx2(int64_t *, const int64_t *, const int64_t *, size_t, size_t);
static int use_avx2;
#endif
static FILE *words;
static double ticks[2][3];
static int8_t input[2 * MAX_N];
static int64_t fixed[2 * MAX_N];
static double reference[2 * MAX_N];
static double cosine[MAX_N / 2], sine[MAX_N / 2];
static int64_t wr[MAX_N / 2], wi[MAX_N / 2];

static int64_t RoundDivide(int64_t value, int64_t divisor)
{
    return (value + (value < 0 ? -divisor / 2 : divisor / 2)) / divisor;
}

static void Prepare(size_t n, int inverse, int64_t unit)
{
    for (size_t k = 0; k < n / 2; ++k) {
        double angle = (inverse ? TAU : -TAU) * (double)k / (double)n;
        cosine[k] = cos(angle);
        sine[k] = sin(angle);
        /* Store both endpoints exactly using a wider coefficient container. */
        wr[k] = llround(cosine[k] * (double)unit);
        wi[k] = llround(sine[k] * (double)unit);
    }
    for (size_t k = 0, reversed = 0; k < n; ++k) {
        for (size_t part = 0; part < 2; ++part) {
            reference[2 * reversed + part] = input[2 * k + part];
            fixed[2 * reversed + part] = input[2 * k + part];
        }
        size_t bit = n / 2;
        while (bit && (reversed & bit)) {
            reversed ^= bit;
            bit /= 2;
        }
        reversed ^= bit;
    }
}

static int use_compact;
static int8_t compact[2 * MAX_N];

static int8_t ShiftAndClip(int64_t value, unsigned *clips)
{
    value = RoundDivide(value, 2);
    *clips += value < -128 || value > 127;
    return (int8_t)(value < -128 ? -128 : value > 127 ? 127 : value);
}

static void CompactStage(size_t n, size_t width, int64_t unit, unsigned *clips)
{
    for (size_t base = 0; base < n; base += width) {
        for (size_t j = 0; j < width / 2; ++j) {
            size_t a = 2 * (base + j), b = a + width, k = j * (n / width);
            int64_t r = RoundDivide(compact[b] * wr[k] - compact[b + 1] * wi[k], unit);
            int64_t i = RoundDivide(compact[b] * wi[k] + compact[b + 1] * wr[k], unit);
            compact[b] = ShiftAndClip(compact[a] - r, clips);
            compact[b + 1] = ShiftAndClip(compact[a + 1] - i, clips);
            compact[a] = ShiftAndClip(compact[a] + r, clips);
            compact[a + 1] = ShiftAndClip(compact[a + 1] + i, clips);
        }
    }
}
static void Run(size_t n, unsigned pattern, int inverse, int bits, int mode)
{
    const int64_t unit = INT64_C(1) << bits;
    const int64_t input_scale = mode == 0 ? 256 : 1;
    unsigned shift = 0;
    unsigned clips = 0;
    Prepare(n, inverse, unit);
    for (size_t k = 0; k < 2 * n; ++k)
        fixed[k] *= input_scale;

    for (size_t width = 2; width <= n; width *= 2) {
        for (size_t base = 0; base < n; base += width) {
            for (size_t j = 0; j < width / 2; ++j) {
                size_t a = 2 * (base + j), b = a + width, k = j * (n / width);
                double real = reference[b] * cosine[k] - reference[b + 1] * sine[k];
                double imag = reference[b] * sine[k] + reference[b + 1] * cosine[k];
                reference[b] = reference[a] - real;
                reference[b + 1] = reference[a + 1] - imag;
                reference[a] += real;
                reference[a + 1] += imag;
            }
        }
    }
    const int compact_case = use_compact && mode == 1;
    if (compact_case)
        for (size_t k = 0; k < 2 * n; ++k)
            compact[k] = (int8_t)fixed[k];
    const double started = Seconds();
    for (size_t width = 2; width <= n; width *= 2) {
        if (compact_case) {
            CompactStage(n, width, unit, &clips);
            ++shift;
            continue;
        }
#ifdef CI8_LAB_AVX2
        if (use_avx2 && mode == 0 && bits == 15 && width >= 4) {
            Ci8WideQ15StageAvx2(fixed, wr, wi, n, width);
            continue;
        }
#endif
        for (size_t base = 0; base < n; base += width) {
            for (size_t j = 0; j < width / 2; ++j) {
                size_t a = 2 * (base + j), b = a + width, k = j * (n / width);
                int64_t r = RoundDivide(fixed[b] * wr[k] - fixed[b + 1] * wi[k], unit);
                int64_t i = RoundDivide(fixed[b] * wi[k] + fixed[b + 1] * wr[k], unit);
                fixed[b] = fixed[a] - r;
                fixed[b + 1] = fixed[a + 1] - i;
                fixed[a] += r;
                fixed[a + 1] += i;
            }
        }
        if (mode == 0)
            continue;
        int64_t divisor = mode == 1 ? 2 : 1;
        for (size_t k = 0; mode == 2 && k < 2 * n; ++k) {
            int64_t value = RoundDivide(fixed[k], divisor);
            while (value < -128 || value > 127) {
                divisor *= 2;
                value = RoundDivide(fixed[k], divisor);
            }
        }
        for (int64_t d = divisor; d > 1; d /= 2)
            ++shift;
        for (size_t k = 0; k < 2 * n; ++k) {
            int64_t value = RoundDivide(fixed[k], divisor);
            clips += value < -128 || value > 127;
            fixed[k] = value < -128 ? -128 : value > 127 ? 127 : value;
        }
    }
    const double elapsed = Seconds() - started;
    if (compact_case)
        for (size_t k = 0; k < 2 * n; ++k)
            fixed[k] = compact[k];
    if (words && fwrite(fixed, sizeof *fixed, 2 * n, words) != 2 * n)
        exit(EXIT_FAILURE);
    double max_error = 0, sum = 0;
    for (size_t k = 0; k < 2 * n; ++k) {
        double error = (ldexp((double)fixed[k], shift) / input_scale - reference[k]) / n;
        max_error = fmax(max_error, fabs(error));
        sum += error * error;
    }
    ticks[(bits - 7) / 8][mode] += elapsed;
    printf("%zu,%u,%d,%d,%d,%u,%u,%.9g,%.9g\n", n, pattern, inverse, bits, mode,
           shift, clips, max_error, sqrt(sum / (2 * n)));
}

int main(int argc, char **argv)
{
#ifdef CI8_LAB_AVX2
    if (argc == 3 && strcmp(argv[2], "avx2") == 0) {
        if (!NgDspAvx2Available())
            return 2;
        use_avx2 = 1;
    }
#endif
    use_compact = argc == 3 && strcmp(argv[2], "compact") == 0;
    if (argc >= 2 && !(words = fopen(argv[1], "wb")))
        return 1;
    puts("n,pattern,inverse,twiddle_bits,mode,shift,clips,max_error,rms_error");
    for (size_t n = 8; n <= MAX_N; n *= 2) {
        for (unsigned pattern = 0; pattern < 4; ++pattern) {
            uint32_t random = UINT32_C(20261124);
            for (size_t k = 0; k < 2 * n; ++k) {
                random = random * UINT32_C(1664525) + UINT32_C(1013904223);
                double angle = TAU * 2.375 * (double)(k / 2) / (double)n;
                int value = pattern == 0 ? (k == 2 * (n / 3) ? -128 : 0)
                          : pattern == 1 ? ((k & 2) ? -128 : 127)
                          : pattern == 2 ? (int)lround(127 * ((k & 1) ? sin(angle) : cos(angle)))
                          : (int)(random >> 24) - 128;
                input[k] = (int8_t)value;
            }
            for (int inverse = 0; inverse <= 1; ++inverse)
                for (int bits = 7; bits <= 15; bits += 8)
                    for (int mode = 0; mode < 3; ++mode)
                        Run(n, pattern, inverse, bits, mode);
        }
    }
    for (int q = 0; q < 2; ++q)
        for (int mode = 0; mode < 3; ++mode)
            fprintf(stderr, "%d,%d,%.9f\n", 7 + 8 * q, mode, ticks[q][mode]);
    if (words && fclose(words) != 0)
        return 1;
    return 0;
}
