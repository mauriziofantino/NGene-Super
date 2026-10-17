# NGene 0.4 — diagnostic, DC, and tuning consolidation

> Historical report, 2 October 2026. Verification results and limitations describe that revision, not the current release. See the [current operator manual](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Home).

2 October 2026. Incremental changes to project 0.3; original NGene remains the basis. Frontend → shared conditioning → single ring → channels remains the receiver path. No changes to existing FFT butterflies to correct the waterfall.

## Waterfall issue outcome

**Reproducible cause identified in the GUI:** `renderAnalysis()` added `null` to history when a sequence was missing, and `drawWaterfall()` painted that `null` as an amber row `[230,173,88]`. Producer and browser refresh were not synchronized: an ordinary update skip therefore generated an artificial horizontal row. This explains why the effect increased with rate, even with a valid PSD.

The artificial row is removed. Skips are counted as `display skips`; they do not become rows, zeros, or synthetic frames. The browser test exercises a 12.5 Hz producer and 1 Hz display, verifies actual skips and a history containing only finite frames. An intentionally altered PSD is rejected without changing history. This does not claim to rule out every possible RF artifact or USB fault on the user's hardware.

A second issue was fixed: the server silently ignored WebSocket requests arriving too close together, leaving the browser waiting. New pacing is consistent with GUI options up to 30 Hz; an abusive request closes the connection instead of leaving it suspended. Rates are upper limits, not guaranteed deadlines.

## Ownership and integrity

| Step | Ownership / check |
|---|---|
| Frontend/ring → diagnostics | Bounded copy into private workspace, no RF pointer retained |
| Worker → snapshot | Arrays published by value under mutex; workspace being processed is not reused by the producer |
| Snapshot → telemetry | Explicit deep copy before serialization; mutexes released before JSON and socket operations |
| WebSocket → browser | Only one outstanding status request; only the latest message waits for `requestAnimationFrame` |
| Browser → history | Frozen copy of the PSD array; bounded history, no reference to a later frame's buffer |

No lifetime/ownership corruption appeared in the tests performed. Telemetry retains a deep copy to make this boundary verifiable. Every PSD exposes `sequence_id`, `produced_seconds` (process monotonic clock), min/max/mean, NaN/Inf counts, and an FNV-1a checksum over signed milli-dB values in little-endian order. `telemetry_checksum` is recomputed over the copy. The browser verifies length, finiteness, statistics, and hash before accepting a frame. The hash is a diagnostic check, not a cryptographic signature. Quantization to 0.001 dB happens once before publication.

## Histograms and statistics

PSD and histogram have independent collection, length, frequency, and pre/post selection. NFFT no longer determines histogram burst length. Changing the analysis profile starts a new revision and clears old results; on a real signal, different time windows may still have different statistics.

I and Q show mean, population standard deviation, RMS, minimum/maximum, counts at −128/+127 rails, and zero counts. DC magnitude is `hypot(meanI, meanQ)` in ADC units. Rails indicate clipping, not an automatic RF saturation diagnosis. Bias remains visible.

Known-distribution test: I alternates 20/22 → mean 21, std 1, RMS √442; Q=12 → mean/RMS 12 and std 0. Pre histograms unchanged when switching NFFT from 256 to 4096 over the same data; separate post spectrum. Signedness test covers all 256 int8 codes, SIMD tails, and int16 powers including `(-32768)^2+(-32768)^2 = 2^31`. The reference build also uses `-funsigned-char`.

## DC offset

