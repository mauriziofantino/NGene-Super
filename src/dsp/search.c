/** @file search.c @brief Allocation-free search kernels with SSE2, AArch64 NEON and C fallback. */
#include "ngene/dsp/search.h"
#include "ngene/dsp/gps_ca.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "dsp/sample_backend.h"
#if NG_SSE2
#define NG_SEARCH_SSE 1
#endif
#if NG_NEON
#define NG_SEARCH_NEON 1
#endif
#define NG_SEARCH_TAU 6.28318530717958647692

/** Immutable transform geometry, twiddles and bit reversal. */
struct TNgSearchFft {
    size_t size;
    size_t *reverse;
    /** Radix-2^2 stage pairs from half-width 2 upward, in execution order. For each pair of
     * half-width h: W1 = exp(-j2pi k/2h) then W2 = exp(-j2pi k/4h), k < h, each stored as
     * h duplicated real parts {r0,r0,r1,r1,...} followed by h duplicated imaginary parts. */
    float *pairs;
};

/** Half-width of the first twiddled stage pair: the trivial stages before it are one
 * radix-2 stage (odd log2 size) followed by one radix-4 stage without twiddles. */
static size_t PairStart(size_t size)
{
    size_t stages = 0;
    while (((size_t)1 << stages) < size)
        ++stages;
    return (stages & 1u) ? 8u : 4u;
}

/** Build reusable transform tables outside the sample path. */
int NgSearchFftCreate(size_t size, TNgSearchFft **out, TNgError *error)
{
    TNgSearchFft *plan;
    size_t k;
    *out = NULL;
    if (size < 8 || size > 16384 || (size & (size - 1)) != 0)
        return NgFail(error, "Invalid search FFT size");
    plan = calloc(1, sizeof(*plan));
    if (plan == NULL)
        return NgFail(error, "Search FFT allocation failed");
    plan->size = size;
    plan->reverse = calloc(size, sizeof(*plan->reverse));
    plan->pairs = calloc(4 * size, sizeof(*plan->pairs));
    if (plan->reverse == NULL || plan->pairs == NULL) {
        NgSearchFftDestroy(plan);
        return NgFail(error, "Search FFT tables allocation failed");
    }
    for (k = 0; k < size; ++k) {
        size_t bit;
        size_t value = k;
        for (bit = size; bit > 1; bit >>= 1) {
            plan->reverse[k] = (plan->reverse[k] << 1) | (value & 1);
            value >>= 1;
        }
    }
    {
        size_t half = PairStart(size), used = 0, table;
        for (; half < size; half *= 4)
            for (table = 0; table < 2; ++table) {
                double width = (double)(table == 0 ? 2 * half : 4 * half);
                for (k = 0; k < half; ++k) {
                    double angle = NG_SEARCH_TAU * (double)k / width;
                    plan->pairs[used + 2 * k] = plan->pairs[used + 2 * k + 1] = (float)cos(angle);
                    plan->pairs[used + 2 * half + 2 * k] = plan->pairs[used + 2 * half + 2 * k + 1] =
                        (float)-sin(angle);
                }
                used += 4 * half;
            }
    }
    *out = plan;
    return NG_OK;
}

/** Release owned tables. */
void NgSearchFftDestroy(TNgSearchFft *plan)
{
    if (plan != NULL) {
        free(plan->reverse);
        free(plan->pairs);
        free(plan);
    }
}

/** Complex product by (-j) for the forward and (+j) for the inverse transform. */
static TNgComplex Quarter(TNgComplex value, int inverse)
{
    TNgComplex out;
    out.i = inverse ? -value.q : value.q;
    out.q = inverse ? value.i : -value.i;
    return out;
}

/** Iterative radix-2^2 decimation in time: bit reversal, one radix-2 stage when log2 size is
 * odd, one radix-4 stage without twiddles, then radix-4 stage pairs with contiguous twiddles.
 * Each pair computes the two radix-2 stages it replaces with three products instead of four
 * and one memory pass instead of two. The workspace is the only mutable object. */
