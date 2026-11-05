#include "frontend/sige.h"
#include "dsp/sample_backend.h"

int NgSigeDecode(TNgSigeDecoder *s, const unsigned char *raw, size_t bytes, TNgIq8 *output,
                 size_t capacity, size_t *pairs)
{
    size_t k = 0, n = 0;
    if (!s || !raw || !output || !pairs) {
        return NG_ERROR;
    }
    if (!s->synchronized && bytes) {
        if (!(raw[0] & 2u)) {
            ++k;
            s->gap = 1;
        }
        s->synchronized = 1;
    }
    if (s->pending && k < bytes) {
        if ((raw[k] & 2u) || n == capacity) {
            return NG_ERROR;
        }
        output[n].i = (int8_t)(1 - 2 * (s->inPhase & 1));
        output[n++].q = (int8_t)(1 - 2 * (raw[k++] & 1));
        s->pending = 0;
    }
#if NG_SSE2
    for (; k + 16u <= bytes && n + 8u <= capacity; k += 16u, n += 8u) {
        __m128i v = _mm_loadu_si128((const __m128i *)(raw + k));
        __m128i markers = _mm_and_si128(v, _mm_set1_epi8(2));
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(markers, _mm_set1_epi16(2))) != 0xffff) {
            return NG_ERROR;
        }
        v = _mm_sub_epi8(_mm_set1_epi8(1), _mm_slli_epi16(_mm_and_si128(v, _mm_set1_epi8(1)), 1));
        _mm_storeu_si128((__m128i *)(output + n), v);
    }
#elif NG_NEON
    for (; k + 16u <= bytes && n + 8u <= capacity; k += 16u, n += 8u) {
        uint8x16_t v = vld1q_u8(raw + k);
        uint8_t markers[16];
        unsigned j;
        vst1q_u8(markers, vandq_u8(v, vdupq_n_u8(2)));
        for (j = 0; j < 16; j++) {
            if (markers[j] != (j % 2 ? 0 : 2)) {
                return NG_ERROR;
            }
        }
        vst1q_s8((int8_t *)(output + n),
                 vreinterpretq_s8_u8(
                     vsubq_u8(vdupq_n_u8(1), vshlq_n_u8(vandq_u8(v, vdupq_n_u8(1)), 1))));
    }
#endif
    while (k < bytes) {
        if (!s->pending) {
            if (!(raw[k] & 2u)) {
                return NG_ERROR;
            }
            s->inPhase = raw[k++];
            s->pending = 1;
        } else {
            if ((raw[k] & 2u) || n == capacity) {
                return NG_ERROR;
            }
            output[n].i = (int8_t)(1 - 2 * (s->inPhase & 1));
            output[n++].q = (int8_t)(1 - 2 * (raw[k++] & 1));
            s->pending = 0;
        }
    }
    *pairs = n;
    return NG_OK;
}
