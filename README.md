# NGene — Software GNSS Receiver

<img src="web/ngene-logo.svg" alt="NGene logo" width="72" align="right">

NGene is a software GNSS receiver project for receiving, inspecting and processing satellite
navigation signals from software defined radios or recorded I/Q samples. This repository
develops the original NGene receiver into a portable C11 application with a shared sample
stream, modular signal processing and a browser based operator console.

The current application provides live I/Q reception, recording, file
replay and spectrum diagnostics. Acquisition, tracking,
navigation message decoding and position, velocity and time (PVT) computation are not yet integrated.

![NGene receiver console](docs/images/receiver-dashboard.png)

*Actual NGene console running a deterministic I/Q replay fixture. This illustration is not
an over-the-air GNSS capture or a position solution.*

## Capabilities

- Native HackRF, RTL-SDR, bladeRF RX0 and fixed-profile SiGe I/Q inputs, plus file replay.
- Common signed CI8/CI16 rings, SIMD conversion and a dedicated source worker. Automatic precision preserves native ADC information.
- SIMD block-mean DC removal before diagnostic spectra, histograms and gain analysis;
  no IIR filter or conditioning controls. Old DC profile keys are retired and ignored.
- GNSS acquisition is unavailable: the previous implementation has been removed.
- Common-stream recording with segmentation, format/ADC provenance and gap accounting.
- Spectrum/PSD, synchronized 2D and rectangular relief waterfalls, I/Q histograms and live receiver counters.
- Runtime diagnostic controls: switching off all three RF views stops their shared
  FFT/PSD production. Histograms can remain active independently.
- Internal integer FFT and mathematical functions with compiler selected SSE2 or NEON
  implementations, a scalar reference and an optional pool for parallel channel processing.

