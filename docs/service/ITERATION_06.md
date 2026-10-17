# Increment 0.6 — Windows/HackRF, naming, and replay

> Historical report, 9 October 2026. Verification results and limitations describe that revision, not the current release. See the [current operator manual](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Home).

9 October 2026. Revision of the existing project, without changing the conditioning → ring → channels path or FFT arithmetic. Original NGene remains the basis for GNSS modules to migrate.

## Windows build from a clean checkout

- Versioned manifest: `builtin-baseline=0699a19d0c6386247ce50d4dedbb8217d484d536`; libusb 1.0.30 and pthreads 3.0.0, port-version 14. No `x-update-baseline` required.
- Bootstrap before `project()`: discovers an existing vcpkg installation or creates a pinned checkout in the build tree; automatic x64/ARM64 triplet from the generator target.
- HackRF AUTO uses an existing SDK or downloads and builds only the C library v2026.01.3, SHA256 `48238f3a21189fa8cbe67838584cc045b9e5433767db8c9c30c1da02a1489f2c`. No hackrf-tools/FFTW.
- Removed the extra `ADD_EXPORTS` definition. The DLL uses the original header; the Windows consumer copy uses `dllimport`, without changing upstream sources or ABI.
- Warnings are disabled only on external targets, using SYSTEM includes. NGene retains `/W4 /WX` and strict GCC/Clang warnings. No global suppression.
- Separate Debug/Release libusb import libraries; pthreads through a CMake target. Automatic DLL collection after linking and during installation.
- Presets have no hardcoded VS toolset; Microsoft C is required with `NGENE_REQUIRE_MSVC=ON`. Scripts and CI for Visual Studio 18/2026: Debug/Release, tests, installation, and relocated package with a cleaned PATH. No MinGW path.

From PowerShell, with Visual Studio 2026 C++ and CMake ≥4.2 available:

```powershell
./scripts/verify-windows.ps1
```

The script exercises HackRF ON/AUTO, vcpkg bootstrap ON, Web ON, tests ON, and REQUIRE_MSVC ON. It requires a new build directory so an earlier cache cannot hide problems. Prerequisites in [WINDOWS.md](WINDOWS.md).

## Code and GUI

Consistent naming: `TNg...` types/tags, capitalized functions, lowerCamel variables/parameters/members, and uppercase macros. `TNgRecorder` uses `statsMutex`, `writerError`, `segmentOffset`, etc. Versioned mappings for consumers. Doxygen, member documentation, matching declaration/definition parameter names, headers without implementations, and consistent formatting. The 21 vendored copies are identical to the previous version.

Removed the production `synthetic` frontend and profiles. FILE frontend with signed interleaved CI8, 64-bit seeks, pair ranges, optional pacing, and looping with a discontinuity. The deterministic generator is only a test fixture.

Independent primary controller and replay: separate configurations, clocks, rings, conditioning, diagnostics, and recorders for each stream. All channels of a stream share one window. No live/replay mixing. Dark GUI with session selector and replay controls; commands/plots act on the selected session. REST `/api/v1/replay/...`, WebSocket `replay:MASK`. [REPLAY.md](REPLAY.md).

## Checks performed

| Configuration | Result | Limitations |
|---|---|---|
| Linux x86-64 Release, pinned HackRF SOURCE | Warning-free build; 10/10 CTest | Real SDK compiled; simulated USB input |
| Linux x86-64 ASan + UBSan | 9/9 CTest | LeakSanitizer disabled because of container limitations; assembly not instrumented |
| Linux x86-64 headless | 8/8 CTest; repeated on sources extracted from the final archive in a new build tree | Without Web/HackRF |
| Linux ARM64, Zig/Clang cross-build, QEMU | Full build, 9/9 CTest executed | Emulation, not ARM hardware or a benchmark |
| ARM64 HackRF adapter | Object compiled with strict warnings and upstream headers | No ARM SDK linking/real USB |
| Chromium desktop/mobile | End-to-end test passed | Real live input unavailable |
| Doxygen 1.18.0 | HTML generated, zero warnings | Not semantic ESA certification |
| AST naming check | Zero violations in analyzed project-owned C code | clang-tidy CI prepared, not run on the remote repository |
| Native Windows MSVC x64/ARM64 and macOS | Procedures and CI prepared | Not run locally; zero-touch Windows still requires the native job |

Tests: baseline/hash, CRLF/idempotent import header with unchanged upstream files, lease lifetime, signed I/Q, partial EOF, ranges/loops, source independence, replay during a simulated HackRF lease, two simultaneous Web controllers, recording/download/delete, PSD integrity, and a slow GUI. Updated screenshots in `docs/images/`.

## Open issues

The Windows build was corrected and made automatically verifiable; it is not claimed to be validated before execution. Native MSVC and real HackRF tests are needed. Windows Unicode paths, GNSS algorithms, additional frontends, and automatic metadata/segment import remain open. Replay uses closed immutable files, not files still being written; Fs and metadata must be explicitly configured.

No new SIMD or numerical FFT changes in this revision; previous benchmarks are retained, with no new results. Full ESA compliance beyond naming requires a rule matrix, including OS/SIMD deviations.
