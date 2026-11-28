#include "ci8_fft_kernels.h"

/* The compact path's products sum to at most 2 * 128 * 32768 in
 * magnitude. Use unsigned shifts to avoid shifting negative signed values. */
static inline int32_t RoundShift(int32_t value, unsigned shift)
{
    if (shift == 0)
        return value;
    uint32_t magnitude = (uint32_t)(value < 0 ? -value : value);
    int32_t rounded = (int32_t)((magnitude + (UINT32_C(1) << (shift - 1))) >> shift);
    return value < 0 ? -rounded : rounded;
}

static inline void CompactButterflyShift(const int8_t *values, const int64_t *wr, const int64_t *wi,
                                         size_t a, size_t b, size_t k, unsigned bits,
                                         int32_t out[4])
{
    int32_t c = (int32_t)wr[k], s = (int32_t)wi[k];
    int32_t r = RoundShift(values[b] * c - values[b + 1] * s, bits);
    int32_t i = RoundShift(values[b] * s + values[b + 1] * c, bits);
    out[0] = values[a] + r;
    out[1] = values[a + 1] + i;
    out[2] = values[a] - r;
    out[3] = values[a + 1] - i;
}

static unsigned CompactStageShift(const int8_t *samples, const int64_t *wr, const int64_t *wi,
                                  size_t n, size_t width, unsigned bits)
{
    unsigned shift = 0;
    size_t stride = n / width;
    for (size_t base = 0; base < n; base += width) {
        for (size_t j = 0; j < width / 2; ++j) {
            size_t a = 2 * (base + j);
            int32_t values[4];
            CompactButterflyShift(samples, wr, wi, a, a + width, j * stride, bits, values);
            for (size_t k = 0; k < 4; ++k) {
                /* Preimages of INT8 under round-to-nearest, ties away from zero.
                 * Two bits always suffice: each |output| is at most 384. */
                if (values[k] < -256 || values[k] > 254)
                    return 2;
                shift |= values[k] < -128 || values[k] > 127;
            }
        }
    }
    return shift;
}

CI8_FFT_API unsigned Ci8FftCompactAdaptiveStage(int8_t *samples, const int64_t *wr,
                                                const int64_t *wi, size_t n, size_t width,
                                                unsigned bits)
{
    unsigned shift = CompactStageShift(samples, wr, wi, n, width, bits);
    size_t stride = n / width;
    for (size_t base = 0; base < n; base += width) {
        for (size_t j = 0; j < width / 2; ++j) {
            size_t a = 2 * (base + j), b = a + width;
            int32_t values[4];
            CompactButterflyShift(samples, wr, wi, a, b, j * stride, bits, values);
            samples[a] = (int8_t)RoundShift(values[0], shift);
            samples[a + 1] = (int8_t)RoundShift(values[1], shift);
            samples[b] = (int8_t)RoundShift(values[2], shift);
            samples[b + 1] = (int8_t)RoundShift(values[3], shift);
        }
    }
    return shift;
}

static int8_t FixedScale(int32_t value, unsigned *clips)
{
    value = RoundShift(value, 1);
    *clips += value < -128 || value > 127;
    return (int8_t)(value < -128 ? -128 : value > 127 ? 127 : value);
}

CI8_FFT_API unsigned Ci8FftCompactFixedStage(int8_t *samples, const int64_t *wr, const int64_t *wi,
                                             size_t n, size_t width, unsigned bits)
{
    unsigned clips = 0;
    size_t stride = n / width;
    for (size_t base = 0; base < n; base += width) {
        for (size_t j = 0; j < width / 2; ++j) {
            size_t a = 2 * (base + j), b = a + width;
            int32_t values[4];
            CompactButterflyShift(samples, wr, wi, a, b, j * stride, bits, values);
            samples[a] = FixedScale(values[0], &clips);
            samples[a + 1] = FixedScale(values[1], &clips);
            samples[b] = FixedScale(values[2], &clips);
            samples[b + 1] = FixedScale(values[3], &clips);
        }
    }
    return clips;
}

static int64_t WideRoundDivide(int64_t value, int64_t divisor)
{
    return (value + (value < 0 ? -divisor / 2 : divisor / 2)) / divisor;
}

CI8_FFT_API void Ci8FftWideStage(int64_t *samples, const int64_t *wr, const int64_t *wi, size_t n,
                                 size_t width, unsigned bits)
{
    const int64_t unit = INT64_C(1) << bits;
    for (size_t base = 0; base < n; base += width) {
        for (size_t j = 0; j < width / 2; ++j) {
            size_t a = 2 * (base + j), b = a + width, k = j * (n / width);
            int64_t r = WideRoundDivide(samples[b] * wr[k] - samples[b + 1] * wi[k], unit);
            int64_t i = WideRoundDivide(samples[b] * wi[k] + samples[b + 1] * wr[k], unit);
            samples[b] = samples[a] - r;
            samples[b + 1] = samples[a + 1] - i;
            samples[a] += r;
            samples[a + 1] += i;
        }
    }
}
