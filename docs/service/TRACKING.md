# Tracking integration

## Delivered scope

The `NGene::Tracking` library consumes native CI8/CI16 common-stream windows. GPS
L1 C/A and Galileo E1-B PRNs 1..32 have independent coherent accumulators, carrier
and code NCOs, FLL/Costas PLL and power DLL. GPS integrates one primary-code period,
Galileo four milliseconds with the same BOC(1,1) replica used by EVO acquisition.
This is a tracking loop integration, not the complete Classic tracking supervisor:
GPS bit synchronization, Galileo I/NAV, PLL power qualification and bounded loss/reacquisition are integrated. Complete C/N0 supervision, false-lock/BOC side-peak correction, NCO HOLDOVER and verified fast reacquisition remain pending. A running PLL is not a qualified
lock, decoded navigation message or usable observable.

## Classic provenance and adaptations

Reviewed Classic `tracking/tracking.c`, `trackingDiscriminatorsLoops.c`,
`trackingDataStructure.h`, `trackingHoldover.c`, and
`sampleConfigurationFiles/configuration/Tracking.conf`, along with LOGBOOK and
OPTIMIZATION_REPORT. Original discriminator/loop authorship is retained in source.

Retained in this increment:

- FLL discriminator `-atan(cross/dot)/(2*pi)` and Costas PLL `-atan(Q/I)/(2*pi)`,
  including zero-denominator behavior and data-polarity ambiguity.
- DLL normalized Early/Late power difference: GPS factor 0.5, Galileo 1/6.
- Second-order recurrence, damping 1/sqrt(2), natural frequency B/0.53, DLL
  bandwidth 2 Hz, carrier bandwidths 15 Hz, gains 1 (PLL gain scaled by milliseconds).
- Exponential carrier smoothing of length 20, a 100-ms FLL convergence check and
  a 1-Hz transition criterion. This flag selects PLL; it does not certify lock.

Numerical adaptations:

- Native complex `exp(-j phase)` mixing replaces real IF packed globals. Spectrum
  inversion is corrected before rotation. Carrier correction sign is reversed
  relative to Classic's IF quadrature convention.
- Taps use signed replica delay in chips: negative delay is Early (advanced local
  replica). Code-loop application sign is reversed relative to Classic's code-buffer
  offset convention. Both signs have convergence tests from positive/negative errors.
- Double NCO phase and double coherent accumulation; native ADC levels are not
  quantized to signs. Existing reusable SIMD float mixing/sign/product kernels run
  on bounded private scratch. No new SIMD backend is claimed.
- Capture code phase is propagated using acquisition Doppler aiding to the current
  sample index. Absolute carrier phase is unknown at acquisition and starts at zero;
  Costas tracking resolves it modulo the data-polarity ambiguity.
- First partial code period is discarded. Integrations end at code wraps, across
  arbitrary engine windows, with fractional sample rounding; no sample is reused.
- Any epoch change, index gap or GAP flag invalidates the channel and its coherent
  history. A new confirmed capture is required. This conservative reset is not Classic
  HOLDOVER or a claim of recovery through missing RF samples.

Acquisition search decisions, stages and thresholds were not changed. The display
picker fix only remembers each constellation's automatically selected search map.
Classic already contains optional multiple correlators; EVO exposes a bounded,
explicit chip-delay geometry rather than retaining its sample-pointer arrangement.

## Project boundaries and replacement

`include/ngene/tracking.h` is independent of controller, web server and devices.
`src/tracking/tracking.c` owns algorithms and private scratch. `NGene::Tracking`
links DSP only. `NGENE_TRACKING_SOURCE` selects alternative source implementations
of the public API at CMake configuration time. There is no dynamic plugin ABI.

`src/tracking/bank.c` allocates bounded receiver slots per enabled constellation,
not a tracking channel for every detected PRN. Default capacities follow Classic's
sample GNSS_RX.conf: eight GPS slots and eight Galileo slots. Optional EGNOS L1
uses an auxiliary tracking channel outside the position-channel pool.
Counts are independently configurable in 0..32. A slot retains its channel ID when
assigned a PRN and when recycled after discontinuity. Waiting, acquisition and
tracking are independently represented; the same PRN cannot occupy two tracking
slots. Completed acquisition transfers a free slot to tracking. Assigned PRNs are
excluded from subsequent searches, and search admission pauses when capacity fills.

