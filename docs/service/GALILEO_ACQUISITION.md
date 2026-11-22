# Galileo E1-B acquisition API and common noise policy

## Scope

`NgAcquisitionCreateGalileo` registers an independent acquisition worker with the
existing engine/channel API. This increment supports E1-B PRNs 1..32 at native
2.048..20 Msps, not E1-C, tracking, I/NAV or a navigation fix.
Galileo can now be enabled independently in Settings / Acquisition, using the
Galileo acquisition group and Apply before receiver start. GPS settings retain
their existing keys and defaults. The same source can run GPS and Galileo together,
or either alone, with separate workers on the common stream and distinct channel
instance IDs. Record-only mode starts neither module.

`[galileo_acquisition]` accepts `enabled`, `first_prn`, `last_prn`, `square_carrier`,
`xor_correlation` and `pfa`, mirroring `[acquisition]` for GPS. Galileo defaults off.
Profiles without that section remain valid. Both ranges are 1..32 in this increment.
CLI also registers the enabled modules and reports their results separately.

The Signal dropdown at the top of Acquisition selects GPS L1 C/A or Galileo E1-B
for worker telemetry, the stable satellite table and simultaneous 2D/3D plots.
It changes observation only; neither module is stopped or reprioritized. Each signal
remembers its selected completed satellite while that PRN is available.

Status retains the existing `acquisition` object for GPS and adds
`galileo_acquisition` with the same fields. Completed maps use
`/api/v1/acquisition/search-space?prn=7&signal=galileo`; omitted `signal` means GPS.
Maps have separate archives and survive Stop; both archives reset on receiver start.
GPS returns 16 Doppler rows / 1023 chips, Galileo 116 rows / 4092 chips. Plots use
returned dimensions, intervals, capture origin and peak; display reduction never
feeds a search decision. Archives reserve at most 116 x 1024 float cells per PRN.

## Classic references and adaptations

Classic `asmFolder/prnTable.c:InitPrnTable` explicitly loads
`otherData/Galileo_E1b_codes.dat`, despite the generic `E1_BC` enum. Its
`codesFileFormat.txt` specifies 4092 chips, two representations per PRN, and no
subcarrier in that file. EVO packs the bipolar primary codes losslessly; source
SHA256 is recorded in `src/acquisition/galileo_codes.h`. The test's separate binary
fixture is the original bipolar record for PRN 7, not a sampled production replica.

Classic's `carrierTable.c`, `refinement.c` and `confirmation.c` combine the primary
code with a square subcarrier at 1.023 MHz: BOC(1,1). EVO uses that replica; it does
not claim a complete CBOC matched filter. `Acquisition.conf` specifies 4-ms coherent
periods, eight noncoherent sums, Pfa 1e-9 and confirmation 3-of-4. These are retained.
Doppler coarse trials follow Classic's 87-Hz step, -5000..+5005 Hz (116 points).
Doppler refinement is +/-667 Hz with 10-Hz steps and the final endpoint; code
refinement spans approximately +/-1 chip with native-sample steps.

Numerical adaptation uses floating complex PCPS with the BOC-modulated replica
rather than Classic's subcarrier wipe-off before packed FFT compression. FFT sizes
are 8192 or 16384 per 4 ms, with rational box integration and native-rate code
refinement/confirmation. Automatic sizing selects 8192 below 4.096 Msps and 16384
at or above that rate. The optional 4096 mode uses fixed-phase native subcarrier
removal and a primary-code-only coarse replica.

The workspace requires as many whole native pairs per 4-ms period as coarse
FFT cells. An explicit 16384 with an effective stream rate below 4096000
pairs/s fails receiver startup with the effective and minimum rates in the
diagnostic. Replay metadata supplies the rate when a capture is selected or
passed via `--replay`. The fractional period boundary uses floor, so 4095999
pairs/s yields 16383 usable pairs even when the buffer capacity rounds up to
16384. The 4096 and 8192 choices fit across the supported acquisition rate
range of 2.048–20 Msps.

No ADC precision is narrowed. Replica/code-rate and IF
origins are capture-relative; inverted spectra are conjugated only in private data.
The 224-ms capture has seven disjoint 32-ms intervals: coarse, Doppler refinement,
code refinement and four confirmation attempts. This follows EVO's frozen-capture
architecture; Classic evaluates hypotheses on advancing input data. No claim of
sample-for-sample equivalence or equivalent BOC sidelobe behaviour is made.

