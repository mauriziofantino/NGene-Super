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

#ifndef CI8_LAB_WORKSPACE
#define CI8_LAB_WORKSPACE static
#endif

#ifdef CI8_LAB_LINK_KERNELS
#include "ci8_fft_kernels.h"
#else
#define CI8_FFT_API static
#include "ci8_fft_scalar.c"
#endif

#if defined(CI8_LAB_AVX2) || defined(CI8_LAB_COMPACT_AVX2)
#include "dsp/x86/avx2_available.h"
#endif
#ifdef CI8_LAB_COMPACT_AVX2
CI8_LAB_WORKSPACE int use_compact_avx2;
#endif
#ifdef CI8_LAB_AVX2
static int use_avx2;
#endif
static FILE *lab_words;
static double ticks[2][3];
static double adaptive_ticks[2][2];
CI8_LAB_WORKSPACE int8_t lab_input[2 * MAX_N];
CI8_LAB_WORKSPACE int64_t fixed[2 * MAX_N];
CI8_LAB_WORKSPACE double reference[2 * MAX_N];
CI8_LAB_WORKSPACE double cosine[MAX_N / 2], sine[MAX_N / 2];
CI8_LAB_WORKSPACE int64_t lab_wr[MAX_N / 2], lab_wi[MAX_N / 2];

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
        lab_wr[k] = llround(cosine[k] * (double)unit);
        lab_wi[k] = llround(sine[k] * (double)unit);
    }
    for (size_t k = 0, reversed = 0; k < n; ++k) {
        for (size_t part = 0; part < 2; ++part) {
            reference[2 * reversed + part] = lab_input[2 * k + part];
            fixed[2 * reversed + part] = lab_input[2 * k + part];
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
static int use_adaptive;
static int adaptive_before;
static int timing_adaptive;
static int compare_avx2;
#ifdef CI8_LAB_BUFFERED
static int compare_buffered;
static int compare_madd;
CI8_LAB_WORKSPACE int use_buffered;
CI8_LAB_WORKSPACE int use_madd;
#endif
static int adaptive_variant;
static int timing_only;
static volatile int64_t timing_sink;
CI8_LAB_WORKSPACE int8_t compact[2 * MAX_N];

#ifdef CI8_LAB_BUFFERED
CI8_LAB_WORKSPACE int16_t stage_scratch[2 * MAX_N];
#endif

static int8_t ScaleAndClip(int64_t value, int64_t divisor, unsigned *clips)
{
    value = RoundDivide(value, divisor);
    *clips += value < -128 || value > 127;
    return (int8_t)(value < -128 ? -128 : value > 127 ? 127 : value);
}

static void CompactButterfly(size_t a, size_t b, size_t k, int64_t unit, int64_t values[4])
{
    int64_t r = RoundDivide(compact[b] * lab_wr[k] - compact[b + 1] * lab_wi[k], unit);
    int64_t i = RoundDivide(compact[b] * lab_wi[k] + compact[b + 1] * lab_wr[k], unit);
    values[0] = compact[a] + r;
    values[1] = compact[a + 1] + i;
    values[2] = compact[a] - r;
    values[3] = compact[a + 1] - i;
}

static int64_t AdaptiveDivisor(size_t n, size_t width, int64_t unit, unsigned *shift)
{
    int64_t low = 0, high = 0;
    for (size_t base = 0; base < n; base += width) {
        for (size_t j = 0; j < width / 2; ++j) {
            size_t a = 2 * (base + j);
            int64_t values[4];
            CompactButterfly(a, a + width, j * (n / width), unit, values);
            for (size_t k = 0; k < 4; ++k) {
                if (values[k] < low)
                    low = values[k];
                if (values[k] > high)
                    high = values[k];
            }
        }
    }
    /* Rounding is monotone and the signed endpoints have different limits. */
    int64_t divisor = 1;
    while (RoundDivide(low, divisor) < -128 || RoundDivide(high, divisor) > 127) {
        divisor *= 2;
        ++*shift;
    }
    return divisor;
}

static void CompactStage(size_t n, size_t width, int64_t unit, int64_t divisor,
                         unsigned *clips)
{
    for (size_t base = 0; base < n; base += width) {
        for (size_t j = 0; j < width / 2; ++j) {
            size_t a = 2 * (base + j), b = a + width;
            int64_t values[4];
            CompactButterfly(a, b, j * (n / width), unit, values);
            compact[a] = ScaleAndClip(values[0], divisor, clips);
            compact[a + 1] = ScaleAndClip(values[1], divisor, clips);
            compact[b] = ScaleAndClip(values[2], divisor, clips);
            compact[b + 1] = ScaleAndClip(values[3], divisor, clips);
        }
    }
}

static void IntegerStages(size_t n, int bits, int mode, int compact_case,
                          unsigned *shift, unsigned *clips)
{
    const int64_t unit = INT64_C(1) << bits;
    for (size_t width = 2; width <= n; width *= 2) {
        if (compact_case && mode == 2 && !adaptive_before) {
#ifdef CI8_LAB_BUFFERED
            if (use_buffered && width >= 16) {
                if (use_madd)
                    *shift += Ci8FftCompactAdaptiveMaddAvx2(compact, stage_scratch, lab_wr, lab_wi,
                                                           n, width, (unsigned)bits);
                else
                    *shift += Ci8FftCompactAdaptiveBufferedAvx2(compact, stage_scratch, lab_wr, lab_wi,
                                                               n, width, (unsigned)bits);
                continue;
            }
#endif
#ifdef CI8_LAB_COMPACT_AVX2
            if (use_compact_avx2 && width >= 16) {
                *shift += Ci8FftCompactAdaptiveStageAvx2(compact, lab_wr, lab_wi, n, width,
                                                        (unsigned)bits);
                continue;
            }
#endif
            *shift += Ci8FftCompactAdaptiveStage(compact, lab_wr, lab_wi, n, width, (unsigned)bits);
            continue;
        }
        if (compact_case && mode == 1 && !adaptive_before) {
            *clips += Ci8FftCompactFixedStage(compact, lab_wr, lab_wi, n, width, (unsigned)bits);
            ++*shift;
            continue;
        }
        if (compact_case) {
            int64_t divisor = 2;
            if (mode == 2)
                divisor = AdaptiveDivisor(n, width, unit, shift);
            else
                ++*shift;
            CompactStage(n, width, unit, divisor, clips);
            continue;
        }
#ifdef CI8_LAB_AVX2
        if (use_avx2 && mode == 0 && bits == 15 && width >= 4) {
            Ci8WideQ15StageAvx2(fixed, lab_wr, lab_wi, n, width);
            continue;
        }
#endif
        Ci8FftWideStage(fixed, lab_wr, lab_wi, n, width, (unsigned)bits);
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
            ++*shift;
        for (size_t k = 0; k < 2 * n; ++k) {
            int64_t value = RoundDivide(fixed[k], divisor);
            *clips += value < -128 || value > 127;
            fixed[k] = value < -128 ? -128 : value > 127 ? 127 : value;
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
    const int compact_case = (use_compact && mode == 1) || (use_adaptive && mode == 2);
    if (compact_case)
        for (size_t k = 0; k < 2 * n; ++k)
            compact[k] = (int8_t)fixed[k];
    const double started = Seconds();
    IntegerStages(n, bits, mode, compact_case, &shift, &clips);
    const double elapsed = Seconds() - started;
    if (timing_adaptive)
        adaptive_ticks[(bits - 7) / 8][adaptive_variant] += elapsed;
    else
        ticks[(bits - 7) / 8][mode] += elapsed;
    if (timing_only) {
        timing_sink = (compact_case ? compact[0] : fixed[0]) + shift + clips;
        return;
    }
    if (compact_case)
        for (size_t k = 0; k < 2 * n; ++k)
            fixed[k] = compact[k];
    if (lab_words && fwrite(fixed, sizeof *fixed, 2 * n, lab_words) != 2 * n)
        exit(EXIT_FAILURE);
    double max_error = 0, sum = 0;
    for (size_t k = 0; k < 2 * n; ++k) {
        double error = (ldexp((double)fixed[k], shift) / input_scale - reference[k]) / n;
        max_error = fmax(max_error, fabs(error));
        sum += error * error;
    }
    printf("%zu,%u,%d,%d,%d,%u,%u,%.9g,%.9g\n", n, pattern, inverse, bits, mode,
           shift, clips, max_error, sqrt(sum / (2 * n)));
}

static void Matrix(int round)
{
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
                lab_input[k] = (int8_t)value;
            }
            for (int inverse = 0; inverse <= 1; ++inverse)
                for (int bits = 7; bits <= 15; bits += 8)
                    for (int slot = 0; slot < (timing_adaptive ? 2 : 3); ++slot) {
                        if (timing_adaptive) {
                            adaptive_variant = slot ^ (round & 1);
#ifdef CI8_LAB_BUFFERED
                            if (compare_buffered || compare_madd) {
                                use_compact_avx2 = 1;
                                use_buffered = compare_madd || adaptive_variant != 0;
                                use_madd = compare_madd && adaptive_variant == 1;
                                Run(n, pattern, inverse, bits, 2);
                                continue;
                            }
#endif
                            adaptive_before = !compare_avx2 && adaptive_variant == 0;
#ifdef CI8_LAB_COMPACT_AVX2
                            use_compact_avx2 = compare_avx2 && adaptive_variant == 1;
#endif
                            Run(n, pattern, inverse, bits, 2);
                            continue;
                        }
                        /* Each six rounds cover every order for each vector. */
                        int mode = round < 0 ? slot
                                 : (round / 2 + (round % 2 ? 3 - slot : slot)) % 3;
                        Run(n, pattern, inverse, bits, mode);
                    }
        }
    }
}

int main(int argc, char **argv)
{
    if ((argc == 2 && strcmp(argv[1], "--timing-buffered-threaded") == 0) ||
        (argc == 3 && strcmp(argv[2], "compact-buffered-threaded") == 0))
        return 2;
#ifdef CI8_LAB_BUFFERED
    compare_buffered = argc == 2 && strcmp(argv[1], "--timing-buffered") == 0;
    compare_madd = argc == 2 && strcmp(argv[1], "--timing-buffered-madd") == 0;
    use_madd = argc == 3 && strcmp(argv[2], "compact-buffered-madd") == 0;
    use_buffered = use_madd || (argc == 3 && strcmp(argv[2], "compact-buffered") == 0);
    if ((compare_buffered || compare_madd || use_buffered) && !NgDspAvx2Available())
        return 2;
    if (compare_buffered || compare_madd) {
        const char *first_variant = compare_madd ? "buffered" : "recompute";
        const char *second_variant = compare_madd ? "buffered-madd" : "buffered";
        use_adaptive = timing_only = timing_adaptive = 1;
        Matrix(0);
        puts("round,twiddle_bits,variant,stage_seconds");
        for (int round = 0; round < 18; ++round) {
            memset(adaptive_ticks, 0, sizeof adaptive_ticks);
            Matrix(round);
            for (int q = 0; q < 2; ++q)
                for (int variant = 0; variant < 2; ++variant)
                    printf("%d,%d,%s,%.9f\n", round, 7 + 8 * q, variant == 0 ? first_variant : second_variant,
                           adaptive_ticks[q][variant]);
        }
        return 0;
    }
#endif
    compare_avx2 = argc == 2 && strcmp(argv[1], "--timing-adaptive-avx2") == 0;
    if (compare_avx2) {
#ifdef CI8_LAB_COMPACT_AVX2
        if (!NgDspAvx2Available())
            return 2;
#else
        return 2;
#endif
    }
    if (compare_avx2 || (argc == 2 && strcmp(argv[1], "--timing-adaptive") == 0)) {
        use_adaptive = timing_only = timing_adaptive = 1;
        Matrix(0);
        puts("round,twiddle_bits,variant,stage_seconds");
        for (int round = 0; round < 16; ++round) {
            memset(adaptive_ticks, 0, sizeof adaptive_ticks);
            Matrix(round);
            for (int q = 0; q < 2; ++q)
                for (int variant = 0; variant < 2; ++variant)
                    printf("%d,%d,%s,%.9f\n", round, 7 + 8 * q,
                           (compare_avx2 ? (variant == 0 ? "scalar" : "avx2")
                                         : (variant == 0 ? "before" : "after")),
                           adaptive_ticks[q][variant]);
        }
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--timing") == 0) {
        use_compact = use_adaptive = timing_only = adaptive_before = 1;
        Matrix(0);
        puts("round,twiddle_bits,mode,stage_seconds");
        for (int round = 0; round < 12; ++round) {
            memset(ticks, 0, sizeof ticks);
            Matrix(round);
            for (int q = 0; q < 2; ++q)
                for (int mode = 0; mode < 3; ++mode)
                    printf("%d,%d,%d,%.9f\n", round, 7 + 8 * q, mode, ticks[q][mode]);
        }
        return 0;
    }

    const int requested_wide_avx2 = argc == 3 && strcmp(argv[2], "avx2") == 0;
    const int requested_compact_avx2 = argc == 3 && strcmp(argv[2], "compact-adaptive-avx2") == 0;
#ifdef CI8_LAB_AVX2
    use_avx2 = requested_wide_avx2;
    if (use_avx2 && !NgDspAvx2Available())
        return 2;
#else
    if (requested_wide_avx2)
        return 2;
#endif
#ifdef CI8_LAB_COMPACT_AVX2
    use_compact_avx2 = requested_compact_avx2;
    if (use_compact_avx2 && !NgDspAvx2Available())
        return 2;
#else
    if (requested_compact_avx2)
        return 2;
#endif
    use_compact = argc == 3 && strcmp(argv[2], "compact") == 0;
    adaptive_before = argc == 3 && strcmp(argv[2], "compact-adaptive-before") == 0;
    use_adaptive = adaptive_before || (argc == 3 &&
                   (strcmp(argv[2], "compact-adaptive") == 0 || requested_compact_avx2));
#ifdef CI8_LAB_BUFFERED
    use_adaptive |= use_buffered;
#endif
    if (argc >= 2 && !(lab_words = fopen(argv[1], "wb")))
        return 1;
    puts("n,pattern,inverse,twiddle_bits,mode,shift,clips,max_error,rms_error");
    Matrix(-1);
    for (int q = 0; q < 2; ++q)
        for (int mode = 0; mode < 3; ++mode)
            fprintf(stderr, "%d,%d,%.9f\n", 7 + 8 * q, mode, ticks[q][mode]);
    if (lab_words && fclose(lab_words) != 0)
        return 1;
    return 0;
}
