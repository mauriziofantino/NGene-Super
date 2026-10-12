# NGene 0.6.4 — Native multi-SDR increment

This increment starts from commit `c32d459` of NGene 0.6.3. It preserves the capture engine, conditioner-before-shared-ring order, channel dispatcher, FFT assembly, SIMD CI8 primitives, recorder, replay browser and bounded diagnostic workers. No second ring implementation or parallel statistics framework was introduced.

## Source and integer sample formats

| Source | Library | Native format | Raw extension |
| --- | --- | --- | --- |
| HackRF | libhackrf | signed interleaved CI8 | `.cs8` |
| RTL-SDR | librtlsdr | unsigned interleaved CU8 | `.cu8` |
| bladeRF RX0 | libbladeRF | little-endian signed SC16 Q11, 12 significant ADC bits | `.sc16q11` |
| File | existing replay adapter | all three formats | as above |

`TNgFrontendOps` still has the same six lifecycle/lease/statistics callbacks. Native calls remain inside the frontend adapters. `TNgIqView` appends a format field; zero retains the legacy CI8 default. `samples` remains the compatibility pointer, but must only be dereferenced as `TNgIq8` for CI8. For CU8/SC16 use `NgIqComponent`, `NgIqStore` or a validated native kernel. Buffer length is `meta.count * NgSampleBytes(format)`. `meta.count` and all loss indices count simultaneous complex pairs, never components or bytes. Public structure layouts changed: recompile clients; binary compatibility with 0.6.3 is not claimed.

CU8 is transported and recorded unchanged. Signed statistics/DC arithmetic subtracts its unsigned midpoint 128; this is representation conversion, not optional DC cancellation. SC16 Q11 retains every ADC bit through ingress, assembly, shared ring and recording. The diagnostic worker maps signed levels to the existing Q14 integer FFT range using exact integer factors: 128 for CI8/CU8 and 8 for SC16 Q11. The FFT implementation itself is unchanged. Histograms span -128..127 or -2048..2047 and report native signed ADC units; 256 bins give one CI8/CU8 level or sixteen SC16 levels per bin. No float32 common stream and no one-bit quantization were added.

The continuous DC remover supports the three native formats. Its Q32 state persists across blocks, resets on actual gaps/epochs, and saturates to the representation's ADC rails. Pre-conditioning recording keeps original native bytes; post-conditioning keeps the conditioned representation and uses matching sidecar metadata.

The current 0.6.3 repository is a capture/diagnostics kernel with channel interfaces; the legacy GPS/Galileo acquisition, tracking, navigation and XOR kernels are not yet migrated into it. No claim of a working 24-channel GNSS receiver is made. Existing CI8 channel callbacks remain supported; non-CI8 channel registration fails explicitly until native multibit channel kernels are implemented.

## Integrity and ownership

All native adapters use `TNgIngress`, a bookkeeping wrapper around the existing `TNgRing`. HackRF's former callback-local splitting/counters move into that common helper with the same stop/drop-newest policy. RTL-SDR uses a private native asynchronous dispatcher; bladeRF uses a private bounded-timeout synchronous RX worker with metadata. Copies finish before vendor storage expires. No allocation, recording or networking occurs in sample callbacks. One reader may hold one immutable lease; a leased slot remains occupied until release, and a full ring never overwrites it.

`NgRingHighWater()` still reads the original peak slot counter. `NgRingSnapshot()` adds a consistent mutex-protected snapshot and O(1) pending/accepted/consumed counters. It reports occupied slots (including leases), capacity, peak, full-ring refusals, occupancy percentage and free buffering milliseconds. Partial blocks occupy whole slots; free time is therefore calculated from free slots and their maximum pair capacity, not from a misleading average pair occupancy.

Three different loss classes remain distinct:

- **Software overflow:** exact delivered pairs refused by NGene ingress; `droppedSamples` and `ingress.overflowEvents`. Stop rejects the remainder of that transfer and stops RX. Drop-newest continues, advances indices and annotates the next accepted block with the known gap.
- **Transport error:** malformed transfers, invalid metadata, native RX failure or unexpected stopped streaming. It does not increase software-overflow counters.
- **Device/library continuity:** bladeRF metadata overrun flags and positive hardware timestamp deltas. Overrun alone does not identify hardware-only loss. A positive delta gives exact missing pairs; an overrun with no measurable delta retains an unknown duration. An unknown vendor gap remains unknown even if its first frame also overflows NGene ingress.

Hardware-only loss is reported as `unsupported` with JSON `null`, not as zero. HackRF firmware M0/M4 polling is not added in this increment. RTL-SDR's ordinary RX callback has neither a hardware sample timestamp nor a reliable RF/USB lost-sample counter; zero software overflow cannot certify the antenna-to-host path. bladeRF counters start at the first valid timestamp; loss before that origin cannot be measured here.

Published transfer counters and the ring snapshot are individually synchronized, but are sampled at different instants while running. Do not require `received = accepted + dropped` from a simultaneous-looking live telemetry frame. After stop/join, valid-transfer runs satisfy that accounting; tests also assert `accepted - consumed = pending` inside the consistent ring snapshot. Counters reset on a newly opened instance; close/drain preserve high-water until destruction. Samples remaining on intentional stop are reported separately.

