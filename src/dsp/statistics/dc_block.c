/* Two-pass block-mean subtraction, following Classic's widened SIMD boundary.
 * No history, IIR, gain normalization or writes to the native input stream. */
#include "ngene/dsp/statistics.h"
#include "dsp/sample_backend.h"

static int Offset(int64_t sum, size_t n)
{
    int64_t count = (int64_t)n;
    return (int)(sum >= 0 ? (sum + count / 2) / count : -((-sum + count / 2) / count));
}

int NgIqCenterBlock(const TNgIqView *view, int32_t *output, int dc[2], TNgError *error)
{
    int64_t sum[2] = {0, 0};
    size_t k = 0, j, n;
    int narrow;
    TNgSampleFormat format;
    if (!view || !view->samples || !output || !dc || !view->meta.count ||
        view->meta.count > 262144)
        return NgFail(error, "DC block requires 1..262144 I/Q pairs and owned output storage");
    n = view->meta.count;
    format = view->format ? view->format : NG_FORMAT_CI8;
    narrow = format == NG_FORMAT_CI8 || format == NG_FORMAT_CU8;
    if (!narrow && format != NG_FORMAT_CI16 && format != NG_FORMAT_SC16_Q11)
        return NgFail(error, "Unsupported DC block sample format");
#if NG_SSE2
    {
        __m128i si = _mm_setzero_si128(), sq = si;
        for (; k + (narrow ? 8u : 4u) <= n; k += narrow ? 8u : 4u) {
            const unsigned char *p = (const unsigned char *)(const void *)view->samples;
            __m128i raw = _mm_loadu_si128((const __m128i *)(const void *)(p + k * (narrow ? 2u : 4u)));
            if (narrow) {
                __m128i i, q;
                const __m128i one = _mm_set1_epi16(1);
                if (format == NG_FORMAT_CU8) raw = _mm_xor_si128(raw, _mm_set1_epi8((char)-128));
                i = _mm_srai_epi16(_mm_slli_epi16(raw, 8), 8);
                q = _mm_srai_epi16(raw, 8);
                si = _mm_add_epi32(si, _mm_madd_epi16(i, one));
                sq = _mm_add_epi32(sq, _mm_madd_epi16(q, one));
            } else {
                if (format == NG_FORMAT_SC16_Q11 &&
                    _mm_movemask_epi8(_mm_or_si128(_mm_cmpgt_epi16(raw, _mm_set1_epi16(2047)),
                                                   _mm_cmpgt_epi16(_mm_set1_epi16(-2048), raw))))
                    return NgFail(error, "SC16 Q11 level outside ADC range");
                si = _mm_add_epi32(si, _mm_srai_epi32(_mm_slli_epi32(raw, 16), 16));
                sq = _mm_add_epi32(sq, _mm_srai_epi32(raw, 16));
            }
        }
        {
            int32_t i[4], q[4];
            _mm_storeu_si128((__m128i *)(void *)i, si);
            _mm_storeu_si128((__m128i *)(void *)q, sq);
            for (j = 0; j < 4; ++j) { sum[0] += i[j]; sum[1] += q[j]; }
        }
    }
#elif NG_NEON
    {
        int32x4_t si = vdupq_n_s32(0), sq = si;
        for (; k + (narrow ? 8u : 4u) <= n; k += narrow ? 8u : 4u) {
            const unsigned char *p = (const unsigned char *)(const void *)view->samples;
            if (narrow) {
                int8x8x2_t v = vld2_s8((const int8_t *)(const void *)(p + 2 * k));
                if (format == NG_FORMAT_CU8) {
                    v.val[0] = veor_s8(v.val[0], vdup_n_s8(-128));
                    v.val[1] = veor_s8(v.val[1], vdup_n_s8(-128));
                }
                si = vpadalq_s16(si, vmovl_s8(v.val[0]));
                sq = vpadalq_s16(sq, vmovl_s8(v.val[1]));
            } else {
                int16x4x2_t v = vld2_s16((const int16_t *)(const void *)(p + 4 * k));
                if (format == NG_FORMAT_SC16_Q11) {
                    uint16x4_t invalid = vorr_u16(vorr_u16(vcgt_s16(v.val[0], vdup_n_s16(2047)),
                                                        vclt_s16(v.val[0], vdup_n_s16(-2048))),
                                                vorr_u16(vcgt_s16(v.val[1], vdup_n_s16(2047)),
                                                        vclt_s16(v.val[1], vdup_n_s16(-2048))));
                    if (vmaxv_u16(invalid)) return NgFail(error, "SC16 Q11 level outside ADC range");
                }
                si = vaddw_s16(si, v.val[0]);
                sq = vaddw_s16(sq, v.val[1]);
            }
        }
        sum[0] = vaddlvq_s32(si); sum[1] = vaddlvq_s32(sq);
    }
#endif
    for (; k < n; ++k) for (j = 0; j < 2; ++j) {
        int v = NgIqComponent(view, k, (unsigned)j);
        if (format == NG_FORMAT_SC16_Q11 && (v < -2048 || v > 2047))
            return NgFail(error, "SC16 Q11 level outside ADC range");
        sum[j] += v;
    }
    dc[0] = Offset(sum[0], n); dc[1] = Offset(sum[1], n);
    return NgIqSubtractDc(view, output, dc, error);
}