void NgSearchFftExecute(const TNgSearchFft *plan, TNgComplex *values, int inverse)
{
    size_t size = plan->size, k, base, half = PairStart(size) / 4;
    const float *table = plan->pairs;
    for (k = 0; k < size; ++k) {
        if (k < plan->reverse[k]) {
            TNgComplex temp = values[k];
            values[k] = values[plan->reverse[k]];
            values[plan->reverse[k]] = temp;
        }
    }
    if (half == 2) {
        for (base = 0; base < size; base += 2) {
            TNgComplex a = values[base], b = values[base + 1];
            values[base].i = a.i + b.i;
            values[base].q = a.q + b.q;
            values[base + 1].i = a.i - b.i;
            values[base + 1].q = a.q - b.q;
        }
    }
    for (base = 0; base < size; base += 4 * half)
        for (k = 0; k < half; ++k) {
            /* First pair: half 1 needs no products; half 2 needs W1 = -j and W2 = exp(-jpi/4)
             * for k = 1 only. */
            TNgComplex *x = values + base + k, b0, b1, b2, b3, t;
            TNgComplex a0 = x[0], a1 = x[half], a2 = x[2 * half], a3 = x[3 * half];
            if (half == 2 && k == 1) {
                a1 = Quarter(a1, inverse);
                a3 = Quarter(a3, inverse);
            }
            b0.i = a0.i + a1.i;
            b0.q = a0.q + a1.q;
            b1.i = a0.i - a1.i;
            b1.q = a0.q - a1.q;
            b2.i = a2.i + a3.i;
            b2.q = a2.q + a3.q;
            b3.i = a2.i - a3.i;
            b3.q = a2.q - a3.q;
            if (half == 2) {
                /* W2 = exp(-j2pi k/8) for k = 0, 1 on this second radix-2 level. */
                if (k == 1) {
                    const float r = 0.70710678118654752f;
                    TNgComplex c = b2, d = b3;
                    b2.i = r * (c.i + (inverse ? -c.q : c.q));
                    b2.q = r * (c.q - (inverse ? -c.i : c.i));
                    b3.i = r * (d.i + (inverse ? -d.q : d.q));
                    b3.q = r * (d.q - (inverse ? -d.i : d.i));
                }
            }
            t = Quarter(b3, inverse);
            x[0].i = b0.i + b2.i;
            x[0].q = b0.q + b2.q;
            x[2 * half].i = b0.i - b2.i;
            x[2 * half].q = b0.q - b2.q;
            x[half].i = b1.i + t.i;
            x[half].q = b1.q + t.q;
            x[3 * half].i = b1.i - t.i;
            x[3 * half].q = b1.q - t.q;
        }
    for (half *= 4; half < size; half *= 4) {
        const float *r1 = table, *i1 = table + 2 * half, *r2 = table + 4 * half,
                    *i2 = table + 6 * half;
        for (base = 0; base < size; base += 4 * half) {
            TNgComplex *x0 = values + base, *x1 = x0 + half, *x2 = x1 + half, *x3 = x2 + half;
            k = 0;
#ifdef NG_SEARCH_SSE
            {
                const __m128 sign = _mm_set_ps(0.0f, -0.0f, 0.0f, -0.0f);
                const __m128 conj = inverse ? _mm_set1_ps(-0.0f) : _mm_setzero_ps();
                /* Quarter turn of [a, b]: forward [b, -a], inverse [-b, a]. */
                const __m128 turn = inverse ? _mm_set_ps(0.0f, -0.0f, 0.0f, -0.0f)
                                            : _mm_set_ps(-0.0f, 0.0f, -0.0f, 0.0f);
                for (; k + 2 <= half; k += 2) {
                    __m128 a0 = _mm_loadu_ps((const float *)(x0 + k));
                    __m128 a1 = _mm_loadu_ps((const float *)(x1 + k));
                    __m128 a2 = _mm_loadu_ps((const float *)(x2 + k));
                    __m128 a3 = _mm_loadu_ps((const float *)(x3 + k));
                    __m128 wr = _mm_loadu_ps(r1 + 2 * k);
                    __m128 wi = _mm_xor_ps(_mm_loadu_ps(i1 + 2 * k), conj);
                    __m128 t1 = _mm_add_ps(
                        _mm_mul_ps(a1, wr),
                        _mm_xor_ps(_mm_mul_ps(_mm_shuffle_ps(a1, a1, _MM_SHUFFLE(2, 3, 0, 1)), wi),
                                   sign));
                    __m128 t3 = _mm_add_ps(
                        _mm_mul_ps(a3, wr),
                        _mm_xor_ps(_mm_mul_ps(_mm_shuffle_ps(a3, a3, _MM_SHUFFLE(2, 3, 0, 1)), wi),
                                   sign));
                    __m128 b0 = _mm_add_ps(a0, t1), b1 = _mm_sub_ps(a0, t1);
                    __m128 b2 = _mm_add_ps(a2, t3), b3 = _mm_sub_ps(a2, t3);
                    wr = _mm_loadu_ps(r2 + 2 * k);
                    wi = _mm_xor_ps(_mm_loadu_ps(i2 + 2 * k), conj);
                    __m128 t2 = _mm_add_ps(
                        _mm_mul_ps(b2, wr),
                        _mm_xor_ps(_mm_mul_ps(_mm_shuffle_ps(b2, b2, _MM_SHUFFLE(2, 3, 0, 1)), wi),
                                   sign));
                    __m128 t4 = _mm_add_ps(
                        _mm_mul_ps(b3, wr),
                        _mm_xor_ps(_mm_mul_ps(_mm_shuffle_ps(b3, b3, _MM_SHUFFLE(2, 3, 0, 1)), wi),
                                   sign));
                    t4 = _mm_xor_ps(_mm_shuffle_ps(t4, t4, _MM_SHUFFLE(2, 3, 0, 1)), turn);
                    _mm_storeu_ps((float *)(x0 + k), _mm_add_ps(b0, t2));
                    _mm_storeu_ps((float *)(x2 + k), _mm_sub_ps(b0, t2));
                    _mm_storeu_ps((float *)(x1 + k), _mm_add_ps(b1, t4));
                    _mm_storeu_ps((float *)(x3 + k), _mm_sub_ps(b1, t4));
                }
            }
#elif defined(NG_SEARCH_NEON)
            {
                const uint32_t signBits[4] = {UINT32_C(0x80000000), 0, UINT32_C(0x80000000), 0};
                const uint32_t forwardTurn[4] = {0, UINT32_C(0x80000000), 0, UINT32_C(0x80000000)};
                uint32x4_t sign = vld1q_u32(signBits);
                uint32x4_t conj = vdupq_n_u32(inverse ? UINT32_C(0x80000000) : 0);
                uint32x4_t turn = inverse ? sign : vld1q_u32(forwardTurn);
#define NG_CMUL(v, wr, wi)                                                                         \
    vaddq_f32(vmulq_f32((v), (wr)),                                                                \
              vreinterpretq_f32_u32(                                                               \
                  veorq_u32(vreinterpretq_u32_f32(vmulq_f32(vrev64q_f32(v), (wi))), sign)))
                for (; k + 2 <= half; k += 2) {
                    float32x4_t a0 = vld1q_f32((const float *)(x0 + k));
                    float32x4_t a1 = vld1q_f32((const float *)(x1 + k));
                    float32x4_t a2 = vld1q_f32((const float *)(x2 + k));
                    float32x4_t a3 = vld1q_f32((const float *)(x3 + k));
                    float32x4_t wr = vld1q_f32(r1 + 2 * k);
                    float32x4_t wi = vreinterpretq_f32_u32(
                        veorq_u32(vreinterpretq_u32_f32(vld1q_f32(i1 + 2 * k)), conj));
                    float32x4_t t1 = NG_CMUL(a1, wr, wi), t3 = NG_CMUL(a3, wr, wi);
                    float32x4_t b0 = vaddq_f32(a0, t1), b1 = vsubq_f32(a0, t1);
                    float32x4_t b2 = vaddq_f32(a2, t3), b3 = vsubq_f32(a2, t3);
                    wr = vld1q_f32(r2 + 2 * k);
                    wi = vreinterpretq_f32_u32(
                        veorq_u32(vreinterpretq_u32_f32(vld1q_f32(i2 + 2 * k)), conj));
                    float32x4_t t2 = NG_CMUL(b2, wr, wi), t4 = NG_CMUL(b3, wr, wi);
                    t4 = vreinterpretq_f32_u32(
                        veorq_u32(vreinterpretq_u32_f32(vrev64q_f32(t4)), turn));
                    vst1q_f32((float *)(x0 + k), vaddq_f32(b0, t2));
                    vst1q_f32((float *)(x2 + k), vsubq_f32(b0, t2));
                    vst1q_f32((float *)(x1 + k), vaddq_f32(b1, t4));
                    vst1q_f32((float *)(x3 + k), vsubq_f32(b1, t4));
                }
#undef NG_CMUL
            }
#endif
            for (; k < half; ++k) {
                float w1i = inverse ? -i1[2 * k] : i1[2 * k], w2i = inverse ? -i2[2 * k] : i2[2 * k];
                TNgComplex a0 = x0[k], a1 = x1[k], a2 = x2[k], a3 = x3[k], t1, t2, t3, t4;
                t1.i = a1.i * r1[2 * k] - a1.q * w1i;
                t1.q = a1.q * r1[2 * k] + a1.i * w1i;
                t3.i = a3.i * r1[2 * k] - a3.q * w1i;
                t3.q = a3.q * r1[2 * k] + a3.i * w1i;
                TNgComplex b0 = {a0.i + t1.i, a0.q + t1.q}, b1 = {a0.i - t1.i, a0.q - t1.q};
                TNgComplex b2 = {a2.i + t3.i, a2.q + t3.q}, b3 = {a2.i - t3.i, a2.q - t3.q};
                t2.i = b2.i * r2[2 * k] - b2.q * w2i;
                t2.q = b2.q * r2[2 * k] + b2.i * w2i;
                t4.i = b3.i * r2[2 * k] - b3.q * w2i;
                t4.q = b3.q * r2[2 * k] + b3.i * w2i;
                t4 = Quarter(t4, inverse);
                x0[k].i = b0.i + t2.i;
                x0[k].q = b0.q + t2.q;
                x2[k].i = b0.i - t2.i;
                x2[k].q = b0.q - t2.q;
                x1[k].i = b1.i + t4.i;
                x1[k].q = b1.q + t4.q;
                x3[k].i = b1.i - t4.i;
                x3[k].q = b1.q - t4.q;
            }
        }
        table += 8 * half;
    }
    if (inverse) {
        float scale = 1.0f / (float)size;
        for (k = 0; k < size; ++k) {
            values[k].i *= scale;
            values[k].q *= scale;
        }
    }
}

