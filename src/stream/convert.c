/** Common signed Q7/Q15 stream conversion. SIMD handles whole vectors; scalar tails
 * define rounding (nearest, ties away from zero) and little-endian storage. */
#include "ngene/types.h"
#include "dsp/sample_backend.h"
#include <string.h>

int NgIqConvert(void *output, TNgSampleFormat target, const TNgIqView *input)
{
    size_t k = 0, n;
    TNgSampleFormat source;
    unsigned shift;
    if (!output || !input || !input->samples || input->meta.count > SIZE_MAX / 4u ||
        NgSampleBytes(input->format) == 0 ||
        (target != NG_FORMAT_CI8 && target != NG_FORMAT_CI16)) {
        return NG_ERROR;
    }
    source = input->format == 0 ? NG_FORMAT_CI8 : input->format;
    n = input->meta.count * 2u;
    if (source == target) {
        memcpy(output, input->samples, input->meta.count * NgSampleBytes(target));
        return NG_OK;
    }
    shift = source == NG_FORMAT_SC16_Q11 ? 4u : 8u;
#if NG_SSE2
    for (; k + 16u <= n; k += 16u) {
        const unsigned char *bytes = (const unsigned char *)input->samples;
        __m128i a, b, zero = _mm_setzero_si128();
        if (NgSampleBytes(source) == 2u) {
            a = _mm_loadu_si128((const __m128i *)(bytes + k));
            if (source == NG_FORMAT_CU8) {
                a = _mm_xor_si128(a, _mm_set1_epi8((char)-128));
            }
            if (target == NG_FORMAT_CI8) {
                _mm_storeu_si128((__m128i *)((unsigned char *)output + k), a);
            } else {
                __m128i sign = _mm_cmpgt_epi8(zero, a);
                _mm_storeu_si128((__m128i *)((unsigned char *)output + 2u * k),
                                 _mm_slli_epi16(_mm_unpacklo_epi8(a, sign), 8));
                _mm_storeu_si128((__m128i *)((unsigned char *)output + 2u * k + 16u),
                                 _mm_slli_epi16(_mm_unpackhi_epi8(a, sign), 8));
            }
        } else {
            a = _mm_loadu_si128((const __m128i *)(bytes + 2u * k));
            b = _mm_loadu_si128((const __m128i *)(bytes + 2u * k + 16u));
            if (source == NG_FORMAT_SC16_Q11) {
                __m128i lo = _mm_set1_epi16(-2048), hi = _mm_set1_epi16(2047);
                __m128i invalid =
                    _mm_or_si128(_mm_or_si128(_mm_cmpgt_epi16(lo, a), _mm_cmpgt_epi16(a, hi)),
                                 _mm_or_si128(_mm_cmpgt_epi16(lo, b), _mm_cmpgt_epi16(b, hi)));
                if (_mm_movemask_epi8(invalid)) {
                    return NG_ERROR;
                }
            }
            if (target == NG_FORMAT_CI16) {
                a = _mm_slli_epi16(a, 4);
                b = _mm_slli_epi16(b, 4);
                _mm_storeu_si128((__m128i *)((unsigned char *)output + 2u * k), a);
                _mm_storeu_si128((__m128i *)((unsigned char *)output + 2u * k + 16u), b);
            } else {
                __m128i half = _mm_set1_epi16((short)(1u << (shift - 1u)));
                a = _mm_adds_epi16(a, _mm_add_epi16(half, _mm_srai_epi16(a, 15)));
                b = _mm_adds_epi16(b, _mm_add_epi16(half, _mm_srai_epi16(b, 15)));
                if (shift == 4u) {
                    a = _mm_srai_epi16(a, 4);
                    b = _mm_srai_epi16(b, 4);
                } else {
                    a = _mm_srai_epi16(a, 8);
                    b = _mm_srai_epi16(b, 8);
                }
                _mm_storeu_si128((__m128i *)((unsigned char *)output + k), _mm_packs_epi16(a, b));
            }
        }
    }
#elif NG_NEON && (!defined(__BYTE_ORDER__) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    for (; k + 16u <= n; k += 16u) {
        const unsigned char *bytes = (const unsigned char *)input->samples;
        int16x8_t a, b;
        if (NgSampleBytes(source) == 2u) {
            uint8x16_t raw = vld1q_u8(bytes + k);
            if (source == NG_FORMAT_CU8) {
                raw = veorq_u8(raw, vdupq_n_u8(128));
            }
            if (target == NG_FORMAT_CI8) {
                vst1q_u8((uint8_t *)output + k, raw);
            } else {
                int8x16_t v = vreinterpretq_s8_u8(raw);
                vst1q_s16((int16_t *)output + k, vshlq_n_s16(vmovl_s8(vget_low_s8(v)), 8));
                vst1q_s16((int16_t *)output + k + 8u, vshlq_n_s16(vmovl_s8(vget_high_s8(v)), 8));
            }
        } else {
            a = vld1q_s16((const int16_t *)(const void *)(bytes + 2u * k));
            b = vld1q_s16((const int16_t *)(const void *)(bytes + 2u * k + 16u));
            if (source == NG_FORMAT_SC16_Q11) {
                uint16x8_t invalid = vorrq_u16(
                    vorrq_u16(vcltq_s16(a, vdupq_n_s16(-2048)), vcgtq_s16(a, vdupq_n_s16(2047))),
                    vorrq_u16(vcltq_s16(b, vdupq_n_s16(-2048)), vcgtq_s16(b, vdupq_n_s16(2047))));
                uint64x2_t words = vreinterpretq_u64_u16(invalid);
                if (vgetq_lane_u64(words, 0) | vgetq_lane_u64(words, 1)) {
                    return NG_ERROR;
                }
            }
            if (target == NG_FORMAT_CI16) {
                vst1q_s16((int16_t *)output + k, vshlq_n_s16(a, 4));
                vst1q_s16((int16_t *)output + k + 8u, vshlq_n_s16(b, 4));
            } else {
                int16x8_t half = vdupq_n_s16((int16_t)(1u << (shift - 1u)));
                a = vqaddq_s16(a, vaddq_s16(half, vshrq_n_s16(a, 15)));
                b = vqaddq_s16(b, vaddq_s16(half, vshrq_n_s16(b, 15)));
                if (shift == 4u) {
                    a = vshrq_n_s16(a, 4);
                    b = vshrq_n_s16(b, 4);
                } else {
                    a = vshrq_n_s16(a, 8);
                    b = vshrq_n_s16(b, 8);
                }
                vst1q_s8((int8_t *)output + k, vcombine_s8(vqmovn_s16(a), vqmovn_s16(b)));
            }
        }
    }
#endif
    for (; k < n; ++k) {
        int x = NgIqComponent(input, k / 2u, (unsigned)(k & 1u));
        if (source == NG_FORMAT_SC16_Q11 && (x < -2048 || x > 2047)) {
            return NG_ERROR;
        }
        if (target == NG_FORMAT_CI16) {
            x *= source == NG_FORMAT_SC16_Q11 ? 16 : 256;
        } else if (NgSampleBytes(source) == 4u) {
            int half = (int)(1u << (shift - 1u));
            x = x >= 0 ? (x + half) / (int)(1u << shift) : -((-x + half) / (int)(1u << shift));
            if (x > 127) {
                x = 127;
            }
            if (x < -128) {
                x = -128;
            }
        }
        NgIqStore(output, target, k / 2u, (unsigned)(k & 1u), x);
    }
    return NG_OK;
}
