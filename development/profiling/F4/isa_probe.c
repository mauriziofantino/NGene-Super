/* Build for the baseline ISA so this probe can run on older CPUs. */
#include <stdio.h>

#if defined(_M_X64)
#include <intrin.h>
#elif defined(__linux__) && defined(__aarch64__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#include <sys/prctl.h>
#endif

int main(void)
{
#if defined(_M_X64)
    int cpu[4];
    __cpuid(cpu, 0);
    if (cpu[0] < 7) {
        puts("avx512f=0 avx512bw=0 os_zmm=0");
        return 0;
    }
    __cpuid(cpu, 1);
    const int osxsave = (cpu[2] & (1 << 27)) != 0;
    const int os_zmm = osxsave && (_xgetbv(0) & 0xe6) == 0xe6;
    __cpuidex(cpu, 7, 0);
    printf("avx512f=%d avx512bw=%d os_zmm=%d\n", (cpu[1] & (1 << 16)) != 0,
           (cpu[1] & (1 << 30)) != 0, os_zmm);
#elif defined(__linux__) && defined(__aarch64__)
    const int sve = (getauxval(AT_HWCAP) & HWCAP_SVE) != 0;
    const int sve2 = (getauxval(AT_HWCAP2) & HWCAP2_SVE2) != 0;
    const int vl = sve ? prctl(PR_SVE_GET_VL, 0L, 0L, 0L, 0L) : 0;
    if (vl < 0) {
        perror("PR_SVE_GET_VL");
        return 1;
    }
    printf("sve=%d sve2=%d vector_bytes=%d\n", sve, sve2, vl & PR_SVE_VL_LEN_MASK);
#else
    fputs("Expected Windows x64 (MSVC) or Linux AArch64\n", stderr);
    return 1;
#endif
    return 0;
}