/** Recurrent carrier with periodic renormalization, avoiding hot-path trig. */
void NgSearchCarrier(TNgComplex *output, size_t count, double rateHz, double frequencyHz,
                     double phaseCycles, int square)
{
    double angle = -NG_SEARCH_TAU * phaseCycles;
    double step = -NG_SEARCH_TAU * frequencyHz / rateHz;
    double ci = cos(angle);
    double cq = sin(angle);
    double di = cos(step);
    double dq = sin(step);
    size_t k = 0;
#ifdef NG_SEARCH_SSE
    {
        __m128d real = _mm_set_pd(ci * di - cq * dq, ci);
        __m128d imag = _mm_set_pd(ci * dq + cq * di, cq);
        __m128d stepI = _mm_set1_pd(di * di - dq * dq);
        __m128d stepQ = _mm_set1_pd(2 * di * dq);
        for (; k + 2 <= count; k += 2) {
            __m128 ri = _mm_cvtpd_ps(real);
            __m128 rq = _mm_cvtpd_ps(imag);
            __m128 pair = _mm_unpacklo_ps(ri, rq);
            __m128d next = _mm_sub_pd(_mm_mul_pd(real, stepI), _mm_mul_pd(imag, stepQ));
            if (square)
                pair = _mm_or_ps(_mm_set1_ps(1.0f), _mm_and_ps(_mm_cmplt_ps(pair, _mm_setzero_ps()),
                                                               _mm_set1_ps(-0.0f)));
            _mm_storeu_ps((float *)(output + k), pair);
            imag = _mm_add_pd(_mm_mul_pd(real, stepQ), _mm_mul_pd(imag, stepI));
            real = next;
            if ((k & 1023u) == 1022u) {
                __m128d norm =
                    _mm_sqrt_pd(_mm_add_pd(_mm_mul_pd(real, real), _mm_mul_pd(imag, imag)));
                real = _mm_div_pd(real, norm);
                imag = _mm_div_pd(imag, norm);
            }
        }
        ci = _mm_cvtsd_f64(real);
        cq = _mm_cvtsd_f64(imag);
    }
#elif defined(NG_SEARCH_NEON)
    {
        double re[2] = {ci, ci * di - cq * dq}, im[2] = {cq, ci * dq + cq * di};
        float64x2_t real = vld1q_f64(re), imag = vld1q_f64(im);
        float64x2_t stepI = vdupq_n_f64(di * di - dq * dq), stepQ = vdupq_n_f64(2 * di * dq);
        for (; k + 2 <= count; k += 2) {
            float32x2_t ri = vcvt_f32_f64(real), rq = vcvt_f32_f64(imag);
            float32x4_t pair = vcombine_f32(vzip1_f32(ri, rq), vzip2_f32(ri, rq));
            float64x2_t next = vsubq_f64(vmulq_f64(real, stepI), vmulq_f64(imag, stepQ));
            if (square)
                pair = vreinterpretq_f32_u32(vorrq_u32(
                    vreinterpretq_u32_f32(vdupq_n_f32(1)),
                    vandq_u32(vcltq_f32(pair, vdupq_n_f32(0)), vdupq_n_u32(UINT32_C(0x80000000)))));
            vst1q_f32((float *)(output + k), pair);
            imag = vaddq_f64(vmulq_f64(real, stepQ), vmulq_f64(imag, stepI));
            real = next;
            if ((k & 1023u) == 1022u) {
                float64x2_t norm =
                    vsqrtq_f64(vaddq_f64(vmulq_f64(real, real), vmulq_f64(imag, imag)));
                real = vdivq_f64(real, norm);
                imag = vdivq_f64(imag, norm);
            }
        }
        ci = vgetq_lane_f64(real, 0);
        cq = vgetq_lane_f64(imag, 0);
    }
#endif
    for (; k < count; ++k) {
        double next = ci * di - cq * dq;
        output[k].i = square ? (ci < 0 ? -1.0f : 1.0f) : (float)ci;
        output[k].q = square ? (cq < 0 ? -1.0f : 1.0f) : (float)cq;
        cq = ci * dq + cq * di;
        ci = next;
        if ((k & 1023u) == 1023u) {
            double norm = 1.0 / sqrt(ci * ci + cq * cq);
            ci *= norm;
            cq *= norm;
        }
    }
}

