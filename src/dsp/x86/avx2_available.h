#ifndef NGENE_DSP_X86_AVX2_AVAILABLE_H
#define NGENE_DSP_X86_AVX2_AVAILABLE_H

#ifdef _MSC_VER
#include <intrin.h>
#endif

static inline int NgDspAvx2Available(void)
{
#ifdef _MSC_VER
    int info[4];
    __cpuid(info, 0);
    if (info[0] < 7)
        return 0;
    __cpuid(info, 1);
    if ((info[2] & (3 << 27)) != (3 << 27) || (_xgetbv(0) & 6) != 6)
        return 0;
    __cpuidex(info, 7, 0);
    return (info[1] & (1 << 5)) != 0;
#else
    return __builtin_cpu_supports("avx2") != 0;
#endif
}
#endif
