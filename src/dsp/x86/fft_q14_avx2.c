#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

/* Each block has a power-of-two width of at least 16 complex samples. */
void NgFftQ14StageAvx2(int16_t *data, const int16_t *twiddles, size_t n, size_t width)
{
    for (size_t base = 0; base < n; base += width) {
        int16_t *u = data + 2 * base;
        int16_t *v = u + width;
        for (size_t j = 0; j < width / 2; j += 8) {
            const __m256i top = _mm256_loadu_si256((const __m256i *)(const void *)(u + 2 * j));
            const __m256i bot = _mm256_loadu_si256((const __m256i *)(const void *)(v + 2 * j));
            const __m256i w0 =
                _mm256_loadu_si256((const __m256i *)(const void *)(twiddles + 4 * j));
            const __m256i w1 =
                _mm256_loadu_si256((const __m256i *)(const void *)(twiddles + 4 * j + 16));
            const __m256i wrLow = _mm256_permute2x128_si256(w0, w1, 0x20);
            const __m256i wrHigh = _mm256_permute2x128_si256(w0, w1, 0x31);
            const __m256i prodLow =
                _mm256_srai_epi32(_mm256_madd_epi16(_mm256_unpacklo_epi32(bot, bot), wrLow), 14);
            const __m256i prodHigh =
                _mm256_srai_epi32(_mm256_madd_epi16(_mm256_unpackhi_epi32(bot, bot), wrHigh), 14);
            const __m256i signs = _mm256_srai_epi16(top, 15);
            const __m256i low = _mm256_unpacklo_epi16(top, signs);
            const __m256i high = _mm256_unpackhi_epi16(top, signs);
            const __m256i sum = _mm256_packs_epi32(
                _mm256_srai_epi32(_mm256_add_epi32(low, prodLow), 1),
                _mm256_srai_epi32(_mm256_add_epi32(high, prodHigh), 1));
            const __m256i diff = _mm256_packs_epi32(
                _mm256_srai_epi32(_mm256_sub_epi32(low, prodLow), 1),
                _mm256_srai_epi32(_mm256_sub_epi32(high, prodHigh), 1));
            _mm256_storeu_si256((__m256i *)(void *)(u + 2 * j), sum);
            _mm256_storeu_si256((__m256i *)(void *)(v + 2 * j), diff);
        }
    }
}