/** Fixed-point sampled code, advancing without transcendental operations. */
void NgSearchCode(float *output, size_t count, const int8_t *chips, double rateHz,
                  double codeRateHz, double phaseChips)
{
    TNgGpsCaNco nco;
    size_t k = 0;
    (void)NgGpsCaNcoInit(&nco, rateHz, codeRateHz, phaseChips, NULL);
#ifdef NG_SEARCH_SSE
    /* Q32 chip positions advance in two independent 64-bit lanes. A 1-ms replica
     * stays below 2*1023 chips, so unsigned low Q32 overflow is not involved. */
    if (nco.denominator == 0 && count <= 32768u) {
        __m128i phase = _mm_set_epi64x((long long)(nco.phase + nco.step), (long long)nco.phase);
        __m128i step = _mm_set1_epi64x((long long)(2 * nco.step));
        for (; k + 2 <= count; k += 2) {
            uint64_t indices[2];
            float a;
            float b;
            _mm_storeu_si128((__m128i *)indices, _mm_srli_epi64(phase, 32));
            a = (float)chips[indices[0] % NG_GPS_CA_LENGTH];
            b = (float)chips[indices[1] % NG_GPS_CA_LENGTH];
            _mm_storel_pi((__m64 *)(output + k), _mm_set_ps(0, 0, b, a));
            phase = _mm_add_epi64(phase, step);
        }
        nco.phase = (nco.phase + (uint64_t)k * nco.step) % ((uint64_t)NG_GPS_CA_LENGTH << 32);
    }
#elif defined(NG_SEARCH_NEON)
    if (nco.denominator == 0 && count <= 32768u) {
        uint64_t initial[2] = {nco.phase, nco.phase + nco.step};
        uint64x2_t phase = vld1q_u64(initial), step = vdupq_n_u64(2 * nco.step);
        for (; k + 2 <= count; k += 2) {
            uint64_t indices[2];
            vst1q_u64(indices, vshrq_n_u64(phase, 32));
            float signs[2] = {(float)chips[indices[0] % NG_GPS_CA_LENGTH],
                              (float)chips[indices[1] % NG_GPS_CA_LENGTH]};
            vst1_f32(output + k, vld1_f32(signs));
            phase = vaddq_u64(phase, step);
        }
        nco.phase = (nco.phase + (uint64_t)k * nco.step) % ((uint64_t)NG_GPS_CA_LENGTH << 32);
    }
#endif
    for (; k < count; ++k)
        output[k] = (float)NgGpsCaNext(chips, &nco);
}

/** Vector complex multiply, with a portable tail. */
void NgSearchMix(TNgComplex *values, const TNgComplex *carrier, size_t count)
{
    size_t k = 0;
#ifdef NG_SEARCH_SSE
    const __m128 sign = _mm_set_ps(0.0f, -0.0f, 0.0f, -0.0f);
    for (; k + 2 <= count; k += 2) {
        __m128 a = _mm_loadu_ps((const float *)(values + k));
        __m128 b = _mm_loadu_ps((const float *)(carrier + k));
        __m128 real = _mm_shuffle_ps(b, b, _MM_SHUFFLE(2, 2, 0, 0));
        __m128 imag = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 3, 1, 1));
        __m128 swap = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1));
        _mm_storeu_ps((float *)(values + k),
                      _mm_add_ps(_mm_mul_ps(a, real), _mm_xor_ps(_mm_mul_ps(swap, imag), sign)));
    }
#elif defined(NG_SEARCH_NEON)
    const uint32_t signs[4] = {UINT32_C(0x80000000), 0, UINT32_C(0x80000000), 0};
    for (; k + 2 <= count; k += 2) {
        float32x4_t a = vld1q_f32((const float *)(values + k));
        float32x4_t b = vld1q_f32((const float *)(carrier + k));
        float32x4_t cross = vmulq_f32(vrev64q_f32(a), vtrn2q_f32(b, b));
        cross = vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(cross), vld1q_u32(signs)));
        vst1q_f32((float *)(values + k), vaddq_f32(vmulq_f32(a, vtrn1q_f32(b, b)), cross));
    }
