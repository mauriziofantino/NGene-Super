# NGene 0.6.3 — profiles, file selection, and replay metadata

> Historical report for revision 0.6.3. Verification results and limitations describe that revision, not the current release. See the [current operator manual](../wiki/Home.md).

## Configuration error correction

The `Empty/long key or value at line 38` message came from the INI parser: the writer and GUI emitted `replay.path=` even when no file was used. The parser now accepts empty values only for optional string fields (replay path, serial, directory); the writer and GUI omit empty replay paths. Profiles saved by earlier revisions remain readable. Empty numbers and booleans remain errors. Starting a FILE frontend without a path does not change controller state.

## Recording and reading

Every `.cs8` has a `.cs8.meta.ini` sidecar, schema 2, with I/Q parameters and counts for that segment. Header/footer belong to the writer: no I/O is added to the SDR callback. Structures are copied and owned, without temporary references to conditioner names. Import verifies format, count, RF/digital IF, and basename; an inconsistent sidecar does not modify the draft. Compatibility with schema 1 session sidecars is retained. Fs is nominal; host time is not RF/GNSS time.

The file picker browses receiver directories/drives, filters before the 500-entry limit, and shows metadata before confirmation. The Replay button in captures uses the independent session; live reception continues. Import uses bypass to preserve saved values, full range, and automatic recording disabled. Applying/starting remain explicit. Stale browsing responses are discarded. Files read by active replay are protected against deletion.

FFT/SIMD, ring, acquisition/tracking algorithms, and sample management were not rewritten. External libraries are unchanged. Windows retains existing MSVC presets, warnings as errors, vcpkg baseline, and staging. Narrow/ASCII Windows path support remains the current contract.

## Tests performed

Linux x86-64, GCC 13, CMake 4.4.3, warnings as errors:

| Configuration | Build | CTest |
|---|---|---|
| HackRF OFF Debug | pass | 10/10 |
| HackRF OFF Release | pass | 10/10 |
| HackRF ON Debug | pass | 11/11 |
| HackRF ON Release | pass | 11/11 |

HackRF ON uses locally available libhackrf sources and system libusb. It does not certify network download/bootstrap or Windows USB runtime. PowerShell is unavailable in that environment; the two optional PowerShell tests are not counted.

Real Chromium test: file picker with paths containing spaces, import from a newly recorded sidecar, parallel sessions, deletion protection, filtering beyond 500 files, and sidecar rejection without profile mutation; previous PSD, histogram, GUI pacing, idle diagnostics, transfer, mobile, and shutdown checks retained. A delayed GUI request could be followed too soon by the next one: pacing measured time since sending, whereas the server measures time between received requests. The GUI now also spaces requests from the previous response; a test delaying transmission by 80 ms reproduces the issue before the correction and passes afterward. Switching sessions also disables old message/error handlers, preventing late snapshots from reaching the new controller. No FFT algorithm or integrity check was changed.

CLI `--replay` with sidecar and a path containing spaces: 2000 pairs processed, correct digital IF and bypass, exit status 0. Build without WebGUI verified for the metadata API used by the CLI. New metadata and filesystem helpers compiled as ARM64 objects with warnings as errors; ARM64 recorder also compiled. This is not an ARM64 runtime test. Doxygen 1.18.0: complete documentation with warnings as errors, pass.

Clean-checkout tests and logs are in `validation/replay-063/`. Fixtures also check signed CI8 and minimum values, segmentation, historical sidecars, incorrect count/basename, and unsupported formats.

## Limitations to retain

- No native MSVC execution of 0.6.3: OFF/ON × Debug/Release and Windows staging still need testing. The user's 0.6.2 log covered ON Release, not the full matrix.
- No USB/RF test with a connected HackRF: enumeration on that host returns a libusb error, not positive hardware verification.
- Replay reads the selected segment; concatenation and gap/epoch reconstruction from CSV are not implemented yet.
- Raw and sidecar must be kept/transferred together. Delete always removes one file, without implicitly removing associated files.

## Updating an existing Windows clone

Place `ngene-0.6.3.bundle` beside the clone directory; run from that directory after closing NGene:

```powershell
git fetch ..\ngene-0.6.3.bundle main
git merge --ff-only FETCH_HEAD
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Action Build -Hackrf ON -Configuration Release
.\build\msvc-x64-hackrf\Release\ngene.exe --web --port 8080
```

There is no need to clone again or run the full verifier for a normal build. The same tasks are available in VS Code. Build configures/compiles without running tests.