Official [HackRF documentation](https://hackrf.readthedocs.io/en/stable/troubleshooting.html) describes quadrature receiver bias and offset tuning. An external RF DC block does not subtract a component introduced internally in the conversion chain.

`conditioning.module=dc_iir` enables a continuous Q32 DC estimator, separately for I and Q:

- `y[n] = x[n] - round(mean[n-1])`, with CI8 saturation;
- `mean[n] = mean[n-1] + 2^-dc_shift * (x[n] - mean[n-1])`;
- `dc_shift=4..24`, default 16; approximate time constant `2^shift/Fs`, transition frequency approximately `Fs/(2π 2^shift)`;
- State preserved across blocks, reset only at stream start, gaps, new epochs, or index discontinuities.

Default remains `bypass`. The FFT DC bin is not zeroed and data are not hidden. The IIR is scalar with continuous state: no forced vectorization of the recurrence. Output remains CI8, so quantization exists and saturation can occur after subtraction. DC removal also attenuates useful signals near hardware zero; the initial transient is real. Pre recording preserves the original signal.

## Tuning, axes, and future NCO

Historical key `frontend.center_frequency_hz` remains **hardware tuning**; new `frontend.signal_frequency_hz` is the signal center. A value of 0 follows hardware tuning to retain compatibility with old profiles.

```ini
[frontend]
type=hackrf
sample_rate_hz=16000000
center_frequency_hz=1576420000
signal_frequency_hz=1575420000
```

This gives `digital_if_hz = −1000000`. The HackRF setter receives only hardware frequency. Stream information, file metadata, and telemetry report both values and their signed difference. The signal center is checked to fall within the sampled band; occupied signal bandwidth and analog-filter margin must still be selected correctly.

The GUI provides three references: hardware baseband (`f_bin`), signal-centered (`f_bin − digital_IF`), and absolute RF (`hardware_frequency + f_bin`). Axes and labels change, not samples. Displayed sample rate is the nominal rate applied/accepted by the frontend, not a calibrated oscillator measurement: libhackrf provides no metrological readback here.

C helpers `NgCarrierFromDoppler()` and `NgDopplerFromCarrier()` establish the contract `NCO = digital_IF + Doppler` and `Doppler = carrier − digital_IF`. **Acquisition, tracking, and PVT are not yet connected in the incremental project:** the reference is prepared and tested, not a claim of implemented satellite search. No global mixer is introduced over the 16 MS/s stream.

## GUI and diagnostic cost

Configuration/Observation and the dark theme remain. Discrete FFT sizes 256..16384; overlap at any integer percentage 0..99; Welch with 1..64 segments, within a 262144-pair burst limit. Five windows; separate PSD/histogram rates 0.1..30 Hz; linear temporal averaging over 1..16 frames or EMA with coefficient (0,1].

dBFS/bin or dBFS/Hz scale using the quantized window's ENBW. In dBFS/bin, a coherent/DC tone has /N-normalized level: larger NFFT does not increase its physical amplitude; Δf, per-bin floor, and visual separation change. In dBFS/Hz, a coherent line's spectral height depends on resolution, as expected for density. No dBm calibration. Above 1024 bins the display preserves group maxima: do not integrate it as a complete PSD to estimate total power.

Scale controls: spectrum min/max, global waterfall color min/max, frequency limits, one-shot fit, histogram counts/percentage and Y maximum. The same color thresholds apply to all retained rows. History 32..2048 rows and independent waterfall rate; no continuous STFT of every sample is constructed.

Continuous raw I/Q preview is no longer transmitted. PSDs, histograms, metrics, and state are sent. The browser renews diagnostic interest only in Observation with the relevant panels enabled; Configuration, pause, hidden tabs, and page closure let interest expire after two seconds. Multiple clients keep active whatever at least one requests. The recorder and channels continue.

Diagnostic taps use try-lock: if busy, they skip the block and increment `contention_blocks`; any incomplete burst is discarded at the next discontinuous index. The worker processes the newest ready data without accumulating a plotting queue. The recorder lock is separate from the GUI lock. Receiver synchronization and I/O remain, so no hard real-time claim is made.

## SIMD and benchmarks

New primitives in `src/dsp/sample_ops.c`: deinterleaving/sign extension and I/Q moments, complex power with uint64 accumulation, SSE2/NEON intrinsics, and C reference. No mandatory AVX2; `NGENE_SAMPLE_REFERENCE=ON` forces the reference. Histogram, windowing, and IIR keep simple paths; new correlators/mixers are not invented in this increment.

Linux x86-64, GCC Release `-O3`, 16000 pairs per call, 10000 repetitions per primitive, 1000 for DC:

| Operation | C reference | SSE2 / implementation | Ratio |
|---|---:|---:|---:|
| CI8 moments | 50.615 µs | 3.751 µs | 13.49× |
| CI16 power and accumulation | 11.153 µs | 5.371 µs | 2.08× |
| Continuous Q32 DC IIR | — | 110.141 µs / 16000 pairs | scalar |

Reproduce: `ngene_sample_tests --benchmark` in Release. Results apply to the test environment; the C reference is the distributed one, not every possible scalar implementation. No NEON benchmark on ARM hardware or overall CPU percentage is claimed.

## Windows and remaining issues

Windows build is intended to be **native MSVC**, using x64 MASM and ARM64 ARMASM64; explicit presets and CI, no MinGW dependency. Minimum CMake 3.26 for ASM_MARMASM. The ARM64 preset supports cross-builds from an x64 Windows host with the ARM64 toolset installed. See `WINDOWS.md`.

Linux, browser, sanitizer, and Linux ARM64 cross-build tests are described in `VALIDATION.md`. That environment had no Windows/MSVC: new MSVC/ARMASM paths were prepared but not compiled/executed natively there. Real HackRF, real Windows and ARM, long-run/overall throughput, full Windows Unicode paths, and GNSS algorithm migration remain to be tested. FFT arithmetic routines from 0.3 are unchanged; only equivalent ARMASM64 syntax is added.
