/* Included only by the generated F4 search probe. */
#ifdef CI8_PCPS_SCHEDULED
#ifdef _MSC_VER
#define CI8_LAB_WORKSPACE static __declspec(thread)
#else
#define CI8_LAB_WORKSPACE static _Thread_local
#endif
#endif
#define main Ci8FftLabMain
#include "ci8_fft_lab.c"
#undef main

#ifdef CI8_PCPS_PHASES
static size_t phase_calls;
static double phase_prepare, phase_stages, phase_restore;

void Ci8PcpsPrintPhases(void)
{
    printf("fft_phases,%zu,%.9f,%.9f,%.9f\n", phase_calls, phase_prepare,
           phase_stages, phase_restore);
}
#endif

static int Ci8PcpsTransform(TNgSearchFft *plan, const int32_t *source, int32_t *output,
                            int inverse, TNgError *error)
{
    CI8_LAB_WORKSPACE int mode = -1;
    if (mode < 0) {
        const char *profile = getenv("NGENE_F4_PCPS_PROFILE");
        if (!profile || strcmp(profile, "wide") == 0)
            mode = 0;
        else if (strcmp(profile, "fixed") == 0)
            mode = 1;
        else if (strcmp(profile, "adaptive") == 0)
            mode = 2;
#ifdef CI8_LAB_COMPACT_AVX2
        else if (strcmp(profile, "adaptive-avx2") == 0) {
            if (!NgDspAvx2Available())
                return NgFail(error, "AVX2 is unavailable for F4 PCPS");
            use_compact_avx2 = 1;
            mode = 2;
        }
#endif
#ifdef CI8_LAB_BUFFERED
        else if (strcmp(profile, "adaptive-buffered") == 0 || strcmp(profile, "adaptive-madd") == 0) {
            if (!NgDspAvx2Available())
                return NgFail(error, "AVX2 is unavailable for buffered F4 PCPS");
            use_buffered = 1;
            use_madd = strcmp(profile, "adaptive-madd") == 0;
            mode = 2;
        }
#endif
        else if (strcmp(profile, "native") == 0)
            mode = 3;
        else
            return NgFail(error, "Unknown F4 PCPS profile");
    }
#ifdef CI8_PCPS_PHASES
    double started = Seconds();
    ++phase_calls;
#endif
    if (mode == 3) {
        int status = NgSearchFftExecuteInt32(plan, source, output, inverse, error);
#ifdef CI8_PCPS_PHASES
        phase_stages += Seconds() - started;
#endif
        return status;
    }
    const size_t n = plan->size;
    if (n > MAX_N)
        return NgFail(error, "F4 PCPS FFT exceeds probe capacity");
    int64_t peak = 0;
    for (size_t k = 0; k < 2 * n; ++k) {
        int64_t magnitude = source[k] < 0 ? -(int64_t)source[k] : source[k];
        if (magnitude > peak)
            peak = magnitude;
    }
    int exponent = 0;
    if (peak) {
        /* Use a common power of two, reserving +127 as the symmetric limit. */
        (void)frexp((double)peak, &exponent);
        exponent -= 7;
        if (ldexp((double)peak, -exponent) > 127)
            ++exponent;
    }

    CI8_LAB_WORKSPACE size_t cachedSize;
    CI8_LAB_WORKSPACE int cachedInverse;
    CI8_LAB_WORKSPACE size_t bitReversed[MAX_N];
    if (cachedSize != n) {
        Prepare(n, inverse, 32768);
        for (size_t k = 0, reversed = 0; k < n; ++k) {
            bitReversed[k] = reversed;
            size_t bit = n / 2;
            while (bit && (reversed & bit)) {
                reversed ^= bit;
                bit /= 2;
            }
            reversed ^= bit;
        }
        cachedSize = n;
        cachedInverse = inverse;
    }
    if (cachedInverse != inverse) {
        for (size_t k = 0; k < n / 2; ++k)
            lab_wi[k] = -lab_wi[k];
        cachedInverse = inverse;
    }
    const double packScale = ldexp(1.0, -exponent);
    for (size_t k = 0; k < n; ++k) {
        size_t dest = 2 * bitReversed[k];
        for (size_t part = 0; part < 2; ++part) {
            int8_t packed = (int8_t)lround((double)source[2 * k + part] * packScale);
            if (mode == 0)
                fixed[dest + part] = 256 * packed;
            else
                compact[dest + part] = packed;
        }
    }
    unsigned shift = 0, clips = 0;
#ifdef CI8_PCPS_PHASES
    double packed = Seconds();
    phase_prepare += packed - started;
#endif
    IntegerStages(n, 15, mode, mode != 0, &shift, &clips);
#ifdef CI8_PCPS_PHASES
    double transformed = Seconds();
    phase_stages += transformed - packed;
#endif
    const double restore = ldexp(1.0, exponent + (int)shift - (mode == 0 ? 8 : 0)) /
                           (inverse ? (double)n : 1.0);
    for (size_t k = 0; k < 2 * n; ++k) {
        double value = (double)(mode == 0 ? fixed[k] : compact[k]) * restore;
        if (value < INT32_MIN || value > INT32_MAX)
            return NgFail(error, "F4 PCPS FFT output exceeds INT32");
        output[k] = (int32_t)value; /* Preserve inverse output truncation. */
    }
#ifdef CI8_PCPS_PHASES
    phase_restore += Seconds() - transformed;
#endif
    return NG_OK;
}
