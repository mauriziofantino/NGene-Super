# Signal diagnostics

NGene diagnostics inspect the sampled input. They help identify DC offset, strong narrowband energy, gain problems, clipping indicators, and stream continuity issues. They do not identify satellites, measure tracking C/N0, decode navigation messages, or produce a position fix.

## Spectrum and units

The spectrum uses a windowed integer complex FFT, with Welch averaging over the configured overlapping segments. The displayed band is FFT-shifted, from approximately `-sample_rate / 2` to `+sample_rate / 2` in the hardware-baseband reference.

| Quantity | Interpretation |
| --- | --- |
| FFT size | Number of complex pairs in each transform |
| Δf | Physical FFT bin spacing, `sample_rate_hz / fft_size` |
| ENBW | Equivalent noise bandwidth of the selected window, in hertz |
| dBFS/bin | Spectral power relative to digital full scale using window coherent-gain correction |
| dBFS/Hz | Power density obtained by normalizing linear power by the window's ENBW |
| Peak | Largest published spectral value and its frequency in the chosen reference |

The values are relative to the ADC's digital full scale. They are not calibrated antenna-port dBm. Changing analogue gain changes ADC levels; a power reading alone does not identify an RF source.

The diagnostic FFT accepts 256 through 16,384 points in powers of two. At 16 Msps, 2048 points give 7812.5 Hz bin spacing; 16,384 points give 976.5625 Hz. Larger FFTs increase work and require a longer uninterrupted observation. The browser receives at most 1024 spectral values: larger FFTs are reduced by retaining the maximum power in each group. The reported Δf remains the physical FFT spacing, even though a displayed point can represent several FFT bins.

Select the window in **Configuration → Diagnostics**. Available windows are rectangular, Hann, Hamming, Blackman, and Blackman-Harris. Window choice affects spectral leakage and ENBW. The application reports the selected window and its ENBW alongside the spectrum.

## Averaging and capture cadence

**Welch segments** averages linear power from several FFTs within one contiguous captured burst. **FFT overlap** changes the spacing between those segments. The worker validates the full burst length; some combinations of a large FFT and many segments exceed its 262,144-pair capture limit.

**Temporal power averaging** operates across completed observations:

- **none** publishes each Welch result without temporal smoothing.
- **mean** averages up to the chosen number of recent observations.
- **ema** weights the newest linear-power result by `smoothing_alpha` and the previous result by `1 - smoothing_alpha`.

Averaging precedes logarithmic conversion. A smaller EMA coefficient responds more slowly. Configuration changes, stream discontinuities, and restarting demand invalidate older accumulation so that incompatible observations are not combined.

The **Spectrum rate limit** is the maximum capture cadence, not a command to transform every incoming sample. Diagnostics collect bounded observations from the shared stream. When the worker or its storage is busy, diagnostic observations may be skipped so that optional plots do not make the receiver wait.

## Frequency reference

Choose **Configuration → Display → Frequency reference**:

| Reference | Displayed value |
| --- | --- |
| Hardware-centred / baseband | FFT frequency relative to hardware tuning, in kHz |
| Signal-centred | Baseband frequency minus digital IF, in kHz |
| Absolute RF | Hardware tuning plus baseband frequency, in MHz |

Changing the reference changes labels only. It does not retune the SDR, shift samples, or remove an interferer. The signal centre must be inside the sampled hardware band. Digital IF uses the convention `signal frequency - hardware tuning frequency`.

## Waterfall

The waterfall draws successive published PSD observations. It shares the spectrum's FFT and averaging settings. It does not compute a second transform. A row is an observation at a given completion time, not a contiguous recording of every input window.

In 2D the newest row is at the top; in the default 3D camera it is at the front. The 3D axes show frequency, observation age, and power. **Waterfall history** limits retained rows; **Waterfall rate limit** limits insertion of rows independently of the PSD producer cadence. The 3D surface draws a bounded subset of the retained rows and columns to keep browser rendering manageable.

Colour minimum and maximum apply to all rows together, making their levels comparable. **Clear history** clears browser history. It does not reset the RF source or delete recordings. See [GUI guide](GUI-Guide.md) for camera and keyboard controls.

