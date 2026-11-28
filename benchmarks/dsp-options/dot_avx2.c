#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

static __m256i Accumulate(__m256i sum, __m256i pairs)
{
    __m256i low = _mm256_cvtepi32_epi64(_mm256_castsi256_si128(pairs));
    __m256i high = _mm256_cvtepi32_epi64(_mm256_extracti128_si256(pairs, 1));
    return _mm256_add_epi64(sum, _mm256_add_epi64(low, high));
}

static int64_t Reduce(__m256i sum)
{
    int64_t lanes[4];
    _mm256_storeu_si256((__m256i *)(void *)lanes, sum);
    return lanes[0] + lanes[1] + lanes[2] + lanes[3];
}

/* Q15 weights are limited to [-32767, 32767], so neither pair sum wraps. */
int64_t DspOptionsDot16Avx2(const int16_t *x, const int16_t *w, size_t n)
{
    __m256i acc = _mm256_setzero_si256();
    size_t k = 0;
    for (; n - k >= 16; k += 16) {
        __m256i samples = _mm256_loadu_si256((const __m256i *)(const void *)(x + k));
        __m256i weights = _mm256_loadu_si256((const __m256i *)(const void *)(w + k));
        acc = Accumulate(acc, _mm256_madd_epi16(samples, weights));
    }
    int64_t sum = Reduce(acc);
    for (; k < n; ++k) {
        sum += (int32_t)x[k] * w[k];
    }
    return sum;
}

int64_t DspOptionsDot8Avx2(const int8_t *x, const int16_t *w, size_t n)
{
    __m256i acc = _mm256_setzero_si256();
    size_t k = 0;
    for (; n - k >= 16; k += 16) {
        __m128i bytes = _mm_loadu_si128((const __m128i *)(const void *)(x + k));
        __m256i weights = _mm256_loadu_si256((const __m256i *)(const void *)(w + k));
        acc = Accumulate(acc, _mm256_madd_epi16(_mm256_cvtepi8_epi16(bytes), weights));
    }
    int64_t sum = Reduce(acc);
    for (; k < n; ++k) {
        sum += (int32_t)x[k] * w[k];
    }
    return sum;
}
