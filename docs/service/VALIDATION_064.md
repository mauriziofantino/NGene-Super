# NGene 0.6.4 validation — Linux candidate

> Historical validation report for revision 0.6.4. These results describe that revision and its test environment, not the current release. See the [current operator manual](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Home).

Validation evidence is under `validation/multisdr-064/`. This increment uses source baseline `c32d459`, CMake 4.4.4 and GCC 13.3.0 on Linux x86-64 (virtualized AMD EPYC 9V74 host). First-party warnings remain errors. Native Windows/MSVC, macOS, ARM64 execution and physical RF hardware were not available.

| Configuration | Result |
| --- | --- |
| Baseline 0.6.3 Release, all native drivers disabled | 10/10 tests passed |
| 0.6.4 Release, all native drivers disabled | 11/11 tests passed |
| 0.6.4 Debug, all native drivers disabled | 11/11 tests passed |
| 0.6.4 Release, HackRF + RTL-SDR + bladeRF | 14/14 tests passed |
| 0.6.4 Debug, HackRF + RTL-SDR + bladeRF | 14/14 tests passed |
| AddressSanitizer + UBSan, Debug, native drivers disabled | 11/11 tests passed; leak detection disabled, not verified |
| ARM64 cross-compilation of 14 changed module/test objects | Passed with warnings as errors; compile only |
| Headless Chromium 134 / Playwright 1.51 GUI regression | Passed |
| Doxygen 1.18.0 API/implementation reference | Passed with warnings as errors |
| CMake-only RFIC patch application | All 8 files match GNU-patch reference byte-for-byte |
| Fresh Linux checkout and build paths containing spaces, all three source providers downloaded automatically | 14/14 tests passed |
| Native Windows/MSVC Debug/Release | Not executed |
| macOS builds | Not executed |
| Hardware reception / long-run RF continuity | Not executed |

Ring microbenchmark on this host: CI8 0.946 us/block, CU8 0.891 us/block, SC16 Q11 1.741 us/block. These are one-run isolated measurements; no cross-machine performance comparison is implied.

The source providers were compiled from official hash-pinned HackRF v2026.01.3, RTL-SDR v2.0.2, bladeRF libbladeRF_v2.6.0 and no-OS commit 0bba46e6f6f75785a65d425ece37d0a04daf6157. Linux uses matching libusb 1.0.27 header/runtime; Windows retains the manifest's pinned libusb 1.0.30. Local source overrides in some validation builds avoid repeated downloads; a separate fresh-checkout/source-fetch check is documented with its own log.

Native adapter tests use deterministic implementations of vendor API calls linked to production adapters. They verify USB-buffer copy lifetime, RTL-SDR's early-cancel race, exact discrete gains, bladeRF SC16 values 1235/-1901, native timestamps, five-pair device gaps, overflow-stop and malformed transfer handling. The bladeRF overrun fixture verifies that the valid prefix has no GAP, the next interval carries the measured gap, and the event is counted once. RTL-SDR/bladeRF fixtures inject errors while a consumer waits and require an error instead of clean EOF. They are not hardware tests. Portable tests verify three native representations, 60,000 concurrent ring blocks, leased storage protection, partial occupancy, high-water/reset semantics, exact software loss counts, retained unknown device gaps, per-file metadata, byte-identical recording/replay, continuous native DC removal, ADC histogram moments and calibrated integer PSD.

The browser regression verifies one active source, removal of the obsolete independent-replay routes, file browsing including spaces, metadata import, recording/delete ownership guards, token/revision/path/symlink restrictions, transfers and persistence. Existing spectrum/waterfall frame integrity, sequence skips, slow-client/latest-frame behavior, 30 Hz polling, diagnostic suspension, independent histogram settings, RF axes and gain steps remain tested. Desktop source configuration stays compact and mobile avoids horizontal overflow.

`ngene --help` and `--version` run successfully with all three actual native libraries linked. `--list-devices` was attempted; this container cannot initialize physical libusb access (LIBUSB_ERROR_OTHER / vendor enumeration error), so no device-enumeration or RF success is claimed.

Run the optional microbenchmark with `ngene_sdr_tests --benchmark`. It measures copy + acquire + release of 16,000 native complex pairs per block, 20,000 iterations per format, without producer/consumer scheduling, USB, conditioning, FFT, disk, GUI or GNSS channels. The log is provided; its throughput must not be used as a whole-receiver real-time guarantee. FFT assembly and existing CI8 SIMD kernels are unchanged. New native CU8/SC16 diagnostic/DC arithmetic has scalar reference paths; optimization/dispatch and complete GNSS performance remain future work.

See `MULTISDR_064.md` for precise integrity semantics and the physical SDR validation procedure. Zero software overflow certifies only NGene's accepted-buffer path; it does not certify loss before callbacks, particularly on RTL-SDR/HackRF. The old recorder/index replay limitation remains documented: replay does not reconstruct original RF gaps from the CSV.
