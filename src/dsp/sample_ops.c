/** @file sample_ops.c @brief SSE2/NEON intrinsics, exact integer reference and tails. */
#include "ngene/dsp/sample_ops.h"
#if !defined(NGENE_SAMPLE_REFERENCE) && (defined(__x86_64__) || defined(_M_X64))
#include <emmintrin.h>
#define NG_SSE2 1
#elif !defined(NGENE_SAMPLE_REFERENCE) && (defined(__aarch64__) || defined(_M_ARM64))
#if defined(_MSC_VER) && !defined(__clang__)
#include <arm64_neon.h>
#else
#include <arm_neon.h>
#endif
#define NG_NEON 1
#endif

/**
 * @brief Report the compiled sample primitive backend.
 *
 * @details
 * The returned static string may be used in diagnostics; no allocation or runtime ISA probing
 * occurs.
 *
 * Result: SSE2-intrinsics, NEON-intrinsics or C-reference.
 */
const char *NgSampleOpsBackend(void)
{
#if NG_SSE2
    return "SSE2-intrinsics";
#elif NG_NEON
    return "NEON-intrinsics";
#else
    return "C-reference";
#endif
}

/**
 * @brief Compute exact sums and sums of squares for signed I and Q.
 *
 * @details
 * Overwrites both output pairs; does not change samples. Count must be <=262144 to respect
 * vector lane bounds. Arrays may be naturally aligned with no extra SIMD alignment; scalar
 * tails cover remainders. Scalar oracle for verifying SIMD results.
 *
 * Parameter samples: Read-only CI8 complex pairs; valid for n pairs.
 * Parameter n: Complex-pair count in 0..262144.
 * Parameter sum: Receives int64 sums: index 0 I, index 1 Q.
 * Parameter squares: Receives uint64 sums of squares: index 0 I, index 1 Q.
 */
void NgIqMomentsReference(const TNgIq8 *samples, size_t n, int64_t sum[2], uint64_t squares[2])
{
    size_t k;
    sum[0] = sum[1] = 0;
    squares[0] = squares[1] = 0;
    for (k = 0; k < n; ++k) {
        int i = samples[k].i;
        int q = samples[k].q;
        sum[0] += i;
        sum[1] += q;
        squares[0] += (unsigned)(i * i);
        squares[1] += (unsigned)(q * q);
    }
}

/**
 * @brief Compute exact sums and sums of squares for signed I and Q.
 *
 * @details
 * Overwrites both output pairs; does not change samples. Count must be <=262144 to respect
 * vector lane bounds. Arrays may be naturally aligned with no extra SIMD alignment; scalar
 * tails cover remainders. Uses SSE2/NEON intrinsics where selected.
 *
 * Parameter samples: Read-only CI8 complex pairs; valid for n pairs.
 * Parameter n: Complex-pair count in 0..262144.
 * Parameter sum: Receives int64 sums: index 0 I, index 1 Q.
 * Parameter squares: Receives uint64 sums of squares: index 0 I, index 1 Q.
 */
void NgIqMoments(const TNgIq8 *samples, size_t n, int64_t sum[2], uint64_t squares[2])
{
    size_t k = 0;
    sum[0] = sum[1] = 0;
    squares[0] = squares[1] = 0;
#if NG_SSE2
    __m128i si = _mm_setzero_si128();
    __m128i sqv = si;
    __m128i pi = si;
    __m128i pq = si;
    const __m128i one = _mm_set1_epi16(1);
    for (; k + 8 <= n; k += 8) {
        __m128i raw = _mm_loadu_si128((const __m128i *)(const void *)(samples + k));
        __m128i iv = _mm_srai_epi16(_mm_slli_epi16(raw, 8), 8);
        __m128i qv = _mm_srai_epi16(raw, 8);
        si = _mm_add_epi32(si, _mm_madd_epi16(iv, one));
        sqv = _mm_add_epi32(sqv, _mm_madd_epi16(qv, one));
        pi = _mm_add_epi32(pi, _mm_madd_epi16(iv, iv));
        pq = _mm_add_epi32(pq, _mm_madd_epi16(qv, qv));
    }
    {
        int32_t a[4];
        int32_t b[4];
        uint32_t c[4];
        uint32_t d[4];
        unsigned j;
        _mm_storeu_si128((__m128i *)(void *)a, si);
        _mm_storeu_si128((__m128i *)(void *)b, sqv);
        _mm_storeu_si128((__m128i *)(void *)c, pi);
        _mm_storeu_si128((__m128i *)(void *)d, pq);
        for (j = 0; j < 4; ++j) {
            sum[0] += a[j];
            sum[1] += b[j];
            squares[0] += c[j];
            squares[1] += d[j];
        }
    }
#elif NG_NEON
    int32x4_t si = vdupq_n_s32(0);
    int32x4_t sqv = si;
    uint32x4_t pi = vdupq_n_u32(0);
    uint32x4_t pq = pi;
    for (; k + 8 <= n; k += 8) {
        int8x8x2_t v = vld2_s8((const int8_t *)(const void *)(samples + k));
        int16x8_t i = vmovl_s8(v.val[0]);
        int16x8_t q = vmovl_s8(v.val[1]);
        si = vpadalq_s16(si, i);
        sqv = vpadalq_s16(sqv, q);
        pi = vpadalq_u16(pi, vreinterpretq_u16_s16(vmulq_s16(i, i)));
        pq = vpadalq_u16(pq, vreinterpretq_u16_s16(vmulq_s16(q, q)));
    }
    sum[0] = vaddlvq_s32(si);
    sum[1] = vaddlvq_s32(sqv);
    squares[0] = vaddlvq_u32(pi);
    squares[1] = vaddlvq_u32(pq);
#endif
    for (; k < n; ++k) {
        int i = samples[k].i;
        int q = samples[k].q;
        sum[0] += i;
        sum[1] += q;
        squares[0] += (unsigned)(i * i);
        squares[1] += (unsigned)(q * q);
    }
}

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
