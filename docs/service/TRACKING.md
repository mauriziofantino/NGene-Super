# Tracking integration

## Delivered scope

The `NGene::Tracking` library consumes native CI8/CI16 common-stream windows. GPS
L1 C/A and Galileo E1-B PRNs 1..32 have independent coherent accumulators, carrier
and code NCOs, FLL/Costas PLL and power DLL. GPS integrates one primary-code period,
Galileo four milliseconds with the same BOC(1,1) replica used by EVO acquisition.
This is a tracking loop integration, not the complete Classic tracking supervisor:
C/N0 qualification, bit synchronization, false-lock/BOC side-peak checks, HOLDOVER
and verified fast reacquisition are not ported. A running PLL is not a qualified
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

`src/tracking/bank.c` composes a borrowed acquisition channel with 32 private PRN
workspaces. All workspaces consume the same borrowed engine window, synchronously;
none has a ring cursor or retains its samples. The acquisition worker still owns
its frozen capture. Confirmed results are copied under its existing snapshot lock
and seeded on the receiver thread, once per capture. Channels already active are
not restarted by repeat detections. CLI and service both install this composition.
The current bank runs its satellites sequentially; per-satellite pool dispatch and
tracking-aware acquisition budgets remain scheduling work, not implemented claims.
All allocations occur before reception; no allocation or I/O is added per window.

Controller progress/final callbacks copy telemetry under the controller mutex.
HTTP and WebSocket serialize copies, never live workspace pointers. Memory and
telemetry are bounded to 64 PRNs and 33 taps per channel. UI history retains at most
240 received snapshots per channel, timestamps them with sample indices/Fs, and
breaks lines over missing telemetry/integrations. It is a sampled display, not a
lossless export of every integration.

## Configuration and display

```ini
[tracking]
enabled=true
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

The Tracking view selects a GPS/Galileo channel and shows measured tap points
versus chip delay and signal time. Magnitude, I and Q can be selected. The dashed
GPS triangle is an ideal reference, never synthesized measurements. Galileo BOC
is not represented as a triangular reference. FLL/PLL text reports the running
loop, without fabricated C/N0, navigation eligibility or PVT fixes.

## Reproducible validation

Windows x64, Clang 22.1.3, CMake 4.3.1, Ninja, Release, strict compiler diagnostics.
Build directory: `build/tracking-validation`. Native SDR adapters disabled for this
validation. A separate Ninja build was used because the installed Visual Studio
instance remains incomplete; this is not acceptance of the native MSVC presets.
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
