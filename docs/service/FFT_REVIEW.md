# NGene FFT: analysis and migration · 1 October 2026

> Historical analysis with dated updates through 17 October 2026. Earlier target and validation tables describe the indicated revision; later sections supersede them. See the [current operator manual](../wiki/Home.md).

## Sources examined

Original `ngene.tar` archive (identical to `ngene(2).tar`), SHA256 `c15c72e9e715e6a77ee6ccf694adb3e2d939a355b1f6ad3ff4e82121de8e1acb`. Fully examined `asmFolder/fft.c` (495 lines), `fft.h` (71), and relevant `simd.h` macros (520). Original copies remain unchanged; the project distributes the adapted port and this traceability, not the entire historical archive.

`PerformFft` implements radix-2 DIT, bit-reversal permutation, signed16 interleaved complex input/output, and Q14 twiddles. The first two stages have specialized kernels; subsequent stages use SSE2, expand pairs, and apply `pmaddwd` to coefficients `[Re,-Im,Im,Re]`. The gain parameter determines how many stages reduce the result by one bit. The default maximum size is 16384.

## Identified problems and actions in increment 0.3

| Evidence in historical code | Consequence | Change |
|---|---|---|
| `paddsw` / `psubsw` precede `psraw 1` | Saturation destroys information before division: 20000+20000 becomes 32767, then 16383 instead of 20000 | Signed32 addition/subtraction, shift, then signed16 narrowing |
| Separate GNU asm macros without complete clobber declarations | Compiler lacks the contract needed for optimization and register allocation | Leaf assembly functions with explicit ABI in `.S` / `.asm` |
| XMM backup in global `ngeneXmmsBackup` | Concurrent calls may share/overwrite the same backup | No mutable global state; immutable plan and per-call workspace |
| `posix_memalign`, sine/cosine VLAs, build-dependent types/pointers | Linux/GNU coupling and inconsistent alignment requirements | Bounded heap tables, explicit types, unaligned SIMD loads/stores |
| Minimum-size check but no `log2Size > table->maxLog2FftSize` check | Invalid shifts/indices for requests beyond the table | API checks power-of-two size |
| Library errors call receiver logging/shutdown | Mathematical kernel cannot be reused independently | Return code and `TNgError`, no exit in DSP |
| Data-dependent variable gain and saturation | Difficult spectrum calibration and backend comparison | Explicit initial `/N` contract, input headroom, same floor for C/SSE2/NEON |

Bit-for-bit equivalence with every gain mode of the old `PerformFft` is not claimed: correcting saturation placement specifically changes pathological cases. Before inserting this kernel into acquisition/tracking, the complete NGene chain must be checked: gain, complex-product convention, possible inverse transform through conjugation, thresholds, and replica quantization.

## Implemented contract

- C API `NgFftCreate`, `NgFftExecute`, `NgFftReference`, `NgFftDestroy`.
- Forward complex FFT `exp(-j 2πkn/N)`, natural order, output normalized by N, power-of-two sizes 8..16384. The GUI exposes 256..16384.
- Interleaved signed16 data; input components in [-16384,16384], checked. Shareable plan, disjoint input/output, at least natural C int16 alignment. No allocation or floating-point arithmetic during `execute`.
- Q14 twiddles follow NGene; complex signed32 products use SSE2 `pmaddwd` or NEON `smull`/`addp`, followed by a 14-bit shift. NEON, the scalar oracle and SSE2 tails divide signed32 sums by two with arithmetic-floor rounding before saturating narrowing. The updated main SSE2 loop computes four independent complex butterflies with equivalent overflow-safe signed16 floor averaging; the first two stages are fused. See the October 2026 update below for the headroom proof and measurements.
- C manages the plan, bit reversal, and stage sequence. Production butterflies are assembly. The C reference is a verification oracle and an explicitly selectable build, not a silent fallback.
- The plan contains only immutable tables; two threads may execute simultaneous FFTs with separate arrays. The observation worker uses one thread to limit interference with the receiver.

Headroom keeps the initial magnitude below approximately 23171 and Q14 products comfortably within signed32. Power and averages use uint64. `floor` is defined for negative values too and reproduced by both backends' arithmetic instructions. Q14 remains finite precision: the spectrum floor may include rounding/quantization error, particularly with long FFTs and weak signals. Do not confuse it with a calibrated RF noise measurement.

