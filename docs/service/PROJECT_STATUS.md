# Project status and implementation principles

Last reviewed: 2026-11-16
Repository baseline: `062ecdc` (`PVT test 2`), plus the current scheduling and validation changes

This is the concise, current project handoff for contributors. The roadmap contains
the accepted direction; subsystem documents contain detailed contracts and evidence.
Historical increment reports describe only the revisions and environments they name.

The [GNSS design review](CONSOLIDAMENTO_GNSS.md) records how Classic acquisition,
per-cell detection, coherent dwell, data/pilot timing and real-time scheduling
map onto the current code. Its actions are integrated into the existing B and F
increments in [the incremental plan](PIANO_INCREMENTALE_EVO.md).

## Current state

NGene EVO is a portable C11 GNSS receiver foundation with native HackRF, RTL-SDR,
bladeRF RX0, and fixed-profile SiGe inputs; file replay; shared-stream recording;
browser diagnostics; and modular DSP and channel interfaces.

GPS L1 C/A acquisition is implemented for PRNs 1–32. It preserves the established
coarse search, Doppler refinement, code refinement, and 3-of-4 confirmation sequence.
The numerical and I/Q adaptations, limits, and validation are documented in
[GPS_ACQUISITION.md](GPS_ACQUISITION.md). Acquisition results are not tracking locks
or navigation fixes.

Galileo E1-B acquisition for PRNs 1..32 and GPS L1 C/A share an adaptive worker
pool. Capacity follows available processors and free channel slots; cooperative
stage permits also limit already running jobs. Tracking/PVT precede acquisition.
Small source windows are grouped into an owned native millisecond for tracking,
amortizing worker barriers without resampling or changing arithmetic. Diagnostics
pause while foreground processing catches up, independently of OS priorities.

Native acquisition checks the coarse FFT grid against the opened stream
before workspace allocation. Galileo FFT size 16384 requires at least
4.096 Msps, including rates supplied by replay metadata.
[F2a verification](../../development/profiling/F2a/README.md).

GPS and Galileo compress each capture once and allocate FFT scratch for the
selected arithmetic. Float mode skips integer baseband quantization. The
Windows benchmark measured lower preparation time for the tested float
profiles and lower memory for all arithmetic modes.
[F2b verification](../../development/profiling/F2b/README.md).

Integer PCPS uses magnitude shifts for spectrum packing while preserving
signed truncation. Windows Q14 searches were faster at one worker and
matched the previous decisions within each tested mode.
[F3 verification](../../development/profiling/F3/README.md).

On x64, the native int32 FFT uses SSE2 butterflies from 2048 complex points.
The Windows Release benchmark matched the scalar output bit for bit and
measured a 30–53% reduction in full int32 PCPS search time on its GPS and
Galileo fixtures with one and four workers. The scalar path covers smaller
sizes and the default ARM build. The opt-in NEON candidate matched the scalar
reference in emulation. Native performance qualification is pending.
[F4 verification](../../development/profiling/F4/README.md).

The opt-in x86-64 AVX2 int32 FFT matched the baseline bit for bit at all
12 sizes on Windows. The paired PCPS bench measured a median reduction
of 15% for GPS and 24% for Galileo at one worker. The macOS ARM64 job
collects paired NEON timings on its next run.

