/** @file moments.c @brief Exact CI8 moments, scalar oracle and selected SIMD implementation. */
#include "ngene/dsp/statistics.h"
#include "dsp/sample_backend.h"

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
