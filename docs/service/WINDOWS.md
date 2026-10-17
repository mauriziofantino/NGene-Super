# Windows — native MSVC build and dependencies, v0.6.3 (candidate)

> Historical build notes for versions 0.6.3/0.6.4. Validation statements below describe those iterations, including the Windows environments unavailable at the time. Current operator instructions are in [Getting Started](../wiki/Getting-Started.md).

The standard path starts from a clean checkout: Visual Studio 18/2026 with the C/C++ workload, Windows SDK, Git, and CMake >=4.2. No project file needs editing. The ARM64 toolset is required only for an ARM64 target. The general CMake minimum remains 3.26 for other generators/operating systems.

**Complete native acceptance remains open.** The supplied 0.6.2 log documents successful HackRF ON Release, tests, and staging/package without warnings. It does not contain the entire OFF/ON × Debug/Release matrix. Version 0.6.3 adds file selection and replay metadata; it was not run here with MSVC. Linux tests and fixtures do not certify a Windows build.

## Standard procedure

For daily builds from PowerShell or VS Code, use the shared entry point, which
detects tools even when they are outside PATH:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Action Test
```

In VS Code, open the repository and use **Ctrl+Shift+B**; for tests, choose
**Terminal → Run Task → NGene: Test (Windows MSVC)**. [Details](VSCODE.md).
`-Configuration Debug`, `-Hackrf OFF`, and `-Architecture ARM64` select presets.
This mode accepts the development working tree and does not require a new clone.

From an ordinary PowerShell session, the verifier also automatically discovers
CMake bundled with Visual Studio, without changing PATH or opening a Developer Prompt:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-windows.ps1
```

It searches PATH, standard standalone installations, and Visual Studio installations
identified by vswhere, including Build Tools/preview. It requires a complete
suite >=4.2 and uses absolute paths to cmake/ctest/cpack from the same version,
preferring the most recent compatible one. It reports paths in the console and
acceptance.json. If CMake is missing, it identifies the precise “C++ CMake tools
for Windows” component to install/update in Visual Studio, or the standalone
CMake >=4.2 prerequisite. It does not install system software or modify the
persistent PATH.

The following direct commands are available when CMake is already in PATH:

```powershell
cmake --preset windows-msvc-x64-hackrf
cmake --build --preset windows-msvc-x64-hackrf --parallel
ctest --preset windows-msvc-x64-hackrf
.\build\msvc-x64-hackrf\Release\ngene.exe --web
```

The preset without a suffix selects Release. Debug uses the same sources and configuration:

```powershell
cmake --build --preset windows-msvc-x64-hackrf-debug --parallel
ctest --preset windows-msvc-x64-hackrf-debug
```

For HackRF OFF, use `windows-msvc-x64`, including for configuration; the corresponding Debug build/test preset is `windows-msvc-x64-debug`. `NGENE_WITH_HACKRF=OFF` is explicitly set rather than merely relying on the cache default. Configuration does not impose a generator or toolset from a specific Visual Studio version. CMake policy errors during configuration are not suppressed.

The preset requires MSVC and enables HackRF, the AUTO provider, vcpkg bootstrap, web, and tests. CMake chooses the available Visual Studio generator; the project does not force v143 or VS 2022 references. MinGW, MSYS, WSL, Node, Python, a manually prepared HackRF SDK, and a Developer Prompt are not required for these Visual Studio generator calls.

If vcpkg is unavailable, NGene downloads it into the build directory at the pinned commit. Before `project()`, CMake selects the toolchain and the `x64-windows` or `arm64-windows` triplet. The explicit platform takes precedence over the host; `ARM64,version=10.0` and the Visual Studio ARM64 default are handled correctly. Ninja considers compiler target, processor, and Developer Prompt. An explicit triplet remains unchanged. A vcpkg installation already bundled with Visual Studio is found using vswhere; `VCPKG_ROOT` and `VCPKG_INSTALLATION_ROOT` are optional overrides. The tool is bootstrapped if its executable is missing.

## Reproducibility

`vcpkg.json` versions:

- builtin baseline `0699a19d0c6386247ce50d4dedbb8217d484d536`;
- libusb `1.0.30`;
- pthreads `3.0.0`, port revision `14`.

`cmake/DependencyLock.cmake` contains the same baseline; a mismatch causes configuration to fail. **Do not run `x-update-baseline`: the repository is already configured.** Toolset/SDK versions remain those installed on the PC; pinning dependencies does not make the entire environment hermetic.

`AUTO` uses an installed HackRF library if one is discoverable. In a clean environment, it downloads `v2026.01.3` from the upstream repository and verifies SHA-256 `48238f3a21189fa8cbe67838584cc045b9e5433767db8c9c30c1da02a1489f2c`. To enforce the same provenance on PCs with existing SDKs as well, use `NGENE_HACKRF_PROVIDER=SOURCE`. Only `hackrf.c` is compiled; no hackrf-tools, FFTW, or pkgconf. A custom release requires its own explicit SHA-256.