All tracking slots share one fixed worker pool (four workers by default), private
NCOs/scratch and an immutable borrowed engine window. GPS and Galileo share that
same worker budget. One barrier completes all tracking before capture copies or
new search admission. With no active tracking slots, idle DSP jobs are not dispatched.
There is no per-satellite thread or independent sample cursor. Workers never publish
to controller/GUI state directly. The channel transition validator lives in the
base library; the reusable dispatcher lives in the platform library to avoid a
tracking/engine dependency cycle. On timeline discontinuity the owner resets slot
registrations before dispatching, then invalidated slots return to acquisition;
no samples are bridged and no HOLDOVER behavior is claimed.

Acquisition still has one sequential strategy worker per enabled constellation,
with its own frozen capture. Scheduling masks are mutex-protected, applied between
satellites; a single-satellite search already in flight finishes unchanged. Classic's
4/3/2/1 concurrent-acquisition limits are upper bounds: two constellation workers
already stay below the limits through 16 tracked satellites. Above 16, GPS retains
the sole new admission while it has free slots. At most one Galileo attempt runs
while GPS is filling, as in Classic. The single-satellite search, refinements,
confirmation and thresholds are unchanged. CLI and service install the same slot
allocator. All memory, thread and job resources are prepared before frontend start.

Controller progress/final callbacks copy telemetry under the controller mutex.
HTTP and WebSocket serialize copies, never live workspace pointers. Memory and
telemetry are bounded to 64 slots and 33 taps per channel. UI history retains at most
240 received snapshots per channel, timestamps them with sample indices/Fs, and
breaks lines over missing telemetry/integrations. It is a sampled display, not a
lossless export of every integration.

## Configuration and display

```ini
[tracking]
enabled=true
gps_channels=8
galileo_channels=8
workers=4
square_carrier=false
xor_correlation=false
tap_count=9
spacing_chips=0.25
```

Count must be odd, 3..33; adjacent spacing is positive and at most one chip.
The middle tap is Prompt. Adjacent taps are Early/Late and feed DLL; extra taps
are observational and never feed detection/lock decisions. With more than three
taps, changing tap count at fixed spacing does not change DLL geometry.

Carrier and product choices are independent, frozen while running, persisted in
INI and editable in browser Tracking settings. Precise sinusoidal rotation and
SIMD products are defaults. Fast sign XOR is exact for the +/-1 replica and
preserves amplitude. Fast square carrier is explicitly approximate, with harmonic
images; it is experimental and has no demonstrated RF sensitivity/lock accuracy.
There is no automatic downgrade in response to CPU load.

The Channels view shows clickable slot status and selects a GPS/Galileo channel and shows measured tap points
versus chip delay and signal time. Magnitude, I and Q can be selected. The dashed
GPS triangle is an ideal reference, never synthesized measurements. Galileo BOC
is not represented as a triangular reference. FLL/PLL text reports the running
loop, without fabricated C/N0, navigation eligibility or PVT fixes.

## Reproducible validation

Windows x64, Clang 22.1.3, CMake 4.3.1, Ninja, Release, strict compiler diagnostics.
Build directory: `build/tracking-validation`. Native SDR adapters disabled for this
validation. A separate Ninja build was used because the installed Visual Studio
instance was incomplete at the initial validation; this is not acceptance of the native MSVC presets.
The local build supplies `_CRT_SECURE_NO_WARNINGS` and an explicit existing dumpbin
path, required by the host's incomplete developer environment.

- `ctest --test-dir build/tracking-validation --output-on-failure`
- `NGENE_TEST_TRACKING=1 node tests/web_acquisition.cjs`
- Add `NGENE_TEST_GALILEO=1` for the mixed signal/browser test.
- `node tests/web_workspace.cjs`

Browser test environment uses `NGENE_EXECUTABLE` for the built receiver,
`PLAYWRIGHT_MODULE` for the existing local Playwright installation and
`BROWSER_EXECUTABLE` for installed Edge. Test-generated RF is confined to tests.

Deterministic C cases cover arbitrary 37-sample splits versus whole windows,
CI8/CI16 amplitude scaling, all four independent arithmetic choices, invalid tap
geometry and discontinuities. One-second GPS and Galileo fixtures include initial
+/-2-Hz Doppler and +/-0.05-chip errors, data transitions, negative digital IF and
both spectrum orientations. Their final Doppler errors are below 0.5 Hz; this is a
synthetic assertion, not an accuracy specification. Galileo uses the independent
legacy bipolar PRN7 fixture. Browser checks exercise real controller/acquisition/
tracking handoff, persisted nine-tap geometry, plots, component selection and mobile
layout. No physical RF, recorded RF tracking, ARM or MSVC acceptance is established.