#endif
    for (; k < count; ++k) {
        TNgComplex a = values[k];
        values[k].i = a.i * carrier[k].i - a.q * carrier[k].q;
        values[k].q = a.i * carrier[k].q + a.q * carrier[k].i;
    }
}

/** Widen products before reduction; XOR changes only the exact sign bit. */
void NgSearchCorrelate(const TNgComplex *values, const float *code, size_t count, int xorSigns,
                       double *i, double *q)
{
    size_t k = 0;
    *i = 0;
    *q = 0;
#ifdef NG_SEARCH_SSE
    __m128d low = _mm_setzero_pd();
    __m128d high = _mm_setzero_pd();
    for (; k + 2 <= count; k += 2) {
        __m128 a = _mm_loadu_ps((const float *)(values + k));
        __m128 c = _mm_set_ps(code[k + 1], code[k + 1], code[k], code[k]);
        __m128 p = xorSigns ? _mm_xor_ps(a, _mm_and_ps(c, _mm_set1_ps(-0.0f))) : _mm_mul_ps(a, c);
        low = _mm_add_pd(low, _mm_cvtps_pd(p));
        high = _mm_add_pd(high, _mm_cvtps_pd(_mm_movehl_ps(p, p)));
    }
    {
        double sum[2];
        _mm_storeu_pd(sum, _mm_add_pd(low, high));
        *i = sum[0];
        *q = sum[1];
    }
#elif defined(NG_SEARCH_NEON)
    float64x2_t low = vdupq_n_f64(0), high = vdupq_n_f64(0);
    for (; k + 2 <= count; k += 2) {
        float32x4_t a = vld1q_f32((const float *)(values + k));
        float32x2_t signs = vld1_f32(code + k);
        float32x4_t c = vcombine_f32(vdup_lane_f32(signs, 0), vdup_lane_f32(signs, 1));
        float32x4_t p =
            xorSigns ? vreinterpretq_f32_u32(veorq_u32(
                           vreinterpretq_u32_f32(a),
                           vandq_u32(vreinterpretq_u32_f32(c), vdupq_n_u32(UINT32_C(0x80000000)))))
                     : vmulq_f32(a, c);
        low = vaddq_f64(low, vcvt_f64_f32(vget_low_f32(p)));
        high = vaddq_f64(high, vcvt_f64_f32(vget_high_f32(p)));
    }
    float64x2_t sum = vaddq_f64(low, high);
    *i = vgetq_lane_f64(sum, 0);
    *q = vgetq_lane_f64(sum, 1);
#else
    (void)xorSigns;
#endif
    for (; k < count; ++k) {
        *i += values[k].i * code[k];
        *q += values[k].q * code[k];
    }
}

/** Interleaved (I, Q) running double sums: prefix[2n..2n+1] sums values[0..n-1]. */
void NgSearchPrefix(double *prefix, const TNgComplex *values, size_t count)
{
    size_t k;
    double i = 0, q = 0;
    prefix[0] = prefix[1] = 0;
    for (k = 0; k < count; ++k) {
        i += (double)values[k].i;
        q += (double)values[k].q;
        prefix[2 * k + 2] = i;
        prefix[2 * k + 3] = q;
    }
}

/** One pass of NgSearchCarrier, CI8 widening, NgSearchMix and NgSearchPrefix. The carrier
 * recurrence, its renormalization and the product keep the separate kernels' arithmetic,
 * so the mixed samples are the same; the independent carrier and running-sum dependency
 * chains overlap instead of running one after the other. */
void NgSearchMixPrefixCi8(double *prefix, const TNgIq8 *input, size_t count, double rateHz,
                          double frequencyHz, double phaseCycles, int square, int inverted)
{
    double angle = -NG_SEARCH_TAU * phaseCycles;
    double step = -NG_SEARCH_TAU * frequencyHz / rateHz;
    double ci = cos(angle);
    double cq = sin(angle);
    double di = cos(step);
    double dq = sin(step);
    double sumI = 0, sumQ = 0;
    float polarity = inverted ? -1.0f : 1.0f;
    size_t k = 0;
    prefix[0] = prefix[1] = 0;
#ifdef NG_SEARCH_SSE
    {
        __m128d real = _mm_set_pd(ci * di - cq * dq, ci);
        __m128d imag = _mm_set_pd(ci * dq + cq * di, cq);
        __m128d stepI = _mm_set1_pd(di * di - dq * dq);
        __m128d stepQ = _mm_set1_pd(2 * di * dq);
        __m128d sum = _mm_setzero_pd();
        const __m128 sign = _mm_set_ps(0.0f, -0.0f, 0.0f, -0.0f);
        const __m128 flip = _mm_set_ps(polarity, 1.0f, polarity, 1.0f);
        for (; k + 2 <= count; k += 2) {
            __m128 ri = _mm_cvtpd_ps(real);
            __m128 rq = _mm_cvtpd_ps(imag);
            __m128 b = _mm_unpacklo_ps(ri, rq);
            __m128d next = _mm_sub_pd(_mm_mul_pd(real, stepI), _mm_mul_pd(imag, stepQ));
            int32_t packed;
            if (square)
                b = _mm_or_ps(_mm_set1_ps(1.0f),
                              _mm_and_ps(_mm_cmplt_ps(b, _mm_setzero_ps()), _mm_set1_ps(-0.0f)));
            imag = _mm_add_pd(_mm_mul_pd(real, stepQ), _mm_mul_pd(imag, stepI));
            real = next;
            if ((k & 1023u) == 1022u) {
                __m128d norm =
                    _mm_sqrt_pd(_mm_add_pd(_mm_mul_pd(real, real), _mm_mul_pd(imag, imag)));
                real = _mm_div_pd(real, norm);
                imag = _mm_div_pd(imag, norm);
            }
            memcpy(&packed, input + k, sizeof(packed));
            __m128i bytes = _mm_cvtsi32_si128(packed);
            __m128i words = _mm_srai_epi16(_mm_unpacklo_epi8(bytes, bytes), 8);
            __m128i ints = _mm_srai_epi32(_mm_unpacklo_epi16(words, words), 16);
            __m128 a = _mm_mul_ps(_mm_cvtepi32_ps(ints), flip);
            __m128 breal = _mm_shuffle_ps(b, b, _MM_SHUFFLE(2, 2, 0, 0));
            __m128 bimag = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 3, 1, 1));
            __m128 swap = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1));
            __m128 m =
                _mm_add_ps(_mm_mul_ps(a, breal), _mm_xor_ps(_mm_mul_ps(swap, bimag), sign));
            sum = _mm_add_pd(sum, _mm_cvtps_pd(m));
            _mm_storeu_pd(prefix + 2 * k + 2, sum);
            sum = _mm_add_pd(sum, _mm_cvtps_pd(_mm_movehl_ps(m, m)));
            _mm_storeu_pd(prefix + 2 * k + 4, sum);
        }
        ci = _mm_cvtsd_f64(real);
        cq = _mm_cvtsd_f64(imag);
        sumI = _mm_cvtsd_f64(sum);
        sumQ = _mm_cvtsd_f64(_mm_unpackhi_pd(sum, sum));
    }
