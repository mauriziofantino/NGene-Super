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
    TNgComplex *twiddle;
};

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
    plan->twiddle = calloc(size / 2, sizeof(*plan->twiddle));
    if (plan->reverse == NULL || plan->twiddle == NULL) {
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
    for (k = 0; k < size / 2; ++k) {
        double angle = NG_SEARCH_TAU * (double)k / (double)size;
        plan->twiddle[k].i = (float)cos(angle);
        plan->twiddle[k].q = (float)-sin(angle);
    }
    *out = plan;
    return NG_OK;
}

/** Release owned tables. */
void NgSearchFftDestroy(TNgSearchFft *plan)
{
    if (plan != NULL) {
        free(plan->reverse);
        free(plan->twiddle);
        free(plan);
    }
}

/** Iterative radix-2 transform; the workspace is the only mutable object. */
void NgSearchFftExecute(const TNgSearchFft *plan, TNgComplex *values, int inverse)
{
    size_t k;
    size_t width;
    for (k = 0; k < plan->size; ++k) {
        if (k < plan->reverse[k]) {
            TNgComplex temp = values[k];
            values[k] = values[plan->reverse[k]];
            values[plan->reverse[k]] = temp;
        }
    }
    for (width = 2; width <= plan->size; width <<= 1) {
        size_t base;
        size_t half = width / 2;
        size_t stride = plan->size / width;
        for (base = 0; base < plan->size; base += width) {
            k = 0;
#ifdef NG_SEARCH_SSE
            for (; k + 2 <= half; k += 2) {
                TNgComplex t0 = plan->twiddle[k * stride];
                TNgComplex t1 = plan->twiddle[(k + 1) * stride];
                __m128 a = _mm_loadu_ps((const float *)(values + base + k));
                __m128 b = _mm_loadu_ps((const float *)(values + base + half + k));
                __m128 real = _mm_set_ps(t1.i, t1.i, t0.i, t0.i);
                __m128 imag = _mm_set_ps(t1.q, t1.q, t0.q, t0.q);
                __m128 sign = _mm_set_ps(0.0f, -0.0f, 0.0f, -0.0f);
                __m128 swap = _mm_shuffle_ps(b, b, _MM_SHUFFLE(2, 3, 0, 1));
                __m128 product;
                if (inverse)
                    imag = _mm_xor_ps(imag, _mm_set1_ps(-0.0f));
                product = _mm_add_ps(_mm_mul_ps(b, real), _mm_xor_ps(_mm_mul_ps(swap, imag), sign));
                _mm_storeu_ps((float *)(values + base + k), _mm_add_ps(a, product));
                _mm_storeu_ps((float *)(values + base + half + k), _mm_sub_ps(a, product));
            }
#elif defined(NG_SEARCH_NEON)
            for (; k + 2 <= half; k += 2) {
                TNgComplex t0 = plan->twiddle[k * stride];
                TNgComplex t1 = plan->twiddle[(k + 1) * stride];
                float components[4] = {t0.i, t0.q, t1.i, t1.q};
                uint32_t signs[4] = {UINT32_C(0x80000000), 0, UINT32_C(0x80000000), 0};
                float32x4_t tw = vld1q_f32(components);
                float32x4_t real = vtrn1q_f32(tw, tw), imag = vtrn2q_f32(tw, tw);
                float32x4_t a = vld1q_f32((const float *)(values + base + k));
                float32x4_t b = vld1q_f32((const float *)(values + base + half + k));
                if (inverse)
                    imag = vnegq_f32(imag);
                float32x4_t cross = vmulq_f32(vrev64q_f32(b), imag);
                cross = vreinterpretq_f32_u32(
                    veorq_u32(vreinterpretq_u32_f32(cross), vld1q_u32(signs)));
                float32x4_t product = vaddq_f32(vmulq_f32(b, real), cross);
                vst1q_f32((float *)(values + base + k), vaddq_f32(a, product));
                vst1q_f32((float *)(values + base + half + k), vsubq_f32(a, product));
            }
#endif
            for (; k < half; ++k) {
                TNgComplex tw = plan->twiddle[k * stride];
                TNgComplex a = values[base + k];
                TNgComplex b = values[base + half + k];
                float ri;
                float rq;
                if (inverse)
                    tw.q = -tw.q;
                ri = b.i * tw.i - b.q * tw.q;
                rq = b.i * tw.q + b.q * tw.i;
                values[base + k].i = a.i + ri;
                values[base + k].q = a.q + rq;
                values[base + half + k].i = a.i - ri;
                values[base + half + k].q = a.q - rq;
            }
        }
    }
    if (inverse) {
        float scale = 1.0f / (float)plan->size;
        for (k = 0; k < plan->size; ++k) {
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
