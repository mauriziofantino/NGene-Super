/** @file benchmark.c @brief Reproducible full-call integer FFT latency and legacy comparison. */
#include "ngene/dsp/fft.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#else
#include <time.h>
#endif

#ifdef _WIN32
/** @brief Keep all backends on the same logical processor during a hybrid-CPU comparison.
 * @return Previous thread mask, restored by main; zero means pinning was unavailable.
 */
static DWORD_PTR PinThread(void)
{
    DWORD_PTR processMask;
    DWORD_PTR systemMask;
    DWORD_PTR selected;
    DWORD_PTR previous;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask)) {
        printf("# affinity=unavailable\n");
        return 0;
    }
    selected = processMask & (~processMask + 1);
    previous = SetThreadAffinityMask(GetCurrentThread(), selected);
    printf("# affinity_mask=0x%llx,status=%s\n", (unsigned long long)selected,
           previous != 0 ? "pinned" : "unavailable");
    return previous;
}
#endif

#define ROUNDS 21
#define BATCHES 8
#define MAX_SIZE 16384
#define MAX_BACKENDS 4
#ifdef NGENE_BENCH_LEGACY
#define BASE_BACKENDS 3
#else
#define BASE_BACKENDS 2
#endif
#ifdef NGENE_BENCH_BASELINE
#define BACKENDS (BASE_BACKENDS + 1)
/** @brief Create a separate immutable plan owned by the frozen EVO baseline. */
int NgBenchBaselineCreate(size_t maximumSize, TNgFftPlan **out, TNgError *error);
/** @brief Release a plan using the same frozen implementation that allocated it. */
void NgBenchBaselineDestroy(TNgFftPlan *baseline);
/** @brief Execute the frozen complete public FFT call without arithmetic changes. */
int NgBenchBaselineExecute(const TNgFftPlan *baseline, size_t n, const int16_t *input,
                           int16_t *output, TNgError *error);
/** @brief Report the frozen implementation's compiled architecture backend. */
const char *NgBenchBaselineBackend(void);
/** Separate plan prevents dependence on current private plan layout. */
static TNgFftPlan *baselinePlan;
#else
#define BACKENDS BASE_BACKENDS
#endif
#ifdef NGENE_BENCH_LEGACY
/** @brief Allocate the original NGene immutable plan and register-backup buffer. */
int NgBenchLegacyCreate(void);
/** @brief Release original plan state after the single benchmark thread stops. */
void NgBenchLegacyDestroy(void);
/** @brief Run original NGene forward FFT with gain=-log2(N), yielding /N scaling. */
int NgBenchLegacyExecute(size_t n, const int16_t *input, int16_t *output);
#endif
/** Observable consumption prevents discarded timing results. */
static volatile uint32_t checksum;
/** Shared immutable production plan; benchmark execution remains single-threaded. */
static TNgFftPlan *plan;
/** Bounded diagnostic destination for production API validation. */
static TNgError error;
/** Eight independently generated input windows, 16-byte aligned for the legacy API. */
static int16_t *inputs;
/** Disjoint output workspace reused by one backend at a time. */
static int16_t *output;
/** Bit-exact scalar result retained during validation. */
static int16_t *reference;
/** Fixed seed makes input generation independent of the host C library. */
static uint32_t randomState = 0x4e47454e;

/** @brief Abort the tool with the failed contract and production diagnostic. */
static void Require(int ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s (%s)\n", message, error.message);
        exit(1);
    }
}

/** @brief Allocate caller-owned 16-byte-aligned buffers outside timed execution. */
static void *AlignedAllocate(size_t bytes)
{
#ifdef _WIN32
    return _aligned_malloc(bytes, 16);
#else
    void *p = NULL;
    return posix_memalign(&p, 16, bytes) == 0 ? p : NULL;
#endif
}

/** @brief Release a buffer with the allocator matching AlignedAllocate. */
static void AlignedFree(void *p)
{
#ifdef _WIN32
    _aligned_free(p);
#else
    free(p);
#endif
}

/** @brief Read the monotonic wall clock in seconds for batch latency measurements. */
static double Seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER now;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    return (double)now.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
#endif
}

/** @brief Generate eight interleaved complex ADC windows with bounded components. */
static void Fill(size_t n, int amplitude)
{
    size_t i;
    for (i = 0; i < BATCHES * 2 * n; ++i) {
        randomState = randomState * 1664525u + 1013904223u;
        inputs[i] = (int16_t)((int)(randomState % (uint32_t)(2 * amplitude + 1)) - amplitude);
    }
}

