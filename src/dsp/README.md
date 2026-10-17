# NGene DSP

`ngene_dsp` (`NGene::Dsp`) is the mathematical library, independent of the engine,
frontends, and GUI. It depends only on `ngene_base` for common errors and the system
mathematics library where needed. Public APIs expose no SIMD types or device details.

| Family | API | Source |
|---|---|---|
| Complex Q14 FFT | `ngene/dsp/fft.h` | `fft/fft.c`, private `x86/` and `arm/` kernels |
| CI8 I/Q statistics | `ngene/dsp/statistics.h` | `statistics/moments.c` |
| CI16 power and uint64 accumulation | `ngene/dsp/power.h` | `power/power.c` |

`ngene/dsp/sample_ops.h` remains the compatible umbrella header. Private `sample_backend.h`
centralizes primitive SIMD selection without duplication between families. Formulas,
loops, and oracles are preserved by the split.

`cmake/FftBackend.cmake` selects the FFT kernel from the compiler target, including in
benchmarks: x64 SSE2 assembly, x86-32 SSE2 intrinsics, ARM64/ARMv7 NEON intrinsics.
Statistics/power primitives retain SSE2 on x64, NEON on ARM64, and C on other targets.
Reference options remain available for verification. Immutable FFT plans and disjoint
workspaces permit concurrent calls without internal library threads.

Correlators, NCOs, and replicas will receive their own families when migrated and validated.
See the [architecture](../../docs/service/ARCHITECTURE.md),
[FFT contract](../../docs/service/FFT_REVIEW.md), and [benchmark](../../benchmarks/fft/README.md).
