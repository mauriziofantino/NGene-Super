# NGene — Software GNSS Receiver

<img src="web/ngene-logo.svg" alt="NGene logo" width="72" align="right">

NGene is a software GNSS receiver project for receiving, inspecting and processing satellite
navigation signals from software defined radios or recorded I/Q samples. This repository
develops the original NGene receiver into a portable C11 application with a shared sample
stream, modular signal processing and a browser based operator console.

The current application provides live I/Q reception, signal conditioning, recording, file
replay, spectrum diagnostics and GPS L1 C/A acquisition. Tracking, Galileo acquisition,
navigation message decoding and position, velocity and time (PVT) computation are not yet integrated.

![NGene receiver console](docs/images/receiver-dashboard.png)

*Actual NGene console running a deterministic I/Q replay fixture. This illustration is not
an over-the-air GNSS capture or a position solution.*

## Capabilities

- Native HackRF, RTL-SDR and bladeRF RX0 inputs, plus recorded I/Q file replay.
- Native CI8, CU8 and SC16 Q11 sample storage, preserving the selected frontend precision.
- Optional continuous DC removal before a common stream is delivered to receiver channels.
- Incremental GPS L1 C/A acquisition derived from NGene: compressed FFT search, Doppler/code
  refinement and M-of-N confirmation, with cached code spectra and native complex I/Q mixing.
- Recording before or after conditioning, with segmentation, metadata and gap accounting.
- Spectrum/PSD, 2D and interactive 3D waterfall, I/Q histograms and live receiver counters.
- Runtime diagnostic controls: switching off both spectrum and waterfall stops their shared
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
| Recorded I/Q | CI8, CU8 or SC16 Q11 | Closed raw file and its stream parameters, preferably a metadata sidecar |

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

The script configures and builds all three native SDR adapters and stages their DLLs.
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

Enable the required native adapters with `-DNGENE_WITH_HACKRF=ON`,
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
5. Use the separate spectrum and waterfall **Enable** switches to select diagnostic work.
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

## GPS acquisition

While stopped, enable **Receiver configuration → Receiver & signals → GPS L1 C/A acquisition**,
select the concurrent searches/PRNs, and review the **Acquisition** settings. Apply the profile
and start the receiver. **Acquisition** shows each attempted PRN, processing stage, Doppler,
code delay, confirmation and sample epoch. Acquired results are ready for a future tracking
module; they do not establish signal tracking or a position solution. Acquisition is disabled
by default for existing capture/replay profiles, and is independent of spectrum/waterfall enables.

This first implementation follows NGene's 1 ms GPS coherent periods and short/long sample
accumulation directly onto the configured power-of-two FFT grid (4096 maximum by default,
reduced at lower sample rates). Chip-rate 1023-to-1024 padding, uncompressed PRECISE acquisition
and longer coherent integrations are subsequent strategies, not silently substituted here.

The acquisition workspace removes per-frame I/Q offsets without changing the frontend
stream. Detection uses the off-peak correlation background at the candidate Doppler;
confirmation uses 32 code-shift references from fresh frames, with a finite-reference
threshold. The configured false-alarm probability is nominal per cell under the white-noise
model, not an exact guarantee across repeated searches or arbitrary interference.

Search method, detector and confirmation are separate settings. This build supports
`search_method=ngene_fft` and `detector=pfa`: the threshold comes from the false-alarm
probability; detection probability is a consequence of signal strength, noise and integration,
not a separately adjustable target. The FFT evaluates code delays together while Doppler
bins are visited sequentially. Bayesian detectors, alternative serial/parallel search engines
and TONG confirmation are not implemented and are rejected by the configuration parser.

Choose `confirmation=none` to finish after coarse detection and Doppler/code refinement,
or `confirmation=m_of_n` (the backwards-compatible default) for fresh-frame M-of-N checks.
M and N are hidden when unused. Disabling confirmation saves its correlations but also removes
that additional false-positive rejection. Results explicitly report whether confirmation was
requested, and always display a metric paired with the threshold used for that decision.

Module forms are described by [module-parameters.json](web/module-parameters.json), including
groups, labels, units, bounds, choices, conditional visibility and cross-field validation rules.
The same renderer supports acquisition and tracking groups and keeps the existing UI layout.
CMake embeds the default file. To customize the form without rebuilding, set
`NGENE_MODULE_SCHEMA` to the path of a complete version-1 JSON descriptor before starting
the web service, then reload the browser after restarting. The receiver exposes its loaded
descriptor at `/api/v1/module-schema`. For example, in PowerShell:

```powershell
$env:NGENE_MODULE_SCHEMA = (Resolve-Path .\web\module-parameters.json).Path
.\build\msvc-x64-multisdr\Release\ngene.exe --web
```

A user-supplied algorithm needs its C processing module and backend configuration parser/
validator as well as its descriptor. JSON changes presentation; it does not load native code
or make unsupported algorithms operational. Unknown configuration keys remain errors.

Custom modules can use the public [acquisition API](include/ngene/acquisition.h) independently,
or the [kernel adapter](include/ngene/acquisition_receiver.h) with the replaceable
[channel callback](include/ngene/channel.h). Immutable plans outlive their private contexts.
`NgAcquisitionProcess` accepts ordered native I/Q chunks of any size; `NG_OK` includes a pending
result, and copied results identify their continuity epoch and reference sample. No borrowed
sample pointer survives a call. Gaps invalidate accumulations and acquired estimates; disabling
the module avoids allocating its plans, scratch and workers.

The kernel completes tracking, holdover and fast-reacquisition callbacks before acquisition
on each shared window, in both serial and parallel execution. Its optional `setTrackingLoad`
callback reports the active priority-channel count. The GPS adapter admits at most
`ceil(search_slots / (tracking_channels + 1))` unfinished searches, with a minimum of one.
Acquired slots do not consume this allowance; unfinished suspended searches release their
PRNs and restart from fresh samples when admitted again. This preserves integration continuity
and bounds simultaneous acquisition work. Telemetry reports `search_limit`, `tracking_channels`
and per-slot `deferred_windows`. This is cooperative scheduling, not a hard real-time guarantee;
the tracking module and hardware deadline measurements are still pending.

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
