# x86-64 SSE2

`fft_sse2.S`: GNU/Clang, ABI System V/Win64 and Mach-O. `fft_sse2.asm`: MASM for MSVC x64. No AVX instructions are required. Shared C code handles plans, permutation and stage scheduling. SSE2 fuses the first two stages, validates input headroom and processes four complex butterflies per main iteration. See [FFT review](../../../docs/FFT_REVIEW.md) and the [benchmark](../../../benchmarks/fft/README.md).
