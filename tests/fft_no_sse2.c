/** @file fft_no_sse2.c @brief Test-only CPU probe and trap kernels for x86 scalar dispatch. */
#include "dsp/fft/butterfly.h"
#include <stdlib.h>

/**
 * @brief Simulate a 32-bit x86 processor without SSE2, regardless of the test host.
 * @details Linked only into the alternate FFT dispatch test executable. Its real
 * scalar FFT implementation must pass the complete public FFT test suite without
 * calling any vector kernel below. Production executables use the CPUID probe.
 */
int NgFftSimdAvailable(void)
{
    return 0;
}

/** @brief Fail immediately if the no-SSE2 execution path enters a vector butterfly. */
void NgFftButterflies(int16_t *upper, int16_t *lower, const int16_t *twiddle, size_t count)
{
    (void)upper;
    (void)lower;
    (void)twiddle;
    (void)count;
    abort();
}

/** @brief Fail immediately if the no-SSE2 execution path enters the vector codelet. */
void NgFftFirstStages(int16_t *data, size_t size)
{
    (void)data;
    (void)size;
    abort();
}

/** @brief Fail immediately if the no-SSE2 execution path enters vector validation. */
int NgFftInputInRange(const int16_t *data, size_t size)
{
    (void)data;
    (void)size;
    abort();
}