/** @brief Execute one backend using its complete public FFT entry point. */
static int Execute(unsigned backend, size_t n, const int16_t *input)
{
#ifdef NGENE_BENCH_BASELINE
    if (backend == BASE_BACKENDS) {
        return NgBenchBaselineExecute(baselinePlan, n, input, output, &error);
    }
#endif
#ifdef NGENE_BENCH_LEGACY
    if (backend == 2) {
        return NgBenchLegacyExecute(n, input, output);
    }
#endif
    return backend == 0 ? NgFftExecute(plan, n, input, output, &error)
                        : NgFftReference(plan, n, input, output, &error);
}

/** @brief Time repeated complete calls in microseconds, rotating eight input windows. */
static double Measure(unsigned backend, size_t n, unsigned repeats)
{
    unsigned i;
    double start = Seconds();
    for (i = 0; i < repeats; ++i) {
        Require(Execute(backend, n, inputs + (i % BATCHES) * 2 * n) == NG_OK, "FFT execution");
        checksum += (uint16_t)output[2 * (i % (unsigned)n)];
    }
    return (Seconds() - start) * 1e6 / repeats;
}

/** @brief Order elapsed times for the median, p95 and minimum batch statistics. */
static int CompareDouble(const void *a, const void *b)
{
    double x = *(const double *)a;
    double y = *(const double *)b;
    return (x > y) - (x < y);
}

/** @brief Check exact EVO arithmetic and an independent analytic tone before timing. */
static void Validate(size_t n)
{
    unsigned batch;
    size_t i;
    int maxLegacyDifference = 0;
    Fill(n, 2047);
    for (batch = 0; batch < BATCHES; ++batch) {
        int16_t *input = inputs + batch * 2 * n;
        Require(NgFftReference(plan, n, input, reference, &error) == NG_OK, "scalar validation");
        Require(Execute(0, n, input) == NG_OK, "SIMD validation");
        Require(memcmp(reference, output, 2 * n * sizeof(*output)) == 0,
                "bit-exact SIMD/reference");
#ifdef NGENE_BENCH_BASELINE
        Require(Execute(BASE_BACKENDS, n, input) == NG_OK, "frozen EVO baseline validation");
        Require(memcmp(reference, output, 2 * n * sizeof(*output)) == 0,
                "bit-exact frozen EVO baseline/reference");
#endif
#ifdef NGENE_BENCH_LEGACY
        Require(Execute(2, n, input) == NG_OK, "original NGene validation");
        for (i = 0; i < 2 * n; ++i) {
            int difference = abs((int)reference[i] - output[i]);
            if (difference > maxLegacyDifference) {
                maxLegacyDifference = difference;
            }
        }
#endif
    }
    /* An analytic complex tone checks sign, gain and natural bin order independently. */
    for (i = 0; i < n; ++i) {
        double phase = 6.2831853071795864769 * 3.0 * (double)i / (double)n;
        inputs[2 * i] = (int16_t)lround(1000.0 * cos(phase));
        inputs[2 * i + 1] = (int16_t)lround(1000.0 * sin(phase));
    }
#ifdef NGENE_BENCH_BASELINE
    Require(NgFftReference(plan, n, inputs, reference, &error) == NG_OK,
            "analytic tone scalar validation");
#endif
    for (batch = 0; batch < BACKENDS; ++batch) {
        Require(Execute(batch, n, inputs) == NG_OK, "tone execution");
#ifdef NGENE_BENCH_BASELINE
        if (batch == BASE_BACKENDS) {
            Require(memcmp(reference, output, 2 * n * sizeof(*output)) == 0,
                    "bit-exact frozen EVO analytic tone/reference");
        }
#endif
        for (i = 0; i < 2 * n; ++i) {
            Require(abs((int)output[i] - (i == 6 ? 1000 : 0)) <= 20,
                    "analytic tone /N, bin order and forward sign");
        }
    }
#ifdef NGENE_BENCH_LEGACY
    /* Q14 generation truncates in the original and rounds in EVO, so equality is not expected. */
    Require(maxLegacyDifference <= 20, "original versus EVO low-amplitude numerical bound");
    printf("# validation,N=%zu,SIMD=bit-exact,legacy_max_component_delta=%d\n", n,
           maxLegacyDifference);
#else
    (void)maxLegacyDifference;
    printf("# validation,N=%zu,SIMD=bit-exact,legacy=unavailable\n", n);
#endif
#ifdef NGENE_BENCH_BASELINE
    printf("# validation,N=%zu,EVO-before=bit-exact,windows=%d,tone=passed\n", n, BATCHES);
#endif
}

