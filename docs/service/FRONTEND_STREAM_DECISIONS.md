# Frontend stream and ring decisions

Date: 2026-10-31. Implemented in Evo; Classic is a protocol/numerical reference,
not modified by this work. This record complements [the DSP options study](DSP_OPTIONS_PLAN.md).
It records the chosen boundaries so later acquisition/tracking work can change
implementation without silently changing sample meaning or continuity.

## Scope and pipeline

```text
SDR native transport / replay
    -> native driver-owned buffers and ingress (where provided by the driver)
    -> dedicated common-source worker: acquire, SIMD convert, publish, release
    -> aligned common CI8/CI16 SPSC ring
    -> receiver common-window assembly and engine ring
    -> immutable DSP windows / asynchronous recording / diagnostic worker
```

`NgFrontendOpen` returns the common source. `NgFrontendOpenNative` is the lower
boundary for native calibration and adapter/transport fixtures. Each opened common
source owns its worker, native source, queues, counters and lifetime. There is one
producer and one logical reader per ring; parallel channel consumers do not acquire
independent leases on that ring.

The native driver remains responsible for device APIs, timestamps, transport
layout and sample-loss reports. The common worker performs one conversion directly
into an unused ring slot, then releases the native lease. This prevents DSP or
recording from keeping a vendor/ingress pointer alive. Available queue milliseconds
are an upper bound based on full slots; partial native blocks can exhaust slots
sooner. SiGe's queued USB payload is 1 MiB, about 64 ms at the nominal byte rate. There is no conversion
allocation during reception. Existing vendor RX threads remain separate; SiGe
uses queued asynchronous USB reads serviced by its common-source worker.

All ring slot starts are 64-byte aligned, including odd-sized windows and wrap.
Padding belongs to storage geometry, not the sample stream or recorded file.
Geometry overflow and a conservative 512 MiB queue plan are checked before RX.
Short native blocks remain short blocks with their original metadata; receiver
window assembly never fabricates missing samples or substitutes a byte count for
a complex-pair count.

## Common representation

The profile key `frontend.stream_format` is `auto`, `CI8` or `CI16`. A run has one
common format shared by every downstream consumer. The format is selected before
RX, not dispatched by hardware model in each DSP sample operation.

| Transport | Automatic common format | Mapping |
|---|---|---|
| HackRF CI8 | signed CI8 | Preserve bytes |
| RTL-SDR CU8 | signed CI8 | Subtract unsigned midpoint 128 exactly |
| bladeRF native SC8 Q7 | signed CI8 | Preserve bytes |
| bladeRF SC16 Q11 | CI16, signed Q15 | Multiply ADC counts by 16 exactly |
| Replay CU8/CI8 | signed CI8 | Same conversion as a live adapter |
| Replay SC16 Q11/CI16 | CI16 | Exact Q11 widening / preserve Q15 bytes |
| SiGe native markers | signed CI8 | Decode adjacent I,Q markers to values +1/-1 |

