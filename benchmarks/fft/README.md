# Integer FFT performance comparison

This standalone development tool builds the actual production FFT sources and measures complete
FFT calls. It can also compile the untouched original NGene `asmFolder/fft.c`, `fft.h` and `simd.h`
from a separately supplied source tree. It does not link legacy code into the receiver, copy the
archive into this repository, or add receiver dependencies.

The latest eight-point codelet update reduces EVO latency by approximately 39–44% at 1024, 4096
and 16384 points against the preceding EVO implementation in same-process comparisons. The
original SSE2 FFT remains competitive at smaller sizes; the latest EVO is faster at 16384 in
these measurements. These results do not establish a globally fastest FFT or receiver throughput.

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
MASM assembles the production x64 SSE2 kernel for either compiler; Win32 uses intrinsics.

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
NGene comparison. Native MSVC, x86, ARMv7 NEON and AArch64 builds support that mode; the historical baseline is
x86 SSE2. To compare an older EVO checkout using the same benchmark harness, set
`-DNGENE_FFT_SOURCE_ROOT=/path/to/older/checkout` in a separate build directory.

For a direct, interleaved before/after comparison in **one process**, add
`-DNGENE_FFT_BASELINE_ROOT=/path/to/older/checkout`. This compiles a separate
`EVO-before` backend with the same compiler and flags, independently owned plan and mechanically
renamed symbols. Only symbol names change in the frozen assembly; its arithmetic stays intact.
The kernel is selected independently in each tree: an older AArch64 assembly implementation can
be compared with the new NEON intrinsics, and a 32-bit tree without a SIMD kernel uses its
`C-reference` implementation. The printed `before_backend` identifies that distinction.
This option works with or without `NGENE_LEGACY_ROOT`. Current/before C and kernel SHA-256 hashes
are printed into the CSV. Before timing, all eight noise windows and an analytic tone must agree
bit for bit with the current scalar oracle.

## Method

- Each size, 8 through 16384, uses eight deterministic complex noise windows with components
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

That earlier revision retained fused first two stages, four complex butterflies per main SSE2
iteration, overflow-safe signed16 averaging, complete SIMD input-headroom validation and a whole
complex-pair bit-reversal copy. NEON and the scalar oracle keep their arithmetic. Correctness tests
cover all powers of two 8–16384, independent full-scale I/Q signs, odd neighbors, rotating complex
corners, independent DFT/tone checks, unaligned buffers, invalid lanes and shared-plan concurrency.

## Eight-point codelet update

