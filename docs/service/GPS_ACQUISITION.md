# First GPS L1 C/A acquisition module

This increment implements GPS PRNs 1..32. It does not implement tracking, navigation
decoding, Galileo/SBAS acquisition or PVT. A confirmed acquisition is a capture-relative
observation, not a channel lock or navigation fix.

## Running

Build the normal Release target, then from the repository root:

```powershell
.\build\msvc-x64-multisdr\Release\ngene.exe --web --config config/gps-hackrf.ini
# Other initial device profiles: config/gps-rtlsdr.ini and config/gps-sige.ini
```

Open `http://localhost:8080`, start the receiver and open Acquisition. With an existing
saved capture profile, enable GPS L1 C/A under Settings / Acquisition and apply before
starting. Generic capture defaults keep acquisition disabled. CLI without `--web`
prints the latest per-PRN outcomes at the end of the run. Stop receiver cancels the
outstanding search. Recording-only mode never registers acquisition.

Initial profiles use HackRF 16 Msps / zero IF, RTL-SDR 2.4 Msps / zero IF and native
SiGe 8.1838 Msps / 38.5 kHz residual IF. Gains and oscillator corrections must match the
actual antenna/device. No gain calibration or antenna-power setting is implicitly changed.
The implementation takes rate and IF from the opened stream; those values are not
hardcoded inside the acquisition strategy. The initial admitted rate range is
2.048..20 Msps. This is not a claim that every rate/filter combination is validated.

## Kernel and replacement contract

`include/ngene/acquisition.h` defines `TNgAcquisitionStrategy`. A replacement supplies
one `search` operation and a caller-owned context to `NgAcquisitionCreateWithStrategy`.
The runner returns a normal `TNgChannel` registration. The existing engine and ring
ownership rules remain unchanged: the callback receives the immutable common ring
window, copies samples into one preallocated 56-ms buffer, and returns its lease.
It never retains ring pointers and performs no FFT, allocation or I/O on that callback.

One worker searches the owned capture. Capture memory remains immutable until the
strategy returns. Samples received while the slot is occupied are counted as samples
outside acquisition captures; they are not losses from tracking, recording or the common
stream. A known gap, changed epoch or unexpected sample index invalidates a partial
capture and cancels a pending search. A stopped receiver joins the worker before freeing
its capture/context. Alternative strategies must poll the supplied cancellation predicate.

The capture contains raw ADC levels converted losslessly to float from CI8/CI16, plus
actual stream metadata and epoch/first-sample provenance. Spectral convention correction
belongs to the strategy; the runner does not silently change Q for custom modules.
Completed results retain their capture origin even when later PRNs use a newer capture.
An acquired PRN does not transition to TRACKING until a real tracking module exists.

Public DSP services are independent of the runner: integer SIMD FFT (`dsp/fft.h`),
floating complex search FFT, carrier/code replicas, mixing and prompt correlation
(`dsp/search.h`), and GPS chips/persistent NCOs (`dsp/gps_ca.h`, `dsp/nco.h`). Plans
are immutable and workspaces belong to the caller. Search kernels use SSE2 on x64;
the portable implementation remains available on other targets. There is no per-sample
sine/cosine evaluation. Carrier recurrence uses double precision; prompt reductions
accumulate in double. The floating FFT uses single precision and is specific to search;
tracking/mixer/PVT implementations are not replaced by this increment.

`square_carrier=false` uses sinusoidal rotation. `true` selects squared sin/cos; native
refinement and confirmation generate the complete IF-plus-Doppler oscillator.
`xor_correlation=true` applies the sign bit of the bipolar local code with SIMD XOR.
It is mathematically equivalent to multiplication by +/-1 and preserves input amplitude;
it does not hard-limit a multi-bit SDR to one bit.

## Classic logic and numerical adaptation

Reference implementation: Classic `acquisition/coarseAcquisition.c`, `refinement.c`,
`confirmation.c`, `acquisition.c`, and the GPS defaults in `Acquisition.conf`.

The sequence is retained:

