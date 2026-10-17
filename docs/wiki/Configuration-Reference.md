# Configuration reference

NGene profiles use a strict INI format. Sections and keys are case-sensitive. Write booleans as lowercase `true` or `false`; write integer counts and frequencies in decimal without unit suffixes or thousands separators. Real-valued fields accept finite numbers. Unknown sections, unknown keys, duplicate keys, malformed values, and inconsistent combinations are rejected. Put comments on separate lines beginning with `#` or `;`.

Omitted keys receive the defaults below. These are the **library defaults**, checked against the current configuration parser, shared limits, and analysis validator. Example INI files and GUI source presets intentionally override some values. In particular, a fresh web launch sets duration to zero and selects HackRF when its adapter is compiled in, otherwise file input; a saved GUI profile takes precedence on subsequent launches without `--config`.

One sample means one simultaneous **complex I/Q pair**, not one component or one byte. CI8 and CU8 use two bytes per pair; SC16 Q11 uses four. Frequencies are in hertz unless stated otherwise.

## Input source: `[frontend]`

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `type` | `file` | `file`, `hackrf`, `rtlsdr`, or `bladerf`; one active source per receiver |
| `serial` | empty | Device identifier, at most 127 bytes; empty requests default selection. Select a particular device when more than one is connected |
| `sample_rate_hz` | `16000000` | Complex pairs/s. HackRF: 2,000,000–20,000,000. RTL-SDR: 225,000 < rate ≤ 300,000 or 900,000 < rate ≤ 3,200,000. File and bladeRF profiles: 1–61,440,000, with additional device limits for live bladeRF |
| `center_frequency_hz` | `1575420000` | Hardware tuning or recorded RF reference, integer 0–6,000,000,000. The selected device must also support the requested tuning |
| `signal_frequency_hz` | `0` | Desired signal centre, integer 0–6,000,000,000. Zero follows hardware tuning. Otherwise the absolute difference from hardware tuning must be strictly less than half the sample rate |
| `bandwidth_hz` | `0` | Accepted alias of `hackrf.bandwidth_hz`, retained for profile compatibility. Integer 0–4,294,967,295 Hz before device validation. Prefer the single canonical key below |

`signal_frequency_hz - center_frequency_hz` defines the digital IF metadata. Setting a signal centre does not retune hardware or enable a digital frequency translator. The strict sampled-band check applies even during file replay.

The bandwidth alias and canonical key address the same setting; do not put both into one profile. Device-supported values and applied sample rates can differ from the generic parser limits. Use **Query device limits** while stopped and review the applied stream information after starting.

## Analogue bandwidth and HackRF: `[hackrf]`

The historical section name is retained: **`bandwidth_hz` also supplies the requested analogue bandwidth to the RTL-SDR and bladeRF adapters**. The remaining controls are HackRF-specific.

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `bandwidth_hz` | `0` | Integer 0–4,294,967,295 Hz, then constrained by the native device API. Zero selects automatic behavior: HackRF rounds down to a supported filter below the rate; bladeRF requests its sample rate as bandwidth; RTL-SDR leaves its automatic/default bandwidth behavior |
| `lna_gain_db` | `16` | 0, 8, 16, 24, 32, or 40 dB |
| `vga_gain_db` | `20` | 0–62 dB in steps of 2 |
| `rf_amp` | `false` | Enable the HackRF RF amplifier |
| `antenna_power` | `false` | Enable HackRF antenna-port power; set according to the connected RF hardware |

HackRF gain step checks are part of complete profile validation, including profiles currently using another source. Analogue bandwidth values are checked by the selected device when it is opened.

## RTL-SDR: `[rtlsdr]`

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `auto_gain` | `true` | Use automatic tuner gain; false selects manual gain |
| `gain_tenths_db` | `0` | Integer −1000–1000 in 0.1 dB units (for example, 200 means 20.0 dB). In manual mode it must exactly match a gain reported by the connected tuner |
| `ppm` | `0` | Integer −1000–1000 parts per million; oscillator frequency correction sent to the native driver |
| `bias_tee` | `false` | Requested RTL-SDR bias-tee state |

RTL-SDR supplies native unsigned 8-bit interleaved I/Q (CU8). Gain choices depend on the tuner. The native tuning API also has a 32-bit frequency limit and device-specific RF coverage.

## bladeRF: `[bladerf]`

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `gain_db` | `30` | Integer −100–100 dB at generic validation, then constrained by the device's frequency-dependent minimum, maximum, and step |
| `bias_tee` | `false` | Requested RX bias-tee state |
| `format` | `SC16_Q11` | `SC16_Q11` or `CI8`. SC16 Q11 preserves 12 significant ADC bits in signed 16-bit I/Q storage; CI8 requests native 8-bit SC8 Q7 transfers |