#elif defined(NG_SEARCH_NEON)
    {
        double re[2] = {ci, ci * di - cq * dq}, im[2] = {cq, ci * dq + cq * di};
        float64x2_t real = vld1q_f64(re), imag = vld1q_f64(im);
        float64x2_t stepI = vdupq_n_f64(di * di - dq * dq), stepQ = vdupq_n_f64(2 * di * dq);
        float64x2_t sum = vdupq_n_f64(0);
        const uint32_t signBits[4] = {UINT32_C(0x80000000), 0, UINT32_C(0x80000000), 0};
        const float flipValues[4] = {1.0f, polarity, 1.0f, polarity};
        uint32x4_t sign = vld1q_u32(signBits);
        float32x4_t flip = vld1q_f32(flipValues);
        for (; k + 2 <= count; k += 2) {
            float32x2_t ri = vcvt_f32_f64(real), rq = vcvt_f32_f64(imag);
            float32x4_t b = vcombine_f32(vzip1_f32(ri, rq), vzip2_f32(ri, rq));
            float64x2_t next = vsubq_f64(vmulq_f64(real, stepI), vmulq_f64(imag, stepQ));
            if (square)
                b = vreinterpretq_f32_u32(vorrq_u32(
                    vreinterpretq_u32_f32(vdupq_n_f32(1)),
                    vandq_u32(vcltq_f32(b, vdupq_n_f32(0)), vdupq_n_u32(UINT32_C(0x80000000)))));
            imag = vaddq_f64(vmulq_f64(real, stepQ), vmulq_f64(imag, stepI));
            real = next;
            if ((k & 1023u) == 1022u) {
                float64x2_t norm =
                    vsqrtq_f64(vaddq_f64(vmulq_f64(real, real), vmulq_f64(imag, imag)));
                real = vdivq_f64(real, norm);
                imag = vdivq_f64(imag, norm);
            }
            uint32_t packed;
            memcpy(&packed, input + k, sizeof(packed));
            int8x8_t bytes = vreinterpret_s8_u32(vdup_n_u32(packed));
            int32x4_t ints = vmovl_s16(vget_low_s16(vmovl_s8(bytes)));
            float32x4_t a = vmulq_f32(vcvtq_f32_s32(ints), flip);
            float32x4_t cross = vmulq_f32(vrev64q_f32(a), vtrn2q_f32(b, b));
            cross = vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(cross), sign));
            float32x4_t m = vaddq_f32(vmulq_f32(a, vtrn1q_f32(b, b)), cross);
            sum = vaddq_f64(sum, vcvt_f64_f32(vget_low_f32(m)));
            vst1q_f64(prefix + 2 * k + 2, sum);
            sum = vaddq_f64(sum, vcvt_f64_f32(vget_high_f32(m)));
            vst1q_f64(prefix + 2 * k + 4, sum);
        }
        ci = vgetq_lane_f64(real, 0);
        cq = vgetq_lane_f64(imag, 0);
        sumI = vgetq_lane_f64(sum, 0);
        sumQ = vgetq_lane_f64(sum, 1);
    }
#endif
    for (; k < count; ++k) {
        TNgComplex carrier, a, value;
        double next = ci * di - cq * dq;
        carrier.i = square ? (ci < 0 ? -1.0f : 1.0f) : (float)ci;
        carrier.q = square ? (cq < 0 ? -1.0f : 1.0f) : (float)cq;
        cq = ci * dq + cq * di;
        ci = next;
        if ((k & 1023u) == 1023u) {
            double norm = 1.0 / sqrt(ci * ci + cq * cq);
            ci *= norm;
            cq *= norm;
        }
        a.i = (float)input[k].i;
        a.q = (float)input[k].q * polarity;
        value.i = a.i * carrier.i - a.q * carrier.q;
        value.q = a.i * carrier.q + a.q * carrier.i;
        sumI += (double)value.i;
        sumQ += (double)value.q;
        prefix[2 * k + 2] = sumI;
        prefix[2 * k + 3] = sumQ;
    }
}

/** Four-lane variant of NgSearchMixPrefixCi8 for throughput. The carrier stays a double
 * precision rotation without tables or phase quantization, but four samples advance
 * independently by exp(-j4w) from exactly computed starting phases, and running sums are
 * formed four samples at a time. Rounding therefore differs from the one-lane kernel at
 * double precision level (carrier deviation well below 1e-12 before float conversion). */
