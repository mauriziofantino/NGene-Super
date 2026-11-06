# Signal analysis

Engineering reference. For current operator instructions and native I/Q formats, see the
[diagnostics manual](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Signal-Diagnostics). The CI8 arithmetic examples below retain
the original numerical explanation; current input formats also include CU8 and SC16 Q11.

Frontend → shared bypass → single common ring → same window delivered to every channel.
PSD and histogram workers copy independent diagnostic bursts. Legacy pre/post tap API
fields remain compatible, but their redundant UI selectors have been removed.

Current frontend streams are common signed CI8 or CI16 Q15; legacy CU8/Q11 replay is
converted before publication. Histogram amplitudes and rail indicators use the common
representation. Source ADC bits are separate metadata. Before statistics or FFT processing,
each worker subtracts its burst's rounded I/Q mean using SIMD and widened int32 storage.
There is no IIR or streaming DC remover; shared and recorded samples remain unchanged.
See [frontend decisions](FRONTEND_STREAM_DECISIONS.md).

## Capture and ownership

An optional worker computes PSD and histograms. Each lane has two private bursts of at most
262144 complex pairs, with independent lengths and rates. The tap uses a try-lock and bounded
copy, skipping a submission on contention; it performs no DSP or I/O. A discontinuity
discards an incomplete burst. Snapshots are published by value, deeply copied before
telemetry and copied again into browser history. No frontend/ring pointer crosses these
boundaries.

The worker selects the most recent ready burst and exposes one current snapshot. The browser
allows one outstanding status request and retains only the latest frame awaiting rendering.
Skips, contention and invalid frames are counted. A skipped sequence never creates a
synthetic waterfall row. The GUI withdraws interest when panels are unused. The server
combines the connected browsers' requests and revokes capture when the last consumer disables
a panel or closes its connection. A two-second lease handles clients that stop requesting
updates. The capture-only CLI does not instantiate the worker.

With spectrum and both waterfalls disabled, the GUI does not validate or copy PSD data,
accumulate history, prepare bitmaps or draw canvases. Resize callbacks and
already queued redraws check the enable state again. Histograms remain independent.
Separate PSD and histogram sequences avoid redrawing unchanged data or an unrelated panel;
the waterfalls redraw only when their shared history accepts a new row, respecting its
rate limit. While paused, resizing and reenabling can redraw cached data without new FFTs.

Spectrum and both waterfalls have independent header **Enable** switches. The first enabled plot
also sets the runtime producer to `spectrum_enabled=true`; disabling the last sets it to
false. Disabled content is hidden. Any enabled plot maintains their shared PSD demand.
A disabled view skips bitmap preparation and drawing. The single waterfall history advances
while either waterfall is enabled; with both disabled it stays frozen while the spectrum
continues. Changing the same Diagnostics setting synchronizes all three switches, including
while paused. The runtime update uses the server's
applied settings and preserves unrelated parameters and local drafts. Saving the profile is
required to persist it to INI.

The 3D view projects frequency/time/power as an orthographic surface: power changes
vertex height and foreground surfaces can hide background surfaces. The colour-only
relief implementation was rejected by the user on 2026-11-05. The views share stored
rows, bins, frequency crop, power normalization, pause and Clear. Their screen time
coordinates differ because 3D adds power height. Projection uses the configured history
capacity, so filling history does not rescale existing ages. A per-frequency visibility
horizon avoids drawing hidden strips without pooling bins or rows. The renderer is bounded
by 2048 history rows and 1024 telemetry bins and runs only in the browser.

The backend discards incomplete and queued bursts when interest is revoked. Active work
checks cancellation between bounded stages and does not publish cancelled results. An FFT
already executing may finish before the next check. An idle worker waits for notification
without periodic polling. WebSocket replies omit arrays and checksum work for unrequested
diagnostics; receiver metadata, counters and recording remain available.
`spectrum_enabled=false` suppresses calculation even if a browser continues to request it.

Each diagnostic instance uses less than 6 MiB for its workspace, temporal history and plan,
separate from receiver queues. There are no per-block allocations. Window coefficients are
regenerated only when their shape or size changes. Recording retains its overflow/error
policy; recordings are not dropped to keep the GUI smooth.

## Runtime parameters

| INI `[analysis]` key | Values / default |
| --- | --- |
| spectrum_enabled / histogram_enabled | true/false; both default to true |
| spectrum_source / histogram_source | pre/post; both default to post |
| fft_size | 256, 512, 1024, 2048, 4096, 8192, 16384; default 2048 |
| window | rectangular, hann, hamming, blackman, blackman_harris; default hann |
| overlap_percent | Integer 0..99; default 50; hop=floor(N*(100−overlap)/100) |
| averages | 1..64 Welch segments; default 4; N+(segments−1)*hop ≤262144 |
| update_hz | PSD 0.1..30 Hz; default 4 |
| histogram_update_hz | Histogram 0.1..30 Hz; default 4 |
| histogram_pairs | 1024..262144; default 65536 |
| histogram_bins | 16, 32, 64, 128, 256; default 64 |
| units | dbfs_bin / dbfs_hz; default dbfs_bin |
| temporal | none / mean / ema; default none |
| temporal_frames | 1..16; default 4; used for mean |
| smoothing_alpha | New-frame weight (0,1]; default 0.2; used for ema |

Rates bound burst start frequency; capture and computation can lower the achieved rate.
FFT contents are not decimated: samples within each FFT are contiguous. Overlap applies to
Welch segments inside a burst, rather than GUI updates. Histograms always use their own
sample count, independently of NFFT.

GET/POST `/api/v1/analysis/config`; writes require a token and revision. A new revision
invalidates earlier bursts and results. **Apply analysis settings** changes the in-memory
session/profile. **Apply & save** while stopped persists it. INI export includes runtime
changes.

## Arithmetic and display

For CI8: block mean subtraction → ×128 → Q15 window → Q14 FFT /N → uint64 power and Welch accumulation. Segment
averaging uses linear power. Temporal mean/EMA uses double linear power, followed by log10
for display. FFT butterflies remain integer. Configuration, epoch or detected discontinuity
changes reset temporal state.

Coherent gain G=sum(w)/N; ENBW=Fs*sum(w²)/(sum(w))². dBFS/bin uses tone normalization
(CI8 radius 128 = 0 dBFS); dBFS/Hz divides normalized power by ENBW. This is not dBm.
Integer quantization and numerical noise remain: a longer FFT does not guarantee greater
dynamic range. Bin zero is not overwritten; block-mean subtraction removes the constant
component before windowing. Any useful signal exactly at DC is removed along with offset.

FFT-shifted output is reduced to at most 1024 display values using maximum pooling for larger
FFTs; peak frequency and Δf retain native resolution. Pooling preserves peaks rather than
integrated noise power. The waterfall is a sequence of snapshots, rather than a continuous
STFT. Every row uses the same colour scale; history, rate and axes have separate controls.

Histograms describe samples after mean subtraction, using uint32 counts and 64-bit moment
accumulators. The axis covers the input format's signed range and doubles if needed to retain
centered excursions without clipping into an end bin. NGene computes residual mean,
standard deviation, RMS, minimum/maximum, zeros and residual DC magnitude, and separately
reports the removed I/Q offsets. Endpoint counts refer to input stream values before subtraction;
they do not establish physical ADC or RF clipping. Burst-mean subtraction and its checks are
documented in [calibration and DC verification](CALIBRATION_DC.md).

[Implementation, tuning, ownership, benchmarks and limitations](ITERATION_04.md).