Native CI8 transfer requires compatible bladeRF hardware/FPGA support. If unsupported, choose SC16 Q11. Query limits after choosing the tuning and precision. The adapter uses manual RX gain and checks the native ranges before starting.

## Signal conditioning: `[conditioning]`

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `module` | `bypass` | `bypass` leaves samples unchanged; `dc_iir` continuously estimates and subtracts the I and Q means |
| `dc_shift` | `16` | Integer 4–24. Estimator coefficient `alpha = 2^(-dc_shift)`; a larger shift responds more slowly |

The DC estimator keeps state across common windows and resets at a stream discontinuity. Its approximate response time in seconds is `2^dc_shift / sample_rate_hz`. Recording and diagnostics can independently choose pre- or post-conditioning samples. The conditioner is not a channel filter, resampler, or carrier tracker.

## Stream buffers: `[buffer]`

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `window_samples` | `16000` | Integer 1–1,048,576 complex pairs per shared processing window |
| `ingress_blocks` | `128` | Integer 2–4096 windows in the bounded live-device callback ingress queue |
| `ring_blocks` | `16` | Integer 2–4096 windows in the central engine ring |
| `overflow_policy` | `stop` | `stop` fails the stream when a mandatory input buffer fills; `drop_newest` rejects incoming data, records known loss, and exposes a discontinuity |

A 16,000-pair window at 16 Msps spans 1 ms. The ingress capacity is approximately `window_samples × ingress_blocks / sample_rate_hz` seconds. File replay has no live-device callback ingress queue.

The profile is rejected if this buffer plan exceeds 512 MiB:

```text
window_samples × bytes_per_pair ×
    (ingress_blocks + ring_blocks + recording.queue_blocks + 2)
```

This is a configuration guard for stream-buffer storage, not a measurement of total process memory. Diagnostics and library/runtime allocations also use memory. More buffering absorbs temporary stalls; it does not increase sustained processing or disk throughput.

## Receiver operation: `[receiver]`

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `mode` | `capture` | `capture` runs the normal sample pipeline; `record` runs capture-only and requires `recording.enabled=true` |
| `run_seconds` | `2` | Finite 0–31,536,000 seconds. Zero runs until Stop, Ctrl+C, source EOF, or a stream error |
| `idle_timeout_ms` | `3000` | Integer 100–3,600,000 ms without frontend progress before reporting a stream error |

Normal `capture` mode may also record when recording is enabled. Neither mode currently attaches satellite acquisition, tracking, or PVT algorithms.

## Recording: `[recording]`

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `enabled` | `false` | Start recording automatically when the receiver starts |
| `point` | `post` | `pre` stores original frontend samples; `post` stores the shared conditioned stream |
| `prefix` | `ngene_capture` | Filename prefix. The GUI requires 1–64 ASCII letters, digits, `_`, or `-` and adds a unique session suffix. The core CLI accepts at most 511 bytes, including a path only when no recording directory is specified |
| `directory` | empty | Receiver-side capture directory, at most 319 bytes in the core profile. Empty uses the web service's output directory or preserves CLI prefix paths. Web mode requires the resolved directory to fit within 240 bytes; its parent must exist |
| `queue_blocks` | `128` | Integer 2–4096 windows in the asynchronous disk-writer queue |
| `segment_samples` | `0` | Integer 0–18,446,744,073,709,551,615 complex pairs per raw segment. Zero disables rotation |