1. Coarse PCPS over +/-5 kHz, a 667-Hz grid plus the positive endpoint, eight
   noncoherent 1-ms coherent powers. The first 8 ms are common to all hypotheses.
2. Doppler refinement over +/-667 Hz in 10-Hz steps, on the next 8 ms.
3. Code refinement over approximately +/-1 chip with one native-sample step, on
   the next 8 ms. Code rate includes the Doppler correction.
4. Three successful confirmations out of at most four disjoint 8-ms intervals.
   Failures are rejected as soon as three successes become impossible.

Numerical differences from Classic are explicit. The strategy uses native complex
I/Q, fractional DC centering on its private capture, rational box integration to
4096 samples/ms (2048 below 4.096 Msps), and a floating FFT without the legacy FFT
gain shifts or byte packing. Absolute rational sample boundaries avoid rounding the
SiGe rate to 8184 samples/ms. All phase outputs reference the capture's first sample;
code phase is a local replica advance in [0,1023), not an unspecified delay index.

The coarse threshold retains Classic's gamma survival equation for a sum of eight
complex prompt powers, with per-cell Pfa=1e-9. Its noise population is measured at
a fake Doppler of 22 kHz. Confirmation uses the same probability law in native prompt
units, estimated using eight wrong-Doppler references on each fresh confirmation interval.
The candidate code phase/rate are preserved; only the carrier is shifted by
+20..27 kHz, outside search. This replaces the previous off-code estimator. The
shared policy, numerical impact and statistical limits are documented in
[Galileo acquisition and common noise policy](GALILEO_ACQUISITION.md).
This explicitly replaces the legacy fixed FFT-to-prompt gain factor; its numerical
threshold is therefore not bit-equivalent to Classic. Eight noncoherent sums are fixed
for this increment; Classic's progressive multi-satellite search scheduler is not ported.
These tests do not establish an experimentally measured system-wide false-alarm rate.

## SiGe frequency convention

Classic's native SiGe input uses the frequency plan documented in its 5 November
logbook: residual IF is 4130400 - 16367600/4 = 38500 Hz. Its mixer rotates by exp(+j*phi)
and its SiGe Doppler correction is +1; Classic's HackRF correction is -1. Consequently,
the two device profiles cannot be transferred by copying an unqualified Doppler sign.

Evo's search convention is rotation by exp(-j*phi), with carrier equal to digital IF
plus Doppler. Native SiGe is labelled `spectrumInverted` and the default strategy conjugates
its private complex samples to express them in that convention. I and Q are not swapped.
The ADC stream, histograms, recorder and common ring are not conjugated. Recordings
preserve `spectrum_inverted=1` in both session and segment metadata. Replay loads it;
old raw SiGe files without a sidecar require `replay.spectrum_inverted=true` after decoding
their marker-tagged transport bytes to signed CI8. This is a frequency-reference
adaptation, not an RF fault diagnosis.

## Verification and limits

`gps_acquisition_contract` exercises synthetic GPS with independently calculated sample
times, known carrier/code phase, RTL/HackRF/SiGe rates, both carrier modes, SIMD XOR and
products, pure noise, and a signal that disappears before confirmation. It also tests
FFT scaling, exact sign-correlation agreement, custom strategy replacement, native
16-bit amplitudes, capture ownership after ring-buffer reuse, and cancellation on gaps.

Local RF validation uses existing Classic recordings, without modifying them:
`recordings/native-ab.raw` (marker-tagged SiGe, decoded to CI8 for Evo replay) and
`recordings/diagnosi/hackrf-l1-ci8.raw` (native CI8, 8.1838 Msps). Three-second excerpts
confirm multiple GPS PRNs in each source. The SiGe satellites include PRNs 18, 26, 29,
31 present in the Classic acquisition/tracking logs. The HackRF source is already
heavily clipped as recorded in Classic's report; detection does not certify RF quality.

This establishes acquisition on recorded RF and deterministic synthetic inputs. It does
not establish live RTL-SDR satellite reception, tracking handoff, navigation accuracy,
or robustness for every device clock/filter and IF setting. Those remain distinct tests.
