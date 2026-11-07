# Gain calibration and fixed diagnostic DC removal

Updated 7 November 2026. This describes the current implementation and verification, rather
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
still depends on the driver. The HackRF sweep can test up to 384 settings.
Moving it to a worker does not reduce that maximum scan duration.

Every window must satisfy the existing ADC-distribution bounds:

| Criterion | Accepted range |
| --- | --- |
| Per-component AC RMS, relative to signed full scale | 0.06 through 0.32 |
| Native components within the outer 3% of full scale | Less than 0.001 |
| Gaussian error (kurtosis deviation plus 8 times maximum CDF error) | Less than 0.9 |
| I/Q standard-deviation imbalance | Less than 0.25 |

HackRF exhausts distinct physical states without pruning at the first overload.
A rejected third window cannot be hidden by median aggregation. Rejection counts
report settings and can overlap. The loading score uses distance from 0.12 RMS,
Gaussian error, imbalance, relative DC amplitude and native near-rail occupancy.
The model-specific priority described below chooses the split before that score
compares different total-gain/RF-amp groups.

The previous hard DC test multiplied relative offset by the square root of sample
count. It could reject a small, stable DC offset solely because a longer observation
made its mean statistically significant. That gate has been removed. DC remains a
score penalty proportional to amplitude; native headroom checks remain in place.

This is an ADC-loading suggestion. It does not measure GNSS C/N0, sensitivity or
position accuracy, and the 0.12 RMS target is a heuristic. A valid suggestion only
updates the unchanged originating browser draft. It never saves automatically.
Cancellation, failure and rejected results preserve the existing gains.

## Implemented device strategies, 7 November 2026

Stopped-device discovery now identifies HackRF board ID/revision, RTL tuner type,
and bladeRF board family. Identity and policy are copied into owned capability
storage and exposed in the capability/calibration HTTP responses and calibration
status. The probed serial pins all capture attempts to the same selected device.
Probing HackRF does not start RX or change gains, tuning or antenna power.

| Device policy | Calibration behaviour |
| --- | --- |
| HackRF MAX2837 | Measure all 384 settings, including both RF-amp states. |
| HackRF MAX2839 | Apply the firmware's LNA/VGA compensation map, deduplicate equivalent states and measure all 252 distinct settings. |
| HackRF MAX2831 | Collapse legacy LNA aliases into three physical states and measure all 192 distinct settings. |
| RTL R82xx, FC001x | Identify the tuner and sweep every distinct reported manual gain through its native allocation. Automatic mode is not an alternative fixed gain configuration. |
| RTL E4000 | Sweep the native LNA/mixer list and all 54 realizable IF totals (3..56 dB): 756 settings with the stock 14-entry list. Allocate each IF total to earlier stages first. Within each accepted equal RF+IF total group prefer more RF gain, then compare group representatives by loading score. |
| bladeRF 1 / 2 | Identify the board, query the tuned device's gain limits and sweep its overall manual gain through the native allocation/table. |
| Unknown models, RTL FC2580, SiGe, files | No gain calibration. Unknown/no-op policies fail before starting capture; SiGe/files remain excluded. |

For HackRF, compare physical relative LNA+VGA gain at a fixed RF-amp state. Within
each group, accepted configurations outrank rejected ones and larger early-stage
gain outranks the loading score. Thus 32/8 can beat 8/32 even if its ADC score is
slightly worse; all three windows still have to pass. After choosing each group's
representative, compare representatives using the loading score. Recompute the
global winner whenever a representative changes, avoiding an order-dependent
pairwise comparison. Exact inter-group score ties have a deterministic ordering.

The state maps use firmware `v2026.01.3` semantics and gains relative to each RFIC's
lowest LNA state. They are not calibrated antenna gains or measured noise figures.
This is an early-gain preference under the stated loading checks. RF amplifier
on/off remains a separate measured comparison; its gain is frequency dependent.
ADC histograms alone do not rule out compression inside an earlier RF stage.
Native bladeRF allocation is retained instead of imposing a generic LNA/VGA rule.

