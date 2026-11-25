#include "ci8_fft_kernels.h"

#include <immintrin.h>

typedef struct {
    __m256i real, imag;
} Complex8;

static inline Complex8 Load8(const int8_t *p)
{
    __m256i iq = _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i *)(const void *)p));
    Complex8 v = {_mm256_srai_epi32(_mm256_slli_epi32(iq, 24), 24),
                  _mm256_srai_epi32(_mm256_slli_epi32(iq, 16), 24)};
    return v;
}

static inline __m256i RoundShift8(__m256i value, unsigned shift)
{
    if (!shift)
        return value;
    __m256i bias = _mm256_set1_epi32(1 << (shift - 1));
    __m256i magnitude = _mm256_add_epi32(_mm256_abs_epi32(value), bias);
    __m256i rounded = _mm256_srl_epi32(magnitude, _mm_cvtsi32_si128((int)shift));
    return _mm256_sign_epi32(rounded, value);
}

static inline void Store8(int8_t *p, Complex8 v, unsigned shift)
{
    __m256i real = _mm256_and_si256(RoundShift8(v.real, shift), _mm256_set1_epi32(255));
    __m256i imag = _mm256_slli_epi32(RoundShift8(v.imag, shift), 8);
    __m256i iq = _mm256_or_si256(real, _mm256_and_si256(imag, _mm256_set1_epi32(0xff00)));
    __m128i packed = _mm_packus_epi32(_mm256_castsi256_si128(iq), _mm256_extracti128_si256(iq, 1));
    _mm_storeu_si128((__m128i *)(void *)p, packed);
}

static inline __m256i Coefficients(const int64_t *p, size_t stride)
{
    return _mm256_setr_epi32((int)p[0], (int)p[stride], (int)p[2 * stride], (int)p[3 * stride],
                             (int)p[4 * stride], (int)p[5 * stride], (int)p[6 * stride],
                             (int)p[7 * stride]);
}

static inline void Butterfly(const int8_t *a, const int8_t *b, __m256i c, __m256i s,
                             unsigned bits, Complex8 *top, Complex8 *bottom)
{
    Complex8 x = Load8(a), y = Load8(b);
    __m256i r = _mm256_sub_epi32(_mm256_mullo_epi32(y.real, c), _mm256_mullo_epi32(y.imag, s));
    __m256i i = _mm256_add_epi32(_mm256_mullo_epi32(y.real, s), _mm256_mullo_epi32(y.imag, c));
    r = RoundShift8(r, bits);
    i = RoundShift8(i, bits);
    top->real = _mm256_add_epi32(x.real, r);
    top->imag = _mm256_add_epi32(x.imag, i);
    bottom->real = _mm256_sub_epi32(x.real, r);
    bottom->imag = _mm256_sub_epi32(x.imag, i);
}

static unsigned FindShift(const int8_t *values, const int64_t *wr, const int64_t *wi,
                          size_t n, size_t width, unsigned bits)
{
    __m256i low = _mm256_setzero_si256(), high = low;
    size_t stride = n / width;
    for (size_t j = 0; j < width / 2; j += 8) {
        __m256i c = Coefficients(wr + j * stride, stride);
        __m256i s = Coefficients(wi + j * stride, stride);
        for (size_t base = 0; base < n; base += width) {
            const int8_t *a = values + 2 * (base + j);
            Complex8 top, bottom;
            Butterfly(a, a + width, c, s, bits, &top, &bottom);
            __m256i lowPair = _mm256_min_epi32(_mm256_min_epi32(top.real, bottom.real),
                                               _mm256_min_epi32(top.imag, bottom.imag));
            __m256i highPair = _mm256_max_epi32(_mm256_max_epi32(top.real, bottom.real),
                                                _mm256_max_epi32(top.imag, bottom.imag));
            __m256i outside = _mm256_or_si256(_mm256_cmpgt_epi32(_mm256_set1_epi32(-256), lowPair),
                                               _mm256_cmpgt_epi32(highPair, _mm256_set1_epi32(254)));
            if (_mm256_movemask_epi8(outside))
                return 2;
            low = _mm256_min_epi32(low, lowPair);
            high = _mm256_max_epi32(high, highPair);
        }
    }
    __m256i outside = _mm256_or_si256(_mm256_cmpgt_epi32(_mm256_set1_epi32(-128), low),
                                       _mm256_cmpgt_epi32(high, _mm256_set1_epi32(127)));
    return _mm256_movemask_epi8(outside) != 0;
}

