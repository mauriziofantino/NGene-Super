# NGene — kernel and WebGUI · v0.6.4 (multi-SDR candidate)

**NGene is the functional and algorithmic foundation of this project.** Development remains incremental: I/Q frontends, conditioning, a shared ring buffer, a scheduler and internal DSP libraries. The [migration map](docs/LEGACY_MIGRATION.md) connects this code to the original receiver.

The kernel uses C11, with integer FFTs and SSE2/NEON assembly butterflies, SIMD primitives and a scalar reference implementation. The HTML/CSS/JavaScript GUI is included in the same executable. Targets are Windows, Linux and macOS, on x86-64 and ARM64 architectures. FFTW, SoapySDR, Python and Node are not required to build or use the receiver.

This version receives and records I/Q and produces diagnostics. Satellite acquisition, tracking, demodulation and PVT will be integrated through the existing interfaces.

## Multi-SDR increment 0.6.4

Native HackRF, RTL-SDR and bladeRF RX0 drivers; CI8, CU8 and SC16 Q11 formats are preserved through ingress, conditioning, the ring buffer and recording. Histograms and PSD use the native ADC resolution; existing assembly FFTs and CI8 SIMD primitives remain unchanged. The GUI has a single active source, selected in Configuration. Common occupancy/high-water/overflow/transport counters include explicit indications of device information availability.

[Contracts, build procedure, limitations and hardware verification](docs/MULTISDR_064.md) · [Checks actually performed](docs/VALIDATION_064.md).

To build all frontends from PowerShell, without the test matrix:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -MultiSdr
.\build\msvc-x64-multisdr\Release\ngene.exe --web --port 8080
```

Open the project folder in VS Code and use the **NGene: Build multi-SDR (Windows MSVC)** task for the same build. Native MSVC support remains configured; this iteration was run on Linux, without Windows or physical boards.

## Consolidation 0.6

- **vcpkg manifest with a pinned baseline and versions**, with automatic bootstrapping if vcpkg is not installed; HackRF `v2026.01.3` is downloaded with SHA-256 verification.
- **Native Windows MSVC**, x64 and ARM64: no CMake file changes, manual baseline updates or manual DLL copying. [Windows procedure and validation](docs/WINDOWS.md).
- Strict warnings apply only to NGene targets; dependencies use `SYSTEM`, with `/W0` only on external targets. The HackRF import header is generated without altering upstream or duplicating `ADD_EXPORTS`.
- ESA naming for **functions, types, struct tags, members and variables**: `NgRingDestroy`, `TNgRing`, `TNgRecorder`, `statsMutex`, `segmentOffset`. [Conventions and checks](docs/CODING_STANDARD.md), [identifier migration](docs/IDENTIFIER_RENAMES.csv).
- **CI8 file replay**, with range selection, pacing and looping; no synthetic frontend. The GUI uses a single active source, selected in Configuration: a file or a live frontend.
- Doxygen checked with warnings treated as errors; ownership, lifetimes and structure members are also documented. [Increment report](docs/ITERATION_06.md).

Further consolidation in 0.6.1: correct triplet selection even with a composite Visual Studio platform and an ARM64 host, manifest cache repair, import headers for installed SDKs as well, clang-tidy checks on production/test code and verification of the actual CPack archive. [Final report](docs/ITERATION_06_COMPLETION.md).

Build consolidation in 0.6.2: fixes for the HTTP path triggering C4701 and the CMP0207 staging policy, Debug/Release runtimes even without HackRF, Debug presets and a verifier using a real clone for all four combinations. **Native Windows acceptance is still pending:** [status and evidence](docs/WINDOWS_BUILD_062.md).

## Windows with HackRF

Development prerequisites: Visual Studio 18/2026 with the C/C++ workload, Windows SDK, CMake supporting VS 2026 (4.2 or later) and Git. The first configuration downloads missing dependencies.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Action Test
.\build\msvc-x64-hackrf\Release\ngene.exe --web
```

The script also detects CMake bundled with Visual Studio outside PATH and selects the installed Visual Studio generator. `-Configuration Debug`, `-Hackrf OFF` and `-Architecture ARM64` select the other presets without modifying them. In **VS Code**, open this folder and use **Ctrl+Shift+B**, or **Terminal → Run Task → NGene: Test (Windows MSVC)**. [VS Code instructions](docs/VSCODE.md). For an MSVC cross-build from x64 to ARM64, use the `windows-msvc-arm64-hackrf` preset and its ARM64 toolset. The build without hardware support is `windows-msvc-x64`; tests use recorded files and fixtures, without a generator in the receiver.

