# WebGUI and control API · v1

> Engineering service document. For the current operating instructions, see the [operator manual](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Home). The final section records an older parallel-session design that has been removed; it is not an available GUI feature or API.

The English GUI is included in the same project and C executable as the kernel. The local service remains running after Stop receiver. Closing the browser tab does not interrupt reception or recording; reopening the page shows the current state. Do not run multiple instances against the same device or profile directory.

## Parameters and persistence

The Acquisition view colours the active satellite search amber and completed confirmed
acquisitions green. Its Completed search space panel selects the latest completed round
per GPS PRN, including below-threshold results, in a simultaneous top-down 2D heatmap
and fixed-camera 3D surface. Values are coarse correlation power divided by the original coarse threshold
(1); the independent confirmation result is shown separately. Code phase spans 0..1023
chips and the 16 original Doppler trials span -5000..5000 Hz. Display cells retain maxima
from the original 2048/4096-bin grid, including when resized; detection uses the original
unreduced double-precision values. Colour is logarithmic and 3D height is linear, with a
marked threshold plane and an automatic height scale.

Both compact plots share one satellite selector, one fetched map and one capture origin.
They appear in two columns above 1100 px and stack below that width. Each plot is
280 px high (260 px on phones). Both mark the same coarse peak with a white cross,
use the same Doppler/code intervals and colour scale, and update together when the
selected satellite or its completed capture changes. The old exclusive view buttons
are removed. Loading, missing-grid, error and reset states clear both views together;
late responses from a previous selection cannot replace the selected map.

`node tests/web_acquisition_views.cjs` tests the production module/CSS at 1440 and
390 px with deterministic maps: simultaneous views, layout, shared requests,
PRN/capture/axes/peak, updates, stale responses, unavailable maps and disconnects.
`tests/web_acquisition.cjs` also checks both plots and their layout against the
embedded service with synthetic GPS input. These are UI/synthetic checks, not RF
hardware evidence.
Verified locally on 2026-11-12 (Linux x86_64, Node 24.19.0, Chrome headless):
`web_acquisition_views.cjs`, `web_acquisition_stability.cjs` and the updated
`web_acquisition.cjs` passed. The CMake build and `gps_acquisition_contract` passed.

The satellite table keeps persistent rows in GPS L1 C/A PRN order. Configured PRNs
have slots before their first result, so search activity, completion counters, peak
ratios and confirmation outcomes cannot insert or promote rows above the satellite
being read. Only changed cells and status colours update; keyboard focus and unchanged
text selections survive live updates. Cells stay on one line to preserve row height
and the user's scroll position, including on narrow screens.

The Classic reference is `utility/ncursesApi.c`: its channel table uses
`rowFirstChannelData + chIdx` and updates state/identity cells in place. EVO's current
acquisition model represents satellites rather than assigned tracking channels, so
GPS L1 C/A PRN is the stable identity here. No Classic scheduling priorities are
transferred into display order.

States use existing telemetry: `searching/current_prn` means Acquiring, `confirmed`
means Confirmed, and a nonconfirmed result is Rejected in confirmation when
`peak_ratio > 1`, otherwise Below threshold. Configured PRNs without a result are
Waiting for search. Active searches show no previous-round numeric values; there is
no per-stage progress telemetry to infer. Confirmed never means tracking lock.
Completed-search options also retain their DOM identity and ascending PRN order;
updates keep the user's selected PRN while it remains available. Disconnects, new
runs and configuration changes may reset/remove unavailable identities.

Deterministic browser regression (Node >=18 and Playwright on the test host):

```sh
node tests/web_acquisition_stability.cjs
# Optional host settings: PLAYWRIGHT_MODULE, BROWSER_EXECUTABLE
```

The fixture uses production table/selector modules and CSS with synthetic status
updates and stubbed search-map responses. It checks desktop (1440 px) and mobile
(390 px): ordering, persistent rows/options, focus, text selection, scroll position,
selected satellite requests, state labels and resets over 40 updates each. This is
UI evidence, not physical RF validation. `tests/web_acquisition.cjs` separately checks
the embedded GUI against the real receiver and synthetic GPS input.