void NgSearchMixPrefixCi8Lanes(double *prefix, const TNgIq8 *input, size_t count, double rateHz,
                               double frequencyHz, double phaseCycles, int square, int inverted)
{
    double step = -NG_SEARCH_TAU * frequencyHz / rateHz;
    size_t k = 0;
    prefix[0] = prefix[1] = 0;
#if defined(NG_SEARCH_SSE) || defined(NG_SEARCH_NEON)
    double sumI = 0, sumQ = 0, start[4][2], di = cos(4 * step), dq = sin(4 * step);
    float polarity = inverted ? -1.0f : 1.0f;
    unsigned lane;
    if (count < 4) {
        NgSearchMixPrefixCi8(prefix, input, count, rateHz, frequencyHz, phaseCycles, square,
                             inverted);
        return;
    }
    for (lane = 0; lane < 4; ++lane) {
        double angle = -NG_SEARCH_TAU * phaseCycles + (double)lane * step;
        start[lane][0] = cos(angle);
        start[lane][1] = sin(angle);
    }
#endif
#ifdef NG_SEARCH_SSE
    {
        __m128d re0 = _mm_set_pd(start[1][0], start[0][0]), im0 = _mm_set_pd(start[1][1], start[0][1]);
        __m128d re1 = _mm_set_pd(start[3][0], start[2][0]), im1 = _mm_set_pd(start[3][1], start[2][1]);
        __m128d stepI = _mm_set1_pd(di), stepQ = _mm_set1_pd(dq), sum = _mm_setzero_pd();
        const __m128 sign = _mm_set_ps(0.0f, -0.0f, 0.0f, -0.0f);
        const __m128 flip = _mm_set_ps(polarity, 1.0f, polarity, 1.0f);
#define NG_LANE_SQUARE(b)                                                                         \
    _mm_or_ps(_mm_set1_ps(1.0f), _mm_and_ps(_mm_cmplt_ps((b), _mm_setzero_ps()), _mm_set1_ps(-0.0f)))
#define NG_LANE_MIX(a, b)                                                                         \
    _mm_add_ps(_mm_mul_ps((a), _mm_shuffle_ps((b), (b), _MM_SHUFFLE(2, 2, 0, 0))),                \
               _mm_xor_ps(_mm_mul_ps(_mm_shuffle_ps((a), (a), _MM_SHUFFLE(2, 3, 0, 1)),          \
                                     _mm_shuffle_ps((b), (b), _MM_SHUFFLE(3, 3, 1, 1))),         \
                          sign))
        for (; k + 4 <= count; k += 4) {
            __m128 b0 = _mm_unpacklo_ps(_mm_cvtpd_ps(re0), _mm_cvtpd_ps(im0));
            __m128 b1 = _mm_unpacklo_ps(_mm_cvtpd_ps(re1), _mm_cvtpd_ps(im1));
            __m128d n0 = _mm_sub_pd(_mm_mul_pd(re0, stepI), _mm_mul_pd(im0, stepQ));
            __m128d n1 = _mm_sub_pd(_mm_mul_pd(re1, stepI), _mm_mul_pd(im1, stepQ));
            im0 = _mm_add_pd(_mm_mul_pd(re0, stepQ), _mm_mul_pd(im0, stepI));
            im1 = _mm_add_pd(_mm_mul_pd(re1, stepQ), _mm_mul_pd(im1, stepI));
            re0 = n0;
            re1 = n1;
            if ((k & 1023u) == 1020u) {
                __m128d g0 = _mm_sqrt_pd(_mm_add_pd(_mm_mul_pd(re0, re0), _mm_mul_pd(im0, im0)));
                __m128d g1 = _mm_sqrt_pd(_mm_add_pd(_mm_mul_pd(re1, re1), _mm_mul_pd(im1, im1)));
                re0 = _mm_div_pd(re0, g0);
                im0 = _mm_div_pd(im0, g0);
                re1 = _mm_div_pd(re1, g1);
                im1 = _mm_div_pd(im1, g1);
            }
            if (square) {
                b0 = NG_LANE_SQUARE(b0);
                b1 = NG_LANE_SQUARE(b1);
            }
            __m128i bytes = _mm_loadl_epi64((const __m128i *)(const void *)(input + k));
            __m128i words = _mm_srai_epi16(_mm_unpacklo_epi8(bytes, bytes), 8);
            __m128 a0 = _mm_mul_ps(
                _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(words, words), 16)), flip);
            __m128 a1 = _mm_mul_ps(
                _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpackhi_epi16(words, words), 16)), flip);
            __m128 m0 = NG_LANE_MIX(a0, b0), m1 = NG_LANE_MIX(a1, b1);
            __m128d v0 = _mm_cvtps_pd(m0), v1 = _mm_cvtps_pd(_mm_movehl_ps(m0, m0));
            __m128d v2 = _mm_cvtps_pd(m1), v3 = _mm_cvtps_pd(_mm_movehl_ps(m1, m1));
            __m128d s01 = _mm_add_pd(v0, v1), s23 = _mm_add_pd(v2, v3);
            __m128d half = _mm_add_pd(sum, s01);
            _mm_storeu_pd(prefix + 2 * k + 2, _mm_add_pd(sum, v0));
            _mm_storeu_pd(prefix + 2 * k + 4, half);
            _mm_storeu_pd(prefix + 2 * k + 6, _mm_add_pd(half, v2));
            sum = _mm_add_pd(sum, _mm_add_pd(s01, s23));
            _mm_storeu_pd(prefix + 2 * k + 8, sum);
        }
#undef NG_LANE_SQUARE
#undef NG_LANE_MIX
        sumI = _mm_cvtsd_f64(sum);
        sumQ = _mm_cvtsd_f64(_mm_unpackhi_pd(sum, sum));
    }
