/** @file fft.c @brief Plan/permutation/stage scheduling for the NGene Q14 FFT. */
#include "ngene/dsp/fft.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef NGENE_EXPERIMENTAL_Q14_AVX2
#include "dsp/x86/avx2_available.h"
void NgFftQ14StageAvx2(int16_t *data, const int16_t *twiddles, size_t n, size_t width);
#endif

/** @brief Immutable integer FFT permutation and Q14 twiddle tables shared by disjoint executions.
 */
struct TNgFftPlan {
    /** Largest supported complex transform length allocated in this plan. */
    size_t maximum;
    /** Log2 of maximum; used to truncate the bit-reversal permutation. */
    unsigned bits;
    /** Owned immutable bit-reversal indices. */
    uint32_t *reverse;
    /** Owned immutable interleaved Q14 sine/cosine butterfly coefficients. */
    int16_t *twiddles;
#ifdef NGENE_EXPERIMENTAL_Q14_AVX2
    int avx2;
#endif
#ifdef NGENE_FFT_RUNTIME_SSE2
    /** ISA decision made once at creation; never execute SSE2 on unsupported CPUs. */
    int simd;
#endif
};

#if defined(NGENE_FFT_ASSEMBLY) || defined(NGENE_FFT_SIMD)
#include "dsp/fft/butterfly.h"
#define NG_FFT_HAVE_KERNEL 1
#endif

/**
 * @brief Check the supported integer FFT size.
 *
 * @details
 * Accepts powers of two from 8 through 16384. Does not allocate or mutate plans.
 *
 * @param n Candidate complex transform size.
 *
 * @return Nonzero if supported, zero otherwise.
 */
static int PowerTwo(size_t n)
{
    return n >= NG_FFT_MIN_SIZE && n <= NG_FFT_MAX_SIZE && (n & (n - 1)) == 0;
}

/**
 * @brief Compute defined signed floor division by a power of two.
 *
 * @details
 * Used by Q14 products and stage scaling to match SIMD rounding; arithmetic intermediates
 * remain in the FFT headroom contract.
 *
 * @param x Signed intermediate.
 * @param shift Small positive shift (1 or 14 in the butterflies).
 *
 * @return floor(x / 2^shift).
 */
static int32_t FloorShift(int32_t x, unsigned shift)
{
    return x >= 0 ? x / (int32_t)(1u << shift) : -1 - ((-1 - x) / (int32_t)(1u << shift));
}

/**
 * @brief Saturate a butterfly intermediate to signed int16.
 *
 * @details
 * Never wrap on narrowing; this defines the numerical contract for the SIMD kernels.
 *
 * @param x Signed32 butterfly intermediate.
 *
 * @return Value clamped to [-32768,32767].
 */
static int16_t Narrow(int32_t x)
{
    return (int16_t)(x < -32768 ? -32768 : x > 32767 ? 32767 : x);
}

/**
 * @brief Compute a group of scalar Q14 butterflies.
 *
 * @details
 * Reads four twiddle coefficients per complex pair, floors products, divides sum/difference by
 * two and saturates to int16. This is the bit-exact oracle for the SIMD kernels.
 *
 * @param u Read/write first butterfly half, 2*count int16 elements.
 * @param v Read/write second disjoint half.
 * @param tw Read-only Q14 table, 4*count coefficients.
 * @param count Number of complex butterflies.
 */
static void ReferenceButterflies(int16_t *u, int16_t *v, const int16_t *tw, size_t count)
{
    size_t j;
    for (j = 0; j < count; ++j) {
        int32_t r = FloorShift((int32_t)v[0] * tw[0] + (int32_t)v[1] * tw[1], 14);
        int32_t i = FloorShift((int32_t)v[0] * tw[2] + (int32_t)v[1] * tw[3], 14);
        int32_t a = u[0];
        int32_t b = u[1];
        u[0] = Narrow(FloorShift(a + r, 1));
        u[1] = Narrow(FloorShift(b + i, 1));
        v[0] = Narrow(FloorShift(a - r, 1));
        v[1] = Narrow(FloorShift(b - i, 1));
        u += 2;
        v += 2;
        tw += 4;
    }
}

/**
 * @brief Create immutable bit-reversal and Q14 twiddle tables.
 *
 * @details
 * Table generation may use floating point before reception. Execution uses integer butterfly
 * operations only. Concurrent calls may share the plan with disjoint workspaces; destroy only
 * after all calls finish.
 *
 * Parameter maximumSize: Largest supported transform, a power of two in 8..16384.
 * Parameter out: Receives caller-owned plan, or NULL on creation failure.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for dimensions or allocation failure.
 */