Verified locally on 2026-11-12 with Linux x86_64, Node 24.19.0 and Chrome headless:
`web_acquisition_stability.cjs` passed both viewports; `web_acquisition.cjs` passed
with `NGENE_EXECUTABLE=build-linux-bladerf/ngene`. The CMake build and CTest
`gps_acquisition_contract`, `core_contracts`, and `controller_lifecycle` also passed.
No physical RF hardware was used for these checks.

`GET /api/v1/acquisition/search-space?prn=1..32` returns one copied completed map and
capture epoch/sample provenance, or `valid: false` before a map is available. Maps stay
available after stop and reset on receiver start. Each process keeps a bounded latest-map
archive, rather than an unbounded history. Arrays are fetched only for the selected PRN
while the Acquisition view is visible; they are excluded from status/WebSocket frames.
An incomplete or cancelled search never replaces a completed map. Acquisition decisions,
noise estimates, refinement and confirmation are unchanged.

Frequency inputs use GHz for the GNSS signal reference (`1.57542` for GPS L1),
MHz for SDR tuning and analogue bandwidth, and Msps for the complex sample rate.
Doppler spans and steps remain in Hz. The GUI converts these values to integer Hz
for INI profiles and API requests, preserving 1 Hz resolution. Signal reference
`0` follows the hardware tuning; bandwidth `0` retains automatic filter selection.

Frontend, buffer, duration, recording base filename, and recording directory settings can be changed only in the `stopped` or `error` state, using the current revision. INI import/export supports reusable profiles. Validate checks syntax and bounds; Apply & save normalizes the path and writes the profile. Actual capture-file permissions are checked when the recorder opens; Validate does not certify disk space or USB availability.

The Folder on receiver computer field accepts a filesystem path on the NGene process's computer, rather than the browser's filesystem. The final directory is created if its parent exists. A receiver-side folder browser is available for replay selection; the recording destination retains its directory field. Relative paths resolve against the process working directory. Recording session displays the actual absolute path.

**Transfer** uses the browser's native download mechanism. It can copy data to another computer or to the same computer's download folder. Originals remain on the receiver. An SSH tunnel can provide remote access without changing the service's loopback binding; the code does not implement an authenticated LAN service.

## Protocol

Prefix: `/api/v1`. Responses are JSON; 64-bit counters and revisions are **decimal strings** to avoid JavaScript rounding. POST requests require `Content-Type: application/json`, a body with Content-Length of at most 16384 bytes, and the `X-NGene-Token` header obtained from bootstrap.

| Method and path | Content / effect |
|---|---|
| GET `/bootstrap` | Version, OS/architecture, compiled frontend availability, single-source capability, FFT backend, and server-session token |
| GET `/config` | `{revision, ini, mutability:"stopped"}` |
| POST `/config/validate` | `{revision:"…", ini:"…"}`; checks syntax and bounds without applying |
| POST `/config` | Same body; checks revision/state, saves, and returns the normalized configuration |
| GET `/status` | State, commands, counters, directory, recording information, and diagnostic snapshot |
| POST `/receiver/start`, `/receiver/stop` | `{revision:"…"}`; 202 response with command_id |
| POST `/recording/start`, `/recording/stop` | `{revision:"…"}`; same asynchronous contract |
| GET `/files` | Revision/directory and up to 200 selected files, sizes, closed and truncated flags |
| POST `/files/delete` | `{name, revision}`; deletes one closed file in the current directory |
| POST `/replay-files/browse` | `{path?, filter?}`; receiver directories and CI8/CU8/SC16 Q11 files, up to 500 entries |
| POST `/replay-files/inspect` | `{path}`; sidecar and draft INI, without applying or starting |
| GET `/files/<name>` | Closed binary attachment: raw, index, or metadata |
| GET / POST `/analysis/config` | Read or revise runtime diagnostic configuration; see [ANALYSIS.md](ANALYSIS.md) |
| POST `/frontend/capabilities` | `{ini:"…"}`; query native device limits while the receiver is stopped |
| POST `/shutdown` | `{}`; closes HTTP, stops the receiver, drains the recorder, and exits |
| WebSocket `/events?token=…` | Client sends a `status` text frame; server replies with a JSON snapshot |

202 means **accepted**, not completed. Wait for `completed_command_id >= command_id` and check `command_ok`, `state`, and `message`. A 409 response indicates a state, revision, or pending-operation conflict; 400 means invalid input; 403 means invalid origin/token. Profile-save failures are returned as 409 conflicts with an explanatory message.

