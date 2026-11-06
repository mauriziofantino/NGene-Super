# Gain calibration and block DC removal

6 November 2026. This describes the current implementation and verification, rather
than acceptance of an SDR/antenna combination.

## Calibration ownership and selection

The HTTP request starts a worker and returns a job ID. Status polling and cancellation
use that ID; a stale cancellation cannot stop a newer job. The service keeps serving
status, diagnostics and shutdown while calibration owns the SDR. Receiver start,
configuration apply and device probing reject conflicting requests immediately.
Shutdown cancels and joins the worker before destroying the controller or its locks.

Each gain setting captures three 32768-pair native ADC observations, with settling
samples discarded before each. RX stops before any histogram or moment calculation.
The ingress queue has 128 blocks of 32768 pairs (about 262 ms at 16 Msps). This gives
transport scheduling headroom; it is not a guarantee against host/USB stalls.
Acquisition waits check cancellation at most every 200 ms; device open/stop latency
still depends on the driver. The exhaustive HackRF sweep can test up to 384 settings.
Moving it to a worker does not reduce that maximum scan duration.

Every window must satisfy the existing ADC-distribution bounds:

| Criterion | Accepted range |
| --- | --- |
| Per-component AC RMS, relative to signed full scale | 0.06 through 0.32 |
| Native components within the outer 3% of full scale | Less than 0.001 |
| Gaussian error (kurtosis deviation plus 8 times maximum CDF error) | Less than 0.9 |
| I/Q standard-deviation imbalance | Less than 0.25 |

HackRF VGA branches stop at the first overloaded setting. A rejected third window
cannot be hidden by median aggregation. Rejection counts report settings and can
overlap. The selected score uses distance from 0.12 RMS, Gaussian error, imbalance,
relative DC amplitude and native near-rail occupancy.

The previous hard DC test multiplied relative offset by the square root of sample
count. It could reject a small, stable DC offset solely because a longer observation
made its mean statistically significant. That gate has been removed. DC remains a
score penalty proportional to amplitude; native headroom checks remain in place.

This is an ADC-loading suggestion. It does not measure GNSS C/N0, sensitivity or
position accuracy, and the 0.12 RMS target is a heuristic. A valid suggestion only
updates the unchanged originating browser draft. It never saves automatically.
Cancellation, failure and rejected results preserve the existing gains.

## DC arithmetic and placement

`NgIqCenterBlock` follows Classic's two-pass approach: exact integer sums, a rounded
mean for each component, then widened subtraction. It has SSE2, ARM64 NEON and scalar
paths, supports CI8/CU8/SC16 Q11/CI16, and accepts at most 262144 pairs. Rounded ties
go away from zero. Output is int32 so even extreme centered CI16 values do not clip.
There is no recursive estimate, IIR state, gain normalization or input modification.

Calibration histograms and distribution measurements use this centered output.
Clipping is checked against the original native ADC levels. Diagnostic histograms,
statistics and FFT input also use centered output; histogram axes widen if needed.
The UI reports removed offsets and residual DC separately. Stream endpoint counts
remain based on original values and are not presented as proof of physical ADC clipping.
No DC policy or conditioning selector is exposed in the UI. Historical conditioning
profile keys stay ignored for compatibility.

Subtraction occurs within owned diagnostic/calibration buffers. The common stream,
ring and recordings retain their original values. Spectrum and histogram lanes have
independent observations/cadences, so their estimated offsets need not be identical.
A constant desired component at DC is removed with the offset. Integer rounding can
leave a residual mean of up to half an input level; the FFT DC bin is not overwritten.

## Verification

Native Windows x64 MSVC Release, all SDR adapters enabled: all 21 CTest cases pass.
The DC checks compare SIMD output with an independent scalar formula for all four
formats, unaligned storage, tails, maximum block length and extreme values. Native
input remains unchanged. Analysis tests cover constant removal, nonzero-frequency
tones, post-subtraction histograms and preservation of input endpoint counts.
Calibration fixtures verify all-three-window acceptance, third-window overload,
RX stopped during statistics, and cancellation from another thread during capture.

Browser checks with Edge and Playwright use the actual HTTP server and embedded UI:

- `tests/web_frontend_stream.cjs`: signed stream/replay metadata, retired DC controls,
  fixed SiGe profile and RF plots. Live SiGe is optional and was not enabled here.
- `tests/web_rf_views.cjs`: all eight RF-view combinations, idle disabled canvases,
  shared waterfalls, all-off FFT demand, independent histogram updates and mobile layout.
- `tests/web_calibration.cjs`: responsive status, conflicting-command guards, progress,
  stale cancellation, cancel/reject/failure gain preservation, accepted draft-only updates,
  and shutdown during calibration. This uses deterministic measurements in the optional
  `ngene_web_calibration_fixture` target, never linked into the delivered application.

Build that optional target explicitly with tests, web and HackRF support enabled, then
set `NGENE_EXECUTABLE` to its path for `tests/web_calibration.cjs`. Set that variable to
the normal receiver for the other browser tests. `PLAYWRIGHT_MODULE` and
`BROWSER_EXECUTABLE` can select locally installed Playwright and Edge/Chromium.

These checks do not validate gain choices on live RF hardware. Linux compilation,
ARM64 NEON execution and sustained on-device USB captures still need native verification.