Validation results: 15/18 CTest cases passed on the initial invocation; three host
setup failures (PowerShell execution policy and nested fixture generator discovery)
were rerun in a process-local developer environment with Ninja in PATH and passed.
Both PowerShell helper suites also passed explicitly. GPS plus tracking and mixed
GPS/Galileo plus tracking browser runs passed, including restart in Galileo-only
mode. The nine-workspace regression passed. No host-wide execution policy was changed.

## Throughput and channel lifecycle correction, 2026-11-13

The first integration's serial bank and per-sample/per-tap double `fmod` replica
loops were not suitable for high-rate multi-channel operation. Input conversion
now selects the CI8 loop once per block; wider formats keep native signed levels.
Double code phases are computed once per chunk using SSE2/ARM64 NEON lanes, then
reused across every tap. Replica generation uses SIMD wrap/index operations and
preexpanded Galileo BOC half chips, preserving the previous double wrap rounding.
There is no Q32 phase approximation, sign-only ADC quantization or scalar `fmod`
inside the sample/tap loops. The carrier generator, complex mixer and correlator
continue to use their existing SIMD paths on x64. Double coherent accumulation
and independent fast/precise choices are retained. ARM64 replica intrinsics are
implemented but no ARM execution result is established here.

The isolated probe in `benchmarks/tracking/` is excluded from normal builds. One
Windows x64 Release run (same machine, generated deterministic samples, precise
carrier/products, half GPS and half Galileo) measured:

| Slots | Fs | Taps | Workers | Signal duration | Elapsed |
|---|---|---|---|---|---|
| 8, original serial implementation | 16 Msps | 3 | 1 | 50 ms | 187.916 ms |
| 8, updated kernels | 16 Msps | 3 | 1 | 50 ms | 45.018 ms |
| 8, updated kernels and pool | 16 Msps | 3 | 4 | 50 ms | 20.462 ms |
| 8, original serial implementation | 16 Msps | 9 | 1 | 50 ms | 533.264 ms |
| 8, updated kernels | 16 Msps | 9 | 1 | 50 ms | 99.562 ms |
| 8, updated kernels and pool | 16 Msps | 9 | 4 | 50 ms | 46.991 ms |
| 16, updated kernels and pool | 16 Msps | 3 | 4 | 200 ms | 135.454 ms |
| 16, updated kernels and pool | 16 Msps | 9 | 4 | 200 ms | 279.854 ms |
| 16, updated kernels and pool | 2.4 Msps | 9 | 4 | 200 ms | 53.098 ms |

Serial/parallel checksums match for the same input/configuration; the eight-slot
50-ms cases also match the saved pre-change checksums. These are isolated tracking
measurements, not whole-receiver deadlines: USB, acquisition, analysis, recording,
OS contention and thermal effects are excluded. In particular, 16 slots with nine
taps at 16 Msps remain above real time on this machine; no live guarantee is made.
Do not compensate by silently reducing precision, dropping samples or enlarging
buffers. The existing stop-on-overflow policy remains intact. Device enumeration
reported zero HackRF devices, so the user's physical ingress overflow has not been
retested here.

Regression coverage now also includes SIMD-vs-original replica parity at exact
chip/wrap/BOC boundaries and odd tails; bounded GPS/Galileo slot allocation sharing
one pool; no duplicate PRNs; paused acquisition at capacity; and reuse of the same
slot identities after a gap. Browser tests click waiting and tracking slots and
verify detailed plots. The mixed test capture is four seconds long so Galileo's
search need not finish before the artificial one-second loop reset of the former
fixture. This changes the test input duration, not acquisition decisions.

Final validation of the slot/pool/SIMD correction: Windows x64 Clang/Ninja Release
with native HackRF support, existing local HackRF SDK and runtime staging:
**21/21 CTest cases passed** in `build/tracking-hackrf`, including the mocked native
HackRF contract. The production build has `NGENE_TRACKING_BENCHMARK=OFF` and no
additional profiling in the receiver. The mixed GPS/Galileo and Galileo-only browser
scenario passed with clickable waiting/tracking slots and nine-tap plots, as did
the nine-workspace regression. Executable: `build/tracking-hackrf/ngene.exe`.
The hardware enumeration test passed with zero devices; it is not an RF reception
or USB-throughput acceptance result. Existing native MSVC build resolution was
reported by the user separately and is not reclassified as a tracking RF result.