WebSocket telemetry is client-requested, with at most one outstanding request and one latest snapshot waiting to render. The GUI provides 1/2/4/10/20/30 Hz refresh limits. Three clients are allowed; requests less than 20 ms apart close the connection to limit abuse. The `status:MASK` command updates interest in PSD (bit 1) and histograms (bit 2); `status` remains compatible and requests both. Each connection retains its own interest: disabling the final consumer immediately suspends collection, with a two-second expiry for silent clients. Responses contain empty arrays for unrequested or disabled diagnostics while retaining configuration and sequences. No raw I/Q is transmitted. JSON and networking run outside sample callbacks. Each PSD includes integrity statistics and a hash; see [ITERATION_04.md](ITERATION_04.md).

Host/Origin are restricted to the local listener; the token changes on restart. The server does not send permissive CORS headers, accept arbitrary download paths, or serve static filesystem directories. This model supports local use and tunnelling, rather than a public website. A directory selected through configuration authorizes writing there with the process's permissions.

## States

Receiver: `stopped → starting → running → stopping → stopped`; errors lead to `error`. Stop during Starting cancels/cleans up startup. Recording: `off → starting → recording → stopping → off`. Stop recording drains the writer while the receiver continues. Stop receiver closes both; the console remains active. A writer error stops the run and remains visible.

The plots and counters show actual runtime results. Spectrum, waterfall, and histograms are implemented; satellite acquisition/tracking/PVT remain to be integrated. Plot refresh and pause are browser runtime settings; the observation module's parameters use the `/analysis/config` runtime API described in [ANALYSIS.md](ANALYSIS.md).

## Optional browser test

On a development machine with Node >=18, Playwright, and Chromium installed:

```sh
NGENE_EXECUTABLE=build/ngene node tests/web_ui.cjs
```

The test creates temporary directories and starts its own server. It configures the name/directory through the GUI, checks persistence and conflicts, records, reloads the page, stops recording while keeping the receiver active, compares downloaded files byte for byte, and checks mobile layout. Optional variables: `PLAYWRIGHT_MODULE`, `BROWSER_EXECUTABLE`, `NGENE_TEST_PORT`, `NGENE_SCREENSHOTS`. Operators do not need Node or Playwright to use NGene.


## Two views, one receiver

**Observation** contains results and operating controls. **Configuration** separates static receiver parameters (locked during a run) from runtime analysis settings and display preferences. Both views use the dark theme. Start/Stop controls remain shared.

The status snapshot includes the `analysis` object. Spectrum and waterfall consume the same result; histograms use backend counts. Scaling, burst sampling, revision, and persistence semantics are described in [ANALYSIS.md](ANALYSIS.md). Do not use the graph reduced to 1024 points as complete numeric output for new algorithms; use the C APIs and native FFT bins.


## Controls introduced in 0.4

PSD and histograms have separate rates and pre/post sources, integer 0..99% overlap, Welch averaging within the burst limit, density per Hz, and temporal mean/EMA. Display controls provide adjustable global Y and colour scales, waterfall history/rate, and hardware/signal/RF references. Plots always show the full frequency band; frequency cropping and viewport expansion controls have been removed. The available waterfall observations fill the time grid even before history reaches capacity. Colour fitting is an explicit one-time command applied to every row; there is no per-row normalization. Hiding a panel, opening Configuration, or pausing withdraws that browser's interest and stops the corresponding display work without stopping reception. The backend retains only the diagnostics still requested by other browsers. Unchanged snapshots and histogram-only updates do not redraw spectrum or waterfall; the waterfall respects its own rate even when the spectrum updates more frequently.

**Enable** in the **Signal spectrum** and **Spectrogram / waterfall** headings controls the two plots independently and the runtime enable state of their shared FFT. Turning on the first plot sets `spectrum_enabled=true`, even if the initial profile set it to Off; turning off the last sets it to false. Turning off one while the other remains on changes only that plot's visibility. Disabled content collapses: canvas, axes and related commands do not remain as a frozen image. Headings and switches remain accessible so the plots can be re-enabled. A disabled waterfall neither accumulates history nor prepares images or meshes. With both disabled, the browser does not request PSD and runtime configuration disables the FFT producer; reception and histograms remain independently enabled. These changes have the same scope as Diagnostics configuration and are shared with other connected browsers.

