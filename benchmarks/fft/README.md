# Integer FFT performance comparison

This standalone development tool builds the actual production FFT sources and measures complete
FFT calls. It can also compile the untouched original NGene `asmFolder/fft.c`, `fft.h` and `simd.h`
from a separately supplied source tree. It does not link legacy code into the receiver, copy the
archive into this repository, or add receiver dependencies.

The original SSE2 FFT is **still faster** on the tested computer. The changes here reduce EVO
latency by approximately 35–38% at 1024, 4096 and 16384 points, while retaining its numerical and
validation contract. These results do not establish a globally fastest FFT or receiver throughput.

## Reproduce

On Linux x86-64 with GCC or Clang:

```sh
cmake -S benchmarks/fft -B build/fft-benchmark -DCMAKE_BUILD_TYPE=Release \
  -DNGENE_LEGACY_ROOT=/path/to/original/ngene
cmake --build build/fft-benchmark --parallel 2
./build/fft-benchmark/ngene_fft_benchmark
```

On Windows, use a Visual Studio developer terminal with CMake and Clang installed. The GNU inline
assembly in the original source requires the ClangCL toolset; native MSVC cannot compile it.
MASM assembles the production SSE2 kernel for either compiler.

```powershell
cmake -S benchmarks/fft -B build/fft-benchmark -G "Visual Studio 18 2026" -A x64 -T ClangCL `
  -DNGENE_LEGACY_ROOT="C:/path/to/original/ngene"
cmake --build build/fft-benchmark --config Release --parallel 2
./build/fft-benchmark/Release/ngene_fft_benchmark.exe
```

Select the installed Visual Studio generator. If ClangCL discovery fails, pass
`-DCMAKE_C_COMPILER="C:/path/to/clang-cl.exe"` in a fresh build directory. The tool explicitly
enables `/O2 /Ob2` for both measured C implementations in Release on Windows, or `-O3` elsewhere, and
refuses to run a Debug build. This also avoids silently timing an empty configuration cache after
failed compiler detection. There is no fast-math or native-ISA flag.

Omit `NGENE_LEGACY_ROOT` to compare EVO with its scalar oracle alone. This is **not** an original
NGene comparison. Native MSVC and AArch64 builds support that mode; the historical baseline is
x86 SSE2. To compare an older EVO checkout using the same benchmark harness, set
`-DNGENE_FFT_SOURCE_ROOT=/path/to/older/checkout` in a separate build directory.

## Method

- Each size, 256 through 16384, uses eight deterministic complex noise windows with components
  in `[-2047,2047]`, representing 12-bit ADC levels with no clipping in either FFT.
- Before timing, EVO must match its scalar oracle exactly. A complex tone independently verifies
  frequency sign, natural bin order and `/N` scaling in every backend. The legacy gain is
  `-log2(N)`, so it also scales by `/N`. Its Q14 coefficient truncation differs from EVO rounding;
  the measured maximum difference was one or two integer output LSBs per component.
- Plans and aligned buffers are prepared first. Calls include entry validation, bit reversal and
  all stages. Plan construction, allocation, windowing, power, I/O and SDR transport are excluded.
  EVO retains its full input-headroom/size/overlap checks; the original retains its original checks.
- Each backend is warmed up, then calibrated to at least 10 ms per batch. Twenty-one rounds
  rotate backend order. CSV reports median, p95 and minimum **batch-average** microseconds per
  transform, plus complex samples per second. The p95 is not a per-call tail-latency percentile.
  A volatile checksum consumes outputs.
- On Windows, the benchmark thread temporarily uses the lowest available logical-processor
  affinity bit, prints the mask and restores its original affinity before exit. This avoids
  comparisons across different core types on a hybrid processor. Close other heavy workloads;
  frequency, thermal limits, OS interruptions and cache effects can still change results.
- Legacy host adapters replace receiver logging, fatal exit and aligned allocation only. Its
  original transform, table generation and SIMD macros are included without edits. The build
  prints SHA-256 for the three source files. GCC reserves the XMM registers omitted from the
  old macro clobber contracts; Clang disables auto-vectorization and the resulting baseline is
  numerically validated before timing. This is a single-threaded comparison because the original
  FFT uses a shared XMM backup buffer.

## Measurements on 16 October 2026

Windows x64, Intel Core i7-1355U, Clang 22.1.3, Visual Studio 18 2026, Release `/O2 /Ob2`, SSE2, logical
processor mask `0x1`. The EVO baseline is commit
`e63ca0545b9265383928d70b76dae9833280e3d7`. Both before and after runs use the same benchmark,
compiler flags, affinity, legacy source, input seed and validation. Raw data and provenance:
[before.csv](results/windows-i7-1355u-before.csv),
[after.csv](results/windows-i7-1355u-after.csv).

| Complex samples | EVO before, median us | EVO after, median us | EVO reduction | Original NGene, median us | EVO after / original |
|---:|---:|---:|---:|---:|---:|
| 1024 | 6.179 | 3.920 | 36.6% | 2.362 | 1.66 |
| 4096 | 27.042 | 16.762 | 38.0% | 9.922 | 1.69 |
| 16384 | 124.945 | 81.082 | 35.1% | 69.564 | 1.17 |

The original column is from the after run, so its ratio uses interleaved measurements. Independent
before/after processes remain subject to host variation; in these files the original medians change
by less than 2% for these three sizes. The 16384-point working set also exercises different
cache behavior. No CPU-independent performance threshold is asserted.

EVO still performs additional safety validation and retains its revised numerical contract. The
old routine saturates signed16 sums **before** division, which changes clipped high-level signals.
EVO retains exact floor-scaled averaging, immutable plans, disjoint per-call workspaces, natural
int16 alignment and thread safety. Its new SSE2 averaging identity gives the same output as its
signed32 oracle. This numerical difference must be considered before acquisition/tracking migration;
matching `/N` gain on unclipped input does not validate the complete GNSS processing chain.

The retained optimizations are fused first two stages, four complex butterflies per main SSE2
iteration, overflow-safe signed16 averaging, complete SIMD input-headroom validation and a whole
complex-pair bit-reversal copy. NEON and the scalar oracle keep their arithmetic. Correctness tests
cover all powers of two 8–16384, independent full-scale I/Q signs, odd neighbors, rotating complex
corners, independent DFT/tone checks, unaligned buffers, invalid lanes and shared-plan concurrency.

Further performance work should profile the remaining permutation/stage and entry-check costs on
the deployment CPU, then compare SIMD variants at identical numerical semantics. AVX2 dispatch,
radix-4 or different scaling require their own measurements and validation; they are not implied by
this benchmark.