The opt-in x86-64 AVX2 Q14 FFT matched SSE2 and scalar outputs on Windows.
The direct FFT bench measured lower median time from 128 to 8192 points.
PSD and full PCPS results depended on size and worker count.
[Q14 verification](../../development/profiling/F4/README.md#incremento-avx2-q14).

The F4 FFT package retains wide, compact fixed-shift and compact adaptive
kernels, including the x64 AVX2 candidates. Compact adaptive AVX2 matched
the scalar output and reduced FFT stage time by about 69% on Windows.
The INT32 path completed more work in scheduled replay. Compare kernels
under the same arithmetic contract and measure packing and scale restoration
before attributing processing cost to the sample width.
[F4 kernel package and measurements](../../development/profiling/F4/README.md#kernel-int8-adattivo-avx2-e-pacco-librerie).

The experimental F4 scheduler can borrow an existing worker and a free
workspace for a second Doppler row. Windows replay matched the published
decisions for common jobs and recovered about 14 MiB of working set versus
dedicated helper workspaces. Throughput and tracking latency varied under
measured foreground load.
[F4 workspace reuse](../../development/profiling/F4/README.md#riuso-del-workspace-e-del-worker-nello-scheduler).

F4 qualification is closed on Windows with the direct precise INT32 path
as the operating profile. The final replay matched published decisions on
common jobs, including with identical tracking work. Doppler-row reuse
and the compact kernels remain available for experiments. Linux and macOS
validation is recorded as pending native work.
[F4 qualification](../../development/profiling/F4/README.md#chiusura-della-qualifica-f4-su-windows) ·
[Native test matrix](../../development/profiling/F4/qualification-native.md).

BeiDou B1C B0 extends the acquisition runner and tracking cache to PRN 63
and a pool with three subscriptions. B1 adds independent data and pilot
primary replicas, verified offline for all 63 PRNs per component. B2 compares
a 16384-cell SIMD grid and native block correlation on synthetic pilot data.
Native Doppler and code refinement recover all injected delays in the fixture.
The reduced grid shows delay-dependent peak loss, so threshold and RF/replay
qualification remain open. Secondary-code support for Galileo and BeiDou is
deferred to a future plan.
[B2 acquisition lab](../../development/profiling/B2/README.md).
B3 verifies pilot BOC(1,1) phase continuity and Early/Prompt/Late on arbitrary
offline window partitions and exercises closed-loop convergence with Doppler
ramps, multiple signal levels and a prompt-driven FLL to PLL transition.
The lab also exercises fade recovery, gap reset with an injected new seed,
and half-chip seed rejection. Offline C/N0 moments agree within 1.124 dB
on the AWGN fixture. Strong coherent echoes can bypass the guard veto.
The closed-loop two-ray test reproduces false PLL locks on strong echoes.
Adding two guard taps does not improve those cases. Qualification of the
tracked code delay on RF remains open.
B4 adds a B-CNAV1 decoder with streaming symbol framing and sample-anchored
frame output, plus a synthetic SIMD I/Q data bridge. The operational
tracker feeds the data prompt to this decoder on replay.
[B-CNAV1 decoder](BEIDOU_BCNV1.md).
B5–B7 adds matched B-CNAV1 broadcast caches, BDT code-time observation
building, and a three-system QR PVT with independent code biases. All seven
constellation combinations are exercised on synthetic observations. Real-signal
accuracy of the code, Doppler and mixed solution remains under investigation.
[BeiDou broadcast and PVT](BEIDOU_PVT.md).
[B3 tracking lab](../../development/profiling/B3/README.md).
[BeiDou incremental plan](PIANO_INCREMENTALE_EVO.md#6-beidou-b1c--incrementi-approvabili).

B8 adds a repeatable, out-of-process replay matrix with input hashes, cold
starts and an explicit continuity break. Windows replay covers the existing
GPS/Galileo chain. B10 exposes confirmation, tracking, decoding and PVT use as
independent states and exercises BeiDou PRN 63 in a labelled mixed-constellation
preview. The receiver configuration also enables the B1C signal on live
or recorded input. The shared channel pool accepts B1C acquisition results,
and the tracking data prompt feeds B-CNAV1. Healthy, matched BDT observations
are eligible for the mixed PVT solve. B8 recovered three-system fixes after
a replay gap. Against the LNIKS antenna reference, the RTL-SDR cold runs
gave 12.67 and 12.64 m 3D RMS on fixes using BeiDou, and the post-gap run
gave 256.76 m on the earlier campaign. On the current Windows revision,
the same replay recovered 27 three-system fixes after one gap with zero
sample loss. 3D RMS was 7.36 m and 59.91 m in the two cold runs and
41.30 m after the gap. A direct local BOC(1,1) reference
compared the actual acquisition outputs for C32 and C12. Residuals of
about 0.63–0.66 chip were already present at the acquisition reference
sample in two historical RTL assignments, one before and one after the
gap. The independently correlated B1C data and pilot components agree
on these discrepancies. Both assignments reached PLL. The code pull-in
envelope and absolute pseudorange accuracy remain open. B9 is deferred
while the antenna is unavailable. B10 passed the three Windows browser
regressions on the same build. Native Linux and macOS qualification
remains pending.
[BeiDou analysis and test plan](PIANO_INCREMENTALE_EVO.md#piano-operativo-di-verifica-beidou-b1c).
[B8–B10 evidence](../../development/profiling/B8/README.md).

The broadcast PVT uses GPS Klobuchar, Galileo E1 NeQuick G and BeiDou B1C
BDGIM when the required coefficients are available. The receiver profile
can disable these corrections and the PVT view reports their per-system
counts. The optional EGNOS L1 receiver can search GEO PRNs 121 and 136.
Enable it under Tracking in the receiver profile. GPS PVT uses fresh fast
range and long-term orbit/clock corrections with matching broadcast IODE.
The PVT view reports the GEO, accepted frames and corrected GPS observations.
Windows x64 replay of the 27 November RTL-SDR recording accepted frames
from both GEOs. Two replay windows produced fixes with one GPS observation
corrected by PRN 121 and two GPS observations corrected by PRN 136. Synthetic CI8 replay independently exercised PRN 121 from
acquisition through CRC validation. GPS L1 can also use MT18/MT26 delays
when fresh grid points enclose the GPS L1 pierce point, at latitudes up to
75 degrees. Unavailable or unsupported cells fall back to the configured
broadcast model. Replay from the start of the 27 November RTL-SDR capture
produced a GPS fix with two differential corrections and one ionospheric
correction from GEO PRN 121. EDAS/SISNeT is deferred. [Ionosphere models](BEIDOU_PVT.md#ionosfera-broadcast-a-frequenza-singola)

The F6 mixed GPS/Galileo replay matrix measures the existing tracking pool
and acquisition permits at fixed precision. More acquisition permits reduced
observed contention and advanced Galileo confirmation on the test capture at
higher CPU cost. Exact event parity and per-job deadlines remain open.
[F6 replay](../../development/profiling/F6/README.md).

The development build can trace acquisition jobs by constellation, epoch,
first sample and PRN. In one 30-second int32 replay pair, the 46 decisions
with matching keys agreed between SSE2 and AVX2. Admission and deadline
qualification remain open.

The F5 offline GPS bench includes a 16-level carrier and a Q15 mixer with
byte rescaling. The new candidates remain experimental. The bench verifies
phase continuity and measures prompt deflection and saturation.
[F5 offline bench](../../development/profiling/F5/README.md).

Channels is a stable table with numeric C/N0 and horizontal bars. Total slots,
GPS/Galileo reservations and automatic preference are configurable. Only the
selected channel computes optional taps/current search display; confirmed maps
are archived after seed publication. Galileo spacing is expressed in half-chip
slots. Static module-status dots have been removed.

Classic GPS LNAV parity and Galileo I/NAV deinterleaving/Viterbi/CRC decoding feed
per-satellite consistent broadcast caches. Common-epoch code/Doppler observations
feed a double-precision C pivoted-QR broadcast least-squares PVT, with separate
GPS/Galileo biases, Earth rotation and Hopfield troposphere. The PVT view includes
numerical coordinates, a map, GDOP/HDOP/VDOP and an actual satellite skyplot. This
is **partial Classic PVT integration**: per-satellite carrier smoothing and GPS LNAV
Klobuchar corrections are present; full weighted/adaptive Kalman processing,
Galileo/EGNOS ionosphere and integrity/outlier qualification remain pending.

PLL power qualification and bounded loss/reacquisition now release failed slots;
parsed broadcast data survives reacquisition, but fresh decoding must establish
TOW. Complete Classic false-lock correction, NCO holdover and verified fast
reacquisition remain pending. A PLL convergence flag alone is not a qualified fix.

A 65-second RTL E4000 physical recording has zero reported software gaps/drops,
but simultaneous losses of GPS correlation also reproduce in an isolated tracker.
The recovery replay reaches mixed PVT with six GPS and two Galileo satellites;
continuity is not yet established, and RTL hardware-internal loss is unmeasured.
The user reports that HackRF is connected to the PC and an antenna (17 November
2026). Live RF performance and navigation accuracy still require a measured run.
See [the measurements and limits](PERFORMANCE_NAVIGATION_2026-11-15.md) and
[the saved work state](WORKPLAN_CHANNELS_GALILEO_2026-11-14.md).

Acquisition now runs under a signal-time CPU budget (`acquisition.cpu_budget`,
default 1 core). Tracking uses a fused front end and chip-run correlation that keep
the decisions of the previous kernels. The search FFT is radix-2^2. In a 16 Msps
replay, process CPU fell from 3.8 to 1.25 cores with 4 workers. With 1 worker the
receiver thread went from 81 % to 22 % busy. This is replay evidence only; HackRF
real-time operation remains unverified. See
[the real-time measurements](PERFORMANCE_REALTIME_2026-11-16.md).

x64 SSE2 and AArch64 NEON cover tracking phases/replicas, carrier, rotation,
product/XOR correlations and the floating search FFT. ARM64 execution was checked
under emulation against independent arithmetic/DFT; ARM physical performance and
Linux/macOS runtime remain unverified. Separate Q14 FFT backends are unchanged.
SBAS decoding and the remaining Classic PNT/authentication features are not implied
by this first broadcast PVT pipeline.

## Implementation principles

1. **Preserve user intent and GNSS decisions.** Do not silently change the algorithm,
   stage ordering, search decisions, confirmation logic, or thresholds. Keep those
   decisions separate from numerical changes required by a different native sample
   format, sample rate, IF, or device.
2. **Precision before apparent availability.** A greater number of detections or fixes
   is not proof of greater accuracy. Preserve mixer, correlator, tracking, and PVT
   precision; evaluate scheduling and load distribution before considering
   approximations. An approximation requires explicit approval, a stated error budget,
   and comparison with an appropriate reference.
3. **Make contracts and provenance explicit.** Specify units, scaling, sample indices,
   time/epoch behavior, ownership, buffer lifetime, concurrency, error behavior, and
   supported formats. Keep hardware-specific transport in adapters and preserve the
   shared-stream architecture.
4. **Fix causes, not symptoms.** Trace failures to the responsible layer; surface
   actionable diagnostics rather than generic success-shaped fallbacks. Prefer focused
   changes and existing abstractions over parallel implementations or unrelated cleanup.
5. **Validate claims at the right level.** Add deterministic regression tests for
   changed behavior; run relevant build/tests; use replay to assess recorded data; and
   separately label physical-device/RF validation. A passing fixture or replay is not
   a claim of live receiver performance or navigation accuracy.
6. **Keep the project memory useful.** Update this status when delivered capabilities,
   limitations, or priorities change. Update subsystem documents with design rationale
   and reproducible evidence. Use Git history for individual changes; do not duplicate
   every prompt or commit in this status file.

## Where to continue

- [ROADMAP.md](ROADMAP.md): accepted requirements, delivered increments, and planned
  direction.
- [GPS_ACQUISITION.md](GPS_ACQUISITION.md): acquisition decision sequence, numerical
  adaptations, and verification limits.
- [ARCHITECTURE.md](ARCHITECTURE.md): module, dataflow, and ownership contracts.
- [CODING_STANDARD.md](CODING_STANDARD.md): C and documentation conventions.
- [README.md](README.md): index of engineering decisions, implementation records, and
  validation evidence.