The new internal SSE2 implementation applies the small-codelet and depth-first locality techniques
described in [Frigo and Johnson, *The Design and Implementation of FFTW3*, §§IV–V](https://www.fftw.org/fftw-paper-ieee.pdf).
It introduces no FFTW dependency or imported FFTW source. The three retained changes are:

- Fuse widths 2, 4 and 8 into an eight-point register codelet, eliminating the separate width-8
  calls and intermediate workspace pass. Unity and quarter-turn rotations are exact sign/swaps;
  the eighth-root products retain the plan's Q14 constant 11585 and original product floors.
- Gather bit-reversed inputs in output-contiguous quartets. One table entry supplies indices
  `r`, `r+N/2`, `r+N/4`, `r+3N/4`, replacing four table loads and scattered output writes.
- Finish independent subtrees of at most 512 points before the global stages. This changes only
  the order of independent butterflies. Trials included blocks 256/512/1024/2048 and a full-size
  breadth-first traversal. The isolated [cache comparison](results/windows-i7-1355u-codelet8-cache-comparison.csv)
  shows small, mixed differences (about −1.4% to +3.7% latency reduction); the substantial combined
  improvement must not be attributed to cache blocking alone.

Two complete runs alternate current, scalar, original NGene and frozen EVO-before backends with
21 rounds on the same pinned logical processor. The before source is the FFT at commit
`0dd9729f40b2837de496b9a5765ed5f0323410a2`; the CSV records hashes for both implementations.
Same Windows i7-1355U/Clang 22.1.3 Release setup as above. Raw runs:
[run 1](results/windows-i7-1355u-codelet8-run1.csv),
[run 2](results/windows-i7-1355u-codelet8-run2.csv).

| Complex samples | EVO-before, median µs (run 1) | Current, median µs (run 1) | Reduction, runs 1–2 |
|---:|---:|---:|---:|
| 1024 | 4.742 | 2.775 | 41.5–43.9% |
| 4096 | 24.283 | 14.208 | 39.4–41.5% |
| 16384 | 102.680 | 59.034 | 40.0–42.5% |

Absolute timings vary visibly with host load and clock changes. The frozen/current backends run
in the same process with alternating order, so these ratios are more useful than comparing a new
absolute time with an older standalone run. At 16384 the current/original ratios were 0.68 and
0.69, while 4096 remained about 4% slower than original. These are full-call microbenchmarks,
including entry checks, not receiver CPU-utilization measurements.

Every original per-stage `/2` floor and Q14 product floor is preserved. A combined `/4` or
negating an already rounded product is not equivalent for odd negative values. The independent
scalar path retains the original scatter permutation and breadth-first schedule; it is not
modified to mirror the optimization. The checked-in regression adds 13,122 eight-point
rail/odd/rotated cases and all 78 plan/subsize pairs, with guarded input/output offset two bytes
from 16-byte alignment. Existing DFT, analytic tone, invalid-input and shared-plan thread checks
also pass. Native MASM, Windows GAS and explicit C-reference builds execute the expanded suite;
GAS assembly also compiles for Linux ELF and macOS Mach-O. ARM kernels are unchanged and no ARM
performance gain is claimed.

## ARM NEON and 32-bit x86 update

The production benchmark now supports x86-64 SSE2 assembly, 32-bit x86 SSE2 intrinsics,
AArch64 NEON intrinsics and ARMv7 NEON intrinsics. Each CSV reports the compiler target and
pointer width, along with the selected backend and source hashes. For Win32 with native MSVC:

```powershell
cmake -S benchmarks/fft -B build/fft-benchmark-win32 -G "Visual Studio 18 2026" -A Win32 `
  -DNGENE_FFT_BASELINE_ROOT="C:/path/to/frozen/evo"
cmake --build build/fft-benchmark-win32 --config Release --parallel 2
./build/fft-benchmark-win32/Release/ngene_fft_benchmark.exe
```

For ARM, use a native ARM environment or supply the appropriate CMake cross-toolchain file.
The ARMv7 NEON source uses `-mfpu=neon` with GCC/Clang; the compiler/toolchain must select the
matching CPU and floating-point ABI. AArch64 has baseline NEON. On x86, the SIMD source alone
uses `/arch:SSE2` or `-msse2`; the common FFT source uses `/arch:IA32` or `-mno-sse2`, preserving
the runtime scalar fallback on CPUs without SSE2. The frozen 32-bit common source receives the
same scalar ISA option. These flags do not change the Q14 arithmetic contract.

Two native Win32 runs on Windows/i7-1355U, MSVC 19.51, Release `/O2 /Ob2`, affinity mask `0x1`:
[run 1](results/windows-i7-1355u-win32-sse2-run1.csv),
[run 2](results/windows-i7-1355u-win32-sse2-run2.csv).
The frozen EVO source hash is recorded in each file. Its 32-bit backend was **C-reference**;
these figures compare with that scalar implementation, not with original NGene or x86-64 SSE2.

| Complex samples | New Win32 SSE2, median us (runs 1 / 2) | Previous Win32 scalar, median us (runs 1 / 2) | Speedup (runs 1 / 2) |
|---:|---:|---:|---:|
| 8 | 0.020 / 0.020 | 0.103 / 0.102 | 5.2x / 5.1x |
| 1024 | 2.341 / 2.087 | 130.860 / 114.055 | 55.9x / 54.7x |
| 4096 | 10.429 / 10.579 | 613.875 / 612.084 | 58.9x / 57.9x |
| 16384 | 47.925 / 41.609 | 2899.025 / 2535.344 | 60.5x / 60.9x |

The large ratios reflect replacing the preceding portable scalar implementation with SIMD in
the same 32-bit build. They cannot be extrapolated to other CPUs, compilers, ARM or receiver
throughput. All compared noise windows and the independent analytic tone pass validation before
timing. Native ARM measurements are still needed to establish the NEON performance change.

For correctness when no ARM hardware is available, the optional
[`test_fft_arm_emulated.py`](../../tests/test_fft_arm_emulated.py) cross-compiles the actual
production execution/oracle and NEON kernels with freestanding Clang/LLD, then executes ARM
instructions with Unicorn. Install `unicorn==2.1.4` and `pyelftools==0.32` into a development
Python environment and run both targets:

```sh
python tests/test_fft_arm_emulated.py --clang /path/to/clang --arch arm
python tests/test_fft_arm_emulated.py --clang /path/to/clang --arch arm64
```

The emulator uses externally initialized immutable tables; it does not validate the native C
library's plan allocation, trigonometric table generation or OS thread ABI. It checks all
transform lengths, every plan/subsize pair, independent input/output alignments, guarded storage,
invalid SIMD lanes, analytic tones and the 13,122-case eight-point arithmetic corpus. This is
numerical correctness evidence only; emulator timing is not a performance benchmark.

## Direct comparison with original NGene — 17 October 2026

Four new runs compare the actual original source against all locally executable production
backends: two x64 runs and two Win32 runs, using Clang 22.1.3, Release `/O2 /Ob2`, Intel
i7-1355U and affinity mask `0x1`. Each process interleaves current SIMD, current scalar,
original NGene and frozen preceding EVO calls. Both architectures run all powers of two
from 8 through 16384. The original has its own native 32-bit path; the benchmark adapter now
selects its existing pointer-width branch correctly rather than forcing the 64-bit branch.
Original files and FFT arithmetic are untouched. Raw results:

- [x64 run 1](results/windows-i7-1355u-original-x64-run1.csv),
  [x64 run 2](results/windows-i7-1355u-original-x64-run2.csv).
- [Win32 run 1](results/windows-i7-1355u-original-win32-run1.csv),
  [Win32 run 2](results/windows-i7-1355u-original-win32-run2.csv).
- [All normalized results](results/windows-i7-1355u-original-normalized.csv), including
  every measured size and backend.

The original source tree used here is external to Git:
`C:/Users/MaurizioFantino/Downloads/ngene.tar/ngene`. Every new raw CSV records the SHA-256 of
its unmodified `asmFolder/fft.c`, `fft.h` and `simd.h`, along with current/frozen EVO source
hashes. For example, original `fft.c` is
`95a36b1dae1b9aafd91229b3b068995e7c52fefbced1800f4c81860bdb57b99f`.

The following are **current/original latency ratios**, using the original median from the
same process. `1.000` means equal latency; smaller is faster. Ranges span the two runs,
not a statistical confidence interval. Normalize within each architecture; these separate
processes are not a controlled 32-bit-versus-64-bit ranking.

| Implementation | N=1024 | N=4096 | N=16384 |
|---|---:|---:|---:|
| Original NGene x64 SSE2 | 1.000 | 1.000 | 1.000 |
| Current EVO x64 SSE2 assembly | 1.047–1.052 | 1.008–1.017 | 0.607–0.649 |
| EVO x64 before the eight-point codelet | 1.769–1.791 | 1.666–1.678 | 1.133–1.147 |
| Current EVO x64 C-reference | 7.362–7.435 | 6.916–7.008 | 4.423–4.521 |
| Original NGene Win32 SSE2 | 1.000 | 1.000 | 1.000 |
| Current EVO Win32 SSE2 intrinsics | 0.824–0.905 | 0.984–0.986 | 0.682–0.685 |
| Current EVO Win32 C-reference | 6.031–6.127 | 6.682–6.705 | 4.757–4.817 |
| Preceding EVO Win32 C-reference | 5.772–6.144 | 6.618–6.885 | 4.833–4.858 |
| Current ARM64 NEON intrinsics | Native timing unavailable | Native timing unavailable | Native timing unavailable |
| Preceding ARM64 NEON assembly | Native timing unavailable | Native timing unavailable | Native timing unavailable |
| Current ARMv7 NEON, 32 bit | Native timing unavailable | Native timing unavailable | Native timing unavailable |
| ARMv7/ARM64 C-reference | Native timing unavailable | Native timing unavailable | Native timing unavailable |

Representative absolute medians, in microseconds per complete call:

| Implementation | N=1024, runs 1 / 2 | N=4096, runs 1 / 2 | N=16384, runs 1 / 2 |
|---|---:|---:|---:|
| Original NGene x64 | 2.138 / 2.136 | 9.942 / 9.997 | 71.658 / 67.959 |
| Current EVO x64 SSE2 | 2.238 / 2.247 | 10.025 / 10.163 | 46.481 / 41.251 |
| Original NGene Win32 | 2.521 / 2.598 | 10.846 / 11.067 | 71.635 / 59.644 |
| Current EVO Win32 SSE2 | 2.077 / 2.350 | 10.668 / 10.915 | 48.837 / 40.870 |

At 16384, current EVO reduces latency by **35.1–39.3% on x64** and **31.5–31.8% on Win32**
against the original in the same process. At 1024, x64 takes 4.7–5.2% longer, while Win32
takes 9.5–17.6% less time. At 4096 both are close to the original (within about 1.7%);
small differences should be treated as approximate parity under this host's variation.
The preceding 55–61x Win32 figure compared SIMD with MSVC's portable C-reference, **not**
original NGene. These direct-original runs use Clang for every compared C source because
the original GNU inline assembly cannot be compiled by native MSVC. Compiler-dependent
scalar timings must not be mixed across the two experiments.

For native Win32 reproduction, use `-A Win32 -T ClangCL` in a fresh directory and supply
`NGENE_LEGACY_ROOT`, just as in the x64 command above. Add `NGENE_FFT_BASELINE_ROOT` if a
frozen EVO tree is available. ARM has no native original SSE2 backend in the supplied source;
the ARM rows report the existing numerical checks, not a fabricated timing comparison.

### Precision versus the original

Every EVO optimized backend is bit-identical to its scalar oracle under the public Q14
contract. ARMv7 and ARM64 each passed 17,140 compiled-instruction emulation cases in the
previous update. In the four native comparison runs, every timed 12-bit noise window passes
bit-exact SIMD/reference validation; original NGene differs by 0–2 output-component LSBs,
depending on transform length/window. Its Q14 coefficient truncation differs from EVO's
rounding. Every backend also passes an independent analytic tone's sign, bin and `/N` checks.

An additional **78,658-case**, untimed eight-point probe combines the existing 13,122
rail/odd/rotation cases with every one of the 65,536 independent I/Q sign assignments at
`±16384`. Current x64 and Win32 SIMD remain bit-exact to the scalar oracle. The largest
observed difference from original NGene is **3394 LSB**, reproduced in all four runs.
This is an observed corpus result, not a general maximum-error bound.

The CSV includes this reproducible worst-case interleaved input:

```text
-16384, 16384, 16384, -16384, -16384, -16384, 16384, 16384,
-16384, -16384, -16384, 16384, 16384, -16384, -16384, -16384
```

For its bin 5 imaginary component, the independent normalized DFT gives `19777.2375029604`.
EVO returns `19777` (absolute error about `0.2375`); original NGene returns `16383` (error
about `3394.2375`). This demonstrates original signed16 saturation before division by two;
EVO's carry-preserving floor averages avoid that clipping. Thus preserving EVO's precision
does not mean reproducing the original's saturation behavior. The FFT benchmarks alone do
not establish GNSS acquisition/tracking interchangeability or receiver-wide CPU utilization.

[Johnson and Frigo's modified split-radix algorithm](https://www.fftw.org/newsplit.pdf) was also
examined. Its operation count and floating-point accuracy results do not establish equivalence
to this staged Q14 contract. Adopting different scaling/rounding would need an independently
specified error budget and GNSS-chain validation; it is not hidden inside this optimization.
