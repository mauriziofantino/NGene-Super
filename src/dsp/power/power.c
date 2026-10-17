/** @file power.c @brief Exact complex integer power, scalar oracle and SIMD implementation. */
#include "ngene/dsp/power.h"
#include "dsp/sample_backend.h"

/**
 * @brief Add exact complex integer power to uint64 accumulators.
 *
 * @details
 * For each pair adds I^2+Q^2 to the existing output. Handles both components -32768 without
 * signed32 overflow. The caller must prevent uint64 accumulator overflow and provide disjoint
 * valid arrays. Scalar oracle for verifying SIMD results.
 *
 * Parameter iq: Read-only interleaved signed int16 components, 2*n elements.
 * Parameter power: Read/write uint64 accumulators, n elements; initialize before first
 * accumulation. Parameter n: Number of complex pairs.
 */
void NgPowerAccumulateReference(const int16_t *iq, uint64_t *power, size_t n)
{
    size_t k;
    for (k = 0; k < n; ++k) {
        int64_t i = iq[2 * k];
        int64_t q = iq[2 * k + 1];
        power[k] += (uint64_t)(i * i + q * q);
    }
}

/**
 * @brief Add exact complex integer power to uint64 accumulators.
 *
 * @details
 * For each pair adds I^2+Q^2 to the existing output. Handles both components -32768 without
 * signed32 overflow. The caller must prevent uint64 accumulator overflow and provide disjoint
 * valid arrays. SIMD operations use unsigned widening and scalar tails.
 *
 * Parameter iq: Read-only interleaved signed int16 components, 2*n elements.
 * Parameter power: Read/write uint64 accumulators, n elements; initialize before first
 * accumulation. Parameter n: Number of complex pairs.
 */
void NgPowerAccumulate(const int16_t *iq, uint64_t *power, size_t n)
{
    size_t k = 0;
#if NG_SSE2
    for (; k + 4 <= n; k += 4) {
        __m128i v = _mm_loadu_si128((const __m128i *)(const void *)(iq + 2 * k));
        /* pmaddwd's 0x80000000 corner represents exactly 2^31 as uint32. */
        __m128i vectorPower = _mm_madd_epi16(v, v);
        __m128i zero = _mm_setzero_si128();
        __m128i lo = _mm_unpacklo_epi32(vectorPower, zero);
        __m128i hi = _mm_unpackhi_epi32(vectorPower, zero);
        _mm_storeu_si128(
            (__m128i *)(void *)(power + k),
            _mm_add_epi64(lo, _mm_loadu_si128((const __m128i *)(const void *)(power + k))));
        _mm_storeu_si128(
            (__m128i *)(void *)(power + k + 2),
            _mm_add_epi64(hi, _mm_loadu_si128((const __m128i *)(const void *)(power + k + 2))));
    }
#elif NG_NEON
    for (; k + 4 <= n; k += 4) {
        int16x8_t v = vld1q_s16(iq + 2 * k);
        uint32x4_t lo = vreinterpretq_u32_s32(vmull_s16(vget_low_s16(v), vget_low_s16(v)));
        uint32x4_t hi = vreinterpretq_u32_s32(vmull_s16(vget_high_s16(v), vget_high_s16(v)));
        vst1q_u64(power + k, vaddq_u64(vld1q_u64(power + k), vpaddlq_u32(lo)));
        vst1q_u64(power + k + 2, vaddq_u64(vld1q_u64(power + k + 2), vpaddlq_u32(hi)));
    }
#endif
    NgPowerAccumulateReference(iq + 2 * k, power + k, n - k);
}