unsigned Ci8FftCompactAdaptiveStageAvx2(int8_t *values, const int64_t *wr, const int64_t *wi,
                                        size_t n, size_t width, unsigned bits)
{
    unsigned shift = FindShift(values, wr, wi, n, width, bits);
    size_t stride = n / width;
    for (size_t j = 0; j < width / 2; j += 8) {
        __m256i c = Coefficients(wr + j * stride, stride);
        __m256i s = Coefficients(wi + j * stride, stride);
        for (size_t base = 0; base < n; base += width) {
            int8_t *a = values + 2 * (base + j);
            int8_t *b = a + width;
            Complex8 top, bottom;
            Butterfly(a, b, c, s, bits, &top, &bottom);
            Store8(a, top, shift);
            Store8(b, bottom, shift);
        }
    }
    return shift;
}


static inline __m256i PackPair16(__m256i real, __m256i imag)
{
    __m256i lanes = _mm256_packs_epi32(real, imag);
    return _mm256_unpacklo_epi16(lanes, _mm256_srli_si256(lanes, 8));
}

static inline void StoreAndReduce(int16_t *scratch, size_t a, size_t b, Complex8 top,
                                  Complex8 bottom, __m256i *low, __m256i *high)
{
    _mm256_storeu_si256((__m256i *)(void *)(scratch + a), PackPair16(top.real, top.imag));
    _mm256_storeu_si256((__m256i *)(void *)(scratch + b), PackPair16(bottom.real, bottom.imag));
    *low = _mm256_min_epi32(*low, _mm256_min_epi32(_mm256_min_epi32(top.real, bottom.real),
                                                  _mm256_min_epi32(top.imag, bottom.imag)));
    *high = _mm256_max_epi32(*high, _mm256_max_epi32(_mm256_max_epi32(top.real, bottom.real),
                                                     _mm256_max_epi32(top.imag, bottom.imag)));
}

static inline unsigned RequiredShift(__m256i low, __m256i high)
{
    __m256i needTwo = _mm256_or_si256(_mm256_cmpgt_epi32(_mm256_set1_epi32(-256), low),
                                       _mm256_cmpgt_epi32(high, _mm256_set1_epi32(254)));
    if (_mm256_movemask_epi8(needTwo))
        return 2;
    __m256i needOne = _mm256_or_si256(_mm256_cmpgt_epi32(_mm256_set1_epi32(-128), low),
                                       _mm256_cmpgt_epi32(high, _mm256_set1_epi32(127)));
    return _mm256_movemask_epi8(needOne) != 0;
}

unsigned Ci8FftCompactBufferColumnsAvx2(const int8_t *values, int16_t *scratch,
                                        const int64_t *wr, const int64_t *wi,
                                        size_t n, size_t width, unsigned bits,
                                        size_t first, size_t end)
{
    __m256i low = _mm256_setzero_si256(), high = low;
    size_t stride = n / width;
    for (size_t j = first; j < end; j += 8) {
        __m256i c = Coefficients(wr + j * stride, stride);
        __m256i s = Coefficients(wi + j * stride, stride);
        for (size_t base = 0; base < n; base += width) {
            size_t a = 2 * (base + j), b = a + width;
            Complex8 top, bottom;
            Butterfly(values + a, values + b, c, s, bits, &top, &bottom);
            StoreAndReduce(scratch, a, b, top, bottom, &low, &high);
        }
    }
    return RequiredShift(low, high);
}

void Ci8FftCompactPackBufferedAvx2(int8_t *values, const int16_t *scratch, size_t n, unsigned shift)
{
    const __m256i bias = _mm256_set1_epi16((short)(shift == 2 ? 2 : 1));
    const __m128i count = _mm_cvtsi32_si128((int)shift);
    for (size_t k = 0; k < 2 * n; k += 16) {
        __m256i iq = _mm256_loadu_si256((const __m256i *)(const void *)(scratch + k));
        if (shift) {
            __m256i magnitude = _mm256_add_epi16(_mm256_abs_epi16(iq), bias);
            iq = _mm256_sign_epi16(_mm256_srl_epi16(magnitude, count), iq);
        }
        __m128i packed = _mm_packs_epi16(_mm256_castsi256_si128(iq), _mm256_extracti128_si256(iq, 1));
        _mm_storeu_si128((__m128i *)(void *)(values + k), packed);
    }
}

