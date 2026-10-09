# NGene 0.6.2 — Windows/MSVC build candidate

**Acceptance status: OPEN.** This revision has not been built or executed on native Windows in this environment. No successful Linux test, simulated dependency resolver, PowerShell parser or unexecuted CI definition is counted as a Windows result.

## Actual failure and correction

The supplied Windows 0.6.1 log was read in full. It shows MSBuild 18, Release, HackRF successfully linked, repeated CMP0207 diagnostics during post-link staging, and a fatal first-party C4701/C2220 in `src/web/server.c`. It does not establish Debug/OFF/clean-clone/package acceptance of the new revision.

- `TNgConfig` in the HTTP configuration path is initialized. The prepare-stream failure path explicitly retains `NG_ERROR`; formatting a diagnostic with `NgFail` cannot turn that path into validation/application of an unread config. `/W4 /WX` stays enabled, with no C4701/C4703 suppression.
- `StageRuntime.cmake` sets its own minimum version and guarded `CMP0207=NEW`, because `cmake -P` does not inherit the project's policy scope. There is no `-Wno-dev` workaround.
- Staging now runs for native Windows executables/tests with HackRF OFF as well as ON. Selected Release/Debug MSVC CRT files are discovered and staged before recursive dependency resolution; Debug includes the SDK debug UCRT. Only the selected configuration's adjacent DLLs are installed. A directory lock prevents simultaneous post-link copies from racing with dependency inspection.
- vcpkg bootstrap is checked again on configure retries with a cached toolchain. `cmd /c call` invokes its batch file with quoted path arguments, including paths with spaces.
- The existing baseline, dependency overrides, source hash, SYSTEM includes and private third-party warning policy are preserved. No new FFTW/pkgconf dependency, SDK preparation, hand-copied DLL or compiler-warning suppression was added.
- OFF is explicit in the base Windows preset. Both OFF/ON configurations have Release and Debug build/test presets; configure never pins VS 2022, v143 or a user-specific path.
- The native verifier creates a fresh Git clone of the committed candidate, runs the four combinations, CTest, build/install/ZIP smoke tests and HackRF enumeration, captures full logs, and requires a complete, nonempty, unskipped JUnit report and records the exact commit in `acceptance.json`. It rejects an older dirty working tree, compiler diagnostics, policy warnings, failed commands, missing DLLs, and an unverified integrated-vcpkg criterion. CI retains the evidence on failure as well as success.

## Available evidence

The tested build source is commit `0ec3155a06a85e52773a33e85bf8b8bbfc3ab68d`. The delivery also contains a further verifier-only correction to XML element handling, checked against all four actual JUnit reports and targeted helper fixtures; it changes no receiver C/assembly.

Fresh-clone Linux build evidence is stored in `validation/msvc-062/`. These builds use GCC, the unchanged GNU SSE2 assembly, native upstream libhackrf and local libusb development headers. They are **not** native MSVC tests or a Windows DLL-loading test.

| Check | Result | Scope |
|---|---|---|
| Linux HackRF OFF Release | 11/11 CTest | Fresh clone, warning-as-errors for own C |
| Linux HackRF OFF Debug | 11/11 CTest | Fresh clone, warning-as-errors for own C |
| Linux HackRF ON Release | 12/12 CTest | Fresh clone, pinned source downloaded/verified |
| Linux HackRF ON Debug | 12/12 CTest | Fresh clone, pinned source downloaded/verified |
| Browser end-to-end | Passed | Rejected INI cannot change saved profile/revision; existing receiver/GUI/replay/recording behavior |
| Runtime staging contract | Passed | Mock dumpbin: transitive DLLs, CRT, OS exclusions, spaces, missing-DLL failure, standalone policy |
| PowerShell verifier contract | Passed | Parser and helper: nonzero native exits, compiler diagnostics including localized messages, CMake policy warnings, incomplete/failed/skipped JUnit report rejection |
| `--help` / `--version` | Passed on Linux | Both frontends and configurations |
| Real `--list-devices` on Linux | Failed: `hackrf_init: Other error` | USB initialization unavailable here; no device/RF proof claimed |
| Native Windows x64 OFF/ON Debug/Release | **Not executed** | Windows host is not accessible from this session |
| Windows integrated vcpkg / MASM / staged runtime | **Not executed** | Requires the actual Windows verifier results |
| Native macOS / Windows ARM64 | **Not executed in this iteration** | Relevant assembly and C paths retained without modification |

The exact upstream third-party bytes and the x86/ARM assembly files are unchanged from the supplied 0.6.1 archive. No DSP arithmetic or receiver architecture was rewritten.

## Native acceptance procedure

Prerequisites remain Visual Studio 2026 with C/C++ tools and Windows SDK, CMake >=4.2 and Git. VS Code is an editor, not the MSVC compiler/toolset. The source repository includes a valid versioned vcpkg manifest. Internet access is needed for the first dependency configure.

From the source ZIP's parent directory:

```powershell
git clone ngene-0.6.2.bundle NGene
cd NGene
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-windows.ps1
```

The script prints the directory containing `acceptance.json`, full command logs, configure/CTest/vcpkg logs and all four installed/extracted packages. No manual CMake edits, baseline update, SDK construction or DLL copying are steps in this procedure. The usual three preset commands remain supported; see [WINDOWS.md](WINDOWS.md), including the Debug presets.

The script itself performs a second fresh clone. Its source must therefore include committed changes: the bundle provided with this candidate does. It temporarily clears SDK/toolchain/compiler overrides and restores them even on failure. Native tests run only on a matching host architecture; ARM64 cross-builds remain supported separately.

If Visual Studio's integrated vcpkg is absent, the normal build automatically uses the pinned bootstrap fallback. The verifier nevertheless keeps that specific integrated-vcpkg acceptance criterion open and reports it explicitly. Debug packages are development-test artifacts; distribute Release packages with the redistributable CRT.

Acceptance closes only when the Windows results verify every requested criterion. The current deliverable is a corrected candidate and a reproducible test procedure, not a certification of unexecuted builds.

## Primary references

- [CMake CMP0207](https://cmake.org/cmake/help/latest/policy/CMP0207.html)
- [Visual Studio 18 2026 generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html)
- [Compiler runtime discovery](https://cmake.org/cmake/help/latest/module/InstallRequiredSystemLibraries.html)
- [Microsoft C4701](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-4-c4701)
