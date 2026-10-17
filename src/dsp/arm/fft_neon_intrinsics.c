/** @file fft_neon_intrinsics.c @brief Bit-exact ARMv7/AArch64 integer NEON FFT kernels. */
#include "dsp/fft/butterfly.h"
#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM64)
#include <arm64_neon.h>
#else
#include <arm_neon.h>
#endif

/** @brief Rotate only the second complex pair by -j without narrowing an intermediate. */
static inline int16x4_t RotateLastPair(int16x4_t value)
{
    const uint16x4_t rotateMask = vcreate_u16(UINT64_C(0xffffffff00000000));
    const int16x4_t negateMask = vreinterpret_s16_u16(vcreate_u16(UINT64_C(0xffff000000000000)));
    int16x4_t rotated = vbsl_s16(rotateMask, vrev32_s16(value), value);
    return vsub_s16(veor_s16(rotated, negateMask), negateMask);
}

/** @brief Fuse width-two/four stages, keeping the independent floor at each butterfly. */
static inline int16x8_t FirstTwoStages(int16x8_t data)
{
    int32x2x2_t pairs = vuzp_s32(vreinterpret_s32_s16(vget_low_s16(data)),
                                 vreinterpret_s32_s16(vget_high_s16(data)));
    int16x4_t upper = vreinterpret_s16_s32(pairs.val[0]);
    int16x4_t lower = vreinterpret_s16_s32(pairs.val[1]);
    int16x4_t sum = vhadd_s16(upper, lower);
    int16x4_t difference = vhsub_s16(upper, lower);
    int32x2x2_t halves = vzip_s32(vreinterpret_s32_s16(sum), vreinterpret_s32_s16(difference));
    upper = vreinterpret_s16_s32(halves.val[0]);
    lower = RotateLastPair(vreinterpret_s16_s32(halves.val[1]));
    return vcombine_s16(vhadd_s16(upper, lower), vhsub_s16(upper, lower));
}

/** @brief Check inclusive signed Q14 headroom with two independent unsigned absolute maxima. */
int NgFftInputInRange(const int16_t *data, size_t size)
{
    uint16x8_t maximum0 = vdupq_n_u16(0);
    uint16x8_t maximum1 = vdupq_n_u16(0);
    uint16x4_t maximum;
    size_t i;
    for (i = 0; i < 2 * size; i += 16) {
        /* vabs(INT16_MIN) has the unsigned bit pattern 32768 and is correctly rejected. */
        maximum0 = vmaxq_u16(maximum0, vreinterpretq_u16_s16(vabsq_s16(vld1q_s16(data + i))));
        maximum1 = vmaxq_u16(maximum1, vreinterpretq_u16_s16(vabsq_s16(vld1q_s16(data + i + 8))));
    }
    maximum0 = vmaxq_u16(maximum0, maximum1);
    maximum = vmax_u16(vget_low_u16(maximum0), vget_high_u16(maximum0));
    maximum = vpmax_u16(maximum, maximum);
    maximum = vpmax_u16(maximum, maximum);
    return vget_lane_u16(maximum, 0) <= 16384;
}

/** @brief Execute an eight-point register codelet with exact Q14 eighth-root coefficients. */
void NgFftFirstStages(int16_t *data, size_t size)
{
    static const int16_t eighthCos[4] = {16384, 11585, 0, -11585};
    static const int16_t eighthMinusSin[4] = {0, 11585, 16384, 11585};
    const int16x4_t cosine = vld1_s16(eighthCos);
    const int16x4_t minusSine = vld1_s16(eighthMinusSin);
    size_t i;
    for (i = 0; i < size; i += 8) {
        int16x8_t upper = FirstTwoStages(vld1q_s16(data + 2 * i));
        int16x8_t lower = FirstTwoStages(vld1q_s16(data + 2 * i + 8));
        int16x4x2_t components = vuzp_s16(vget_low_s16(lower), vget_high_s16(lower));
        int32x4_t real = vmull_s16(components.val[0], cosine);
        int32x4_t imaginary = vmull_s16(components.val[1], cosine);
        int16x4x2_t rotated;
        real = vmlal_s16(real, components.val[1], minusSine);
        imaginary = vmlsl_s16(imaginary, components.val[0], minusSine);
        /* Products are signed32 until the original Q14 floor. The proven <23230
         * component bound makes this non-saturating narrowing exact. */
        rotated = vzip_s16(vshrn_n_s32(real, 14), vshrn_n_s32(imaginary, 14));
        lower = vcombine_s16(rotated.val[0], rotated.val[1]);
        vst1q_s16(data + 2 * i, vhaddq_s16(upper, lower));
        vst1q_s16(data + 2 * i + 8, vhsubq_s16(upper, lower));
    }
}

