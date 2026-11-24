#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

/* Match signed division towards zero. Only the low word is consumed. */
static __m256i TruncateQ30(__m256i value)
{
    const __m256i mask = _mm256_set1_epi64x(INT64_C(1073741823));
    const __m256i sign =
        _mm256_srai_epi32(_mm256_shuffle_epi32(value, _MM_SHUFFLE(3, 3, 1, 1)), 31);
    return _mm256_srli_epi64(_mm256_add_epi64(value, _mm256_and_si256(sign, mask)), 30);
}

/* Width is a power of two >= 8. Each iteration handles four butterfly pairs. */
void NgSearchFftInt32StageAvx2(int32_t *output, const int32_t *twiddles, size_t size,
                              size_t width, int inverse)
{
    const __m256i conjugate = _mm256_set1_epi32(-inverse);
    for (size_t base = 0; base < size; base += width) {
        for (size_t j = 0; j < width / 2; j += 4) {
            int32_t *a = output + 2 * (base + j);
            int32_t *b = a + width;
            const __m256i top = _mm256_loadu_si256((const __m256i *)(const void *)a);
            const __m256i bottom = _mm256_loadu_si256((const __m256i *)(const void *)b);
            const __m256i w =
                _mm256_loadu_si256((const __m256i *)(const void *)(twiddles + 2 * j));
            __m256i wi = _mm256_srli_epi64(w, 32);
            wi = _mm256_sub_epi32(_mm256_xor_si256(wi, conjugate), conjugate);
            const __m256i bi = _mm256_srli_epi64(bottom, 32);
            const __m256i real = TruncateQ30(
                _mm256_sub_epi64(_mm256_mul_epi32(bottom, w), _mm256_mul_epi32(bi, wi)));
            const __m256i imag = TruncateQ30(
                _mm256_add_epi64(_mm256_mul_epi32(bottom, wi), _mm256_mul_epi32(bi, w)));
            const __m256i product = _mm256_blend_epi32(real, _mm256_slli_epi64(imag, 32), 0xaa);
            _mm256_storeu_si256((__m256i *)(void *)a, _mm256_add_epi32(top, product));
            _mm256_storeu_si256((__m256i *)(void *)b, _mm256_sub_epi32(top, product));
        }
    }
}
