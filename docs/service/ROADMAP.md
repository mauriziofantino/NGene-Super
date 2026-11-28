# Decisions and increments

NGene is the basis of the project. Every future GNSS module will begin with analysis of the corresponding original code, documenting reuse, adaptation, and replacement in the [migration map](LEGACY_MIGRATION.md).

## Accepted requirements

C11 for NGene; Windows/Linux/macOS; Intel/AMD and ARM targets; frontends through native libraries. HackRF first, then bladeRF, Pluto, and RTL-SDR. No SoapySDR and no C++ engine.

The engine is the sole logical consumer of the GNSS ring: all channels receive the same stream portion. Conditioning is separate and can be disabled. FFTs, correlations, and other DSP building blocks are internal libraries, with appropriate SIMD/NEON backends and no FFTW. FAST XOR may retain controlled approximations; PRECISE will use SIMD products. Historical CPU utilization figures are not mandatory thresholds.

## Implemented now

- C API and strict, validated INI configuration.
- Receiver lifecycle and native HackRF, RTL-SDR, bladeRF and fixed-profile SiGe I/Q frontends.
- CI8, CU8, SC16 Q11 and CI16 Q15 file replay; one active source per service. Parallel live reception
  and replay use separate processes. Deterministic input generation is confined to tests
  and optional documentation capture tooling.
- Dedicated common-source worker, exact SIMD conversion to signed CI8/CI16, aligned SPSC rings, window assembly and common dispatch.
- Channel state contract and validation; optional dispatcher with a fixed pool and dynamic jobs per window. GPS/Galileo tracking, navigation and a first broadcast PVT are integrated with the current status limits.
- Optional GPS L1 C/A acquisition for PRNs 1..32, retaining coarse search, Doppler/code refinement, and independent 3-of-4 confirmation; numerical adaptations and validation limits are recorded in [GPS acquisition](GPS_ACQUISITION.md). This is not tracking or a navigation fix.
- Galileo E1-B acquisition for PRNs 1..32 with 4-ms coherent periods, eight sums and 3-of-4 confirmation; common wrong-Doppler noise policy with GPS. See [scope/limits](GALILEO_ACQUISITION.md). Independent controller/CLI/INI configuration and selected-channel maps are integrated; RTL recording/recovery evidence is documented in the current performance report.
- Recorder, segments, metadata, and overflow/EOF/stop handling.
- Controller, dark GUI with separate configuration/observation, common-stream recording with ADC/transport provenance, and file transfers.
- Separate DSP library for FFT, statistics, and power; common APIs with SSE2/NEON backends selected for the target. Spectrum/waterfall, histograms, and runtime configuration.
- Diagnostics with end-to-end PSD integrity, synchronized 2D and rectangular relief waterfalls, independent histograms, and I/Q statistics.
- Separate hardware/signal tuning and NCO/digital IF contract. Streaming DC removal is retired; historical conditioning keys are ignored on import and omitted on save.
- SSE2/NEON intrinsics primitives and C reference; native Windows MSVC x64/ARM64 presets.
- Core portability and tests; documentation of checks actually performed.

## Proposed increments

The [frontend stream decision record](FRONTEND_STREAM_DECISIONS.md) records completed
Windows SiGe USB checks and current stream/display contracts. The [arithmetic review plan](PIANO_ARITMETICHE.md)
defines the next kernel work for the receiver and RF monitor. The [DSP options study](DSP_OPTIONS_PLAN.md)
records earlier evidence for the comparison.
GPS/Galileo DLL/FLL/PLL, multi-correlator tracking, bounded slot allocation and a shared tracking pool are integrated with the limits in [TRACKING.md](TRACKING.md). GPS LNAV, Galileo I/NAV, consistent ephemeris caches, common-epoch code/Doppler and unweighted broadcast PVT are integrated. Full Classic supervision, weighted/adaptive PNT, ionosphere/smoothing and integrity qualification remain pending. The increments
below retain their platform and RF validation requirements even where adapter or replay
support exists.

For BeiDou B1C, acquisition, tracking, B-CNAV1 and the mixed PVT are connected
on replay. B8 accuracy qualification starts with per-satellite code timing
and pseudorange on the common sample, using the existing least-squares PVT
as the benchmark. [BeiDou analysis and test plan](PIANO_INCREMENTALE_EVO.md#piano-operativo-di-verifica-beidou-b1c) ·
[B8–B10 evidence](../../development/profiling/B8/README.md)

1. **GNSS DSP contracts and kernels:** expand Galileo PRN coverage and validate recorded/live Galileo separately; port useful NCO, replicas, and correlations from the archive; define scaling/ranges and a C reference before x86/ARM SIMD. Compare safe historical XOR, candidate SAD, and products against the acquisition/channel requirements; do not presume a winner. Approximate arithmetic remains experimental.
2. **Tracking:** adapt NGene algorithms behind the APIs, with private scratch and explicit integration. Implement tracking, HOLDOVER and fast reacquisition; revisit the channel contract as real algorithms are connected.
3. **Navigation and assistance:** demodulation during tracking, message cache, signal/network updates, almanac, previous position, and visibility-guided search.
4. **Observables and PVT/PNT:** common epochs, pseudoranges, satellite positions, and integration of the recoverable solver.
5. **Additional frontends and conditioning:** new I/Q formats, filters/resampling with delay and time-grid contracts; explicitly selected recording before/after conditioning.
6. **Physical-device validation:** verify frontend selection, gains, stream stability, recordings, and timing on Windows/Linux/macOS with the user's hardware. Keep platform-specific evidence separate; select replay datasets for repeatable comparisons.

The interfaces allow compile-time replacements without changing coordination. A stable dynamic plugin ABI is not promised, nor is compatibility of future GNSS algorithms with arbitrary rates without validation.