The fixes have been verified here on Linux, including Debug/Release OFF/ON builds and GUI/staging regressions. **Native MSVC execution still needs verification**: CI includes VS 2026 x64/ARM64 runners and a script using a clean checkout, with Debug, Release and a relocated package.

## Linux and macOS

CMake >=3.26 and a C11 compiler:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/ngene --web
```

For HackRF, add `-DNGENE_WITH_HACKRF=ON`. `AUTO` uses an installed SDK or builds libhackrf from the pinned sources; the latter option requires the libusb development packages. hackrf-tools is not built.

For Linux ARM64, `cmake/toolchains/linux-aarch64.cmake` is available, with a GNU cross-compiler and tests through QEMU if available. The target determines the DSP backend; the architecture of the computer hosting CMake is not used. For now, 32-bit ARM/x86 require the C reference implementation through `-DNGENE_FFT_REFERENCE=ON`.

## Console and parallel sessions

Open **http://127.0.0.1:8080**. The **Receiver session** menu selects the primary session or independent replay; sessions continue running when switching views.

1. In **Configuration**, set the frontend, sample rate, tuning, gain, conditioning and buffers. With the HackRF build, the initial profile defaults to HackRF without starting reception.
2. In **Recording**, choose **Base filename** and **Folder on receiver computer**, then **Apply & save**. The name and folder refer to the computer running NGene.
3. **Start receiver** starts the selected session; **Start recording / Stop recording** control capture without restarting the frontend. Recording is available before or after conditioning.
4. **Transfer** downloads a closed file to the browser's computer. Also transfer the `nome.cs8.meta.ini` sidecar for automatic replay. **Delete** removes only the selected file after confirmation; files used by an active replay are protected.
5. For replay, choose **Independent file replay → Input source → Browse I/Q files…** and browse the receiver's folders. The **Replay** button for a closed recording opens the same selection in the independent session. The sidecar sets Fs and RF references in the draft: review them, **Apply & save**, then **Start receiver**. The live session continues.
6. **Stop receiver** acts on the selected session. **Exit service** or Ctrl+C stops both sessions and closes the program.

Raw I/Q is not continuously sent to the GUI. NGene computes PSD and optional histograms; the browser draws the spectrum, waterfall and statistics. Diagnostic views that are not being observed suspend their respective producers. Overlap from 0–99%, discrete FFT sizes, Welch averaging, windowing, EMA, update rate, pre/post-conditioning source and plot scales are configurable. The waterfall maintains a global scale.

The primary profile is `captures/receiver.ini`; the replay profile is `captures/replay/receiver.ini`. `--output-dir` changes the profile root; `recording.directory` changes the capture destination. Runs are not automatically restarted when profiles are loaded.

To capture from a terminal:

```sh
./build/ngene --config config/hackrf.ini
./build/ngene --replay "captures/prova_000000.cs8"
```

`--replay FILE` imports the adjacent sidecar. If it is missing, `--config FILE` with the stream parameters is also required. Raw CI8 does not itself contain the sample rate or tuning. [Replay contract](docs/REPLAY.md).

The service binds to loopback. For a browser on another computer, use a tunnel, for example `ssh -N -L 8080:127.0.0.1:8080 utente@ricevitore`, and open the same local URL. The download destination is selected by the browser.

## Documentation

- [Native bladeRF 8/12-bit I/Q, ring buffering and waterfall views](docs/NATIVE_IQ_PERFORMANCE.md)
- [Architecture and ownership](docs/ARCHITECTURE.md)
- [Configuration](docs/CONFIGURATION.md), [GUI and API](docs/WEBGUI.md)
- [Recording and formats](docs/RECORDING.md), [replay](docs/REPLAY.md)
- [Diagnostics](docs/ANALYSIS.md), [FFT and provenance](docs/FFT_REVIEW.md)
- [Coding standard](docs/CODING_STANDARD.md), [validation](docs/ITERATION_06.md)
- [Fixes and checks for 0.6.3](docs/ITERATION_063.md)
- [Working manual](docs/WORKING_MANUAL.md), [dependencies and licenses](docs/PROVENANCE.md)

`NGENE_WITH_DOCUMENTATION=ON` enables `ngene_docs` when Doxygen is installed. `tests/web_ui.cjs` is an optional browser check: only the test host requires Node/Playwright. The included screenshots come from an actual program session using a test file, rather than a mockup.