E4000's overall tuner gain setter leaves its six IF stages unchanged; the driver's
default `[6,0,0,0,9,9]` is only 24 dB IF. A weak input can therefore fail at every
reported tuner gain. `rtlsdr.if_gain_db` now persists the selected manual IF total;
`-1` preserves driver defaults. The adapter programs all six stages with native
tenth-dB units after tuning and selecting manual RF gain. Other tuners and automatic
mode do not receive these calls. A failed stage application aborts the open and
closes the device. The GUI exposes IF only after identifying an E4000.

IF stage values are `[-3,6]`, `[0,3,6,9]`, `[0,3,6,9]`, `[0,1,2]`,
`[3,6,9,12,15]`, `[3,6,9,12,15]` dB, from the
[native E4000 driver](https://github.com/osmocom/rtl-sdr/blob/v2.0.2/src/tuner_e4k.c).
The search uses one earlier-stage-first allocation per total, rather than measuring
all 2400 IF splits. This is a noise-allocation preference, not a measured NF model.
The same three-window acceptance gates remain in force; a rejected result does not
change the profile. Tests reproduce an input around 0.4% ADC RMS with the default
IF that becomes acceptable with more analog IF gain, and check persistence and
native application. Live RF performance still requires hardware validation.

Tests cover direct split-ranking cases in both visitation orders, replacement of
the global winner, rejection of a higher-LNA overloaded split, and a complete
capture/statistics/sweep fixture where the preferred split has a worse DC penalty.
They also verify 384/252/192 distinct-state counts, native manual RTL/bladeRF gain
selection, unsupported models, serial pinning, third-window rejection and cancellation.
Native adapter fixtures verify identification, owned capability data and cleanup.
The real HTTP/browser fixture verifies model/policy presentation along with existing
progress, cancellation, conflicting-command and draft-only update behaviour.
These deterministic tests do not replace live RF noise/linearity validation.

## Historical gain-search audit before implementation, 6 November 2026

The following audit describes the earlier selection, superseded by the implemented
policies above. It is retained to explain the reported defect and the research.

| Frontend | Actual search | Equal-total-gain stage combinations |
| --- | --- | --- |
| HackRF | RF amp off/on, IF/LNA 0..40 in 8 dB steps, VGA 0..62 in 2 dB steps; each VGA branch stops at its first overloaded setting | Different LNA/VGA splits are visited until pruning, but are not grouped or ranked by noise figure |
| RTL-SDR | Automatic mode and every manual gain in the tuner's reported list | Internal stage combinations are delegated to librtlsdr, not enumerated by NGene |
| bladeRF | Every reported overall manual RX gain from minimum to maximum at the reported step | `bladerf_set_gain` delegates stage allocation to libbladeRF; NGene does not enumerate stage gains |
| SiGe | Calibration is unavailable | Fixed hardware profile |

Thus HackRF is a pruned grid search, not strictly exhaustive over its 384 possible
settings. The stop assumes increasing VGA will remain overloaded; a single transient
in any one of the three windows can prune the remainder of that branch. `tested`
counts observations, three per completed setting, rather than distinct configurations.

`Consider` chooses an acceptable result before a rejected one, then the smallest
ADC-loading score. Exact score ties keep the first setting visited. For example,
amp-off LNA=0/VGA=40 precedes LNA=40/VGA=0, despite their equal nominal total gain.
There is no explicit preference for placing gain earlier in the chain, noise-figure
model, calibrated noise measurement, or comparison at equal measured total gain in
NGene's selection function. This does not mean the native drivers lack their own
stage-allocation strategies; the device-specific review below documents them.

Gaussian shape and RMS loading do not establish receiver noise figure. A lower-noise
configuration can receive a worse loading score if its RMS lies farther from the
heuristic target. In a linear cascade, a Friis calculation requires each stage's
noise factor and power gain, including preceding losses; the configured dB gains
alone are insufficient. Real mixer, filter, external LNA and overload behaviour also
need to be represented. This audit does not change the gain-search algorithm or
claim that it minimizes total noise figure.

Primary references: [HackRF gain controls](https://hackrf.readthedocs.io/en/latest/setting_gain.html),
[libbladeRF gain API](https://www.nuand.com/libbladeRF-doc/v2.6.0/group___f_n___g_a_i_n.html),
and [Analog Devices receiver noise-figure analysis](https://www.analog.com/en/resources/technical-articles/system-noisefigure-analysis-for-modern-radio-receivers.html).

## Device-specific strategy review, 6 November 2026

These are the source findings and original proposals used for the implementation
above. More extensive RFIC controls mentioned here remain future work where stated.
The inspected release inputs match the local Windows
build: HackRF `v2026.01.3`, libbladeRF `libbladeRF_v2.6.0`, and librtlsdr `v2.0.2`.
An installed SYSTEM provider can differ. No new live RF/NF measurements were made.

### HackRF: identify the actual revision and transceiver

[Official component documentation](https://hackrf.readthedocs.io/en/latest/hardware_components.html)
distinguishes MAX2837-based One boards, the MAX2839-based One r9, and the MAX2831-based
Pro. The [RX guide](https://hackrf.readthedocs.io/en/latest/setting_gain.html) starts with
RF amp off and IF/VGA at 16/16, then suggests adjusting IF and VGA roughly together.
That is a practical noise/linearity starting point, not a statement that the stages
are equivalent. The [MAX2837](https://www.analog.com/media/en/technical-documentation/data-sheets/max2837.pdf)
and [MAX2839](https://www.analog.com/media/en/technical-documentation/data-sheets/max2839.pdf)
datasheets specify different noise figures for different RF gain states and provide
noise-versus-baseband-gain curves under chip evaluation conditions. Those numbers
must not be presented as the complete HackRF noise figure at its antenna connector.

Firmware details materially affect a device policy:

- [MAX2837 mapping](https://github.com/greatscottgadgets/hackrf/blob/v2026.01.3/firmware/common/max2837.c):
  the six legacy LNA requests select distinct gain states; VGA selects its separate control.
- [MAX2839 mapping](https://github.com/greatscottgadgets/hackrf/blob/v2026.01.3/firmware/common/max2839.c):
  the firmware adds 3 dB to VGA to compensate output scale and reallocates LNA/VGA
  where the chip lacks legacy LNA states. Some mappings also depend on requested VGA.
- [MAX2831 mapping](https://github.com/greatscottgadgets/hackrf/blob/v2026.01.3/firmware/common/max2831.c):
  legacy LNA requests 0/8 select the same low state, 16/24 the same middle state,
  and 32/40 the same maximum state. Sweeping all six as independent physical gains
  duplicates settings; summing the legacy labels is not a physical gain calculation.

Proposed policy: detect board ID/revision, use its physical gain-state map, deduplicate
aliases, and compare equal-gain splits within the same RF-amp state. Among acceptable
splits prefer the lower-noise stage allocation supported by that model; retain a
linearity constraint instead of unconditionally maximizing LNA. Compare RF amp on/off
as separate measured states because its actual gain depends on frequency. On r9 the
physical map must include firmware compensation. On Pro use its three actual LNA states.

### bladeRF 1: the native driver already distributes gain

[Nuand's configuration guide](https://www.nuand.com/libbladeRF-doc/v2.6.0/boilerplate.html)
recommends increasing RX LNA, then RXVGA1, then RXVGA2 when controlling stages directly.
The [inspected driver](https://github.com/Nuand/bladeRF/blob/libbladeRF_v2.6.0/host/libraries/libbladeRF/src/board/bladerf1/bladerf1.c)
implements a more specific allocation: LNA up to its middle state, RXVGA1, remaining
LNA, then RXVGA2, with corrections for RXVGA2's discrete step. RXVGA2 steps are 3 dB
in the source, so a generic one-dB-per-stage grid would be wrong.

Proposed policy: retain `bladerf_set_gain` as the normal allocation strategy, sweep
the queried overall range, and read back stage settings when reporting the selected
configuration. Direct stage search would be a separate policy requiring explicit
stage limits. The earlier audit's lack of an NGene NF model does not imply that this
driver allocation is absent: Evo already reaches it through `bladerf_set_gain`.

### bladeRF 2 / AD9361: preserve the frequency-dependent gain table

The inspected [RFIC defaults](https://github.com/Nuand/bladeRF/blob/libbladeRF_v2.6.0/fpga_common/src/ad936x_params.c)
use the full gain table and disable digital gain. The driver exposes the RX `full`
stage and applies a board/frequency gain offset. The [AD9361 manual, pp. 37–38](https://www.analog.com/media/en/technical-documentation/user-guides/AD9361_Reference_Manual_UG-570.pdf)
describes full tables that jointly allocate LNA, mixer, TIA and filter gains for noise
figure and linearity. Split tables can help with out-of-band blockers, but require
separate control and appropriate overload information. A table index is not generally
a dB value, and digital gain does not improve SNR.

Proposed policy: tune first, query the actual overall limits, and use the native full
table in manual mode while selecting ADC loading. Evo already tunes before querying
its gain range. Do not substitute a generic LNA/VGA split or silently load custom
tables. A blocker-aware split-table policy needs dedicated RFIC support and validation.

### RTL-SDR: distinguish tuner and driver capabilities

The [librtlsdr v2.0.2 implementation](https://github.com/osmocom/rtl-sdr/blob/v2.0.2/src/librtlsdr.c)
has different gain backends for different tuners:

| Tuner | Verified behaviour | Proposed policy |
| --- | --- | --- |
| R820T / R828D family | [Manual gain](https://github.com/osmocom/rtl-sdr/blob/v2.0.2/src/tuner_r82xx.c) fixes VGA at 16.3 dB and alternates LNA/mixer increments. Automatic mode uses another VGA setting. The public backend has no independent IF-gain callback. | Sweep the reported manual list through the native allocation. Do not assume generic LNA/VGA controls or compare automatic mode as a fixed physical split. |
| E4000 | The overall setter apportions LNA/mixer; a separate callback controls six IF stages. | The implemented strategy now searches RF gain and all canonical IF totals and persists/applies `rtlsdr.if_gain_db`, as described above. |
| FC0012 / FC0013 | Gain operations and automatic-mode support differ between these backends. | Query the list and identify which operations actually alter hardware; validate suitability at the requested RF before calibration. |
| FC2580 | Gain and gain-mode setters are no-ops; the reported list contains a placeholder zero. | Report gain calibration unavailable rather than interpreting successful API returns as adjustable gain. |

In this release, `rtlsdr_set_tuner_if_gain` can return success when the selected tuner
has no callback. Checking only its return value would falsely suggest support. Tuner
AGC and RTL2832 digital AGC are different controls. The [Blog V4 guide](https://www.rtl-sdr.com/v4/)
also documents device-specific driver requirements and an IF-AGC caveat for its ExtIO
use cases; that caveat must not be generalized to every tuner-AGC control in Evo.

### SiGe and unknown devices

The [implemented SiGe native profile](FRONTEND_STREAM_DECISIONS.md#native-sige-profile-and-windows-usb-findings)
has fixed hardware parameters and no gain controls. Keep calibration disabled; its
one-bit signs do not support the same amplitude-loading criteria as multi-bit ADCs.
For unknown models, use only verified native operations and describe the result as
an ADC-loading suggestion. Do not invent a stage map or noise figure.

### Integration requirements derived from these findings

The original capability API reported only gain bounds/lists. Model identification,
owned identity storage and policy dispatch are now implemented as described above.
Additional USB product detail, individual stage readback, runtime firmware-version
reporting and RFIC overload detectors remain possible extensions; they are not
claimed to be present merely because a vendor API can expose them.

Keep RF tuning, sample rate, bandwidth and external frontend state fixed during gain
measurements. Apply the existing loading,
distribution, I/Q and native headroom gates first. For devices with independent stage
control, choose the preferred physical split within each comparable total-gain/RF-amp
group, then compare the groups for loading. Restricting the preference to exactly equal
floating-point quality scores would usually fail to implement the intended criterion.
Internal RF compression can precede ADC clipping; the histogram alone does not prove
that every amplifier is linear. Device-specific detector/readback support should be
used where available. Validation must cover physical aliases, unsupported/no-op APIs,
preferred-split rejection on overload, and preservation of the native bladeRF policies.

## DC arithmetic and placement

`NgIqCenterBlock` follows Classic's two-pass approach: exact integer sums, a rounded
mean for each component, then widened subtraction. It has SSE2, ARM64 NEON and scalar
paths, supports CI8/CU8/SC16 Q11/CI16, and accepts at most 262144 pairs. Rounded ties
go away from zero. Output is int32 so even extreme centered CI16 values do not clip.
There is no recursive estimate, IIR state, gain normalization or input modification.

Calibration histograms and distribution measurements use this centered output.
Each gain setting has independent native observations; this must not reuse an offset
from a different hardware gain. Clipping is checked against the original native ADC levels.

Diagnostic DC is estimated only from the first complete observation of an input tap
in a receiver session. PSD and histograms of that tap share the fixed rounded estimate.
Later observations call `NgIqSubtractDc`, skipping the mean-estimation pass. Pre/post
compatibility taps keep separate estimates. A sample-format change re-estimates in the
new units; restarting the receiver creates a new estimator, including after changing
gain or tuning. Diagnostic settings, sample gaps and display pause/resume retain it.
The estimate's sample count is reported as `dc_estimate_pairs`. A short or nonstationary
initial observation can give an imperfect estimate; this is not a calibrated measurement
of hardware offset. There is no continuous temperature/drift correction.

Diagnostic histograms, statistics and FFT input use the offset-subtracted output;
histogram axes widen if needed.
Centered excursions can exceed the original ADC range. Diagnostic FFT input then
uses one extra bit of attenuation to satisfy its Q14 component bounds, restoring the
factor of four in power before Welch/temporal normalization. This avoids clipping or
silent missing PSD results; the original scaling remains for observations within range.
The UI reports removed offsets and residual DC separately. Stream endpoint counts
remain based on original values and are not presented as proof of physical ADC clipping.
No DC policy or conditioning selector is exposed in the UI. Historical conditioning
profile keys stay ignored for compatibility.

Subtraction occurs within owned diagnostic/calibration buffers. The common stream,
ring and recordings retain their original values. Spectrum and histogram lanes retain
independent observations/cadences while sharing the fixed offset for the same input tap.
A constant desired component in the initial estimate is removed with the offset.
Integer rounding leaves at most half an input level of mean in the estimating window.
Subsequent means can also reflect noise, changing input or hardware drift and are not
bounded by that rounding limit. They are reported honestly; the FFT DC bin is not overwritten.

## Verification

7 November 2026: native Windows x64 MSVC Release, all SDR adapters enabled:
all 21 CTest cases pass. The final extended calibration fixture also passes after
adding the end-to-end 32/8 versus 8/32 selection scenario. Results are retained in
`build/msvc-x64-multisdr/gain-policy-all-20261107.xml` and
`build/msvc-x64-multisdr/gain-policy-final-20261107.xml`.
The DC checks compare SIMD output with an independent scalar formula for all four
formats, unaligned storage, tails, maximum block length and extreme values. Native
input remains unchanged. Analysis tests cover constant removal, nonzero-frequency
tones, post-subtraction histograms, fixed-offset reuse across settings/gaps, shared
PSD/histogram offsets, and preservation of input endpoint counts. Four-format rail
fixtures also check widened histograms, fractional rounding residuals and normalized
FFT power for excursions beyond the original input range.
Calibration fixtures verify all-three-window acceptance, third-window overload,
RX stopped during statistics, and cancellation from another thread during capture.

Browser checks with Edge and Playwright use the actual HTTP server and embedded UI:

- `tests/web_frontend_stream.cjs`: signed stream/replay metadata, retired DC controls,
  fixed SiGe profile and RF plots. Live SiGe is optional and was not enabled here.
- `tests/web_rf_views.cjs`: all eight RF-view combinations, idle disabled canvases,
  shared waterfalls, full time-grid coverage before history reaches capacity, removed
  zoom controls, all-off FFT demand, independent histogram updates and mobile layout.
- `tests/web_calibration.cjs`: responsive status, conflicting-command guards, progress,
  stale cancellation, cancel/reject/failure gain preservation, accepted draft-only updates,
  and shutdown during calibration. This uses deterministic measurements in the optional
  `ngene_web_calibration_fixture` target, never linked into the delivered application.

The fixed-offset/full-band review reran the frontend-stream and RF-view browser checks.
Frontend-stream assertions now also verify that initial DC offsets and their sample count
remain unchanged across multiple published histograms. Display-only checks
`tests/web_plot_interactions.cjs` and `tests/web_waterfall_geometry.cjs` cover resize,
pause/clear, absent zoom controls, partial/empty/single-row history and 3D visibility at
every transient age/bin. The device-policy implementation reran
`tests/web_calibration.cjs` against the optional fixture with the actual HTTP worker
and embedded UI. It passes model/policy presentation, responsive progress, ownership
guards, cancellation, rejected/failed gain preservation, accepted draft-only updates
and shutdown. Browser evidence is in `build/calibration-ui-7brmnI/`. Native gain
selection and driver identity use their separate deterministic C fixtures, rather
than the scripted browser measurements. The earlier RF-view/DC results remain from
the 6 November review; this increment does not change their arithmetic or layout.

The subsequent E4000/plot correction on 7 November reran all 21 native MSVC Release
CTest checks (`build/msvc-x64-multisdr/frontend-fixes-20261107.xml`), both display-only
browser checks and all three real-service browser scripts above. The C calibration
fixture tests all 756 E4000 RF/IF candidates and a weak-to-acceptable input. The native
RTL adapter fixture checks all 54 IF totals, the six native calls and tenth-dB units,
default/automatic/other-tuner exclusion and failure cleanup. Browser calibration now
also checks E4000-only IF controls, accepted RF/IF draft values, saved IF gain and
automatic common precision. Evidence: `build/calibration-ui-kzDZ26/`,
`build/frontend-stream-ui-MVcqI9/`, `build/rf-views-validation-l5xNdn/`.
The updated plots have large default heights, no height/zoom controls and a fixed
44% vertical projection in 3D. The standalone geometry check passes every age/bin
and full-grid coverage for empty, partial and single-row histories.

Build that optional target explicitly with tests, web and HackRF support enabled, then
set `NGENE_EXECUTABLE` to its path for `tests/web_calibration.cjs`. Set that variable to
the normal receiver for the other browser tests. `PLAYWRIGHT_MODULE` and
`BROWSER_EXECUTABLE` can select locally installed Playwright and Edge/Chromium.

These checks do not validate gain choices on live RF hardware. Linux compilation,
ARM64 NEON execution and sustained on-device USB captures still need native verification.

## Calibration stop and RTL transport reuse, 7 November 2026

The 756-setting E4000 sweep previously reopened and initialized USB for every
setting. Live feedback showed 385 settings taking 523 seconds. The sweep now keeps
one opened RTL handle, stops and joins RX before changing gains, replaces ingress,
resets native USB buffers and restarts capture. Settling is performed once per
gain change, followed by the same three non-overlapping 32768-pair windows. No
candidates or acceptance gates are removed. The normal source remains one-shot;
only the private calibration path can prepare another capture on that handle.
Actual improved elapsed time has not yet been measured on the user's hardware.

The red **Stop calibration** control requests cooperative cancellation. It changes
to **Stopping** until capture and USB resources have been released. Cancel checks
occur before capture, after configuration and between bounded 50 ms acquire waits.
Stopped jobs do not apply a suggestion or save a profile. Native USB calls still
depend on their driver's completion; there is no unsafe thread termination.

All 21 MSVC Release tests pass (`stop-and-frontal-waterfall-20261107.xml`). Fixtures
verify one measurement open for all 756 E4000 settings, fresh ingress between
captures, rejection of gain changes during RX, and cancellation during RTL/HackRF
capture. The HTTP/browser fixture also stops a running E4000 job and verifies that
draft and saved gains are preserved (`build/calibration-ui-hkRhoj/`).

The accompanying display correction preserves the requested fixed frontal view:
18-degree elevation, zero azimuth, global height fit over visible history and
unchanged global colours. The time slices animate over 180 ms without adding or
changing observations. Source/browser tests check visible scrolling, full viewport
height for weak input, every transient age/bin, and idle disabled views. Evidence:
`build/frontal-waterfall-review/`, `build/rf-views-validation-RL0vOt/` and
`build/frontend-stream-ui-pGcyI2/`. The delivered test build is
`build/msvc-x64-multisdr/Review/ngene.exe`; the existing running Release service
was left untouched.

## Verification on the connected SDRs and 3D mesh, 7 November 2026

Windows Application Error/WER confirms an access-violation crash of the earlier
Release service, matching the reported `Failed to fetch`. That record alone does
not identify the faulty native call. Live measurements showed that repeatedly
stopping/restarting RTL asynchronous RX also dominated calibration time.

RTL calibration now keeps both the device and native asynchronous RX open across
the whole sweep, as supported by the driver's own
[rtl_tcp gain-command thread](https://github.com/osmocom/rtl-sdr/blob/v2.0.2/src/rtl_tcp.c).
Publication is gated with an atomic callback handshake before draining old queued
samples and changing gains. Two 32768-pair native buffers bound in-flight old-gain
data; those pairs plus the settling interval are discarded before the three
non-overlapping measurement windows. Publication is paused during statistics, so
the ingress cannot fill while they run. Completion, cancellation or failure joins
RX and closes USB. Ordinary receiver capture does not use this calibration mode.

`tests/web_rtlsdr_live.cjs` is explicitly opt-in (`NGENE_RTL_LIVE=1`). It tests
cancellation during capture, then a complete sweep while polling both calibration
and receiver status, checks that the profile was not saved, and reopens capabilities
to prove that USB was released. `NGENE_LIVE_SOURCE=hackrf` tests that adapter instead.

| Connected hardware | Complete measured search | Elapsed | Stop request to cancelled state | Result |
| --- | --- | --- | --- | --- |
| RTL-SDR E4000 | 756 RF/IF settings, 2268 windows | 89.62 s (90.37 s including HTTP polling) | 125 ms | Completed, accepted |
| HackRF One r9 / MAX2839 | 252 physical settings, 756 windows | 25.84 s (25.93 s including HTTP polling) | 68 ms | Completed, accepted |

Evidence: `build/rtl-live-WN37e8/calibration.json` and
`build/hackrf-live-eZ4iMn/calibration.json`, with service logs alongside. Neither
full sweep lost its HTTP service. These tests demonstrate transport and selection
behaviour on the attached boards, not calibrated RF noise figure or GNSS sensitivity.

The final waterfall uses an actual WebGL frequency/power/time triangle mesh,
depth testing, surface normals, directional lighting and time-profile outlines.
The camera has zero azimuth and 18-degree elevation. A front face joins the most
recent spectrum to the base, avoiding a hollow appearance under raised carriers.
All bins and observations remain in the mesh. The palettes, vertical legends,
initial global range fit and animated scrolling follow the supplied instrument
reference. The full scene is fitted without height/zoom/orbit controls.

`tests/web_plot_interactions.cjs` optionally accepts `NGENE_LIVE_ENDPOINT`: it opens
a read-only diagnostic-interest WebSocket, copies real published PSD frames into
the current UI and saves both frames and screenshots. It does not change the
existing receiver profile or stop its capture. The connected RTL's actual data
was checked this way in `build/fnirsi-live-rtl-review/`. The pure geometry contract
checks actual height/depth, unchanged frequency positions, full viewport fit and
retention of all input values; browser tests exercise the actual WebGL renderer.
All 21 native CTest contracts also passed (`live-sdr-and-mesh-20261107.xml`).

Final embedded-asset browser runs passed with the mesh implementation:
`build/calibration-ui-i7s60J/`, `build/rf-views-validation-9rDnKZ/` and
`build/frontend-stream-ui-HBHe9K/`. The source-asset interaction check also verifies
visible animated motion, fixed frontal camera, triangle rendering, independent
view switches, full-band history and mobile layout. The optional live view check
captured 40 actual RTL PSD observations without changing or stopping the user's
running receiver. The updated executable is in `build/msvc-x64-multisdr/Review/`;
the actively receiving Release process prevented replacing its loaded binaries.

## Stable scrolling, gain priority and histogram axes, 7 November 2026

The subsequent motion review identified three independent causes of the reported
up/down vibration: redistributing partial history over the full time axis, resetting
an unfinished animation at each arrival, and refitting the 3D camera from changing
data extrema/front-floor values. Both views now use the selected history capacity
from the first frame. The oldest known edge fills unused history without increasing
the actual observation count. A shared scroll accumulator retains pending movement
across arrivals, including updates faster than the animation duration. Pause freezes
that accumulator and resume continues from the same position.

The 3D projection is now constant. Boundary relief tapers smoothly at the pane edges
while retaining true height/depth in its interior. The height gain is bounded so an
unchanging sample moves monotonically along time for every permitted power level.
There is no dynamic camera fit or moving front floor. `web_waterfall_geometry.cjs`
checks these invariants; `NGENE_MOTION_TEST=1` in `web_plot_interactions.cjs` tracks
one isolated feature in the actual rendered pixels through initial filling,
overlapping updates, changing outliers and full history. The 132 recorded samples
show 202.5 px forward in 2D and 196 px forward in 3D, with zero backward movement
in either view. Pixel samples and a browser-recorded side-by-side video are in
`build/waterfall-motion-review/`.

A direct audit of the connected HackRF r9 is recorded in
`build/hackrf-gain-probe-20261107.csv`. At the same physical relative total 57 dB,
legacy requests 0/62, 24/38, 32/30 and 40/22 produce different output noise/loading;
the higher-LNA cases were rejected as weak. At a higher total, 40/42 passes all
three windows, but its loading score (about 0.61) was slightly worse than 0/62
(about 0.55). The prior inter-group loading comparison therefore undid the intended
early-gain preference. Accepted HackRF groups now rank physical early-stage gain
first and loading score second. Weak/overloaded/non-Gaussian/imbalanced candidates
still cannot acquire that preference. This remains an allocation heuristic rather
than a measured noise-figure model.

A complete new hardware sweep selected **LNA 40 / VGA 44, RF amp off**, accepted
RMS 12.05%, in about 26 seconds. Evidence is in
`build/hackrf-live-63SW47/calibration.json`; the saved user profile was unchanged.
The optional `ngene_gain_probe` target lives in `development/profiling/` and is
excluded from ordinary builds and packages. No profiling was added to the main.

The displayed histogram example had means 0.0752/0.1533 counts but grouped four
ADC values per bar. Its zero text was also shifted left. Axis labels now use the
inclusive ADC levels' half-integer bin edges and centred text; a zero reference,
exact sample-mean marker and counts-per-bin annotation expose the difference
between a grouped bar's centre and the mean. Statistics and fixed DC removal are
unchanged. `web_histogram_axis.cjs` checks the example numerically. All 21 native
contracts passed (`scrolling-gain-priority-20261107.xml`). The updated executable
is in `build/msvc-x64-multisdr/Release/`; the user's older Review process was left
untouched.
