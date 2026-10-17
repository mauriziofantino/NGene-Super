# WebGUI and control API · v1

> Engineering service document. For the current operating instructions, see the [operator manual](../wiki/Home.md). The final section records an older parallel-session design that has been removed; it is not an available GUI feature or API.

The English GUI is included in the same project and C executable as the kernel. The local service remains running after Stop receiver. Closing the browser tab does not interrupt reception or recording; reopening the page shows the current state. Do not run multiple instances against the same device or profile directory.

## Parameters and persistence

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

PSD and histograms have separate rates and pre/post sources, integer 0..99% overlap, Welch averaging within the burst limit, density per Hz, and temporal mean/EMA. Display controls provide adjustable global Y and colour scales, waterfall history/rate, X limits, and hardware/signal/RF references. Colour fitting is an explicit one-time command applied to every row; there is no per-row normalization. Hiding a panel, opening Configuration, or pausing withdraws that browser's interest and stops the corresponding display work without stopping reception. The backend retains only the diagnostics still requested by other browsers. Unchanged snapshots and histogram-only updates do not redraw spectrum or waterfall; the waterfall respects its own rate even when the spectrum updates more frequently.

**Enable** in the **Signal spectrum** and **Spectrogram / waterfall** headings controls the two plots independently and the runtime enable state of their shared FFT. Turning on the first plot sets `spectrum_enabled=true`, even if the initial profile set it to Off; turning off the last sets it to false. Turning off one while the other remains on changes only that plot's visibility. Disabled content collapses: canvas, axes, resize controls, and related commands do not remain as a frozen image. Headings and switches remain accessible so the plots can be re-enabled. A disabled waterfall neither accumulates history nor prepares images or meshes. With both disabled, the browser does not request PSD and runtime configuration disables the FFT producer; reception and histograms remain independently enabled. These changes have the same scope as Diagnostics configuration and are shared with other connected browsers.

The switches read the currently applied server parameters and change only `spectrum_enabled`: other applied settings, unsent Diagnostics form edits, and the profile draft are preserved. The **Spectrum enabled** selector remains synchronized. The switches are temporarily locked while the request is pending; on error they return to their previous state and a message appears in the panel. The change remains in the session; **Apply & save full profile** while stopped persists it to the INI profile. Pause keeps plot requests suspended even if Enable re-enables the producer. As with Apply analysis settings, an actual reconfiguration invalidates previous snapshots.

The upper-left NGene symbol and favicon use the original vector asset `web/ngene-logo.svg`: a mint-green N monogram and blue orbit. The asset is embedded in the executable and served locally as `image/svg+xml`, without requests to external services.

The waterfall header provides **View → 2D waterfall / 3D waterfall**. Both views use the same bounded PSD history; switching projection also works while paused and preserves every stored snapshot. In the default 3D orientation, frequency runs from left to right, older observations recede from the newest row at the front, and height uses the Spectrum minimum/maximum settings. Colours use the same global Waterfall colour minimum/maximum in both views. The depth axis uses the snapshots' production times, including display skips. The history capacity remains 32–2048 snapshots; 3D drawing samples at most 48 history slices and 192 frequency buckets, retaining each bucket's peak so narrow carriers remain visible. Rendering uses the browser's existing Canvas API, without extra dependencies, raw I/Q transfers or receiver changes. These are browser display preferences and are not saved in the receiver INI profile.

**Spectrum height** and **Waterfall height** independently resize each canvas from 180 to 720 pixels. Drag the divider immediately below either canvas to resize it directly; with the divider focused, Up/Down changes height by 10 pixels, Page Up/Down by 100, and Home/End selects the minimum/maximum. The sliders also work on touchscreens. **Expand plots** uses the browser viewport for both plots, with scrolling if needed; **Restore plots** or Escape returns to the normal page and retains both selected heights. Resizing redraws existing data, including paused plots, without modifying the receiver configuration or the stored PSD history.

In 3D, drag the waterfall with a mouse or one finger to rotate its azimuth and elevation. The four arrow buttons provide the same controls; when the canvas has keyboard focus, Left/Right rotates and Up/Down tilts. Elevation is bounded to 12–78 degrees. **Reset view**, Home with canvas focus, or a double-click restores the default perspective. Frequency, observation-age and power axes rotate with the surface, and facets are sorted by camera depth so rear rows remain correctly ordered when viewing the history from another side. The 2D waterfall retains normal touch scrolling. Rotation works while paused and keeps the same PSD samples and global scales.

Power and colour scales are clipped to −250…100 dB. If manually entered limits become inverted or equal after clipping, plotting uses the default −100…0 dB range, keeping the spectrum and both waterfall views drawable.

