# Recording and replay

NGene writes the native I/Q representation delivered by the selected input, either before or after the configured conditioner. An asynchronous writer drains a bounded queue so that disk I/O is separate from sample acquisition.

## Record a stream

1. Stop the receiver before changing the capture profile.
2. In **Configuration → Recording**, select the capture point, receiver-side directory, filename prefix, writer queue, and optional segment size.
3. Enable **Record on receiver start** if capture should begin with reception, then **Apply & save full profile**.
4. Start the receiver. Alternatively, leave automatic recording off and use **Start recording** during a running stream.
5. Use **Stop recording** to finish the capture while keeping reception active, or **Stop receiver** to finish both.
6. Wait for the recording state to become off and for files to close before transferring or replaying them.

**pre** saves samples directly from the frontend; **post** saves the shared conditioned stream. With bypass conditioning they have the same sample values. With `dc_iir`, post-conditioning samples include DC removal. No FFT window, PSD averaging, or plot scaling is applied to raw capture files.

The capture directory belongs to the computer running NGene. The GUI adds a unique session suffix to the chosen base filename. A CLI recording uses the configured prefix and creates files exclusively, so choose a fresh prefix for repeated runs. If you set `recording.directory`, keep the prefix as a base filename and set the directory separately.

## Files and representations

For a session with basename `survey`, the writer produces files such as:

```text
survey.meta.ini
survey.index.csv
survey_000000.cs8
survey_000000.cs8.meta.ini
survey_000001.cs8
survey_000001.cs8.meta.ini
```

Rotation creates additional numbered raw segments. When rotation is disabled, a single numbered raw file is used. The GUI's unique suffix is omitted above for readability.

| Native format | Raw extension | Storage per complex pair | Signed ADC values used by the engine |
| --- | --- | --- | --- |
| CI8 | `.cs8` | 8-bit signed I followed by 8-bit signed Q; 2 bytes | −128 through 127 |
| CU8 | `.cu8` | 8-bit unsigned I followed by 8-bit unsigned Q; 2 bytes | Stored values minus 128 |
| SC16 Q11 | `.sc16q11` | Little-endian signed 16-bit I followed by signed 16-bit Q; 4 bytes | −2048 through 2047; 12 significant ADC bits |

These raw files have no binary header. Keep their metadata and index files together with them.

The per-segment sidecar records its exact format and raw filename association, nominal sample rate, hardware and signal frequencies, digital IF, analogue bandwidth, conditioning, capture point, and completion information. The session sidecar summarizes the recording. Nominal rate and host timestamps must not be mistaken for a measured sampling oscillator or GNSS time.

The CSV sample index associates raw ranges with continuity epochs, first sample indices, pair counts, segment offsets, block flags, known missing-pair counts, and hardware timestamps when available. Raw files do not insert zero padding for missing RF samples. Use the index to distinguish contiguous samples from concatenated data separated by a gap.

## Capacity and failure behavior

Raw throughput is:

```text
bytes_per_second = sample_rate_hz × bytes_per_complex_pair
```

At 16 Msps, CI8 and CU8 require 32 MB/s (about 115.2 GB/hour); SC16 Q11 requires 64 MB/s (about 230.4 GB/hour), before metadata/index overhead. These are decimal units. Confirm sustained disk capacity before a long run.

`recording.segment_samples=16000000` rotates after one nominal second at 16 Msps. It changes file boundaries, not sample rate or precision. A window crossing a boundary is split between segments, with corresponding index entries.

Stopping recording drains its accepted writer queue. A disk error, full disk, or full recorder queue is an error: the writer does not silently drop capture windows and declare a complete recording. An interrupted or failed segment may remain marked incomplete. Preserve the available raw data and sidecars, inspect completion and gap information, and do not infer continuity from the existence of a file alone.

## Transfer and delete

In **Observation → Capture files & transfer**, click **Refresh files** after closing a recording. **Transfer** downloads a closed file to the computer running the browser, using that browser's download destination. This is a download from receiver storage, not a move of the source file.

Transfer the raw file and its matching metadata; include the session index and summary when preserving the capture. Deletion is permanent and applies to one selected file only. Related raw, metadata, and index files are retained, so remove a complete capture deliberately if that is your intention. Active capture files and inputs currently in use are protected by the service.

## Replay in the GUI

Stop the receiver, then choose **Configuration → Input source → Browse I/Q files…**, or use the **Replay** action beside a closed raw capture. The picker browses storage on the receiver machine and displays the selected capture's metadata and completion information.

**Use selected file** prepares a draft. Metadata loads the recorded format, sample rate, tuning, bandwidth, and DC estimator parameter. The prepared draft selects bypass conditioning, disables recording, resets the range, and requests an unlimited run. This prevents blindly applying the capture's conditioner a second time to already conditioned data. Review the desired current conditioner yourself.

Review the following before **Apply & save full profile → Start receiver**:

| Setting | Behavior |
| --- | --- |
| Native IQ format | Use the actual raw representation; a valid sidecar takes precedence during selection |
| Start sample pair | Zero-based pair offset within the raw file |
| Range length | Maximum pairs to read; zero selects the rest of the file |
| Real-time pacing | Pace according to the configured sample rate; disabling it permits faster processing |
| Repeat range | Restart the selected range at EOF; each restart reports a discontinuity |
| Run duration | Optional wall-clock limit; zero leaves playback governed by Stop or EOF |

Replay opens one selected raw segment and does not automatically concatenate the other segments. The file length is fixed when opened; subsequent file growth is not included, and truncation is an error. Use a closed, immutable input.

Playback creates a new file-playback stream with its own monotonic delivery clock and sample indices. It does not reconstruct the original capture's hardware timestamps or automatically reinsert the historical gaps from its CSV index. Looping increments the continuity epoch and keeps delivered sample indices monotonic. Read the original index when historical continuity matters.

## Replay from the command line

With an adjacent NGene sidecar:

```sh
ngene --replay captures/survey_000000.cs8
```

Without capture metadata, supply a profile containing the correct sampling rate and RF reference:

```sh
ngene --config config/replay.ini --replay captures/external.cs8
```

Edit a copy of the example profile to match the external file. Recognized raw extensions supply a representation hint; they cannot reveal sample rate or RF tuning. A malformed sidecar is reported as an error rather than silently ignored.

For browser diagnostics:

```sh
ngene --web --replay captures/survey_000000.cs8 --output-dir replay-output
```

This opens the console with a prepared file-input profile; click **Start receiver** to begin. A profile without a sidecar still needs `--config` with the correct stream parameters.

The current web console has one active source. To inspect an old closed capture while another NGene process is receiving live samples, start a separate process with a different port and profile/output directory, for example `--port 8081 --output-dir replay-output`. Its stream, recording state, and controls are independent.