The provider retains separate Release/Debug libusb libraries when available in vcpkg. PThreads4W is linked through its CMake configuration target. vcpkg dependencies use the Microsoft toolset rather than a MinGW toolchain.

## Warnings and DLL headers

NGene retains `/W4 /WX /utf-8 /external:W0`. Suppressions are **PRIVATE** to the external `ngene_hackrf_native` and `ngene_http` targets: `/W0` on MSVC, `-w` on other compilers. External includes are SYSTEM. The same strict flags are not propagated to libhackrf/libusb/pthreads; vcpkg builds the latter two separately according to their ports.

`_CRT_SECURE_NO_WARNINGS` remains restricted to the affected C targets: it permits portable ISO C APIs (`fopen`, `snprintf`, `tmpfile`) instead of imposing Microsoft-specific `_s` replacements. It does not disable `/W4 /WX`, conversion checks, uninitialized-flow checks, or general warnings. No suppression of C4701/C4703, C4244, or C4267 was added.

The HTTP configuration branch initializes the draft and preserves explicit `NG_ERROR` after a temporary-file write/flush/seek error. The result of `NgFail` is no longer used to decide whether a read configuration exists. This prevents both the ambiguous MSVC path and application of a draft after an I/O error. Tests verify that malformed configurations do not change the profile or revision.

The pinned HackRF release already defines `ADD_EXPORTS` in its header: NGene does not redefine it on the compiler command line. For DLL consumers, it generates a header copy that removes only the unconditional definition, retaining the ABI, calling convention, license, and dllimport branch. The DLL target still sees the original upstream header. The SYSTEM/AUTO provider with an installed SDK also uses a private consumer header for NGene when necessary; the imported SDK target remains unchanged. No downloaded source or local SDK is modified. This transformation is covered by tests, including CRLF and headers that are already correct.

## DLLs and packaging

After linking, CMake uses `dumpbin` from the selected compiler to resolve the dependency closure and collects non-system DLLs beside the program and tests, **including with HackRF OFF**. The selected configuration's MSVC runtime is found through `InstallRequiredSystemLibraries` and automatically placed beside executables before resolution; Debug includes the Windows SDK debug UCRT. The Release package does not include the debug CRT. Debug archives are for local verification/development rather than redistribution of the Microsoft debug runtime.

`StageRuntime.cmake` declares its CMake minimum and sets `CMP0207=NEW` when available. The `cmake -P` process has its own policy scope: setting the policy only in the main project would be insufficient. Filters remain compatible with CMake versions before 4.3. A per-directory lock serializes copies of the same runtimes during parallel program/test linking. Packaging errors are not suppressed. The standard vcpkg path requires no additional paths or manual copies.

```powershell
cmake --install build/msvc-x64-hackrf --config Release --prefix package
cpack --config build/msvc-x64-hackrf/CPackConfig.cmake -C Release
.\package\bin\ngene.exe --version
```

Custom SDKs can specify `HACKRF_ROOT`, `CMAKE_PREFIX_PATH`, and `NGENE_RUNTIME_DIRS`; these are overrides for external installations rather than standard-procedure steps. Use a new build directory when changing generator or architecture.

## ARM64 and verification from a clean checkout

```powershell
cmake --preset windows-msvc-arm64-hackrf
cmake --build --preset windows-msvc-arm64-hackrf --parallel
```

An x64 host can compile ARM64 with MSVC/ARMASM64; the corresponding tests must run on ARM64. Defined CI jobs use Visual Studio 2026 x64 and ARM64 runners; the new execution was not started in this environment.

