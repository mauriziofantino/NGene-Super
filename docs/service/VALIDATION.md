# Kernel, DSP, and WebGUI verification

> Historical verification record covering revisions 0.3–0.6 in October 2026. Results and unavailable-platform statements describe those revisions and environments, not the current release. See the [current operator manual](../wiki/Home.md).

Results for **0.6** are in [ITERATION_06.md](ITERATION_06.md): Linux HackRF SOURCE 10/10, ASan/UBSan 9/9, headless 8/8, ARM64 executed with QEMU 9/9, warning-free Doxygen, and desktop/mobile browser. Native Windows build had not yet been executed at that stage.

The following sections retain the verification **history** for 0.3 (1 October 2026), 0.4 (2 October), and 0.5. References to synthetic input and cross-builds do not describe current sources or verification. Scope includes capture/recording, integer FFT, and observation; it excludes satellite acquisition, tracking, and PVT.

| Configuration | Actual result | Limitation |
|---|---|---|
| Linux x86-64, GCC 13.3, Release | Compilation, tests, and FFT microbenchmark executed | FFT measurements in `FFT_REVIEW.md`, not GNSS receiver performance |
| Linux x86-64 with real libhackrf | Compilation/linking successful; 6/6 CTest tests passed | Adapter test uses a simulated driver, not actual USB transfers |
| Linux x86-64, Debug, AddressSanitizer + UndefinedBehaviorSanitizer | 5/5 tests passed (analysis, FFT, core, capture, controller) | LeakSanitizer disabled; assembly instructions are not sanitizer-instrumented |
| Linux x86-64, Web disabled | Release build and 4/4 tests passed | No HTTP dependency compiled |
| Linux x86-64, explicit C FFT backend | Release build and 4/4 tests passed | Reference only; does not demonstrate 32-bit target performance |
| Chromium GUI, 1512 px desktop and 390 px mobile | End-to-end test passed, screenshots inspected | Synthetic source; does not measure load at 16 Msps |
| macOS Intel/ARM | Both FFT assembly objects compiled to Mach-O | Full application and Mac SDK not verified; no native execution |
| Windows x86-64, Clang cross-compilation through Zig | Executable with HTTP/WebGUI, controller, and tests compiled/linked to PE32+ | Not executed on Windows; MSVC not run in that environment |
| Linux ARM64, Clang cross-compilation through Zig | Executable with HTTP/WebGUI, controller, NEON, and tests compiled/linked to ELF AArch64 | Not executed on ARM or QEMU; no NEON benchmark |
| NEON butterfly, AArch64 instruction emulation | 66,680 complex butterflies compared against independent integer computation, including single tails | Unicorn emulator runs actual machine code, not the entire ARM FFT/receiver |
| HackRF adapter for Windows x86-64 and Linux ARM64 | Objects compiled against the real libhackrf header | Not linked to HackRF SDKs for those targets; hardware unverified |
| CLI with synthetic recording | 25,017 I/Q pairs, 26 windows, no gaps; segmented files and metadata produced | Deterministic generator, not a GNSS signal |
| Real USB enumeration in workspace | Handled libhackrf initialization error; nonzero exit | USB unavailable; requires a HackRF test on the user's PC |

Tooling: CMake 4.4.3, GCC 13.3.0, Zig 0.16.0/Clang 21.1, clang-format 21.1.8. These tools are not included. Adapter compiled against libhackrf host v2024.02.1, libusb v1.0.27 headers, and system libusb library.

## Contracts checked by C tests

- Configuration: defaults, valid values, unknown/duplicate keys, and limits.
- Ring: wrap, full queue, view immutability, release, close, and drain.
- Producer/consumer concurrency: 5,000 blocks of 7 pairs, with order and content checked.
- Engine: fragment reassembly; two channels in different states receive the same pointer and interval; permitted transitions.
- Discontinuities: partial windows before gaps, missing-sample count, EOF, and requested stop without zero padding.
- Recorder: 13 pairs in segments of 7 and 6, exact byte comparison, index and metadata, overwrite refusal, and nonexistent directory.
- Pre/post recording: test conditioner halves values; byte-for-byte comparison of both modes, metadata, and gap handling.
- Simulated HackRF: API parameters, copy before transfer reuse, stop/drop_newest overflow, indices after loss, invalid transfer, and error cleanup.
- FFT: sizes 8..16384, zero/DC/impulse/random at contract limits, bit-for-bit SSE2/C-reference comparison, independent DFT for small sizes, and complex tone with sign/gain checks.
- Concurrent FFTs: immutable shared plan and separate workspaces; input/output not aligned to 16 bytes with sentinels, rejection of overlap and invalid range.
- Worker: 4096 Hz complex tone with expected amplitude approximately −6.02 dBFS, summed histogram counts, runtime parameter changes, gap-interrupted burst, rails, and spectrum-independent histogram.

The suite does not inject every possible fault: for example, it does not simulate power interruption or every full-filesystem case. Code checks write/flush calls.

## Reproduction

From the repository root:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

For the Linux HackRF contract, configure NGENE_WITH_HACKRF=ON with an available development SDK. The simulated test does not require the device.

For ASan/UBSan on Linux:

    cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DNGENE_SANITIZERS=ON
    cmake --build build-sanitize
    ctest --test-dir build-sanitize --output-on-failure