## Targets and assemblers

| Target | Implementation | Local verification |
|---|---|---|
| Linux x86-64 GCC/Clang | `fft_sse2.S`, System V ABI | Complete FFTs executed and compared against reference/DFT |
| Windows x86-64 GNU/Clang | `fft_sse2.S`, Win64 ABI | Full program cross-compiled and linked |
| Windows x86-64 MSVC | `fft_sse2.asm`, MASM | Sources/CI prepared; MSVC not run in that environment |
| Linux AArch64 | `fft_neon.S`, AAPCS64 | Full program cross-compiled; butterfly emulated at instruction level |
| macOS x86-64 / Apple Silicon | SSE2 / NEON `.S`, Mach-O symbols | Both assembly objects compiled; Mac application/SDK unavailable |
| Windows ARM64 | `.S` with an appropriate Clang/GNU toolchain | Not verified in this delivery; MSVC ARMASM not implemented |
| 32-bit x86/ARM | `NGENE_FFT_REFERENCE=ON` build | No 32-bit assembly kernel ported in this increment |

CMake identifies the **target**, not the host. No `-march=native` or mandatory AVX. SSE2 is the x86-64 baseline; NEON is the AArch64 backend. No SSE2/AVX runtime dispatch in this increment. Windows x64 preserves nonvolatile registers by not using them: the kernel touches only volatile GPRs and XMM0..5. The ARM kernel uses x0..3 and v0..4.

ABI references: [Microsoft x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention), [Arm AAPCS64](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst). Numerical checks are reproducible through included C tests, not inferred solely from instruction manuals.

## Local measurements

GCC 13.3, Linux x86-64, Release `-O3`, 300 calls per measurement, preinitialized plan; includes input checks, permutation, and stages; excludes windowing, power, network, and I/O. Two runs in the same environment produced:

| N | SSE2 assembly | Scalar C reference | C / assembly ratio |
|---|---:|---:|---:|
| 1024 | 5.8–6.4 µs | 17.9–19.8 µs | 2.8–3.4 |
| 4096 | 26.9–29.0 µs | 85.5–93.6 µs | approximately 3.2 |
| 16384 | 139.9–150.8 µs | 469.8–476.4 µs | 3.1–3.4 |

These are microbenchmarks of the new kernel, **not a comparison against old NGene assembly**, whole-receiver measurements, or guarantees for other CPUs. The C reference uses portable floor division to guarantee semantics; it does not represent every possible optimized C FFT. Reproduce with `ngene_fft_tests --benchmark` in Release.

## Reasonable subsequent improvements

First measure costs on real hardware. Then evaluate fusion/specialization of initial stages, radix-4, batching independent FFTs, AVX2 with dispatch, and block-floating scaling returning an exponent. The latter can recover weak-signal dynamic range but changes the numerical contract; it is not implicitly inserted into the current kernel. FFTs remain internal and integer based, without FFTW.

0.4 update: the butterflies described here remain unchanged. Equivalent Microsoft syntax `src/dsp/arm/fft_neon.asm` is added, selected by CMake ASM_MARMASM for Windows ARM64. Native verification of this assembler still needs execution. The waterfall correction concerns GUI history and diagnostic transport, not the FFT algorithm.
## Original NGene comparison and SSE2 update — 16 October 2026

The reproducible [benchmark](../../benchmarks/fft/README.md) now measures EVO against the untouched
original NGene FFT supplied separately. It records source hashes, validates the compiled baseline,
uses the same `/N` gain, alternates backend order and reports repeated batch statistics. The
original is still faster on the tested Intel i7-1355U; this update makes no claim that EVO is the
fastest available FFT. EVO latency improves by about 35–38% at 1024, 4096 and 16384 points versus
the previous EVO implementation. The raw before/after CSV files retain host, compiler and flags.

The x86-64 implementation now fuses its first two stages, executes four butterflies per main
iteration, validates input headroom with SSE2 and copies complete complex pairs during bit reversal.
The four-butterfly path uses `S=(U&T)+((U^T)>>1)` and `D=S-T`, which are exactly
`floor((U+T)/2)` and `floor((U-T)/2)` for signed16 values without saturating the sum before scaling.
The scalar and NEON paths retain their signed32 sum/scaling implementation; all outputs are unchanged.