`tests/web_plot_interactions.cjs` is a focused display regression that serves the current web assets under the production Content Security Policy and supplies deterministic PSD frames. Run it with Node and Playwright installed on the test host (the same `PLAYWRIGHT_MODULE` and optional `BROWSER_EXECUTABLE` overrides as `tests/web_ui.cjs`). It verifies plot size changes, mouse and keyboard divider resizing, expand/restore and keyboard focus, mouse/touch/button/keyboard 3D rotation, reset, mesh/elevation bounds, mobile layout and preservation of paused history. The receiver integration and live transport remain covered by `tests/web_ui.cjs`.

`tests/web_diagnostic_idle.cjs` instruments PSD/histogram reads, Canvas drawing/storage and 3D projection to verify that disabled or hidden diagnostics perform no image work. It also checks independent update sequences, the waterfall rate limit and cached redraws while paused. `tests/web_diagnostic_interest.cjs` runs against `NGENE_EXECUTABLE` with CI8 file replay and real WebSockets: masked payloads, immediate unsubscribe/close, multiple independent clients, runtime FFT disable and continuing reception. Both use the same Playwright/browser overrides as the other browser tests and need no SDR hardware.

Profile and diagnostic validation errors appear next to their fields, with an accessible association, and in the form summary. They remain visible across telemetry updates and request completion; correcting a field or successfully retrying validation updates the feedback. The GUI does not depend on transient browser validation bubbles. RTL-SDR sample rates must be in `(225000,300000]` or `(900000,3200000]` pairs/s; the clock interval `(300000,900000]` is rejected locally as well as by the backend. Integer, buffer and timeout controls follow the backend bounds; server validation remains authoritative. A delayed device-limits response is discarded if its source, serial, tuning or precision no longer matches the draft.

`tests/web_validation.cjs` checks persistent feedback across live WebSocket replies, the RTL-SDR ranges, required/integer fields, delayed backend failures and retry/edit recovery, diagnostic errors and stale device-capability responses against the actual receiver executable. Use the same test-host environment overrides as `tests/web_ui.cjs`; no RF hardware is needed.

For bladeRF, **Input source → Sample precision** selects `SC16_Q11` (12-bit ADC data stored in 16-bit I and 16-bit Q components) or `CI8` (native signed 8-bit I and 8-bit Q transfer, requiring compatible device/FPGA support). The status line displays the actual stream format reported by the receiver. Changing precision invalidates previously queried device limits; query them again before setting gain.


## Configuration and deletion introduced in 0.5

Configuration has six sections: Input source, Receiver, Recording, Diagnostics, Display, and Profile. A single content area and common footer replace the earlier unequal columns. Before-start/runtime/display hints explain when parameters can be changed; backend constraints still apply.

**Delete** requires explicit confirmation of the filename and directory on the receiver computer. It removes one closed file; associated segments/metadata remain. The backend rechecks the revision and active-file state before removal even if the GUI list has become stale. It rejects names unrelated to capture files, traversal, and symlinks/reparse points. Missing file: 404; stale revision/active file: 409; invalid name/operation: 400. Delete uses token/origin checks like other POST requests. Filesystem errors are displayed; the file is not declared deleted before removal succeeds.


## Historical 0.6 design: primary and parallel replay sessions — removed

**This section preserves the description of a removed design.** The current GUI controls one receiver with one active source. It has no Receiver session selector. Requests to `/api/v1/replay/...` are rejected with an explanatory response, and `replay:MASK` is not an accepted WebSocket command. For independent concurrent replay, use a separate NGene process with a different port and profile directory.

In that earlier design, the **Receiver session** selector chose `Primary / live stream` or `Independent file replay`. Both could run simultaneously, and the unselected session continued working. Configuration, Start/Stop, Recording, files, and diagnostics affected only the selected session. Service shutdown stopped both.

The removed replay session used a separate FILE frontend, a `replay/receiver.ini` profile, and the `/api/v1/replay/...` API, with the primary's revision/command semantics. Bootstrap and shutdown were global. The same WebSocket accepted `replay:MASK` instead of `status:MASK`. Switching sessions cleared plot state and reconnected the socket; switching was disabled during unsaved changes or pending operations to prevent responses from being applied to the wrong controller.

The file-source controls themselves remain available in the current single-source GUI. **Input source → Recorded I/Q** provides the path on the NGene computer, first pair, range length, pacing, and repeat. **Browse I/Q files…** selects a receiver-side file and imports the rate and RF references from a validated sidecar. Without one, the operator supplies them manually. Selecting a file does not automatically start the receiver. See the current [Recording and replay operator guide](../wiki/Recording-and-Replay.md), and [REPLAY.md](REPLAY.md) for engineering notes.
