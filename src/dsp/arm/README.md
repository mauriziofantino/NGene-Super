# ARM NEON

`fft_neon_intrinsics.c` is the production FFT kernel on ARM64 and 32-bit ARMv7 NEON:
four butterflies per iteration, an initial eight-point codelet, and SIMD range checking.
It preserves exact Q14 products and per-stage floors, with caller-owned workspace.

`fft_neon.S` and `fft_neon.asm` preserve the preceding two-butterfly ARM64 kernel as a
comparison baseline; they are not selected for current production sources.
Primitives in `../statistics/` and `../power/` use NEON on ARM64 and C on ARM32.
For precision, cross-compilation, emulation, and native measurement limitations, see
[FFT_REVIEW.md](../../../docs/service/FFT_REVIEW.md).