Optional diagnostics still copy selected bursts, use try-locks and do not depend on GUI rendering. Telemetry sends PSD/histogram/metrics, never a continuous raw IQ stream.

## One active source in the web GUI

Select File, HackRF, RTL-SDR or bladeRF in **Configuration → Input source**, apply the profile while stopped, then start. Observation follows that single receiver. The separate replay controller and the session selector were removed. Legacy `/api/v1/replay/...` endpoints return HTTP 410; the existing primary endpoints and telemetry keys remain, with added integrity fields. Source changes and file selection are rejected while running. Recorded-file Replay buttons prepare the same receiver; they do not launch another engine.

The dark style, display scaling, spectrum/waterfall, histogram settings, receiver-side filename/folder selection, transfers and guarded deletion remain. Inactive driver sections are hidden, and desktop source parameters use two balanced groups instead of a tall list.

**Query device limits** opens a stopped RTL-SDR/bladeRF briefly to read native controls, then closes it. Queries are serialized with startup. RTL-SDR's manual gain dropdown uses the tuner's exact tenth-dB list. bladeRF's gain control uses the frequency-dependent native minimum, maximum and step. Gain editing requires a fresh query after device/tuning changes. Requested out-of-range gains fail before RX; they are not silently clipped. HackRF retains its LNA/VGA step dropdowns.

## Recording and replay

Each raw segment has an adjacent `.meta.ini` sidecar with native format, component signedness/width, significant/fractional bits, byte order, rate, RF references, conditioning and completion/sample counts. The index keeps its original columns and appends hardware timestamp validity/ticks. The replay picker accepts the three extensions and imports format/rate/tuning automatically. Historical CI8 sidecars remain readable. Unknown/malformed sidecars, contradictory signedness and incomplete pair lengths are rejected.

As in 0.6.3, replay consumes the stored packed file range; it does not reconstruct original RF time gaps from the CSV index. The index preserves those events for later reconstruction. A sidecar records the nominal/library-reported rate, not a calibrated oscillator measurement.

## Native builds

Keep the existing HackRF presets/PowerShell workflow. The multi-SDR preset is additive; it enables all three adapters.

Windows x64 with Visual Studio C/C++ workload, Windows SDK, CMake supporting the installed Visual Studio and Git:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -MultiSdr
.\build\msvc-x64-multisdr\Release\ngene.exe --web --port 8080
```

The default action builds without running the full test matrix. `-Configuration Debug` chooses Debug; `-Action Test` is optional. VS Code's **NGene: Build multi-SDR (Windows MSVC)** task uses the same entry point. Equivalent presets are `windows-msvc-x64-multisdr` and `windows-msvc-arm64-multisdr`, with Release/Debug build/test presets. ARM64 is a target architecture, not an emulated runtime test.

The existing pinned vcpkg baseline is retained. Windows adds the available `rtlsdr` 2.0.2 port; USB/PThreads dependencies are automatic. bladeRF is built from official 2.6.0 C sources, including its pinned no-OS RFIC submodule and six official patches. A CMake-only patch applicator avoids Python, Unix patch, MinGW, MSYS2, WSL, C++ CyAPI and manual submodule setup. Its generated RFIC files were verified byte-for-byte against the upstream GNU-patch result. Header import views preserve upstream files/licenses. First-party `/W4 /WX` remains; only vendor targets use external warning policy. Existing DLL closure staging and CMP0207 handling are reused and extended to the new library targets. No manual SDK or DLL copy is part of this procedure. Native Windows execution is still unverified in this Linux environment.

Linux/macOS require their normal libusb development dependency. With installed native SDKs, `AUTO` uses them; otherwise hash-pinned source builds supply the C libraries. No FFTW dependency is introduced:

```sh
cmake --preset linux-multisdr
cmake --build --preset linux-multisdr --parallel
./build/linux-multisdr/ngene --web
```

Use `macos-multisdr` on macOS. `NGENE_*_PROVIDER=SOURCE` selects a pinned source provider explicitly. The source/archive hashes are versioned in CMake; no paths specific to an individual PC are embedded. Configure/build paths containing spaces are tested.

bladeRF needs its vendor FPGA configured; NGene does not distribute or flash firmware/FPGA images. libbladeRF's normal vendor autoload mechanism may supply an installed matching image. Linux USB permissions and the proper native Windows USB device driver are hardware prerequisites.

## Reproducible hardware checks — not yet performed

For each physically attached SDR, build the matching native provider and run `ngene --list-devices`. Choose its serial, query limits, set the nominal rate and tune, use bypass conditioning, pre-conditioning recording and overflow policy `stop`. Run a sustained capture while observing common ingress/transport/device counters. Stop, verify no pending writer work, replay the resulting file through its sidecar and inspect sample/byte counts.

First run without diagnostics, then with spectrum/waterfall/histogram, and compare peak occupancy/overflow. Repeat with an intentionally tiny ingress and delayed consumer in the deterministic CTest fixture; this must detect stop/drop-newest without overwriting leases. For bladeRF, review metadata overrun events and hardware-clock deltas independently of software overflow. For HackRF/RTL-SDR, a successful capture with zero ingress loss still cannot certify loss before their callbacks. A long-duration RF/USB continuity validation needs connected hardware and, for RTL-SDR, a separately controlled native test-pattern procedure if supported by the device.

See [validation report](VALIDATION_064.md) for actual tests, benchmark scope and unverified platforms.