Packing Q14 products before averaging is lossless in the public headroom contract. Initially the
complex norm is at most `16384*sqrt(2) < 23171`. A quantized Q14 rotation has norm at most
`1+1/16384`; component floor rounding adds norm less than `sqrt(2)`. If `M_s` bounds a complex
norm after stage `s`, product rounding followed by per-stage floor averaging gives
`M_s <= (1+1/32768)*M_(s-1) + 3*sqrt(2)/2`. Induction yields
`M_s < 23171+4*s <= 23227` through all 14 stages. A rotated lower operand consequently has
norm less than `(1+1/16384)*23227+sqrt(2) < 23230`, safely inside signed16. SSE2 `packssdw`
therefore does not clip these products, and the average/difference identity is bit-exact against
the signed32 oracle. This bound depends on plan-generated twiddles and the enforced input range;
it is not permission to feed arbitrary full-range int16 samples to the FFT.

The updated tests additionally exercise full-scale independent I/Q signs, odd values and rotating
corners that drive valid intermediate components above 16384. The public API, `/N` gain, rounding,
immutable-plan concurrency and input/output ownership remain unchanged. Original NGene still has
different high-level clipping behavior and unsafe shared register backup, so these timing results
do not establish numerical interchangeability in acquisition or tracking.

## Eight-point codelet, permutation and subtree scheduling