int NgFftCreate(size_t maximumSize, TNgFftPlan **out, TNgError *error)
{
    TNgFftPlan *p;
    size_t i;
    size_t m;
    size_t offset = 0;
    if (out == NULL || PowerTwo(maximumSize) == 0) {
        return NgFail(error, "FFT size must be a power of two from 8 to 16384");
    }
    *out = NULL;
    p = calloc(1, sizeof(*p));
    if (p == NULL) {
        return NgFail(error, "Cannot allocate FFT plan");
    }
    p->maximum = maximumSize;
#ifdef NGENE_EXPERIMENTAL_Q14_AVX2
    p->avx2 = NgDspAvx2Available();
#endif
#ifdef NGENE_FFT_RUNTIME_SSE2
    p->simd = NgFftSimdAvailable();
#endif
    for (i = maximumSize; i > 1; i >>= 1) {
        ++p->bits;
    }
    p->reverse = malloc(maximumSize * sizeof(*p->reverse));
    p->twiddles = malloc((maximumSize - 1) * 4 * sizeof(int16_t));
    if (p->reverse == NULL || p->twiddles == NULL) {
        NgFftDestroy(p);
        return NgFail(error, "Cannot allocate FFT tables");
    }
    for (i = 0; i < maximumSize; ++i) {
        unsigned b;
        size_t x = i;
        size_t r = 0;
        for (b = 0; b < p->bits; ++b) {
            r = (r << 1) | (x & 1);
            x >>= 1;
        }
        p->reverse[i] = (uint32_t)r;
    }
    /* Table generation may use FP, exactly as the historical NGene plan did.
     * Every sample-path butterfly and coefficient thereafter is integer. */
    for (m = 2; m <= maximumSize; m *= 2) {
        for (i = 0; i < m / 2; ++i) {
            double phase = 6.2831853071795864769 * (double)i / (double)m;
            int16_t c = (int16_t)lround(16384 * cos(phase));
            int16_t s = (int16_t)lround(-16384 * sin(phase));
            int16_t *t = p->twiddles + offset;
            t[0] = c;
            t[1] = (int16_t)-s;
            t[2] = s;
            t[3] = c;
            offset += 4;
        }
    }
    *out = p;
    return NG_OK;
}

/**
 * @brief Free the immutable integer FFT plan.
 *
 * @details
 * All execution calls using this plan must have returned. A NULL plan is accepted.
 *
 * Parameter plan: Plan to destroy; invalid after return.
 */
void NgFftDestroy(TNgFftPlan *plan)
{
    if (plan != NULL) {
        free(plan->reverse);
        free(plan->twiddles);
        free(plan);
    }
}

/**
 * @brief Validate buffers and execute all integer FFT stages.
 *
 * @details
 * Checks Q14 headroom and disjoint storage, performs bit reversal, then uses immutable plan
 * twiddles with either SIMD or scalar butterflies. Output is natural order and scaled 1/N.
 *
 * @param p Immutable FFT plan.
 * @param n Complex transform size.
 * @param a Read-only interleaved int16 input.
 * @param b Disjoint read/write interleaved output workspace.
 * @param reference Nonzero forces scalar butterfly oracle.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for plan/dimension/overlap/range errors.
 */
