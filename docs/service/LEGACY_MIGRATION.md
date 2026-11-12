# NGene is the basis of the project

The project is an incremental evolution of NGene. The original archive provides the functional and algorithmic basis: before implementing a GNSS function, inspect the corresponding existing module, identify reusable parts, and explain the adaptations required.

The first increment introduces the infrastructure needed for the I/Q path and portability. Its operational code is new; the original GNSS algorithms are not connected yet. This status does not reduce the original code to a mere example.

## Verified reference

The two supplied archives, ngene.tar and ngene(2).tar, are byte-for-byte identical, with a size of 28,467,200 bytes and SHA-256:

    c15c72e9e715e6a77ee6ccf694adb3e2d939a355b1f6ad3ff4e82121de8e1acb

The original archive remains separate from the first-increment package and is preserved unchanged. The working manual contains the inventory and previous analysis. Paths below are relative to the archive's ngene/ root.

## Initial migration map

| NGene basis | Destination/responsibility | Status and required work |
|---|---|---|
| receiver/ngene.c, receiver/channelManager.c | src/core/, channel API | First I/Q engine implemented. Progressively recover useful assignment, scheduling, and control logic; adapt offsets to the new common advancement. |
| frontEnd/circularBuffer.c and its header | src/stream/ | New complex ring implemented. Preserves the shared-stream principle; leases and gaps are explicit. |
| frontEnd/ | src/frontend/ | Native HackRF implemented. Retired drivers are not included in the new target. Additional native adapters will follow. |
| asmFolder/arrays.c, asmFolder/fft.c and other textually included kernels | src/dsp/ and include/ngene/dsp/ | Internal FFT already adapted and verified, with reference and SSE2/NEON kernels. Separate mathematical library for transforms, statistics, and power. Mixers, replicas, and correlations still need migration with explicit numerical contracts. |
| acquisition/ | Future acquisition module | Start from the existing coarse search, refinements, confirmation, and noncoherent sums. Separate state, workspace, and sample access. |
| tracking/ | Future tracking module | Migrate GPS/Galileo, discriminators, loops, and accumulators. Adapt mixers and correlators to I/Q; add the agreed holdover and reacquisition. |
| utilityGnss/ | Future signal, time, and navigation services | Selectively recover codes, parsing, and message decoding. Separate shared data and per-channel state; check individual file provenance. |
| warmStart/ | Future assistance service | Recover useful visibility and assisted-start logic; add a cache, previous position, and defined update sources. |
| pvt/ | Future observables and PVT/PNT solution | Recover useful receiver time, measurements, and solver after fixing common epochs and validating the I/Q chain. |
| osnma/, chimera/ | Future cross-cutting modules | Retain in the inventory: being outside the first increment does not mean removing the functionality. Decide integration and dependencies during migration. |
| guiSupport/, utility/ | Future control, diagnostic, and output services | Separate GNSS computation from the interface and operating system; select reusable code by responsibility. |
| libraries/, embedded external components | Tracked dependencies | Evaluate use; preserve external licenses and attribution when transferring sources. |

Future destinations describe responsibilities, not already implemented modules or final ABIs.

## Rules for every migration

1. Identify files, functions, numerical assumptions, and dependencies in the NGene basis.
2. Define what to keep, adapt, or replace and record the reason. Replacing an implementation does not automatically change the algorithm.
3. Separate algorithms and per-channel state from the ring, drivers, operating system, and CPU backend.
4. Check against reference cases and RF replay when available. Moving from IF to I/Q changes the operations: do not require byte-for-byte comparisons of mathematically different quantities.
5. Integrate into the new engine and update this map with evidence and limitations.

FAST/PRECISE, integer FFT, acquisition, tracking, and PVT are neither blindly rewritten nor presumed already ported. SIMD kernels start from NGene operations and optimizations, checking which assumptions remain valid for multibit input. Historical performance remains a reference, not a mandatory threshold.

## FFT migration — v0.3.0

`asmFolder/fft.c`, `fft.h`, and the SSE2 macros in `simd.h` were analyzed for the new `src/dsp/fft/fft.c` kernel with separate SSE2/NEON butterflies. Radix-2, bit reversal, complex signed16, and Q14 twiddles are retained; register management, ABI, headroom, and scaling position change. This is not a wrapper around the old source, nor a replacement already verified in acquisition/tracking. See the [detailed analysis](FFT_REVIEW.md) and numerical tests.

## Galileo E1-B acquisition API — 2026-11-12

Classic `asmFolder/prnTable.c`, `carrierTable.c`, `acquisition/{acquisition,coarseAcquisition,refinement,confirmation}.c` and `Acquisition.conf` were reviewed.
The E1-B primary table is losslessly repacked for PRNs 1..32. The worker uses
4-ms periods, eight powers and 3-of-4 confirmation. Floating BOC-replica PCPS and
private frozen captures replace packed pre-wipe FFT processing and advancing
hypotheses; see [contracts and limitations](GALILEO_ACQUISITION.md). GPS stages
and grids are retained; only its confirmation noise probes change to wrong Doppler.
The Galileo channel API and controller/CLI/INI/GUI integration are implemented.
Separate result/map archives and a Signal selector mirror the GPS workflow. Full PRN
coverage, acquisition ambiguity tests and RF validation remain pending.