/** @brief Apply cached offsets through widened SIMD subtraction, without a sum pass. */
int NgIqSubtractDc(const TNgIqView *view, int32_t *output, const int dc[2], TNgError *error)
{
    size_t k = 0, j, n;
    int narrow, limit;
    TNgSampleFormat format;
    if (!view || !view->samples || !output || !dc || !view->meta.count ||
        view->meta.count > 262144)
        return NgFail(error, "DC subtraction requires 1..262144 I/Q pairs and owned output");
    n = view->meta.count;
    format = view->format ? view->format : NG_FORMAT_CI8;
    narrow = format == NG_FORMAT_CI8 || format == NG_FORMAT_CU8;
    if (!narrow && format != NG_FORMAT_CI16 && format != NG_FORMAT_SC16_Q11)
        return NgFail(error, "Unsupported DC subtraction sample format");
    limit = narrow ? 128 : format == NG_FORMAT_SC16_Q11 ? 2048 : 32768;
    if (dc[0] < -limit || dc[0] >= limit || dc[1] < -limit || dc[1] >= limit)
        return NgFail(error, "DC offset outside the input format range");
#if NG_SSE2
    {
        const __m128i offset = _mm_set_epi32(dc[1], dc[0], dc[1], dc[0]);
        for (; k + (narrow ? 8u : 4u) <= n; k += narrow ? 8u : 4u) {
            const unsigned char *p = (const unsigned char *)(const void *)view->samples;
            __m128i raw = _mm_loadu_si128((const __m128i *)(const void *)(p + k * (narrow ? 2u : 4u)));
            __m128i lo, hi;
            unsigned half;
            if (narrow) {
                __m128i sign;
                if (format == NG_FORMAT_CU8) raw = _mm_xor_si128(raw, _mm_set1_epi8((char)-128));
                sign = _mm_cmpgt_epi8(_mm_setzero_si128(), raw);
                lo = _mm_unpacklo_epi8(raw, sign); hi = _mm_unpackhi_epi8(raw, sign);
            } else {
                if (format == NG_FORMAT_SC16_Q11 &&
                    _mm_movemask_epi8(_mm_or_si128(_mm_cmpgt_epi16(raw, _mm_set1_epi16(2047)),
                                                   _mm_cmpgt_epi16(_mm_set1_epi16(-2048), raw))))
                    return NgFail(error, "SC16 Q11 level outside ADC range");
                lo = raw; hi = _mm_setzero_si128();
            }
            for (half = 0; half < (narrow ? 2u : 1u); ++half) {
                __m128i v = half ? hi : lo, sign = _mm_srai_epi16(v, 15);
                size_t base = 2 * k + half * 8u;
                _mm_storeu_si128((__m128i *)(void *)(output + base),
                    _mm_sub_epi32(_mm_unpacklo_epi16(v, sign), offset));
                _mm_storeu_si128((__m128i *)(void *)(output + base + 4),
                    _mm_sub_epi32(_mm_unpackhi_epi16(v, sign), offset));
            }
        }
    }
#elif NG_NEON
    {
        const int32_t offsets[4] = {dc[0], dc[1], dc[0], dc[1]};
        int32x4_t offset = vld1q_s32(offsets);
        for (; k + (narrow ? 8u : 4u) <= n; k += narrow ? 8u : 4u) {
            const unsigned char *p = (const unsigned char *)(const void *)view->samples;
            int16x8_t lo, hi;
            unsigned half;
            if (narrow) {
                int8x16_t raw = vld1q_s8((const int8_t *)(const void *)(p + 2 * k));
                if (format == NG_FORMAT_CU8) raw = veorq_s8(raw, vdupq_n_s8(-128));
                lo = vmovl_s8(vget_low_s8(raw)); hi = vmovl_s8(vget_high_s8(raw));
            } else {
                lo = vld1q_s16((const int16_t *)(const void *)(p + 4 * k));
                if (format == NG_FORMAT_SC16_Q11 &&
                    vmaxvq_u16(vorrq_u16(vcgtq_s16(lo, vdupq_n_s16(2047)),
                                        vcltq_s16(lo, vdupq_n_s16(-2048)))))
                    return NgFail(error, "SC16 Q11 level outside ADC range");
                hi = vdupq_n_s16(0);
            }
            for (half = 0; half < (narrow ? 2u : 1u); ++half) {
                int16x8_t v = half ? hi : lo;
                size_t base = 2 * k + half * 8u;
                vst1q_s32(output + base, vsubq_s32(vmovl_s16(vget_low_s16(v)), offset));
                vst1q_s32(output + base + 4, vsubq_s32(vmovl_s16(vget_high_s16(v)), offset));
            }
        }
    }
#endif
    for (; k < n; ++k) for (j = 0; j < 2; ++j) {
        int value = NgIqComponent(view, k, (unsigned)j);
        if (format == NG_FORMAT_SC16_Q11 && (value < -2048 || value > 2047))
            return NgFail(error, "SC16 Q11 level outside ADC range");
        output[2 * k + j] = value - dc[j];
    }
    return NG_OK;
}