/** @brief Apply four independent Q14 butterflies using widening integer multiply/accumulate. */
void NgFftButterflies(int16_t *upper, int16_t *lower, const int16_t *twiddle, size_t count)
{
    while (count >= 4) {
        int16x4x2_t input = vld2_s16(lower);
        int16x4x4_t coefficients = vld4_s16(twiddle);
        int16x4x2_t original = vld2_s16(upper);
        int32x4_t real = vmull_s16(input.val[0], coefficients.val[0]);
        int32x4_t imaginary = vmull_s16(input.val[0], coefficients.val[2]);
        int16x4x2_t product;
        int16x4x2_t sum;
        int16x4x2_t difference;
        real = vmlal_s16(real, input.val[1], coefficients.val[1]);
        imaginary = vmlal_s16(imaginary, input.val[1], coefficients.val[3]);
        product.val[0] = vshrn_n_s32(real, 14);
        product.val[1] = vshrn_n_s32(imaginary, 14);
        /* Signed halving add/subtract retain the carry/sign bit before shifting,
         * including odd negative sums. No FP, rounded multiply or stage fusion of floors. */
        sum.val[0] = vhadd_s16(original.val[0], product.val[0]);
        sum.val[1] = vhadd_s16(original.val[1], product.val[1]);
        difference.val[0] = vhsub_s16(original.val[0], product.val[0]);
        difference.val[1] = vhsub_s16(original.val[1], product.val[1]);
        vst2_s16(upper, sum);
        vst2_s16(lower, difference);
        upper += 8;
        lower += 8;
        twiddle += 16;
        count -= 4;
    }
    if (count >= 2) {
        int16x4_t input = vld1_s16(lower);
        int32x2x2_t repeated = vzip_s32(vreinterpret_s32_s16(input), vreinterpret_s32_s16(input));
        int32x4_t first = vmull_s16(vreinterpret_s16_s32(repeated.val[0]), vld1_s16(twiddle));
        int32x4_t second = vmull_s16(vreinterpret_s16_s32(repeated.val[1]), vld1_s16(twiddle + 4));
        int32x4_t pairs = vcombine_s32(vpadd_s32(vget_low_s32(first), vget_high_s32(first)),
                                       vpadd_s32(vget_low_s32(second), vget_high_s32(second)));
        int16x4_t product = vshrn_n_s32(pairs, 14);
        int16x4_t original = vld1_s16(upper);
        vst1_s16(upper, vhadd_s16(original, product));
        vst1_s16(lower, vhsub_s16(original, product));
        upper += 4;
        lower += 4;
        twiddle += 8;
        count -= 2;
    }
    if (count != 0) {
        int16x4_t input = vset_lane_s16(lower[0], vdup_n_s16(lower[1]), 0);
        int16x4_t original = vset_lane_s16(upper[0], vdup_n_s16(upper[1]), 0);
        int32x2_t repeated = vdup_lane_s32(vreinterpret_s32_s16(input), 0);
        int32x4_t products = vmull_s16(vreinterpret_s16_s32(repeated), vld1_s16(twiddle));
        int32x2_t pairs = vpadd_s32(vget_low_s32(products), vget_high_s32(products));
        int16x4_t product = vshrn_n_s32(vcombine_s32(pairs, pairs), 14);
        int16x4_t sum = vhadd_s16(original, product);
        int16x4_t difference = vhsub_s16(original, product);
        upper[0] = vget_lane_s16(sum, 0);
        upper[1] = vget_lane_s16(sum, 1);
        lower[0] = vget_lane_s16(difference, 0);
        lower[1] = vget_lane_s16(difference, 1);
    }
}
