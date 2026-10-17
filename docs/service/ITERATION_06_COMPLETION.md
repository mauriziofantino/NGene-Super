# NGene 0.6.1 — final consolidation report

> Historical report, 9 October 2026. Verification results and limitations describe that revision, not the current release. See the [current operator manual](../wiki/Home.md).

Follow-up to the delivered 0.6 review, 9 October 2026. The engine, conditioning/ring, FFT arithmetic, and live/replay stream separation remain unchanged.

## Additional corrections

- The Windows triplet is selected from the target platform, separating any SDK options (`ARM64,version=10.0`). Also handles Visual Studio ARM64 defaults, compiler targets, and a Developer Prompt with Ninja. An ARM64 host is no longer forced to x64. Explicit triplets remain unchanged.
- Bootstrap sets the NGene manifest even if the cache contains obsolete modes/directories, adds the HackRF feature without removing existing ones, and removes duplicates. The baseline and ports remain pinned.
- The Windows consumer header is also adapted when AUTO/SYSTEM finds an installed SDK, preserving external files and imported targets. SOURCE keeps the original source/header for the DLL and the import header for consumers. Strict NGene warnings and PRIVATE third-party suppressions are unchanged.
- The CMake contract now verifies x64/ARM64 selection, ARM64 defaults, compound platforms, cross-targets, vcpkg discovery in a path containing spaces, manifest cache handling, and respect for overrides. An SDK fixture checks the Windows SYSTEM branch without building a Windows DLL.
- clang-tidy was actually run on all project-owned C code, including tests and HackRF. Naming CI was corrected: it also configures HackRF to obtain the necessary compile commands and headers. Only `main` and external ABI names required by mocks are excluded from renaming.
- PowerShell script with explicitly allowed architectures. The final test extracts the actual CPack ZIP archive and finds its executable instead of merely copying the install tree. Debug/Release, CTest, and cleaned PATH checks remain.

## Checks in this follow-up

| Check | Result | What it demonstrates |
|---|---|---|
| Linux Release, Web and HackRF SOURCE | 10/10 CTest, warning-free build | HackRF v2026.01.3 sources downloaded/verified, SDK linked; simulated USB driver |
| Linux Release, HackRF AUTO with an available SDK | 10/10 CTest, warning-free build | Installed-provider selection |
| Linux Release without HackRF, Web enabled | 9/9 CTest | Path without USB dependencies |
| Bootstrap/provider contract | Passed | Configuration tests for new cases, including the simulated Windows branch |
| clang-tidy 22.1.8 | Exit status 0, zero naming violations | Project-owned sources in src/apps/tests and their headers |
| clang-format 21.1.8 | Error-free dry run | Formatting of project-owned C/headers |
| Doxygen 1.18.0 | Successful generation, zero warnings | Documentation with warnings treated as errors |
| PowerShell 7.5.3 on Linux | Parsing without errors | Script syntax; not MSVC execution |
| Chromium desktop/mobile | End-to-end test passed | Parallel sessions, recording/download/delete, diagnostics, and layout |
| Clean Git clone from the deliverable bundle, Linux headless | 8/8 CTest | Configuration/build from a real clone with a clean working tree |

The final archive includes the sources and `ngene-0.6.1.bundle`, cloneable with `git clone ngene-0.6.1.bundle NGene`. The bundle contains a single repository with receiver and GUI. No commit/staging state of the original working directory was changed. External attribution remains in its files, and Git rules preserve the bytes of `third_party/`, including line endings.

ARM64/QEMU and ASan/UBSan checks from revision 0.6 are retained in `docs/service/validation/`; C/assembly code does not change in this follow-up. These are neither new benchmarks nor real USB load tests. The deterministic generator remains exclusively a test fixture; the product has no synthetic frontend.

## Windows zero-touch requirement status

Implementation and the automated procedure are complete in the code. **Native Visual Studio 18/2026 execution had not yet been performed**: that environment was Linux, without MSVC/Windows SDK or access to a Windows host; the checkout had no Git remote on which to run CI. The Windows zero-touch requirement cannot be certified from Linux tests or fixtures.

`scripts/verify-windows.ps1` and the native x64/ARM64 jobs are ready for this test. No baseline update, CMake change, or manual DLL copying is expected. Physical HackRF verification and a formal ESA matrix beyond the applied naming conventions also remain. Current replay uses closed CI8 files and configured metadata; it does not automatically import/concatenate segmented recordings.
