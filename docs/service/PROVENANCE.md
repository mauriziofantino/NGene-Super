# Provenance and dependencies

This is an increment in NGene's evolution; NGene remains the project's functional and algorithmic basis. The [migration map](LEGACY_MIGRATION.md) tracks recovery of original modules. It preserves and makes explicit the historical receiver's shared-buffer and per-channel management concepts, incorporating the new common advancement requested by the user. It is not a mechanical reorganization of the entire archive and does not include copies of the old assembly or retired USB drivers.

The supplied archive's `frontEnd/circularBuffer.h`, `frontEnd/circularBuffer.c`, and `receiver/channelManager.c` were consulted, along with the user's manual and diagram. Historical code was not modified. Selection of DSP/GNSS modules for reuse will be tracked in subsequent increments, preserving any licensing obligations.

This increment is written in C; CMake and CI are build tools. Python and C++ are not required to compile or run NGene. Build directories and SDKs used for verification are not distributed with the sources.

## External components

- **libhackrf**, Great Scott Gadgets: native C API for HackRF; optional external dependency. Adapter verification used host version 2024.02.1, library API 0.9, commit `18b485e3b6d2031c15a79ba89cdb42b5fa245f24`.
- **libusb**: libhackrf transport dependency, managed by the distribution/SDK.
- C library and POSIX/Win32 OS primitives; no external DSP dependency.

Primary sources used for the adapter:

- [libhackrf API](https://github.com/greatscottgadgets/hackrf/blob/v2024.02.1/host/libhackrf/src/hackrf.h).
- [Capture client implementation](https://github.com/greatscottgadgets/hackrf/blob/v2024.02.1/host/hackrf-tools/src/hackrf_transfer.c).
- [Installing HackRF software](https://hackrf.readthedocs.io/en/latest/installing_hackrf_software.html).

External libraries' author/copyright headers are not copied away or removed. A new public license is not automatically applied to the project: the owner must decide distribution terms before publication.

## Style

Doxygen-documented APIs, explicit-width types for data/ranges, four-space indentation, and explicit error handling. No author names in new NGene files. No certification or complete ESA compliance check is claimed: this is a dedicated task, separate from functional verification of this increment.

## Included Web dependencies (v0.2.0)

- **CivetWeb v1.16**, commit `d7ba35bbb649209c66e582d5a0244ba988a15159`: [official source](https://github.com/civetweb/civetweb/tree/v1.16). C HTTP/WebSocket server; MIT license retained in `third_party/civetweb/LICENSE.md`. Headers, C implementation, and `.inl` files are included; TLS, CGI, and scripting are not enabled. A local change in `read_websocket()` rejects payloads larger than `NGENE_WS_MAX_PAYLOAD` before allocation (4096 bytes in the build). CMake fixes `BUILD_DATE` to avoid nonreproducible timestamp macros.
- **cJSON v1.7.19**, commit `c859b25da02955fef659d658b8f324b5cde87be3`: [official source](https://github.com/DaveGamble/cJSON/tree/v1.7.19). C parser/serializer; MIT license retained in `third_party/cjson/LICENSE`. Maximum depth 32 set at compile time. JSON patch utilities are not compiled.

Upstream attribution is retained in the corresponding files. Included Web dependencies require no downloads during CMake configuration. The optional HackRF SOURCE provider and vcpkg may download missing dependencies. Playwright/Chromium and Node are optional GUI test tools only, not dependencies of the executable or the user's browser.

## FFT v0.3

The new FFT derives from original NGene's Q14/radix-2 design, following analysis of `asmFolder/fft.c`, `fft.h`, and `simd.h`. Butterflies were rewritten to make the ABI explicit, remove global backup storage, and correct saturation/scaling. No external FFT library is imported; [FFT_REVIEW.md](FFT_REVIEW.md) documents retained and changed features. Unicorn was used only as an external NEON instruction verification tool; it is not a project dependency and is not included.

## v0.4 consolidation

FFT v0.3 arithmetic remains unchanged. Additions include the Windows ARM64 ARMASM64 file, internal SSE2/NEON intrinsics primitives and their C reference, continuous DC conditioner, telemetry integrity, and GUI checks. No external DSP dependencies are added. `ITERATION_04.md` distinguishes checks performed from targets still requiring native validation.

## v0.5 consolidation

NGene APIs renamed and documented; `third_party/` unchanged. The SOURCE provider builds only upstream hackrf.c without modification, from a local checkout or configurable v2026.01.3 tag. The local test used the already available v2024.02.1; the new tag was not downloaded/tested in that environment. libusb and pthreads are optional dependencies manageable through the Windows vcpkg manifest. Installation preserves CivetWeb/cJSON licenses and, for the SOURCE provider, libhackrf's COPYING. The owner still needs to decide the NGene code license, consistently with the dependencies used.

## 0.6 consolidation

Downloaded and compiled upstream HackRF tag **v2026.01.3**, with versioned URL and SHA256 in `cmake/DependencyLock.cmake`. The library reports version **0.9.2**; the tag date is not the ABI version. Manifest with vcpkg baseline `0699a19d0c6386247ce50d4dedbb8217d484d536`, libusb 1.0.30, and pthreads 3.0.0#14.

Upstream source is not modified. Windows generates a separate consumer header correcting the export/import selector while preserving signatures and license; the DLL compiles the original header. The 21 vendored CivetWeb/cJSON copies remain byte-for-byte identical to increment 0.5. Warning reductions apply only to external targets/includes, not NGene code.

Naming, formatting, and documentation apply to NGene code. Replay files and test fixtures are internal C implementations; no external DSP library was added. AST tools, the formatter, QEMU, and Doxygen used for verification are external to the project and are not included in the source package.