The switches read the currently applied server parameters and change only `spectrum_enabled`: other applied settings, unsent Diagnostics form edits, and the profile draft are preserved. The **Spectrum enabled** selector remains synchronized. The switches are temporarily locked while the request is pending; on error they return to their previous state and a message appears in the panel. The change remains in the session; **Apply & save full profile** while stopped persists it to the INI profile. Pause keeps plot requests suspended even if Enable re-enables the producer. As with Apply analysis settings, an actual reconfiguration invalidates previous snapshots.

The upper-left NGene symbol and favicon use the original vector asset `web/ngene-logo.svg`: a mint-green N monogram and blue orbit. The asset is embedded in the executable and served locally as `image/svg+xml`, without requests to external services.

The 2D and 3D waterfalls have separate Enable switches and share the same bounded PSD history. Both show the full frequency band on a fixed time grid. While history fills, the oldest known edge is extended across the unfilled part of the pane; the reported observation count remains the number of actual captured frames. The 3D view is fixed and frontal, with an 18-degree elevation and no lateral rotation. A WebGL triangle mesh uses actual power heights, depth testing, surface normals, directional lighting and spectrum traces at each time row. Its orthographic frontal projection is fixed. Power relief tapers only at the viewport edges, keeping the entire pane occupied while preventing the camera from breathing with changing extrema. All frequency bins and observations are retained. Both waterfalls have vertical colour bars. The first valid PSD fits one global display range to the input; subsequent rows keep that range, and manual changes take precedence. The 2D palette is green around the noise floor; the 3D palette is blue with illuminated relief. New observations join a shared continuous scroll timeline; unfinished motion is retained if another observation arrives within the 180 ms transition; pause, hidden views and disabled views stop animation. Every telemetry bin and observation participates in the surface; narrow events are not pooled into frequency buckets. Both views use the same global colour scale and remain synchronized while paused.

Plots are large immediately: spectrum 430 px, 2D waterfall 460 px and 3D waterfall 500 px on desktop; 320/380/420 px on small screens. Height sliders, drag dividers, viewport expansion and frequency zoom controls are absent. Each Enable switch collapses its own view when it is not needed. Browser resizing redraws the same stored observations without changing the receiver configuration.

The FNIRSI-style frontal orientation is fixed. Power changes surface geometry as well as colour, and new observations enter at the front and older observations recede with fixed row spacing, while partial history still uses the full pane. The shared Clear history buttons clear both projections.

Power and colour scales are clipped to −250…100 dB. If manually entered limits become inverted or equal after clipping, plotting uses the default −100…0 dB range, keeping the spectrum and both waterfall views drawable.

`tests/web_plot_interactions.cjs` serves the current assets under the production Content Security Policy and supplies deterministic PSD frames. It checks large default sizes, absent zoom/height controls, synchronized projections, paused re-enable, shared clear and mobile visibility. `tests/web_waterfall_geometry.cjs` checks transient preservation at every age/bin and grid coverage with partial history; `tests/web_rf_views.cjs` exercises the embedded assets on the real service with replay input.

`tests/web_diagnostic_idle.cjs` instruments PSD/histogram reads, Canvas drawing/storage and 3D projection to verify that disabled or hidden diagnostics perform no image work. It also checks independent update sequences, the waterfall rate limit and cached redraws while paused. `tests/web_diagnostic_interest.cjs` runs against `NGENE_EXECUTABLE` with CI8 file replay and real WebSockets: masked payloads, immediate unsubscribe/close, multiple independent clients, runtime FFT disable and continuing reception. Both use the same Playwright/browser overrides as the other browser tests and need no SDR hardware.

Profile and diagnostic validation errors appear next to their fields, with an accessible association, and in the form summary. They remain visible across telemetry updates and request completion; correcting a field or successfully retrying validation updates the feedback. The GUI does not depend on transient browser validation bubbles. RTL-SDR sample rates must be in `(225000,300000]` or `(900000,3200000]` pairs/s; the clock interval `(300000,900000]` is rejected locally as well as by the backend. Integer, buffer and timeout controls follow the backend bounds; server validation remains authoritative. A delayed device-limits response is discarded if its source, serial, tuning or precision no longer matches the draft.