The application uses one active input source per service instance. To inspect a file while
another instance receives live data, run a second service on a different port and profile
directory. The [operator manual](https://github.com/mauriziofantino/nav-ngene-evo/wiki) explains the workflow and limits.

## Supported inputs

| Input | Native sample format | Requirements |
| --- | --- | --- |
| HackRF | CI8: signed 8-bit I and Q | Native libhackrf and a working USB driver |
| RTL-SDR | CU8: unsigned 8-bit I and Q | Native librtlsdr and a compatible tuner/USB driver |
| bladeRF RX0 | SC16 Q11, or CI8 when supported | Native libbladeRF and compatible device/FPGA configuration |
| SiGe native I/Q | Marker-tagged one-bit I/Q, decoded to signed CI8 | Loaded native-I/Q firmware, VID/PID 16c0:072f and libusb; fixed Fs 8.1838 Msps and IF +38.4 kHz |
| Recorded I/Q | CI8, CU8, SC16 Q11 or CI16 Q15 | Closed raw file and its stream parameters, preferably a metadata sidecar |

Windows, Linux and macOS build paths are provided. Optimized FFT targets include x86-64,
32-bit x86, ARM64 and ARMv7 with NEON, subject to OS and toolchain support. Building a target
does not establish RF hardware validation on every platform.

## Build and launch

### Windows

Install Git, Visual Studio 2026 with the C/C++ workload and Windows SDK, and CMake 4.2 or
later (the Visual Studio CMake component is supported). From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -MultiSdr
.\build\msvc-x64-multisdr\Release\ngene.exe --web --port 8080
```

The script configures and builds all four native input adapters and stages their DLLs.
The first build may download pinned dependencies. USB drivers and device firmware remain
separate setup steps. See [Getting started](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Getting-Started) for replay-only
setup and the first operating session. Additional platform build details are in the
[Windows engineering guide](docs/service/WINDOWS.md).

### Linux and macOS

With CMake 3.26 or later and a C11 toolchain, build a replay-capable receiver:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/ngene --web --port 8080
```

Enable the required native adapters with `-DNGENE_WITH_SIGE=ON`, `-DNGENE_WITH_HACKRF=ON`,
`-DNGENE_WITH_RTLSDR=ON` and/or `-DNGENE_WITH_BLADERF=ON`; their platform dependencies
must be available. Python, Node.js, FFTW and SoapySDR are not runtime requirements.

## Operate the receiver

Open **http://127.0.0.1:8080** in a browser.

1. Open **Receiver configuration → Front end** and select an SDR or **Recorded I/Q**.
2. Set the sample rate, hardware tuning and signal frequency. For replay, select a closed
   recording; a valid sidecar imports its format and stream parameters.
3. Review **Receiver & signals**, **Acquisition**, **Recording** and **RF diagnostics**, then select
   **Apply & save full profile** while the receiver is stopped.
4. Return to **RF observation** and select **Start receiver**. Use **Start recording** when
   samples should also be written to disk.
5. Use the three independent spectrum, 2D waterfall and 3D waterfall **Enable** switches to select diagnostic work.
   **Stop receiver** ends reception; **Exit service** closes the application.

Recording and replay paths refer to the computer running NGene. **Transfer** downloads a
closed file to the browser's computer; retain its metadata and index for later analysis.
The service listens on loopback. A remote browser can use an SSH tunnel to the receiver.

For command line operation:

```sh
ngene --config config/hackrf.ini
ngene --replay "captures/example_000000.cs8"
ngene --config config/replay.ini --replay "captures/file-without-sidecar.cs8"
```

For a file without metadata, set its actual format, sample rate and RF references in the
profile first. Raw I/Q bytes do not encode those parameters.

## GNSS processing and RF views

The previous GPS acquisition implementation, adapter, API and operational configuration
have been removed. No acquisition or CAF calculation runs. Existing profiles containing
an [acquisition] section remain readable; that retired section is ignored and omitted
when saving. Tracking, navigation decoding and PVT are not integrated.

RF observation has three independent, resizable views in order: spectrum, 2D waterfall,
and a rectangular blue/cyan relief waterfall inspired by the FNIRSI DPO350P display.
The relief shades signal power on exactly the same frequency/time grid as 2D; it has no
perspective camera or displaced observations. Both waterfalls use one FFT history, rate,
frequency crop, power range and newest-at-top direction. Pause and either Clear button
operate on that shared history. Reenabling a paused view redraws the cached history.
Each Enable switch stops that view's bitmap preparation and rendering. History advances
while either waterfall is enabled, up to 2048 rows and 1024 telemetry bins, with no extra
temporal or frequency pooling. The views share one PSD producer: turning all three off
disables FFT production. Histograms, recording and input streaming remain independent.

Run tests/web_rf_views.cjs with Playwright and NGENE_EXECUTABLE pointing to the built receiver
for all eight view combinations, per-canvas inactivity, shared history registration,
all-off PSD production, profile compatibility, and desktop/mobile regression coverage.

Gain calibration collects three native I/Q windows per setting and stops RX before statistical
analysis. All three windows must pass. HackRF VGA sweeps stop at the first overloaded setting
in each LNA/RF-amplifier branch. A background worker reports progress and supports cancellation;
the console and service exit remain available. An unsuccessful calibration leaves the current
GUI gains intact. Accepted suggestions update the draft for review, without saving the profile.
The score compares ADC loading and distribution, not GNSS C/N0 or sensitivity. Histograms and
distribution statistics use centered samples; native clipping checks retain the original levels.
See [calibration and DC verification](docs/service/CALIBRATION_DC.md) for the criteria and limits.

The [frontend decision record](docs/service/FRONTEND_STREAM_DECISIONS.md) documents the
common formats, worker ownership, SiGe USB setup, removed DC filter, recording semantics
and alternatives considered.

## Documentation

- [Operator manual / wiki](https://github.com/mauriziofantino/nav-ngene-evo/wiki): setup, GUI, diagnostics, recording and replay.
- [Configuration reference](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Configuration-Reference): keys, units, defaults and limits.
- [Troubleshooting](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Troubleshooting).
- [Engineering and service documentation](docs/service/README.md): architecture, numerical
  contracts, provenance and historical verification records.
- [Wiki maintenance](docs/WIKI-PUBLISHING.md): update the operator manual in its separate
  GitHub Wiki repository.

All maintained documentation is in English. Optional API documentation can be generated
with `-DNGENE_WITH_DOCUMENTATION=ON` and the `ngene_docs` build target when Doxygen is installed.
