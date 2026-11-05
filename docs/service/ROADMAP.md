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
- Channel state contract and validation; optional dispatcher with a fixed pool and dynamic jobs per window. GNSS algorithms still need integration.
- Recorder, segments, metadata, and overflow/EOF/stop handling.
- Controller, dark GUI with separate configuration/observation, common-stream recording with ADC/transport provenance, and file transfers.
- Separate DSP library for FFT, statistics, and power; common APIs with SSE2/NEON backends selected for the target. Spectrum/waterfall, histograms, and runtime configuration.
- Diagnostics with end-to-end PSD integrity, synchronized 2D and rectangular relief waterfalls, independent histograms, and I/Q statistics.
- Separate hardware/signal tuning and NCO/digital IF contract. Streaming DC removal is retired; historical conditioning keys are ignored on import and omitted on save.
- SSE2/NEON intrinsics primitives and C reference; native Windows MSVC x64/ARM64 presets.
- Core portability and tests; documentation of checks actually performed.

## Proposed increments

The [frontend stream decision record](FRONTEND_STREAM_DECISIONS.md) records completed
Windows SiGe USB checks and current stream/display contracts. The [DSP options study](DSP_OPTIONS_PLAN.md)
defines the next kernel work; its approximate arithmetic candidates remain experiments.
Acquisition, tracking and PVT are not integrated. The increments below retain their
platform and RF validation requirements even where adapter or replay support exists.

1. **Real HackRF test:** verify selection, gains, stream stability, recordings, and timing on Windows/Linux/macOS with the user's hardware. Select replay datasets.
2. **File input and inspection tools:** read captures with metadata/gaps; make RF tests repeatable. No GNSS algorithm before obtaining a verifiable sample reference.
3. **DSP contracts and kernels:** port useful NCO, replicas, and correlations from the archive; verify insertion of the already-adapted integer FFT into GNSS chains. Define scaling/ranges, then C reference and x86/ARM SIMD. Compare safe historical XOR, candidate SAD, possible improvements, and products; no presumed winner.
4. **Acquisition and tracking:** adapt NGene algorithms behind the APIs, with private scratch and explicit integrations. Implement HOLDOVER and fast reacquisition. Revisit the channel contract with the first real algorithms.
5. **Navigation and assistance:** demodulation during tracking, message cache, signal/network updates, almanac, previous position, and visibility-guided search.
6. **Observables and PVT/PNT:** common epochs, pseudoranges, satellite positions, and integration of the recoverable solver.
7. **Additional frontends and conditioning:** new I/Q formats, filters/resampling with delay and time-grid contracts; explicitly selected recording before/after conditioning.

The interfaces allow compile-time replacements without changing coordination. A stable dynamic plugin ABI is not promised, nor is compatibility of future GNSS algorithms with arbitrary rates without validation.
