# Project status and implementation principles

Last reviewed: 2026-11-16
Repository baseline: `062ecdc` (`PVT test 2`), plus the current scheduling and validation changes

This is the concise, current project handoff for contributors. The roadmap contains
the accepted direction; subsystem documents contain detailed contracts and evidence.
Historical increment reports describe only the revisions and environments they name.

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
matched the previous decisions within each tested mode. F4 will profile
the FFT stages that dominate the float and int32 coarse rows.
[F3 verification](../../development/profiling/F3/README.md).

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
