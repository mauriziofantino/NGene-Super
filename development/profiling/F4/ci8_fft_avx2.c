#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

static __m256i RoundQ15(__m256i value)
{
    __m256i sign = _mm256_cmpgt_epi64(_mm256_setzero_si256(), value);
    __m256i magnitude = _mm256_sub_epi64(_mm256_xor_si256(value, sign), sign);
    magnitude = _mm256_srli_epi64(_mm256_add_epi64(magnitude, _mm256_set1_epi64x(16384)), 15);
    return _mm256_sub_epi64(_mm256_xor_si256(magnitude, sign), sign);
}

/* The wide CI8 bound keeps both operands within signed 32 bits. */
void Ci8WideQ15StageAvx2(int64_t *values, const int64_t *wr, const int64_t *wi,
                        size_t n, size_t width)
{
    for (size_t base = 0; base < n; base += width) {
        for (size_t j = 0; j < width / 2; j += 2) {
            size_t stride = n / width;
            size_t k = j * stride;
            int64_t *a = values + 2 * (base + j);
            int64_t *b = a + width;
            __m256i top = _mm256_loadu_si256((const __m256i *)(const void *)a);
            __m256i bottom = _mm256_loadu_si256((const __m256i *)(const void *)b);
            __m256i swapped = _mm256_permute4x64_epi64(bottom, _MM_SHUFFLE(2, 3, 0, 1));
            __m256i real = _mm256_setr_epi64x(wr[k], wr[k], wr[k + stride], wr[k + stride]);
            __m256i imag = _mm256_setr_epi64x(-wi[k], wi[k], -wi[k + stride], wi[k + stride]);
            __m256i product = RoundQ15(_mm256_add_epi64(
                _mm256_mul_epi32(bottom, real), _mm256_mul_epi32(swapped, imag)));
            _mm256_storeu_si256((__m256i *)(void *)a, _mm256_add_epi64(top, product));
            _mm256_storeu_si256((__m256i *)(void *)b, _mm256_sub_epi64(top, product));
        }
    }
}