`tests/web_validation.cjs` checks persistent feedback across live WebSocket replies, the RTL-SDR ranges, required/integer fields, delayed backend failures and retry/edit recovery, diagnostic errors and stale device-capability responses against the actual receiver executable. Use the same test-host environment overrides as `tests/web_ui.cjs`; no RF hardware is needed.

For bladeRF, **Input source → Sample precision** selects `SC16_Q11` (12-bit ADC data stored in 16-bit I and 16-bit Q components) or `CI8` (native signed 8-bit I and 8-bit Q transfer, requiring compatible device/FPGA support). The status line displays the actual stream format reported by the receiver. Changing precision invalidates previously queried device limits; query them again before setting gain.


Native sample precision is automatic in the ordinary source form: RTL-SDR supplies unsigned 8-bit I/Q, represented as signed CI8 in the common stream; HackRF also uses 8 bits. Only bladeRF exposes a native CI8/SC16_Q11 transfer choice. The advanced INI conversion option remains available for compatibility; widening RTL samples to CI16 does not add ADC resolution. Source preset buttons restore automatic common precision. For an identified E4000, manual mode also exposes total IF gain (3..56 dB; -1 keeps driver defaults). An accepted calibration updates both RF and IF in the draft, and Apply & save persists them together.

## Configuration and deletion introduced in 0.5

Configuration has six sections: Input source, Receiver, Recording, Diagnostics, Display, and Profile. A single content area and common footer replace the earlier unequal columns. Before-start/runtime/display hints explain when parameters can be changed; backend constraints still apply.

**Delete** requires explicit confirmation of the filename and directory on the receiver computer. It removes one closed file; associated segments/metadata remain. The backend rechecks the revision and active-file state before removal even if the GUI list has become stale. It rejects names unrelated to capture files, traversal, and symlinks/reparse points. Missing file: 404; stale revision/active file: 409; invalid name/operation: 400. Delete uses token/origin checks like other POST requests. Filesystem errors are displayed; the file is not declared deleted before removal succeeds.


## Historical 0.6 design: primary and parallel replay sessions — removed

**This section preserves the description of a removed design.** The current GUI controls one receiver with one active source. It has no Receiver session selector. Requests to `/api/v1/replay/...` are rejected with an explanatory response, and `replay:MASK` is not an accepted WebSocket command. For independent concurrent replay, use a separate NGene process with a different port and profile directory.

In that earlier design, the **Receiver session** selector chose `Primary / live stream` or `Independent file replay`. Both could run simultaneously, and the unselected session continued working. Configuration, Start/Stop, Recording, files, and diagnostics affected only the selected session. Service shutdown stopped both.

The removed replay session used a separate FILE frontend, a `replay/receiver.ini` profile, and the `/api/v1/replay/...` API, with the primary's revision/command semantics. Bootstrap and shutdown were global. The same WebSocket accepted `replay:MASK` instead of `status:MASK`. Switching sessions cleared plot state and reconnected the socket; switching was disabled during unsaved changes or pending operations to prevent responses from being applied to the wrong controller.

The file-source controls themselves remain available in the current single-source GUI. **Input source → Recorded I/Q** provides the path on the NGene computer, first pair, range length, pacing, and repeat. **Browse I/Q files…** selects a receiver-side file and imports the rate and RF references from a validated sidecar. Without one, the operator supplies them manually. Selecting a file does not automatically start the receiver. See the current [Recording and replay operator guide](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Recording-and-Replay), and [REPLAY.md](REPLAY.md) for engineering notes.

During calibration, the red Stop calibration button remains available, including after reconnecting to a running job. A stop request changes it to Stopping until capture has stopped and the SDR has been released; no gains are saved. RTL observations reuse one continuously running USB stream with two bounded native buffers. Publication is gated while applying gains and analyzing copied samples. Old queued data is drained; both in-flight buffers and the settling interval are discarded before the three non-overlapping windows. Only completion, stop or failure cancels USB RX. Normal receiver sources retain their one-run lifecycle.

Histogram amplitudes use inclusive ADC levels with bin edges at half-integer coordinates. The zero label is centred at its actual amplitude coordinate; a dashed zero reference and exact sample-mean marker distinguish the average from the centre of a grouped bar. The caption reports mean and counts per bin (for CI8 with 64 bins, four counts per bin). Histogram statistics and fixed DC removal are unchanged.