#elif defined(NG_SEARCH_NEON)
    {
        const double r01[2] = {start[0][0], start[1][0]}, i01[2] = {start[0][1], start[1][1]};
        const double r23[2] = {start[2][0], start[3][0]}, i23[2] = {start[2][1], start[3][1]};
        float64x2_t re0 = vld1q_f64(r01), im0 = vld1q_f64(i01);
        float64x2_t re1 = vld1q_f64(r23), im1 = vld1q_f64(i23);
        float64x2_t stepI = vdupq_n_f64(di), stepQ = vdupq_n_f64(dq), sum = vdupq_n_f64(0);
        const uint32_t signBits[4] = {UINT32_C(0x80000000), 0, UINT32_C(0x80000000), 0};
        const float flipValues[4] = {1.0f, polarity, 1.0f, polarity};
        uint32x4_t sign = vld1q_u32(signBits);
        float32x4_t flip = vld1q_f32(flipValues);
#define NG_LANE_SQUARE(b)                                                                         \
    vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(vdupq_n_f32(1)),                         \
                                    vandq_u32(vcltq_f32((b), vdupq_n_f32(0)),                      \
                                              vdupq_n_u32(UINT32_C(0x80000000)))))
#define NG_LANE_MIX(a, b)                                                                         \
    vaddq_f32(vmulq_f32((a), vtrn1q_f32((b), (b))),                                                \
              vreinterpretq_f32_u32(veorq_u32(                                                     \
                  vreinterpretq_u32_f32(vmulq_f32(vrev64q_f32(a), vtrn2q_f32((b), (b)))), sign)))
        for (; k + 4 <= count; k += 4) {
            float32x2_t r0 = vcvt_f32_f64(re0), q0 = vcvt_f32_f64(im0);
            float32x2_t r1 = vcvt_f32_f64(re1), q1 = vcvt_f32_f64(im1);
            float32x4_t b0 = vcombine_f32(vzip1_f32(r0, q0), vzip2_f32(r0, q0));
            float32x4_t b1 = vcombine_f32(vzip1_f32(r1, q1), vzip2_f32(r1, q1));
            float64x2_t n0 = vsubq_f64(vmulq_f64(re0, stepI), vmulq_f64(im0, stepQ));
            float64x2_t n1 = vsubq_f64(vmulq_f64(re1, stepI), vmulq_f64(im1, stepQ));
            im0 = vaddq_f64(vmulq_f64(re0, stepQ), vmulq_f64(im0, stepI));
            im1 = vaddq_f64(vmulq_f64(re1, stepQ), vmulq_f64(im1, stepI));
            re0 = n0;
            re1 = n1;
            if ((k & 1023u) == 1020u) {
                float64x2_t g0 = vsqrtq_f64(vaddq_f64(vmulq_f64(re0, re0), vmulq_f64(im0, im0)));
                float64x2_t g1 = vsqrtq_f64(vaddq_f64(vmulq_f64(re1, re1), vmulq_f64(im1, im1)));
                re0 = vdivq_f64(re0, g0);
                im0 = vdivq_f64(im0, g0);
                re1 = vdivq_f64(re1, g1);
                im1 = vdivq_f64(im1, g1);
            }
            if (square) {
                b0 = NG_LANE_SQUARE(b0);
                b1 = NG_LANE_SQUARE(b1);
            }
            int8x8_t bytes = vld1_s8((const int8_t *)(const void *)(input + k));
            int16x8_t words = vmovl_s8(bytes);
            float32x4_t a0 = vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_low_s16(words))), flip);
            float32x4_t a1 = vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_high_s16(words))), flip);
            float32x4_t m0 = NG_LANE_MIX(a0, b0), m1 = NG_LANE_MIX(a1, b1);
            float64x2_t v0 = vcvt_f64_f32(vget_low_f32(m0)), v1 = vcvt_f64_f32(vget_high_f32(m0));
            float64x2_t v2 = vcvt_f64_f32(vget_low_f32(m1)), v3 = vcvt_f64_f32(vget_high_f32(m1));
            float64x2_t s01 = vaddq_f64(v0, v1), s23 = vaddq_f64(v2, v3);
            float64x2_t half = vaddq_f64(sum, s01);
            vst1q_f64(prefix + 2 * k + 2, vaddq_f64(sum, v0));
            vst1q_f64(prefix + 2 * k + 4, half);
            vst1q_f64(prefix + 2 * k + 6, vaddq_f64(half, v2));
            sum = vaddq_f64(sum, vaddq_f64(s01, s23));
            vst1q_f64(prefix + 2 * k + 8, sum);
        }
#undef NG_LANE_SQUARE
#undef NG_LANE_MIX
        sumI = vgetq_lane_f64(sum, 0);
        sumQ = vgetq_lane_f64(sum, 1);
    }
#endif
#if defined(NG_SEARCH_SSE) || defined(NG_SEARCH_NEON)
    for (; k < count; ++k) {
        /* At most three samples: exact phase, then the one-lane arithmetic. */
        double angle = -NG_SEARCH_TAU * phaseCycles + (double)k * step;
        TNgComplex carrier, a, value;
        double ci = cos(angle), cq = sin(angle);
        carrier.i = square ? (ci < 0 ? -1.0f : 1.0f) : (float)ci;
        carrier.q = square ? (cq < 0 ? -1.0f : 1.0f) : (float)cq;
        a.i = (float)input[k].i;
        a.q = (float)input[k].q * polarity;
        value.i = a.i * carrier.i - a.q * carrier.q;
        value.q = a.i * carrier.q + a.q * carrier.i;
        sumI += (double)value.i;
        sumQ += (double)value.q;
        prefix[2 * k + 2] = sumI;
        prefix[2 * k + 3] = sumQ;
    }
#else
    (void)step;
    (void)k;
    NgSearchMixPrefixCi8(prefix, input, count, rateHz, frequencyHz, phaseCycles, square,
                         inverted);
#endif
}