Q11 is the bladeRF SDK transfer scale, not a SiGe mode or receiver accuracy
setting: its signed twelve-bit range has eleven fractional bits, so normalized
values are integer counts divided by 2048. The sixteen-bit word is a container.
See the [official bladeRF format definition](https://www.nuand.com/libbladeRF-doc/v2.6.0/group___s_t_r_e_a_m_i_n_g___f_o_r_m_a_t.html).

CI8 is interleaved signed integer storage, two bytes per complex pair, with a
normalized DSP full scale of 128 (the Q7 interpretation). Historical CI8 metadata
keeps fractional_bits=0 for integer ADC counts. CI16 is
interleaved signed Q15, four little-endian bytes per pair. Native Q11 is retained
as a transport/replay format, not a second common 16-bit scale. Converting CI8 to
CI16 multiplies by 256, preserving every input level without creating precision.
Explicit narrowing to CI8 uses nearest rounding with ties away from zero and
saturation at -128/+127. Q11 values outside -2048..2047 fail conversion rather
than wrapping during widening. SSE2 and NEON have scalar tails and the same rules.

SiGe is a sign sampler: storing its decoded +1/-1 values in CI8 does not turn it
into an eight-bit ADC. There is no synthetic digital gain. The source ADC precision
is one bit per component and is reported separately. Future XOR consumers can use
that representation directly. Do not infer analogue clipping or RMS calibration
from an eight-bit container holding one-bit samples.

Automatic precision preserves the source information. Explicit CI8 on a wider
source is a deliberate lossy choice, visible in the UI; it is never selected as
an automatic response to CPU pressure. Mixer/correlator XOR and one-bit arithmetic
profiles remain future work from the DSP study, not operational changes in this patch.
Wider intermediates and accumulators remain necessary even with CI8 storage.

## Continuity, overflow and shutdown

Epochs, absolute pair indices, hardware timestamps and explicit gap metadata pass
through conversion unchanged. A common-ring software drop advances the source
indices and marks the next accepted block with the known missing-pair count. If a
device gap has unknown duration, adding a known software drop must not turn it
into an exact duration. Mandatory live overflow either stops or drops newest
according to the saved policy. Unread/leased slots are never overwritten.

A file producer can wait for ring capacity without losing RF time; it uses
backpressure rather than treating faster-than-real-time replay as live USB loss.
Stopping joins the common worker before freeing native sources, USB transfers or
sample storage. Initialization, data and stop failures must reach the receiver;
no failed transport is represented as clean EOF. Queued common samples discarded
on stop are counted separately from received samples and known stream drops.

## DC removal and recording semantics

The continuous Q32 IIR implementation and public DC creation functions have been
removed from the build/source. Runtime startup always uses bypass. There are no
DC-removal controls in the UI and saved profiles omit the retired conditioning
keys. Historical `conditioning.module`/`dc_shift` profile entries are accepted and
ignored so an old profile cannot silently re-enable a filter. Reserved legacy C
configuration fields remain for source compatibility and must not enable DC.

Diagnostic and calibration workers now subtract each observation's rounded I/Q mean
with the two-pass SIMD primitive `NgIqCenterBlock`. This follows Classic's widened
block subtraction rather than an IIR recurrence. SSE2 and ARM64 NEON implementations
have a scalar fallback. The owned int32 output avoids saturation of centered CI16
excursions. Histograms, statistics and FFT input use that output; removed offsets and
the remaining sub-level mean are reported separately. Original stream endpoint counts
remain available, and gain calibration checks clipping on the original native ADC levels.
There is no selectable DC policy or pre/post diagnostic control in the UI.

This correction is local to diagnostic/calibration workspaces. It does not modify the
shared ring or recording samples and does not add a continuous DC filter. It removes
the constant component of each observation, including any desired signal exactly at DC.
RF/IF/Fs setup still needs on-device validation. See [verification](CALIBRATION_DC.md).

Operational captures now contain the **common signed stream**. The old pre/post
recording-point INI/API values remain readable for compatibility, but with bypass
they capture the same stream, after transport conversion. The redundant UI choice
has been removed. A source-native raw recording branch is not implemented here;
do not describe a CI16 recording as original Q11 USB bytes. Lossless conversions
preserve the values, while explicit narrowing cannot be undone.

New CI16 captures use `.ci16`, Q15 metadata and full recording/replay/file-picker
support. Sidecar significant_bits reports effective stored precision, not artificial extra
ADC bits from widening; source_adc_bits retains physical ADC precision even after
explicit narrowing. Sidecars distinguish storage format from `source_native_format` and
`source_adc_bits`. Available ADC-bit provenance survives replay; historical files
without it fall back to the declared storage format. Common-stream histogram rail
counts are labelled as such, not asserted to be physical ADC clipping counts.

## Native SiGe profile and Windows USB findings

This adapter supports Classic ID 1's already-loaded marker-tagged native I/Q
firmware at VID/PID `16c0:072f`. It does not flash firmware, use legacy real-IF
reconstruction or infer a format from the board name. I bytes have marker bit 1
set, Q bytes do not; bit 0 selects +1/-1. An initial unmatched Q can be skipped,
and a pending I crosses USB transfer boundaries. An invalid marker sequence after
synchronization stops reception instead of fabricating a counterpart or silently
continuing a coherent stream.

The fixed nominal profile follows Classic's native-IQ configuration:

- 16,367,600 transport component bytes/s; 8,183,800 complex pairs/s.
- GPS L1 signal reference 1,575,420,000 Hz, residual complex IF +38,400 Hz.
- Corresponding hardware-baseband RF reference 1,575,381,600 Hz.
- No gain, RF amplifier or tuneable filter controls.
- USB configuration 1, interface 2, alternate setting 0, IN endpoint 0x86.
- Cypress vendor request 0x01, wValue 1 to start and 0 to stop.
- 128 queued reads of 8192 bytes, matching Classic's established geometry.

The SiGe preset and source selector both apply this profile. Hardware rate/RF/IF,
filter and native stream precision are locked in the UI. The server independently
rejects altered hardware rate/RF/IF values; hiding a control is not validation.
Serial selection and software buffer/recording policies remain separate concerns.
The nominal RF/IF values are configuration references, not oscillator calibration
or a GNSS acquisition result on the antenna-free device.

The first Windows hardware tests completed only the first USB read and then
timed out, with both large requests and Classic-sized requests. The isolated probe
showed all remaining requests pending. Classic's Windows open path revealed the
missing requirement: enable WinUSB RAW_IO before queuing FX2 FIFO reads. The Evo
adapter now queries RAW_IO support and validates maximum transfer size/packet
alignment before enabling it. With this change, both the probe and normal Evo
streamed continuously. The antenna connection is unrelated to this USB finding.
RAW_IO is a Windows WinUSB policy, guarded by platform/API availability in the
adapter; Linux uses its native libusb backend and needs separate execution
validation. The tagged-IQ decoder is shared. See the
[libusb RAW_IO contract](https://libusb.sourceforge.io/api-1.0/group__libusb__dev.html).

`development/profiling/sige_usb_probe.c` is an explicit `ngene_sige_probe` target,
excluded from normal builds. It inspects completion/queue state without adding
experimental instrumentation to the operational main.

## Alternatives considered

| Alternative | Decision and reason |
|---|---|
| Widen every source to CI16 unconditionally | Rejected as default: doubles eight-bit traffic without adding information; explicit CI16 remains available |
| Preserve CU8/Q11/device layouts through all common consumers | Replaced by one signed format per run; transport formats remain private and replay-compatible |
| Convert in vendor callbacks | Rejected for the common normalization stage: a dedicated worker keeps SIMD work and publication off the USB callback |
| Add a separate copy into a conversion scratch buffer | Avoided: conversion writes directly into the next common-ring slot |
| Add multiple independent readers to existing rings | Rejected: violates the SPSC/lease contract; downstream parallelism uses immutable windows |
| Make all common samples one-bit/XOR immediately | Deferred: the study shows sensitivity and harmonic-image tradeoffs; retain as an explicit future DSP option |
| Keep the continuous DC IIR | Removed by user direction; it differs from Classic's exact-window mean subtraction |
| Use SiGe real-IF reconstruction or generic SDR gain controls | Rejected: native firmware exposes tagged I/Q and fixed hardware parameters |
| Increase USB request size as the assumed SiGe fix | Not adopted: it did not solve the first-read stall; RAW_IO was the observed missing setup |
| Recover malformed SiGe markers without a continuity event | Rejected: unmatched components cannot establish exact RF time |

## Verification and remaining limits

Native MSVC x64 builds use the project's warning policy. Tests cover conversion
levels and SIMD tails, invalid Q11, aligned odd-slot wrap, worker lifetime,
backpressure, overflow, unknown gaps, CI16 recording/replay and immutable SiGe
hardware parameters. The SiGe USB ABI fixture covers initialization, queued reads,
GPIF controls, odd transfers, corruption, disconnection and cancellation cleanup.
All 21 native CTest checks passed, together with the complete browser UI regression
and the new frontend/browser test. The latter covers fixed controls, absence of DC
removal, CI16 PSD and the 3D view.
The conversion and SiGe-decoder NEON sources also cross-compile for Windows ARM64
with Clang; that is compile evidence, not ARM execution or performance validation.

On the connected SiGe with no antenna, the isolated native probe received
24,551,424 pairs in 3.000 s. Normal CLI reception processed 40,923,136 pairs in
5.002 s with zero reported gaps/software drops. In the browser test with spectrum,
both waterfalls, histograms and a brief recording, the running snapshot showed
43,472,000 processed pairs, 8,560,000 written pairs, zero gap/drop/transport-error
counters and common ingress high-water 4 blocks. Evidence is retained under
`build/frontend-stream-ui-YHvN7Z` and can be reproduced with
`NGENE_SIGE_LIVE=1` and `tests/web_frontend_stream.cjs`.

A final metadata/browser rerun is retained in `build/frontend-stream-ui-3Jon7W`:
42,608,000 processed pairs, 8,592,000 written pairs, common ingress high-water 5,
and zero reported gap/drop/overflow/transport-error counters. Its SiGe sidecar
declares significant_bits=1 and source_adc_bits=1 in byte storage, and omits
retired DC parameters. Recording/replay contracts were rerun after this metadata
change. Tested Release executable SHA-256:
`DE1A802A6225A0C11636E6913B8573F884B12ED70BC1CFCF3AB158798E7001AF`.

These short tests establish USB delivery and software pipeline behavior on this
Windows/device combination. The device provides no hardware sample-loss counter;
zero software gaps cannot prove no hardware FIFO loss. No antenna/RF sensitivity,
acquisition, tracking, PVT, long-duration thermal test, live HackRF/RTL-SDR/bladeRF,
Linux/macOS runtime or ARM throughput validation is claimed.

## Historical waterfall relief (superseded on 2026-11-05)

The user's FNIRSI DPO350P reference supersedes the earlier 18-degree perspective
camera and 64-by-256 pooled mesh. The requested lower display is a rectangular
frequency/time image with blue/cyan relief lighting. Perspective, camera controls,
displacement and additional row/bin pooling are discarded: they change alignment,
occlude brief signals or make the growing time window appear unstable.

Both waterfalls now consume one immutable FFT history, with identical row selection,
rate, power range, frequency crop and newest-at-top direction. Pause and both Clear
buttons act on that shared history. A disabled view skips rendering; reenabling it
while paused immediately draws the current cached history. History stops accumulating
only when both waterfalls are disabled. All three views still share one PSD producer.

`web/waterfall-relief.js` normalizes the existing power values and shades the local
frequency/time gradient. A direct power contribution keeps a narrow, one-frame peak
visible even on a slope facing away from the light. Boundary extension avoids a false
wall at the oldest populated row. It does not modify observations or their positions.
The existing limits are 2048 history rows and 1024 telemetry bins. No extra FFT, receiver
worker or sample-path processing is introduced. Geometry tests check every age/bin,
history growth and identical 2D/relief transforms; browser regressions cover shared
history, pause, clear, disabled canvases and desktop/mobile layouts.

Validation on 2026-10-31: Release multi-SDR build, `web_waterfall_geometry.cjs`,
`web_plot_interactions.cjs`, `web_rf_views.cjs` and the complete `web_ui.cjs` passed.
Visual fixtures are in `build/relief-validation/`; the receiver-backed eight-view
test is in `build/rf-views-validation-IJJFzQ/`. A warmed Node timing of the pure
lighting function on this Windows host (10 draws, 1024 columns) measured median
3.74 ms at the default 128 rows and 52.88 ms at the maximum 2048 rows. These exclude
Canvas upload/drawing and do not establish a 30 Hz browser guarantee at maximum
history. No acquisition-path throughput or hardware RF validation is claimed by
these display-only tests; the earlier SiGe measurements above remain separate.

## 2026-11-05: Windows stalls and actual 3D surface

The user reports that both SiGe and HackRF stop after a few seconds on Windows,
with Start apparently unavailable afterwards. Neither device is currently accessible
on the test host, and the user cannot connect one during this session. The actual
device failure and USB cleanup hang, if any, remain unconfirmed.

A separate software defect is reproduced in the common frontend: a finite native
burst with two common slots and a 5 ms consumer scheduling delay terminates the
stream immediately. This regression fails before the change and passes afterwards
for both CI8 and CI16. Under stop policy the common producer now waits on reader
release for at most 50 ms before reporting a full-ring failure. Native queues retain
their overflow checks; drop-newest remains immediate and replay may keep waiting.
No unread slots are overwritten or RF samples synthesized. The stalled-reader test
still fails explicitly after the bounded wait. This is a fix for premature failure
on a burst, not proof that the user's live failure has the same cause.

The controller contract additionally exercises two successive worker conversion
failures and a successful restart in the same service. This does not validate
vendor USB cancellation on disconnected hardware.

The user explicitly rejects the colour-only relief view. It is replaced with a
projected surface: frequency remains horizontal, history supplies depth and power
raises the vertices. Shared observations, frequency crop, pause, clear and frame
selection remain synchronized with 2D. Foreground surfaces can occlude background;
there is no bin/row pooling. A visibility horizon visits every vertex but writes
each raster pixel at most once. The raster height follows the canvas, independently
of the selected history capacity. Tests cover power-dependent geometry, one-bin/
one-frame events, history growth and unchanged observations.

The browser interaction test passes on desktop/mobile with production CSP; its
synthetic illustration is `build/surface-validation/relief-detail.png`. This is
display evidence, not antenna data. The eight-view receiver/replay regression and
full browser UI test pass (the latter requires execution outside the Windows
sandbox for capture-file transfers). Release multi-SDR CTest passes 21/21; the
extended controller restart test also passes separately.

A warmed Node measurement with 1024 columns and a 300-pixel raster gave median
11.04 ms at 128 history rows and 48.94 ms at 2048 rows. It excludes browser Canvas
upload and is not a guaranteed frame rate. No live SiGe/HackRF, Linux/macOS or RF
accuracy acceptance is added by this session. The next hardware check must keep
the failure message and status counters and verify several Start/Stop cycles.