The delivery environment required ASAN_OPTIONS=detect_leaks=0. On a machine supporting LeakSanitizer, leave available checks enabled.

Included GNU toolchains and CI document future Windows/Linux/macOS and ARM64 builds. Local cross-builds used Zig with CMAKE_C_COMPILER_ARG1 set to "cc -target x86_64-windows-gnu" and "cc -target aarch64-linux-musl", respectively. GitHub CI is prepared but was not run on a remote repository.

## First hardware test

Verify on the actual system: device identification, parameters, sample arrival, recording/stop, loss counters, sustainable rate, and CPU timings. Repeat on Windows and Linux before declaring hardware support validated on both.

First-increment copies and synchronization are explicit. No CPU percentage or channel capacity is demonstrated; historical NGene figures are not measurements of this version.

## Control and GUI checked on 30 September and 1 October

`controller_lifecycle`: INI round trip, atomic profile save, stale revision rejected, configuration blocked during reception, duplicate Start rejected, two recording sessions without receiver restart, byte counts, eight immediate Start cancellations, partial EOF with automatic recording closed.

`tests/web_ui.cjs`, executed with Chromium Headless 134 and Playwright: configuration through GUI fields, choosing a name and **directory different from the profile directory**, directory creation, INI persistence, invalid input/token/origin rejection, configuration blocked during reception, real WebSocket telemetry, page reload preserving the run, plot pause, independent recording, two sessions, active-file download prohibition, byte-for-byte comparison of all six closed files and native browser download, mobile layout without horizontal overflow, service shutdown with exit status 0. No JavaScript errors detected. Screenshots in `docs/images/`.

The browser test reads and compares synthetic-source bytes; it does not exercise an SSH tunnel or remote network bandwidth. Persistent configuration is also checked by the C test by rereading disk. Code supports startup loading without automatically restoring an active reception.

The 1 October test adds the dark theme, Configuration/Observation separation, spectrum with synthetic 10 kHz peak, waterfall history, histograms with expected counts, runtime changes to FFT 4096/Blackman and 128-bin/8192-pair histogram without changing the run, pre-recording selection, and downloaded metadata checks. No JavaScript errors; desktop, configuration, and mobile screenshots updated. The GUI test uses synthetic 100 ksps to limit test duration, not the operational HackRF sample rate.

To reproduce the browser test with Playwright already available:

    PLAYWRIGHT_MODULE=/path/to/playwright BROWSER_EXECUTABLE=/path/to/chromium \
      NGENE_EXECUTABLE=build/ngene NGENE_SCREENSHOTS=docs/images node tests/web_ui.cjs

FFT benchmarks compare new assembly against the C reference, not old NGene assembly. No whole-receiver throughput/CPU benchmark, long-run test, real HackRF USB test, or native Windows/MSVC or macOS test was performed. The port covers FFT: correlators and other historical routines still need migration. The ARM64 build does not demonstrate bare-metal support or real-time performance on particular embedded boards. Complete ESA compliance is not certified.

## 0.4 verification — 2 October 2026

- Linux GCC, linked libhackrf SDK: **7/7** CTest tests, including new sample/DC and diagnostics tests. Simulated HackRF driver, no real USB.
- ASan/UBSan: **6/6** (without HackRF adapter); LeakSanitizer disabled and assembly not instrumented. An earlier build object was truncated; clean recompilation fixed linking and all tests passed.
- C reference/headless backend with `-funsigned-char`: **5/5**; checks signed sample representation independently of `char`.
- Linux AArch64: full program and tests cross-compiled with Clang, including new NEON intrinsics. No native ARM execution in this iteration.
- C tests: exact statistics for known distributions, independent pre/post, histogram unchanged when only NFFT changes over the same data, PSD/Hz and ENBW, temporal averaging and EMA in linear power, checksum and NaN/Inf, continuous DC invariant to block splitting, tuning/NCO metadata.
- Browser: producer at 12.5 Hz, GUI slowed to 1 Hz, actual skips without synthetic rows; rejection of an intentionally altered frame, producer/telemetry/browser checksum, 30 Hz option without requests stalling, hidden producer suspension while receiver remains active, digital IF axes, and gain menus. Recording, persistence, byte-for-byte download, mobile layout, and shutdown also pass. No JavaScript errors.
- Primitive/DC microbenchmarks in `ITERATION_04.md`, reproducible with Release `ngene_sample_tests --benchmark`. No whole-receiver throughput measurement.

The requested Windows build now uses explicit **MSVC** presets, MASM/ARMASM64, and corresponding CI. That Linux environment lacks `cl.exe`, `ml64.exe`, and Windows SDK: **no local MSVC compilation/execution is claimed**. The previous increment's MinGW compilation is not used as MSVC validation. ARMASM64 tests and Windows HackRF linking still require the platform.

## 0.5 verification — 9 October 2026

[ITERATION_05.md](ITERATION_05.md) records that revision's matrix: Linux with SDK 9/9, local SOURCE provider 9/9, ASan/UBSan 8/8, without Web 7/7, browser end-to-end, ARM64 cross-build, and Mach-O FFT objects. No native MSVC execution of the new automation in that environment; the user's recap validates the preceding increment. No new benchmark or real USB test.

Structural Doxygen/naming check: 274 production signatures, 328 including tests; adjacent brief/details and parameter coverage. Doxygen HTML not generated locally; target and CI prepared. External APIs and their mocks retain the upstream ABI.