/** @brief Observe original/EVO numerical differences at full Q14 headroom, outside timing.
 *
 * Includes the 13,122-case eight-point rail/odd/rotation corpus from test_fft.c and
 * all 65,536 independent I/Q rail-sign assignments across eight complex samples.
 * SIMD must remain bit-identical to the scalar oracle. Original NGene intentionally has a
 * different coefficient-quantization/saturation contract, so its observed difference is reported,
 * not constrained by the low-amplitude timing-validation bound. No timing input PRNG is used.
 */
static void DiagnoseNumericalContract(void)
{
#ifdef NGENE_BENCH_LEGACY
    static const int16_t pairs[3][2] = {{-16384, -16384}, {16384, -16383}, {1, -1}};
    int16_t worstInput[16] = {0};
    int maximumDifference = -1;
    int worstReference = 0;
    int worstLegacy = 0;
    unsigned worstCorpus = 0;
    unsigned worstRotation = 0;
    unsigned worstCode = 0;
    size_t worstComponent = 0;
    unsigned cases = 0;
    unsigned corpus;
    unsigned code;
    size_t i;
    for (corpus = 0; corpus < 3; ++corpus) {
        const unsigned rotation = corpus < 2 ? corpus : 0;
        const unsigned assignments = corpus < 2 ? 6561u : 65536u;
        for (code = 0; code < assignments; ++code) {
            unsigned digits = code;
            for (i = 0; i < 8; ++i) {
                int16_t re;
                int16_t im;
                unsigned quarter;
                if (corpus < 2) {
                    re = pairs[digits % 3u][0];
                    im = pairs[digits % 3u][1];
                    digits /= 3;
                } else {
                    re = (int16_t)((digits & 1u) != 0 ? 16384 : -16384);
                    im = (int16_t)((digits & 2u) != 0 ? 16384 : -16384);
                    digits >>= 2;
                }
                for (quarter = 0; quarter < rotation * (unsigned)(i & 3u); ++quarter) {
                    int16_t previousRe = re;
                    re = (int16_t)-im;
                    im = previousRe;
                }
                inputs[2 * i] = re;
                inputs[2 * i + 1] = im;
            }
            Require(NgFftReference(plan, 8, inputs, reference, &error) == NG_OK,
                    "full-headroom scalar diagnostic");
            Require(Execute(0, 8, inputs) == NG_OK, "full-headroom SIMD diagnostic");
            Require(memcmp(reference, output, 16 * sizeof(*output)) == 0,
                    "bit-exact full-headroom SIMD/reference");
            Require(Execute(2, 8, inputs) == NG_OK, "full-headroom original NGene diagnostic");
            for (i = 0; i < 16; ++i) {
                int difference = abs((int)reference[i] - output[i]);
                if (difference > maximumDifference) {
                    maximumDifference = difference;
                    worstReference = reference[i];
                    worstLegacy = output[i];
                    worstCorpus = corpus;
                    worstRotation = rotation;
                    worstCode = code;
                    worstComponent = i;
                    memcpy(worstInput, inputs, sizeof(worstInput));
                }
            }
            ++cases;
        }
    }
    printf("# diagnostic_full_headroom,N=8,cases=%u,rail_odd_rotation_cases=13122,"
           "independent_IQ_rail_cases=65536,SIMD=bit-exact,"
           "legacy_max_component_delta=%d,comparison=observed_not_equivalence_bound,not_timed\n",
           cases, maximumDifference);
    printf("# diagnostic_full_headroom_worst,N=8,corpus=%s,rotation=%u,code=%u,component=%zu,"
           "EVO=%d,original_NGene=%d\n",
           worstCorpus < 2 ? "rail_odd_rotation" : "independent_IQ_rails", worstRotation, worstCode,
           worstComponent, worstReference, worstLegacy);
    printf("# diagnostic_full_headroom_worst_input,N=8,format=interleaved_IQ,values=");
    for (i = 0; i < 16; ++i) {
        printf("%s%d", i == 0 ? "" : ",", (int)worstInput[i]);
    }
    printf("\n");
#else
    printf("# diagnostic_full_headroom=unavailable,reason=original_NGene_not_linked\n");
#endif
}