`prefix` must be nonempty when recording is enabled. With `directory` set, `prefix` must be a base filename without `/`, `\`, or `:`. Path strings cannot contain line breaks. The writer creates files exclusively; it does not overwrite existing captures. Disk/write errors or a full writer queue stop the capture with an error rather than silently dropping recorded windows.

At 16 Msps, `segment_samples=16000000` creates nominal one-second raw segments. Native CI8/CU8 throughput is 32 MB/s; SC16 Q11 is 64 MB/s at that rate, before index and metadata overhead. See [Recording and replay](Recording-and-Replay.md).

## File input: `[replay]`

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `format` | `CI8` | `CI8`, `CU8`, or `SC16_Q11`; representation for raw input without overriding capture metadata |
| `path` | empty | Existing, nonempty input file on the receiver computer, at most 511 bytes. It must contain complete pairs and remain unchanged during playback |
| `start_sample` | `0` | Initial complex-pair offset, strictly below the file's pair count. The byte offset must fit a signed 64-bit file position: at most `floor(9223372036854775807 / bytes_per_pair)` |
| `sample_limit` | `0` | Integer 0–18,446,744,073,709,551,615 pairs. Zero reads the remainder; a value beyond the remainder is limited to the available pairs |
| `realtime` | `true` | Pace delivery using the configured complex sample rate; false reads as quickly as the pipeline allows |
| `loop` | `false` | Repeat the selected range; each rewind changes the continuity epoch and reports a gap |

The file source reads the explicitly applied profile; it does not guess RF parameters from raw bytes. **Browse I/Q files…** and CLI `--replay` perform metadata inspection before preparing that profile. A valid sidecar overrides the selected format and recorded stream parameters. Recognized `.cs8`, `.cu8`, and `.sc16q11` suffixes identify the sample representation when no sidecar is present. They do not encode sample rate or RF tuning.

File selection resets the proposed range to the full file, disables recording, selects bypass conditioning, and sets unlimited duration. Review and apply the draft before starting. Plain `--config` file input uses its specified keys directly; use `--replay` when you want metadata preparation.

## Diagnostics: `[analysis]`

| Key | Default | Accepted value and meaning |
| --- | --- | --- |
| `spectrum_enabled` | `true` | Enable the optional shared PSD producer for spectrum and waterfall; actual work also requires consumer demand |
| `histogram_enabled` | `true` | Enable the independent I/Q histogram/statistics producer; actual work also requires consumer demand |
| `fft_size` | `2048` | 256, 512, 1024, 2048, 4096, 8192, or 16384 complex pairs per FFT |
| `window` | `hann` | `rectangular`, `hann`, `hamming`, `blackman`, or `blackman_harris` |
| `overlap_percent` | `50` | Integer 0–99% overlap between Welch segments |
| `averages` | `4` | Integer 1–64 Welch segments per PSD observation |
| `update_hz` | `4` | Finite 0.1–30 PSD observations/s; a maximum capture cadence, not a guaranteed output rate |
| `spectrum_source` | `post` | `pre` observes original frontend input; `post` observes conditioned samples |
| `units` | `dbfs_bin` | `dbfs_bin` for dB relative to digital full scale per FFT bin; `dbfs_hz` for density per hertz |
| `temporal` | `none` | `none`, `mean`, or `ema`; average successive observations in linear power before conversion to dB |
| `temporal_frames` | `4` | Integer 1–16 history observations for temporal `mean` |
| `smoothing_alpha` | `0.2` | Finite newest-frame weight in `(0,1]` for `ema`. Larger values follow changes faster. The GUI number control uses 0.001 increments |
| `histogram_update_hz` | `4` | Finite 0.1–30 independent histogram observations/s |
| `histogram_pairs` | `65536` | Integer 1024–262144 contiguous complex pairs per histogram/statistics observation |
| `histogram_bins` | `64` | 16, 32, 64, 128, or 256 equal-width bins over the native ADC component range |
| `histogram_source` | `post` | Independent `pre` or `post` choice for histogram/statistics input |

All dimensions remain validated even when the corresponding diagnostic is disabled. A Welch observation must fit the 262,144-pair diagnostic capture capacity:

```text
hop = floor(fft_size × (100 - overlap_percent) / 100)
observation_pairs = fft_size + (averages - 1) × hop
observation_pairs <= 262144
```

At the defaults this is 5120 pairs. Observation length is independent of `buffer.window_samples`; the worker can collect a burst across several contiguous common windows.

The GUI Enable checkboxes for spectrum and waterfall also update `spectrum_enabled` at runtime. The last one off suppresses new FFT/PSD work; the first one on restarts it even if the saved profile initially disabled it. Histogram demand is separate. See [Signal diagnostics](Signal-Diagnostics.md) for demand, cancellation, and practical CPU settings.

## Display preferences

These browser controls are presentation preferences, **not INI keys** and not parameters of the FFT calculation.

| Control | Initial value and bounds | Meaning |
| --- | --- | --- |
| Refresh | 4 Hz; choices 1, 2, 4, 10, 20, 30 Hz | Browser status request cadence |
| Spectrum minimum / maximum | −100 / 0; form limits −250–99 / −249–100 | Vertical plot range in the selected diagnostic units; minimum must be below maximum |
| Waterfall colour minimum / maximum | −100 / 0; same form limits | One colour scale applied to all waterfall rows |
| Frequency reference | Hardware-centred / baseband | Baseband kHz, signal-centred kHz, or absolute RF MHz; labels only |
| Frequency minimum / maximum | Empty: full band | Visible frequency range in the current reference's units |
| Waterfall history | 128; 32–2048 rows | Browser-retained PSD history |
| Waterfall rate limit | 4; 0.1–30 Hz | Maximum insertion rate of new displayed rows |
| Histogram scale | Counts | Counts or percent of the observation |
| Histogram Y maximum | 0 | Zero selects automatic scaling; positive values set the display maximum |
| Spectrum / waterfall height | 260 / 270; 180–720 pixels | Plot height in the browser |
| Waterfall view | 2D | 2D image or rotatable 3D surface |

**Fit current spectrum & colours** changes axes and colour limits to the current PSD range. **Full frequency band** clears the visible frequency limits. These actions do not retune the SDR or alter stored I/Q samples.