## Shared noise policy

GPS coarse already estimates noise using the correct replica at +22 kHz. GPS
confirmation previously used eight off-code references at the candidate Doppler.
Now GPS and Galileo confirmation preserve the candidate code phase/rate and use
wrong carriers: candidate Doppler +20..27 kHz in 1-kHz increments. All are outside
the coarse/refinement search range. Offsets make integer cycles over both 1-ms
and 4-ms periods, suppressing coherent candidate integration. They do not guarantee
absence of interference, other satellites or noncoherent signal leakage.

Each confirmation attempt uses its own fresh, disjoint data interval; noise and
candidate probes use the same interval. No historical estimate, off-code estimator
or fallback from coarse thresholds is used. The shared `noise_policy.h` defines
the offsets and inverse integer-shape gamma survival law. `acquisition.pfa` sets the
GPS nominal per-cell Pfa (default 1e-9); API Galileo uses the same config field.
Eight summed powers have gamma shape 8 for circular complex white Gaussian noise.
The measured mean single-period power supplies the scale: duration, replica energy,
native sample count and mixer gain enter through the actual correlations, not a
fixed GPS-to-Galileo factor. A 4-ms native noise prompt has approximately four times
the mean power of a 1-ms prompt at the same sample rate and sample noise variance.

This is a plug-in noise-scale estimate, as in the existing coarse model. References
have finite population, may be correlated, and share samples with the candidate.
Nominal Pfa is not an experimentally calibrated system-wide false-alarm rate;
coloured RF noise and interference can invalidate the white Gaussian assumption.
There is no guaranteed direction of change from the previous GPS confirmation
threshold. GPS grids, stage intervals, eight sums and 3-of-4 decisions are unchanged.

## Reproducible validation

Build with CMake and run `ctest --test-dir build-linux-bladerf --output-on-failure`.
`gps_acquisition_contract` retains all GPS scenarios and adds Galileo signal/noise/
disappearance, multiple rates, SiGe residual IF/spectral inversion and CI16 scaling.
An independent scalar waveform oracle drives the real ring subscriber and worker.
A common-policy check verifies gamma inversion, Pfa monotonicity, coherent signal
suppression at wrong Doppler and noise-power scaling for 1-ms versus 4-ms periods.
These checks do not measure rare-event Pfa=1e-9 or certify arbitrary PRNs, CBOC data,
bit-boundary robustness, interference, live scheduling capacity or all supported rates.

No suitable RF recording was found in the available checkouts for this increment.
No Galileo/GPS replay or physical RF test has been performed for this change.

Local verification on 2026-11-12: Linux x86_64, existing CMake
`build-linux-bladerf` configuration; build succeeded and all 17 CTest tests passed.
The acquisition test includes the original seven GPS waveform scenarios plus six
Galileo scenarios (signal at three rates, native CI16 scaling, noise, disappearance).
The CI16 signal fixture scales already quantized samples; it verifies the native
path and lack of narrowing, not an independent physical ADC precision measurement.

## Controller/browser verification

The synthetic real-service browser test is reproducible with
`NGENE_TEST_GALILEO=1 node tests/web_acquisition.cjs` (set `NGENE_EXECUTABLE`,
`PLAYWRIGHT_MODULE` and `BROWSER_EXECUTABLE` for the host if needed). It synthesizes
GPS PRN 7 and Galileo PRN 7 at different Dopplers on one replay source. It checks
UI enable/apply/persistence, mixed and Galileo-only starts, separate status/maps,
correct Galileo dimensions, stable identities, signal selection, per-signal PRN
selection and archived maps after Stop. GPS-only mode remains the default regression.
This is synthetic file replay through the real service, not a recorded RF capture
or a live hardware test. Desktop/mobile fixture tests cover simultaneous plot layout
and persistent selection during live updates.

Integration verified locally on 2026-11-12 (Linux x86_64, Node 24.19.0,
Chrome headless): build and 17/17 CTests passed; GPS-only, mixed/Galileo-only
Acquisition browser runs and the full workspace regression passed. Desktop/mobile
search-space and stable-list fixtures passed. Configuration tests cover absent-section
defaults, malformed Galileo ranges/booleans and independent settings round-trip.
Native map tests check all 116 Galileo rows and maximum-preserving display reduction.