/** @brief Validate both kernels, calibrate repeated batches and emit CSV measurements. */
int main(void)
{
    const char *names[MAX_BACKENDS] = {
        "EVO-SIMD",
        "EVO-scalar",
#ifdef NGENE_BENCH_LEGACY
        "NGene-original",
#endif
#ifdef NGENE_BENCH_BASELINE
        "EVO-before",
#endif
    };
    unsigned backends = BACKENDS;
    size_t n;
    unsigned backend;
#ifdef _WIN32
    DWORD_PTR previousAffinity = PinThread();
#endif
#ifndef NDEBUG
    fprintf(stderr, "Build this performance tool in Release configuration.\n");
    return 2;
#endif
    inputs = AlignedAllocate(BATCHES * 2 * MAX_SIZE * sizeof(*inputs));
    output = AlignedAllocate(2 * MAX_SIZE * sizeof(*output));
    reference = AlignedAllocate(2 * MAX_SIZE * sizeof(*reference));
    Require(inputs != NULL && output != NULL && reference != NULL, "aligned buffers");
    Require(NgFftCreate(MAX_SIZE, &plan, &error) == NG_OK, "FFT plan");
#ifdef NGENE_BENCH_BASELINE
    Require(NgBenchBaselineCreate(MAX_SIZE, &baselinePlan, &error) == NG_OK,
            "frozen EVO baseline plan");
#endif
#ifdef NGENE_BENCH_LEGACY
    Require(NgBenchLegacyCreate() == 0, "original NGene plan");
#endif
    printf("# compiler=%s,backend=%s,rounds=%d,batches=%d\n", NGENE_BENCH_COMPILER, NgFftBackend(),
           ROUNDS, BATCHES);
    printf("# target_arch=%s,pointer_bits=%u\n", NGENE_BENCH_ARCH, (unsigned)(sizeof(void *) * 8));
    printf("# current_c_sha256=%s,current_kernel_sha256=%s\n", NGENE_BENCH_SOURCE_C_SHA256,
           NGENE_BENCH_SOURCE_KERNEL_SHA256);
#ifdef NGENE_BENCH_LEGACY
    printf("# original_fft_c_sha256=%s,original_fft_h_sha256=%s,original_simd_h_sha256=%s\n",
           NGENE_BENCH_LEGACY_FFT_C_SHA256, NGENE_BENCH_LEGACY_FFT_H_SHA256,
           NGENE_BENCH_LEGACY_SIMD_H_SHA256);
#endif
#ifdef NGENE_BENCH_BASELINE
    printf("# before_backend=%s,before_c_sha256=%s,before_kernel_sha256=%s\n",
           NgBenchBaselineBackend(), NGENE_BENCH_BASELINE_C_SHA256,
           NGENE_BENCH_BASELINE_KERNEL_SHA256);
#endif
    printf(
        "# Timed: full call, validation/permutation/stages; excluded: plan, I/O, window, power.\n");
    printf(
        "# Alternating backend order; >=10ms per round; deterministic 12-bit ADC-level noise.\n");
    DiagnoseNumericalContract();
    printf("N,backend,repeats,median_us,p95_us,min_us,Msamples_per_s\n");
    for (n = 8; n <= MAX_SIZE; n *= 2) {
        unsigned repeats[MAX_BACKENDS] = {16, 16, 16, 16};
        double times[MAX_BACKENDS][ROUNDS];
        unsigned round;
        Validate(n);
        Fill(n, 2047);
        for (backend = 0; backend < backends; ++backend) {
            double elapsed;
            (void)Measure(backend, n, 32);
            do {
                elapsed = Measure(backend, n, repeats[backend]) * repeats[backend];
                if (elapsed < 10000.0) {
                    repeats[backend] *= 2;
                }
            } while (elapsed < 10000.0 && repeats[backend] < 1048576);
        }
        for (round = 0; round < ROUNDS; ++round) {
            unsigned step;
            for (step = 0; step < backends; ++step) {
                backend = (step + round) % backends;
                times[backend][round] = Measure(backend, n, repeats[backend]);
            }
        }
        for (backend = 0; backend < backends; ++backend) {
            double *t = times[backend];
            qsort(t, ROUNDS, sizeof(*t), CompareDouble);
            printf("%zu,%s,%u,%.3f,%.3f,%.3f,%.3f\n", n, names[backend], repeats[backend],
                   t[ROUNDS / 2], t[ROUNDS * 95 / 100], t[0], (double)n / t[ROUNDS / 2]);
        }
    }
    printf("# checksum=%u\n", (unsigned)checksum);
#ifdef NGENE_BENCH_LEGACY
    NgBenchLegacyDestroy();
#endif
#ifdef NGENE_BENCH_BASELINE
    NgBenchBaselineDestroy(baselinePlan);
#endif
    NgFftDestroy(plan);
    AlignedFree(inputs);
    AlignedFree(output);
    AlignedFree(reference);
#ifdef _WIN32
    if (previousAffinity != 0) {
        (void)SetThreadAffinityMask(GetCurrentThread(), previousAffinity);
    }
#endif
    return 0;
}
