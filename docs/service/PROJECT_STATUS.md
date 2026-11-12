# Project status and implementation principles

Last reviewed: 2026-11-12  
Repository baseline: `dev` at `e59627f` (`BladeRF bug is fixed`)

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

Galileo E1-B acquisition for PRNs 1..32 is integrated in the controller, CLI, INI
and browser alongside GPS, with independent enable/PRN settings and a Signal
selector for results and synchronized search-space views. Synthetic service/browser
validation and the shared wrong-Doppler noise policy are established; no recorded
RF replay/live result is established. See [the API scope and limits](GALILEO_ACQUISITION.md).

This is **not yet an end-to-end GNSS receiver**: tracking, navigation-message decoding,
observables, and PVT/PNT are not integrated. SBAS acquisition and multibit
correlation kernels are also not established as implemented capabilities. In
particular, native SC16 Q11 transport preserves its ADC precision, but does not mean
all downstream GNSS channel kernels support that format. See
[ARCHITECTURE.md](ARCHITECTURE.md), [MULTISDR_064.md](MULTISDR_064.md), and
[NATIVE_IQ_PERFORMANCE.md](NATIVE_IQ_PERFORMANCE.md) for precise boundaries.

The current engineering baseline is the `dev` commit above. The next work is not
assigned to a release: use [ROADMAP.md](ROADMAP.md) to choose and validate the next
increment, beginning with GNSS processing beyond acquisition. The roadmap is
directional, not evidence that planned work has started or passed hardware validation.
The latest committed code milestone is the bladeRF RX fix in `e59627f`.

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
