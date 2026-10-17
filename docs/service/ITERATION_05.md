# NGene 0.5 — native build, Doxygen, and GUI configuration

> Historical report, 9 October 2026. Verification results and limitations describe that revision, not the current release. See the [current operator manual](../wiki/Home.md).

9 October 2026. Incremental changes to the existing 0.4 project. NGene remains the basis; frontend → shared conditioning → single ring → channels is unchanged.

## Windows

Removed the hardcoded Visual Studio 2022 generator. MSVC x64/ARM64 presets with and without HackRF; vcpkg discovery before `project()`, an included manifest, and a matching target triplet. Only upstream libhackrf is compiled; hackrf-tools does not bring in FFTW. Local SDK/source paths are configurable from the command line. DLLs are resolved and collected after linking; installation/CPack include runtime libraries, configurations, and licenses. [Procedure](WINDOWS.md).

Initialized the recorder snapshot on the C4701 path. `_CRT_SECURE_NO_WARNINGS` policy for NGene MSVC targets, retaining `/W4` and UTF-8. External libraries are excluded from the style/warning policy.

The recap demonstrates the user's previous native build and real capture. **The new Windows automation was not run here with MSVC.** Downloading the default tag and collecting DLLs still require native verification; CI is prepared but was not run from that environment.

## Style and comments

Global functions use `Ng...`; static functions start with capitals and contain no underscores, following the Rule 41 convention. C consumers must migrate. Prototypes, definitions, callbacks, and assembly boundaries are documented with Doxygen. Checked 274 production signatures / 328 including tests. [Conventions and limitations](CODING_STANDARD.md).

No rewrite of FFT/SIMD arithmetic or architecture. The assembly symbol was renamed consistently in SSE2/NEON GNU/Microsoft implementations; bit-exact FFT tests passed.

## GUI and files

Configuration: one tab divided into Input source, Receiver, Recording, Diagnostics, Display, and Profile. Content grouped by topic, with before-start/runtime/display indications and a shared Apply & save footer. Dark theme and mobile layout retained.

**Delete** is available for closed files, with confirmation showing the name and directory on the receiver. Cancellation has no effect; only the selected file is deleted, preserving associated files. The server requires a token and current revision and rejects active files, traversal, unrelated extensions, and symlink/reparse points. POSIX uses the directory descriptor; Windows uses native APIs. Also fixed the race between cancellation/rapid reopening and the previous dialog close event.

## Checks actually performed

| Test | Result | Limitation |
|---|---|---|
| Linux GCC, installed HackRF SDK | Build, 9/9 CTest | Simulated driver, no USB |
| Linux SOURCE, local libhackrf v2024.02.1 | Build, 9/9 CTest, installation | Default tag 2026.01.3 not tested |
| Linux ASan + UBSan, without HackRF | 8/8 CTest | LeakSanitizer disabled, assembly not instrumented |
| Linux without WebGUI | Build, 7/7 CTest | No HTTP |
| Chromium desktop/mobile | End-to-end passed | Synthetic source at 100 ksps |
| Linux ARM64 | Executable/tests cross-compiled and linked, NEON | Not executed on ARM |
| macOS x86-64/ARM64 | Mach-O assembly objects compiled | No complete application executed |
| vcpkg/ARM64 detection | CMake fixture script passed | No real Windows installation |

Browser: confirmation/cancellation, active-file protection, token/revision/traversal/extension/symlink checks, associated files preserved, byte-for-byte downloads, sections, and mobile. Previous PSD/waterfall and diagnostic checks were repeated. Screenshots are in `docs/images/`.

No new SIMD or DSP changes; benchmarks remain the previous ones. No new hardware/CPU measurements. Native Windows verification of the new procedure, Windows ARM64, full macOS, Windows Unicode paths, and the full ESA audit remain open.
