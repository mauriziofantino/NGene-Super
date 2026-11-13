/** @file tracking_replica.c @brief SSE2/NEON double phase lanes and sampled code signs. */
#include "ngene/dsp/tracking_replica.h"
#include "dsp/sample_backend.h"
#include <math.h>

/** Generate two exact sample phase lanes, keeping division before the offset. */
void NgTrackingCodePhases(double *phases, size_t count, double base, double codeRate,
                          double sampleRate)
{
    size_t n = 0;
#if NG_SSE2
    for (; n + 2 <= count; n += 2) {
        __m128d index = _mm_set_pd((double)(n + 1), (double)n);
        _mm_storeu_pd(phases + n, _mm_add_pd(_mm_set1_pd(base),
                                             _mm_div_pd(_mm_mul_pd(index, _mm_set1_pd(codeRate)),
                                                        _mm_set1_pd(sampleRate))));
    }
#elif NG_NEON
    for (; n + 2 <= count; n += 2) {
        double indices[2] = {(double)n, (double)(n + 1)};
        vst1q_f64(phases + n,
                  vaddq_f64(vdupq_n_f64(base), vdivq_f64(vmulq_n_f64(vld1q_f64(indices), codeRate),
                                                         vdupq_n_f64(sampleRate))));
    }
#endif
    for (; n < count; ++n)
        phases[n] = base + (double)n * codeRate / sampleRate;
}

/** Generate +/-1 replicas without per-sample fmod/floor calls. */
void NgTrackingReplica(float *output, const double *phases, size_t count, const int8_t *chips,
                       unsigned period, double offset, int boc)
{
    size_t n = 0;
    int needsWrap = count && (phases[0] - offset < 0 || phases[count - 1] - offset >= period);
    (void)needsWrap;
#if NG_SSE2
    __m128d length = _mm_set1_pd((double)period);
    for (; n + 2 <= count; n += 2) {
        int indices[4];
        __m128d phase = _mm_sub_pd(_mm_loadu_pd(phases + n), _mm_set1_pd(offset));
        if (needsWrap) {
            phase = _mm_add_pd(phase, _mm_and_pd(_mm_cmplt_pd(phase, _mm_setzero_pd()), length));
            phase = _mm_sub_pd(phase, _mm_and_pd(_mm_cmpge_pd(phase, length), length));
        }
        phase = _mm_sub_pd(_mm_add_pd(phase, length), length);
        phase = _mm_sub_pd(phase, _mm_and_pd(_mm_cmpge_pd(phase, length), length));
        if (boc)
            phase = _mm_add_pd(phase, phase);
        __m128i integer = _mm_cvttpd_epi32(phase);
        _mm_storeu_si128((__m128i *)indices, integer);
        __m128 signs = _mm_set_ps(0, 0, (float)chips[indices[1]], (float)chips[indices[0]]);
        _mm_storel_pi((__m64 *)(output + n), signs);
    }
#elif NG_NEON
    float64x2_t length = vdupq_n_f64((double)period);
    for (; n + 2 <= count; n += 2) {
        uint64_t indices[2];
        float64x2_t phase = vsubq_f64(vld1q_f64(phases + n), vdupq_n_f64(offset));
        if (needsWrap) {
            phase = vaddq_f64(phase,
                              vbslq_f64(vcltq_f64(phase, vdupq_n_f64(0)), length, vdupq_n_f64(0)));
            phase = vsubq_f64(phase, vbslq_f64(vcgeq_f64(phase, length), length, vdupq_n_f64(0)));
        }
        phase = vsubq_f64(vaddq_f64(phase, length), length);
        phase = vsubq_f64(phase, vbslq_f64(vcgeq_f64(phase, length), length, vdupq_n_f64(0)));
        if (boc)
            phase = vaddq_f64(phase, phase);
        uint64x2_t integer = vcvtq_u64_f64(phase);
        vst1q_u64(indices, integer);
        float signs[2] = {(float)chips[indices[0]], (float)chips[indices[1]]};
        vst1_f32(output + n, vld1_f32(signs));
    }
#endif
    for (; n < count; ++n) {
        double phase = phases[n] - offset;
        if (phase < 0)
            phase += period;
        else if (phase >= period)
            phase -= period;
        phase = (phase + period) - period;
        if (phase >= period)
            phase -= period;
        int sign = chips[(size_t)(boc ? 2 * phase : phase)];
        output[n] = (float)sign;
    }
}
