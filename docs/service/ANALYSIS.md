# Signal analysis

Engineering reference. For current operator instructions and native I/Q formats, see the
[diagnostics manual](../wiki/Signal-Diagnostics.md). The CI8 arithmetic examples below retain
the original numerical explanation; current input formats also include CU8 and SC16 Q11.

Frontend → pre-conditioning tap → shared conditioning (bypass/DC IIR) → single GNSS ring →
same window delivered to every channel → post-conditioning tap. The recording point is
selected while stopped. PSD and histogram select their taps independently; there is no
second GNSS ring.

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

With spectrum and waterfall disabled, the GUI does not validate or copy PSD data, accumulate
history, or prepare bitmaps, meshes, projections or Canvas drawing. Resize callbacks and
already queued redraws check the enable state again. Histograms remain independent.
Separate PSD and histogram sequences avoid redrawing unchanged data or an unrelated panel;
the waterfall redraws only when it accepts a new row, respecting its own rate. While paused,
explicit rotation and resizing can redraw cached data without producing new FFTs.

Spectrum and waterfall have independent header **Enable** switches. The first enabled plot
also sets the runtime producer to `spectrum_enabled=true`; disabling the last sets it to
false. Disabled content is hidden. Either enabled plot maintains their shared PSD demand.
A disabled spectrum is not drawn while the waterfall continues; a disabled waterfall does
not accumulate history while the spectrum continues. Changing the same Diagnostics setting
synchronizes both switches, including while paused. The runtime update uses the server's
applied settings and preserves unrelated parameters and local drafts. Saving the profile is
required to persist it to INI.

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

For CI8: ×128 → Q15 window → Q14 FFT /N → uint64 power and Welch accumulation. Segment
averaging uses linear power. Temporal mean/EMA uses double linear power, followed by log10
for display. FFT butterflies remain integer. Configuration, epoch or detected discontinuity
changes reset temporal state.

Coherent gain G=sum(w)/N; ENBW=Fs*sum(w²)/(sum(w))². dBFS/bin uses tone normalization
(CI8 radius 128 = 0 dBFS); dBFS/Hz divides normalized power by ENBW. This is not dBm.
Integer quantization and numerical noise remain: a longer FFT does not guarantee greater
dynamic range. Bin zero is never erased.

FFT-shifted output is reduced to at most 1024 display values using maximum pooling for larger
FFTs; peak frequency and Δf retain native resolution. Pooling preserves peaks rather than
integrated noise power. The waterfall is a sequence of snapshots, rather than a continuous
STFT. Every row uses the same colour scale; history, rate and axes have separate controls.

The original signed 8-bit histograms cover [-128,127], with uint32 counts and 64-bit moment
accumulators. Current native formats use their corresponding integer ranges. NGene computes
mean, standard deviation, RMS, minimum/maximum, zeros, rail counts and DC magnitude. Rail
counts do not establish RF clipping. Choose the pre-conditioning tap to inspect input bias
and the post-conditioning tap to inspect the conditioner.

[Implementation, tuning, ownership, benchmarks and limitations](ITERATION_04.md).