## I/Q histogram and statistics

Histograms use a separately configured contiguous observation, cadence, and pre/post-conditioning source. Disabling the spectrum does not disable histograms.

| Statistic | Meaning |
| --- | --- |
| Mean | Average signed ADC component value over the histogram observation |
| Std | Population standard deviation |
| RMS | Root-mean-square component amplitude |
| Min / Max | Lowest and highest observed component values |
| Clipping / rail count | Number of component values at either nominal ADC endpoint |
| Zero count | Number of exactly zero component values |
| DC magnitude | `sqrt(mean(I)^2 + mean(Q)^2)` in ADC counts |

CI8 uses signed levels −128 through 127. CU8 stores 0 through 255 and is recentered by subtracting 128 for analysis. SC16 Q11 uses ADC levels −2048 through 2047 in signed 16-bit storage. Histograms group these native levels into 16, 32, 64, 128, or 256 bins without reducing SC16 samples to 8-bit precision. Statistics use the original component values, rather than the centres of histogram bins.

A nonzero rail count is a clipping indicator: it reports endpoint samples, rather than proving how an upstream analogue stage behaved. Compare gain settings and the signal's distribution. A DC offset appears as nonzero means and DC magnitude; compare **pre** and **post** observations to see the effect of `dc_iir` conditioning.

Counts and percent display modes change the histogram's vertical presentation only. They do not change the statistics or recorded samples.

## Disable calculation to save CPU

| Operator action | Calculation behavior |
| --- | --- |
| Disable spectrum, leave waterfall enabled | Shared FFT/PSD continues for the waterfall |
| Disable waterfall, leave spectrum enabled | Shared FFT/PSD continues for the spectrum |
| Disable both Enable checkboxes | Runtime `spectrum_enabled` becomes false; no new PSD observations or FFT calculations are started |
| Enable either plot again | Runtime `spectrum_enabled` becomes true; fresh observations and FFT/PSD calculations resume, even if the starting profile had disabled them |
| Hide I/Q histograms | Withdraws this browser's independent histogram demand |
| Pause plots | Withdraws this browser's PSD and histogram demand while input and recording continue |
| Switch to Configuration or hide the browser tab | Withdraws this browser's diagnostic demand |

The spectrum/waterfall Enable change updates the receiver's session configuration, rather than merely hiding a canvas. An obsolete queued or partial observation is discarded. A calculation already executing is cancelled between bounded processing stages; an individual FFT call already running can finish, and its obsolete result is discarded.

Consumer demand is aggregated. Another active browser or diagnostic API consumer can keep a permitted producer running after this browser pauses or hides its plots. Without any interested consumer, the corresponding diagnostic work stops after the consumer lease expires. With both diagnostic lanes inactive the worker waits instead of repeatedly polling for samples. The receiver still performs the input, conditioning, buffering, control, and recording work that is needed for the run.

To reduce CPU use while keeping a preview, first lower **Spectrum rate limit** and **Welch segments**, then reduce **FFT size** if its frequency resolution is unnecessary. Reduce histogram rate or observation length separately. Use 2D rather than 3D when browser rendering is the limiting workload. Display refresh, waterfall history, and colour limits affect presentation and should not be confused with the FFT calculation settings.

For example, a low-rate preview can use a 1024-point FFT, one Welch segment, and a 1 Hz spectrum limit. This trades frequency detail and averaging for lower computation; it does not alter the native I/Q recording precision.

## Frame and stream integrity

**Frame integrity** shows PSD publication identity, finite-value counts, summary statistics, and a checksum. The browser rejects stale or inconsistent PSD frames. This check covers transfer/display consistency, rather than establishing RF authenticity or complete hardware continuity.

**Skipped bursts** and **contended blocks** describe optional diagnostics that were skipped when observer storage or a lock was busy. **Display skips** describe PSD publications not shown as separate browser updates. These counters are different from **known input losses**, transport errors, or recording failures. Inspect the stream health and input integrity panels to assess actual sample delivery.
