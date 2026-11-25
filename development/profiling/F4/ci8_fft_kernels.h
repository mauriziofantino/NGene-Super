#ifndef NGENE_F4_CI8_FFT_KERNELS_H
#define NGENE_F4_CI8_FFT_KERNELS_H

#include <stddef.h>
#include <stdint.h>

#ifndef CI8_FFT_API
#define CI8_FFT_API
#endif

/* n and width are radix-2 lengths, with width dividing n.
 * Coefficients have 7 or 15 fractional bits and include both signed endpoints.
 * The compact adaptive stages return the minimum common shift. */
CI8_FFT_API unsigned Ci8FftCompactAdaptiveStage(int8_t *values, const int64_t *wr, const int64_t *wi,
                                     size_t n, size_t width, unsigned bits);
/* Fixed shifts by one bit and returns the number of saturated components. */
CI8_FFT_API unsigned Ci8FftCompactFixedStage(int8_t *values, const int64_t *wr, const int64_t *wi,
                                  size_t n, size_t width, unsigned bits);

CI8_FFT_API void Ci8FftWideStage(int64_t *values, const int64_t *wr, const int64_t *wi,
                                 size_t n, size_t width, unsigned bits);

/* AVX2 entry points require caller-side ISA/OS checks.
 * Adaptive requires width >= 16, wide Q15 requires width >= 4. */
unsigned Ci8FftCompactAdaptiveStageAvx2(int8_t *values, const int64_t *wr, const int64_t *wi,
                                        size_t n, size_t width, unsigned bits);
void Ci8WideQ15StageAvx2(int64_t *values, const int64_t *wr, const int64_t *wi,
                         size_t n, size_t width);


/* Scratch contains 2*n int16_t components disjoint from values.
 * Each [first,end) range is in butterfly columns, with endpoints divisible by 8.
 * Disjoint ranges can run concurrently. Pack after all columns finish, using
 * the maximum shift returned by the ranges. */
unsigned Ci8FftCompactBufferColumnsAvx2(const int8_t *values, int16_t *scratch,
                                        const int64_t *wr, const int64_t *wi,
                                        size_t n, size_t width, unsigned bits,
                                        size_t first, size_t end);
void Ci8FftCompactPackBufferedAvx2(int8_t *values, const int16_t *scratch, size_t n, unsigned shift);
unsigned Ci8FftCompactAdaptiveBufferedAvx2(int8_t *values, int16_t *scratch,
                                           const int64_t *wr, const int64_t *wi,
                                           size_t n, size_t width, unsigned bits);

unsigned Ci8FftCompactAdaptiveMaddAvx2(int8_t *values, int16_t *scratch,
                                       const int64_t *wr, const int64_t *wi,
                                       size_t n, size_t width, unsigned bits);

#endif