static int Execute(const TNgFftPlan *p, size_t n, const int16_t *a, int16_t *b, int reference,
                   TNgError *error)
{
    size_t i;
    size_t m;
    size_t base;
    size_t offset = 0;
    if (p == NULL || a == NULL || b == NULL || a == b || PowerTwo(n) == 0 || n > p->maximum) {
        return NgFail(error, "Invalid FFT plan, size or disjoint arrays");
    }
#ifdef NGENE_FFT_RUNTIME_SSE2
    if (p->simd == 0) {
        reference = 1;
    }
#endif
    {
        uintptr_t source = (uintptr_t)a;
        uintptr_t destination = (uintptr_t)b;
        uintptr_t separation = source > destination ? source - destination : destination - source;
        if (separation < 2 * n * sizeof(int16_t)) {
            return NgFail(error, "FFT input/output arrays overlap");
        }
    }
#ifdef NG_FFT_HAVE_KERNEL
    if (reference == 0) {
        if (NgFftInputInRange(a, n) == 0) {
            return NgFail(error, "FFT input exceeds signed Q14 headroom");
        }
    } else
#endif
    {
        for (i = 0; i < 2 * n; ++i) {
            if (a[i] < -16384 || a[i] > 16384) {
                return NgFail(error, "FFT input exceeds signed Q14 headroom");
            }
        }
    }
#ifdef NG_FFT_HAVE_KERNEL
    if (reference == 0) {
        const size_t quarter = n / 4;
        const size_t stride = p->maximum / n;
        const size_t block = n < 512 ? n : 512;
        size_t chunk;
        /* Bit reversal is an involution. Gather four reversed inputs and write a
         * contiguous output quartet, sharing one reverse-table entry. */
        for (i = 0; i < n; i += 4) {
            size_t r = p->reverse[i * stride];
            memcpy(b + 2 * i, a + 2 * r, 2 * sizeof(*a));
            memcpy(b + 2 * (i + 1), a + 2 * (r + 2 * quarter), 2 * sizeof(*a));
            memcpy(b + 2 * (i + 2), a + 2 * (r + quarter), 2 * sizeof(*a));
            memcpy(b + 2 * (i + 3), a + 2 * (r + 3 * quarter), 2 * sizeof(*a));
        }
        /* Butterfly groups below block are independent. Complete each small
         * subtree while its workspace is hot, retaining every Q14 /2 rounding. */
        for (chunk = 0; chunk < n; chunk += block) {
            NgFftFirstStages(b + 2 * chunk, block);
            offset = 28;
            for (m = 16; m <= block; m *= 2) {
#ifdef NGENE_EXPERIMENTAL_Q14_AVX2
                if (p->avx2) {
                    NgFftQ14StageAvx2(b + 2 * chunk, p->twiddles + offset, block, m);
                    offset += 2 * m;
                    continue;
                }
#endif
                for (base = chunk; base < chunk + block; base += m) {
                    NgFftButterflies(b + 2 * base, b + 2 * base + m, p->twiddles + offset, m / 2);
                }
                offset += 2 * m;
            }
        }
        m = 2 * block;
        offset = 4 * block - 4;
    } else
#endif
    {
        for (i = 0; i < n; ++i) {
            size_t r = p->reverse[i * (p->maximum / n)];
            /* The oracle retains the independent scatter permutation and stage schedule. */
            memcpy(b + 2 * r, a + 2 * i, 2 * sizeof(*a));
        }
        m = 2;
    }
    for (; m <= n; m *= 2) {
#ifdef NGENE_EXPERIMENTAL_Q14_AVX2
        if (reference == 0 && m >= 16 && p->avx2) {
            NgFftQ14StageAvx2(b, p->twiddles + offset, n, m);
            offset += 2 * m;
            continue;
        }
#endif
        for (base = 0; base < n; base += m) {
#ifdef NG_FFT_HAVE_KERNEL
            if (reference == 0) {
                NgFftButterflies(b + 2 * base, b + 2 * base + m, p->twiddles + offset, m / 2);
            } else
#else
            (void)reference;
#endif
                ReferenceButterflies(b + 2 * base, b + 2 * base + m, p->twiddles + offset, m / 2);
        }
        offset += 2 * m;
    }
    return NG_OK;
}

/**
 * @brief Execute the forward complex integer FFT.
 *
 * @details
 * Forward exp(-j*2*pi*k*size/N), natural-order output scaled by 1/N. Components must be in
 * [-16384,16384]; Q14 twiddles and arithmetic-floor scaling are used. Arrays must be disjoint,
 * have 2*N int16 elements and natural int16 alignment. Execution allocates no memory. Uses the
 * selected architecture SIMD backend, or explicitly configured reference.
 *
 * Parameter plan: Immutable plan created by NgFftCreate.
 * Parameter size: Transform size, power of two in 8..plan maximum.
 * Parameter input: Borrowed interleaved I,Q input; not modified.
 * Parameter output: Caller-owned interleaved output/workspace, disjoint from input.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for invalid plan, size, overlap or input range.
 */
int NgFftExecute(const TNgFftPlan *plan, size_t size, const int16_t *input, int16_t *output,
                 TNgError *error)
{
    return Execute(plan, size, input, output, 0, error);
}

/**
 * @brief Execute the bit-exact scalar reference FFT.
 *
 * @details
 * Forward exp(-j*2*pi*k*size/N), natural-order output scaled by 1/N. Components must be in
 * [-16384,16384]; Q14 twiddles and arithmetic-floor scaling are used. Arrays must be disjoint,
 * have 2*N int16 elements and natural int16 alignment. Execution allocates no memory. Uses
 * scalar butterflies for validation against the architecture backend.
 *
 * Parameter plan: Immutable plan created by NgFftCreate.
 * Parameter size: Transform size, power of two in 8..plan maximum.
 * Parameter input: Borrowed interleaved I,Q input; not modified.
 * Parameter output: Caller-owned interleaved output/workspace, disjoint from input.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for invalid plan, size, overlap or input range.
 */
int NgFftReference(const TNgFftPlan *plan, size_t size, const int16_t *input, int16_t *output,
                   TNgError *error)
{
    return Execute(plan, size, input, output, 1, error);
}

/**
 * @brief Report the available Q14 FFT backend.
 *
 * @details
 * The returned string has static storage; do not free it. On 32-bit x86, SSE2 capability
 * is checked and older CPUs report the scalar fallback. Experimental x86-64 builds
 * also check runtime AVX2 support. Plans cache these decisions.
 *
 * Result: Static name of the available AVX2/SSE2, SSE2, NEON or C-reference backend.
 */
const char *NgFftBackend(void)
{
#ifdef NGENE_EXPERIMENTAL_Q14_AVX2
    if (NgDspAvx2Available()) {
        return "AVX2/SSE2";
    }
#endif
#ifdef NGENE_FFT_RUNTIME_SSE2
    if (NgFftSimdAvailable() == 0) {
        return "C-reference";
    }
#endif
    return NGENE_FFT_BACKEND;
}