unsigned Ci8FftCompactAdaptiveBufferedAvx2(int8_t *values, int16_t *scratch,
                                           const int64_t *wr, const int64_t *wi,
                                           size_t n, size_t width, unsigned bits)
{
    unsigned shift = Ci8FftCompactBufferColumnsAvx2(values, scratch, wr, wi, n, width, bits, 0, width / 2);
    Ci8FftCompactPackBufferedAvx2(values, scratch, n, shift);
    return shift;
}

typedef struct {
    __m256i real, imag;
    __m256i cEndpoint, sPositiveEndpoint, sNegativeEndpoint;
    int correct;
} MaddTwiddles;

static inline MaddTwiddles MakeMaddTwiddles(__m256i c, __m256i s)
{
    const __m256i positive = _mm256_set1_epi32(32768);
    const __m256i negative = _mm256_set1_epi32(-32768);
    MaddTwiddles twiddles;
    twiddles.real = PackPair16(c, _mm256_sub_epi32(_mm256_setzero_si256(), s));
    twiddles.imag = PackPair16(s, c);
    twiddles.cEndpoint = _mm256_cmpeq_epi32(c, positive);
    twiddles.sPositiveEndpoint = _mm256_cmpeq_epi32(s, positive);
    twiddles.sNegativeEndpoint = _mm256_cmpeq_epi32(s, negative);
    twiddles.correct = _mm256_movemask_epi8(_mm256_or_si256(
        twiddles.cEndpoint, _mm256_or_si256(twiddles.sPositiveEndpoint, twiddles.sNegativeEndpoint)));
    return twiddles;
}

static inline void ButterflyMadd(const int8_t *a, const int8_t *b, const MaddTwiddles *w,
                                 unsigned bits, Complex8 *top, Complex8 *bottom)
{
    Complex8 x = Load8(a);
    __m256i y = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i *)(const void *)b));
    __m256i r = _mm256_madd_epi16(y, w->real);
    __m256i i = _mm256_madd_epi16(y, w->imag);
    if (w->correct) {
        /* Signed packing represents +32768 as +32767. Add back the
         * corresponding sample before rounding. -32768 fits exactly. */
        __m256i yr = _mm256_srai_epi32(_mm256_slli_epi32(y, 16), 16);
        __m256i yi = _mm256_srai_epi32(y, 16);
        r = _mm256_add_epi32(r, _mm256_add_epi32(_mm256_and_si256(yr, w->cEndpoint),
                                                _mm256_and_si256(yi, w->sNegativeEndpoint)));
        i = _mm256_add_epi32(i, _mm256_add_epi32(_mm256_and_si256(yr, w->sPositiveEndpoint),
                                                _mm256_and_si256(yi, w->cEndpoint)));
    }
    r = RoundShift8(r, bits);
    i = RoundShift8(i, bits);
    top->real = _mm256_add_epi32(x.real, r);
    top->imag = _mm256_add_epi32(x.imag, i);
    bottom->real = _mm256_sub_epi32(x.real, r);
    bottom->imag = _mm256_sub_epi32(x.imag, i);
}

unsigned Ci8FftCompactAdaptiveMaddAvx2(int8_t *values, int16_t *scratch,
                                       const int64_t *wr, const int64_t *wi,
                                       size_t n, size_t width, unsigned bits)
{
    __m256i low = _mm256_setzero_si256(), high = low;
    size_t stride = n / width;
    for (size_t j = 0; j < width / 2; j += 8) {
        __m256i c = Coefficients(wr + j * stride, stride);
        __m256i s = Coefficients(wi + j * stride, stride);
        MaddTwiddles w = MakeMaddTwiddles(c, s);
        for (size_t base = 0; base < n; base += width) {
            size_t a = 2 * (base + j), b = a + width;
            Complex8 top, bottom;
            ButterflyMadd(values + a, values + b, &w, bits, &top, &bottom);
            StoreAndReduce(scratch, a, b, top, bottom, &low, &high);
        }
    }
    unsigned shift = RequiredShift(low, high);
    Ci8FftCompactPackBufferedAvx2(values, scratch, n, shift);
    return shift;
}