The next SSE2 update supersedes the preceding performance comparison: the production path now
fuses widths 2/4/8 in an eight-point register codelet, gathers bit-reversed inputs in contiguous
output quartets and completes independent subtrees of at most 512 points before global stages.
These apply the small-codelet and depth-first locality techniques discussed by
[Frigo and Johnson](https://www.fftw.org/fftw-paper-ieee.pdf); implementation and coefficient
arithmetic remain internal. The [benchmark report](../../benchmarks/fft/README.md#eight-point-codelet-update)
provides same-process frozen/current comparisons, raw CSV, hashes and the isolated cache-blocking
experiment. Latency falls about 39–44% at 1024/4096/16384 against the preceding EVO version on the
tested x86-64 host. The original NGene remains competitive for smaller sizes; the new EVO is
faster at 16384 in these measurements.

This is fusion of the existing scaled radix-2 graph, not a reassociation of its arithmetic.
Widths 2 and 4 use exact W=1/-j sign/permutations, so every resulting component remains within
inclusive ±16384. The signed16 floor-average identity computes the same `/2` at each stage.
Width 8 uses precisely the plan's Q14 eighth-root coefficients (11585 for the diagonal
components); `pmaddwd` and signed shift perform the original product floor before averaging.
Its rotated components have magnitude at most 23171, so packing before the stage average is
lossless. The final averages fit signed16. A single `/4` replacing two floors, or negating a
previously floored noninteger product, would not satisfy this contract and is not used.

Bit reversal is an involution: gathering `b[i]=a[reverse(i)]` gives the same permutation as the
reference scatter. For a quartet starting at an index divisible by four, its reversed inputs
are `r`, `r+N/2`, `r+N/4`, `r+3N/4`. Below a subtree boundary the butterfly groups are disjoint;
completing one subtree first changes no data dependency or rounding boundary. The scalar oracle
keeps the original scatter and breadth-first traversal. The expanded regression checks 13,122
small codelet cases, every plan/subsize combination, odd/full-scale values, unaligned guarded
storage and input immutability, alongside the existing independent DFT and concurrency checks.
MASM, Windows GAS and the explicit scalar builds pass; Linux/macOS assembly objects compile.
ARM arithmetic is unchanged. Public headroom, normalization, immutable plans and ownership stay
the same, and no receiver-wide CPU-utilization reduction is inferred from the FFT microbenchmark.

## NEON and 32-bit backends — 17 October 2026

The production ARM FFT now uses `src/dsp/arm/fft_neon_intrinsics.c` on both AArch64 and
ARMv7 with NEON. Intrinsics generate native integer SIMD instructions and let the compiler
manage the target ABI and its different register sets. The original AArch64 `.S`/`.asm`
files remain available as frozen benchmark kernels; CMake selects the new implementation.
32-bit x86 now uses `src/dsp/x86/fft_sse2_32.c`. The existing x86-64 assembly is retained.

All optimized targets use the eight-point initial codelet, contiguous quartet bit-reversal
gather and independent subtrees of at most 512 points. The NEON loop processes four complex
butterflies per iteration instead of two. Deinterleaved input/coefficient loads feed widening
integer multiply/accumulate instructions directly, avoiding duplicated complex lanes and
pairwise reduction chains. Input headroom validation is vectorized on ARM as well.

### Numerical contract

Output must remain **bit-identical to `NgFftReference`**, including odd negative intermediates,
both Q14 rails and every stage's separate floor division by two. The scalar oracle's arithmetic,
scatter permutation and breadth-first schedule are preserved.

NEON `vmull`/`vmlal` compute the signed32 Q14 products exactly. The existing norm bound
(`|component| < 23230`) proves that narrowing after the 14-bit arithmetic shift loses no bits.
Signed halving add/subtract (`vhadd`/`vhsub`) retain the carry/sign bit of the 17-bit sum before
the arithmetic division by two. These are the same floors as the oracle's signed32 operations;
neither saturating addition before division nor rounded multiply-high instructions are used.
The width-eight constants remain exactly `0`, `±11585` and `±16384`. No floating-point arithmetic
is introduced during execution. The intrinsic semantics are specified in the
[Arm Neon reference](https://arm-software.github.io/acle/neon_intrinsics/advsimd.html).

The x86-32 kernel uses the same SSE2 product and exact floor-average identities as x86-64,
with a register budget suitable for eight XMM registers. CPU capability is checked once when
creating a plan; unsupported SSE2 selects the scalar FFT. The dispatcher/oracle translation
unit uses `/arch:IA32` or `-mno-sse2`, while only the optimized translation unit enables SSE2.
This is an FFT fallback guarantee; other application components and the OS/toolchain retain
their own CPU requirements. ARM32 builds explicitly require NEON hardware and a compatible
softfp/hard-float ABI; non-NEON machines need `NGENE_FFT_REFERENCE=ON`.

### Validation and measurement scope

Native Windows x86-32 MSVC and Clang FFT tests pass for all powers of two from 8 to 16384,
all 78 plan/subsize pairs, the 13,122-case eight-point corpus, all 64 independent input/output
natural-alignment combinations at every size, input immutability, guarded buffers, invalid
components in initial/final vector lanes, independent DFT/tones and shared-plan concurrency.
A separate Win32 test reruns the full contract with a false SSE2 probe and aborting SIMD
entry points to verify dispatch. The Release Win32 receiver and its integration contracts
also build and run; x86-64 FFT, analysis and controller regressions pass after the shared changes.

The ARM kernel cross-compiles for Linux ARMv7/AArch64, Windows ARMv7/ARM64 with Clang and
Apple ARM64. These object checks establish compiler/ISA compatibility, not native ARM speed.
Native MSVC ARM32 requires an older supported toolset; current toolsets may provide ARM64
intrinsics only, and the configure-time feature check reports unavailable NEON headers.
The new `linux-armv7.cmake` toolchain and Linux x86-32/ARMv7 CI jobs provide reproducible
complete builds and contract execution where those toolchains and QEMU are installed.

The optional [`test_fft_arm_emulated.py`](../../tests/test_fft_arm_emulated.py) also runs the
actual compiled NEON instructions with Unicorn 2.1.4. ARMv7 and AArch64 each pass **17,140**
cases against the production scalar oracle: all transform lengths and plan/subsize pairs,
64 independent input/output alignments, guarded storage, input immutability, invalid initial/final
vector lanes, analytic tones and the complete 13,122-case eight-point corpus. The preceding
AArch64 assembly and execution source from commit `0dd9729f40b2837de496b9a5765ed5f0323410a2`
pass the same 17,140-case harness separately.
The freestanding harness supplies externally generated immutable tables, byte copying, integer
division and error return; it does not exercise native plan allocation/trigonometric generation
or the OS thread ABI. These emulator runs establish numerical equivalence for the tested inputs;
they provide no ARM hardware latency or throughput measurement.

Native Win32 benchmarks use the same full-call, alternating-order methodology as above.
The previous 32-bit backend was **C-reference**, so its speedup must not be described as a
comparison with original NGene SSE2 or with the previous ARM64 assembly. Raw runs and exact
compiler/source hashes are in the [FFT benchmark documentation](../../benchmarks/fft/README.md).
Native ARM before/after timings are still required before quoting an ARM speedup; emulated
instruction counts or timings do not establish a hardware performance result.