For complete verification on native Windows, run the following from a Git checkout **with corrections already committed**:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-windows.ps1
```

The script rejects a dirty working tree to avoid accidentally certifying an earlier revision. Users of the delivered archive can obtain the checkout with `git clone ngene-0.6.3.bundle NGene-git`, then run the script inside `NGene-git`. There is no need to initialize or update the baseline.

The script:

1. Creates a new clone and new builds; preserves the exact Git revision in the results.
2. Temporarily removes SDK, toolchain, generator, and compiler-flag overrides, restoring them afterward.
3. Configures both OFF/ON presets and verifies a VS 2026+ generator, MSVC compiler, and `ml64` for x64.
4. Builds **OFF/ON × Debug/Release**, runs CTest, and also detects compiler/linker/CMake warnings in complete logs.
5. Tests `--help`, `--version`, and, with HackRF ON, `--list-devices` from the build, install tree, and actually extracted ZIP, with PATH restricted to Windows system directories.
6. Preserves logs, builds, packages, and `acceptance.json` in a new temporary directory reported in the console. `-WorkRoot` selects a new one without editing project files.
7. Continues with other combinations after a failure and does not report successful acceptance if any combination fails or if Visual Studio's bundled vcpkg was not actually used.

Visual Studio without bundled vcpkg still uses the automatic fallback provided by the build. In that case, the acceptance script reports the specific “bundled vcpkg” test as not yet performed rather than treating fallback use as certification of that requirement.

On native ARM64, use `-Architecture ARM64`. The verifier rejects execution on a host of a different architecture; cross-compilation remains available through presets but does not replace native runtime tests.

## Validation status

Actual Linux 0.6.3 builds/tests are available in all four combinations, along with the browser regression for configuration, file picker, and replay metadata. The staging contract uses simulated `dumpbin` output to test policies, transitive dependencies, spaces in paths, CRT, exclusion of OS files, and failure for missing DLLs. The PowerShell test checks parsing and exit/warning capture, including localized output. **None of these tests is a native Windows build.**

The user's 0.6.2 log covers ON Release. An accessible Windows host/runner is missing for 0.6.3: the four MSVC results with smoke tests and clean clones are not yet available. Acceptance remains open; [WINDOWS_BUILD_062.md](WINDOWS_BUILD_062.md) distinguishes implementation, completed tests, and missing tests. New CI is configured, not claimed to have run.

USB drivers, connected hardware, and RF validation remain tests on the receiver computer: `ngene --list-devices`. The build does not install drivers or firmware. Windows Unicode-path support remains the previous narrow-API implementation: use ASCII paths in this revision.

### Verifier: Windows PowerShell 5.1

The default checkout path is resolved after parameter binding to avoid the empty
`$PSScriptRoot` value observed in the parameter in the first 0.6.3 delivery.
To continue from that bundle without editing files:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-windows.ps1 -SourceRepository "$PWD"
```

The updated package includes the correction and does not require this argument.
The entry-point test covers folders with spaces, a different current directory,
and explicit repositories; running it on PowerShell 7/Linux does not certify
Windows PowerShell 5.1 or an MSVC build.

### Continuing from the previous clone with the updated verifier

To avoid modifying the working tree, save the delivered standalone copy of
`verify-windows.ps1` beside the `NGene-git` folder, outside the repository.
From the terminal already open in `NGene-git`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\verify-windows.ps1 -SourceRepository "$PWD"
```

The external verifier builds the specified clone's commit and records its hash;
it does not automatically apply project changes. The updated bundle contains
the same correction for future checkouts.

### External-dependency warning D9025

The native log supplied during this iteration shows HackRF ON / Release with
zero errors and two D9025 warnings. Verification still fails: the excerpt does
not certify CTest or the other configurations. The previous `/w` option ended
up in AdditionalOptions alongside MSBuild's default `/W1` WarningLevel.
Now only external targets receive `/W0`, which CMake maps to
`WarningLevel=TurnOffAllWarnings`. NGene retains `/W4 /WX`; the verifier still
rejects D9025 and checks .vcxproj files after configuration.

To update a checkout from the earlier bundle, save the new bundle in the
directory containing `NGene-git` (replacing the old bundle), then run from
the checkout:

```powershell
git fetch ..\ngene-0.6.3.bundle main
git merge --ff-only FETCH_HEAD
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-windows.ps1
```

The new verification creates a clean clone of the updated commit; it does not
reuse the previous build. No manual changes to CMake files, warning levels,
or DLLs are required.

### FTK1011 in CMake tests

The new Windows log shows three visible builds with 0 warnings/0 errors,
functional tests passed (including FFT and --list-devices), and only
build_dependency_contract failed. Compiler-probe .tlog paths in the SDK fixture
were 261–269 characters long. SDK and warning test projects are now created in
a short, unique temporary root (`ngt-*`) outside the receiver build.
CMake's compile/link tests remain active; CMAKE_C_COMPILER_WORKS is not forced,
and FileTracker is not disabled. On failure, the directory remains available;
after success, it is removed. The verifier also uses a shorter temporary name
(`ngm-*`). No registry, LongPathsEnabled, or PC CMake-file changes are required.
The correction leaves acceptance open until another complete native test.

## Multi-SDR 0.6.4

The preceding HackRF path remains available. To include HackRF, RTL-SDR, and bladeRF in the same executable:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -MultiSdr
.\build\msvc-x64-multisdr\Release\ngene.exe --web --port 8080
```

This build does not run the test matrix. In VS Code, select **NGene: Build multi-SDR (Windows MSVC)**. `-Configuration Debug` and `-Architecture ARM64` are supported by the same script; presets do not impose a specific Visual Studio generator or toolset. Dependencies and DLLs use automatic bootstrap/staging. The bladeRF build applies RFIC patches through CMake, without Unix patch or MSYS. [Complete procedure and limits](MULTISDR_064.md).

An MSVC build of 0.6.4 was not executed during this iteration; Linux logs and ARM64 object compilation do not certify Windows. Native acceptance still needs verification on the Windows machine.
