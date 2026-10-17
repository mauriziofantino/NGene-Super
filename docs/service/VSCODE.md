# NGene in VS Code on Windows

> Supporting development notes. Current receiver operating instructions are in the [project wiki](../wiki/Getting-Started.md).

Open the repository folder (`NGene-git`, containing CMakeLists.txt), rather than
the folder holding the bundle. The build uses native MSVC and presets shared
with PowerShell. MinGW, MSYS2, WSL, and personal SDK paths are not required.

Prerequisites: Visual Studio 2026 with C/C++, Windows SDK, and CMake >=4.2 installed
(including as a Visual Studio component). ARM64 requires the ARM64 toolset.

## Building and testing

**Ctrl+Shift+B** starts `NGene: Build (Windows MSVC)`. Menus select Release/Debug,
HackRF ON/OFF, and x64/ARM64. The task configures before building; CMake, CTest,
and CPack are automatically discovered from the same suite.

The following tasks are available through **Terminal → Run Task**:

| Task | Operations |
| --- | --- |
| NGene: Configure (Windows MSVC) | Configures the selected preset |
| NGene: Build (Windows MSVC) | Configures and builds |
| NGene: Test (Windows MSVC) | Configures, builds, and runs CTest |
| NGene: Package (Windows MSVC) | Configures, builds, and generates the ZIP with CPack |
| NGene: Verify clean Windows checkout | Complete OFF/ON × Debug/Release verification from a new clone |

Compiler errors are associated with the **Problems** view using the MSVC matcher;
the terminal displays full output and the result. Daily-workflow logs are in
`build/msvc-<arch>[-hackrf]/logs/`. CTest also saves a JUnit report.
The package is in `out/<build-preset>/`.

Development tasks build working-tree code, including uncommitted changes.
Complete verification instead requires a clean, committed Git checkout to record
the commit actually verified. Runtime tests for an ARM64 cross-build must run
on an ARM64 host; an x64 host can build and package ARM64.

## The same commands from PowerShell

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Action Test -Configuration Debug -Hackrf OFF
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Architecture ARM64 -Action Package
```

The entry point does not require CMake in PATH and does not modify the persistent
PATH. VS Code does not need to be started from a Developer Prompt.

## Extensions and verification

Recommendations in `.vscode/extensions.json` include Microsoft C/C++ and CMake
Tools for editing and navigation. NGene tasks also work without CMake Tools:
they use `scripts/build-windows.ps1` directly. The extension can use the same
CMakePresets.json; automatic configuration on opening is disabled to avoid an
unintentional second build or selection of a different generator.
No PC-specific `cmake.cmakePath` is versioned.

This integration is verified through file parsing, plans for all eight presets,
and test tools covering configure/build/test flow and propagation of a CTest
failure. PowerShell/Linux tests do not certify an interactive VS Code session
or an MSVC run on Windows.