## Channel configuration update, 14 November 2026

The receiver uses one total pool. For fixed six GPS and four Galileo slots set receiver.channels=10, gps_reserved=6 and galileo_reserved=4. For twelve automatic slots set channels=12 and both reservations to zero. receiver.auto_priority is 0 for completion order, 1 for GPS preference and 2 for Galileo preference among competing confirmed results for shared slots; existing assignments are retained. Galileo spacing is now presented and saved as tracking.galileo_spacing_slots, one slot equal to half a chip. Legacy chip values remain accepted on import; internal phase and SIMD arithmetic retain chip units. BeiDou uses the same
spacing as Galileo by default. Set tracking.beidou_spacing_chips to a positive
value in Tracking to override it. An enabled EGNOS receiver appears as
auxiliary channel 65 independently of GNSS slot reservations.

The standalone search diagnostics workspace has been removed. Channel detail retains correlator plots with Galileo delay in slots. Selection-dependent search computation and immutable acquisition maps remain pending. Live HackRF evidence reproduces unqualified Galileo PLL states with lost BOC peak consistency; this update does not establish an RF tracking fix. See WORKPLAN_CHANNELS_GALILEO_2026-11-14.md for the continuing work and evidence.

## GPS data demodulation, 14 November 2026

GPS coherent epochs now feed a private navigation module directly within the tracking job. Five verified sign-transition intervals at multiples of 20 ms establish bit timing, retaining Classic sample configuration and the low PLL quality criterion. Twenty native double prompt sums yield each data bit; loop integration remains 1 ms. Classic parity masks, D30 polarity correction and ten-word subframe validation yield HOW TOW telemetry. This is a partial navigation port: ephemeris decoding, Galileo I/NAV, observables and PVT are not yet integrated. Channel Navigation (TOW) text denotes message validation, not navigation eligibility for a position solution. TOW expires after a missing subframe; gaps/reassignment clear all fragments. SIMD tracking kernels remain unchanged.

Deterministic tests exercise prompt-to-bit timing, both polarities, parity corruption, loss of PLL qualification and stale TOW. HackRF live: GPS12 yielded four valid subframes in 30 seconds, last ID4, HOW578544 s. No position or accuracy validation follows from these counts.

## Current increment, 16 November 2026

GPS uses Classic exponential PLL powers (length100), five transition intervals,
word parity and full-subframe TOW. Galileo uses terminated K7 Viterbi, CRC24Q and
adjacent even/odd pages. The per-satellite broadcast cache survives cold slot
reacquisition; FLL or timing loss clears framing/TOW without erasing ephemerides.
PLL entry/retain metrics0.8/0.4, checks100ms, FLL4s/PLL8s deadlines and a1s
relock timeout prevent an invalid PLL flag occupying a slot forever. The complete
Classic correction/holdover supervisor remains work; these gates are not that port.

The receiver batches source fragments smaller than a native millisecond before
the tracking worker barrier. It copies exact native bytes into bounded preallocated
storage; larger windows remain borrowed directly. Both subscriptions deduplicate
the same input. Epoch/gap metadata invalidates pending history, and final partial
windows flush. Tests compare37-pair fragments with2048-pair windows bit for bit.

See PERFORMANCE_NAVIGATION_2026-11-15.md for the observed RTL recovery/PVT and
remaining stream-continuity limits. Historical sections above retain their dates.

### Configurable loop filters

EVO recalculates DLL, FLL and PLL coefficients from per-signal bandwidth and
gain when it assigns a channel. The default settings reproduce the previous
recurrences and preserve the GPS, Galileo, BeiDou and EGNOS coherent periods.
GPS, Galileo and EGNOS use the Classic coefficient formula. BeiDou scales its
existing proportional and integral terms relative to the default profile.

Project note from the user. The loop coefficients published by Kai Borre
require independent derivation and validation. Maintaining lock under some
conditions does not validate those coefficients. Keep the current formula as
a compatibility baseline until the filter design is qualified.

GPS can extend PLL and DLL coherent integration to 2, 4, 5, 10 or 20 ms.
The loops first acquire at 1 ms and extend only over aligned groups within a
synchronized LNAV bit. Demodulation and observations retain their original
1-ms cadence. The default remains 1 ms. The optional C/N0 cutoff uses valid
estimates in the existing qualified-lock failure window and defaults to off.
EGNOS has independent tap spacing and shares the configured tap count.
