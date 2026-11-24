# DSP arithmetic options experiment

Development-only candidate kernels for the October 31, 2026 frontend design study.
Nothing in this directory is linked into the operational receiver. This is not a
benchmark of a complete acquisition/tracking implementation or of the Classic executable.
See [the options plan](../../docs/service/DSP_OPTIONS_PLAN.md).
The F5 extension and output checks are described in the
[F5 report](../../development/profiling/F5/README.md).

## Reproduce

```powershell
cmake -S benchmarks/dsp-options -B build/dsp-options -G "Visual Studio 18 2026" -A x64
cmake --build build/dsp-options --config Release
./build/dsp-options/Release/dsp_options.exe

cmake -S benchmarks/dsp-options -B build/dsp-options-reference -G "Visual Studio 18 2026" -A x64 -DREFERENCE_ONLY=ON
cmake --build build/dsp-options-reference --config Release
./build/dsp-options-reference/Release/dsp_options.exe --verify
./build/dsp-options/Release/dsp_options.exe --verify
```

On a POSIX host, omit the Visual Studio generator and architecture and specify
`-DCMAKE_BUILD_TYPE=Release`. POSIX runtime has not been validated in this study.
`REFERENCE_ONLY` disables explicit intrinsics; compiler auto-vectorization remains enabled.
`--verify` runs contracts, quality and harmonic checks without timing or Monte Carlo.
Run benchmarks sequentially, without other benchmark processes. Windows measurements
use the OS scheduler without CPU affinity. Hybrid-core placement and host load affect timing.

## What is tested

- Exact XOR identity for every signed byte and both replica polarities: complement
  gives `-x-1`, so adding the number of negative replica elements recovers the exact sum.
  Unlike negation in int8 storage, the widened result correctly represents `-(-128)`.
- SIMD vs scalar dot products, signed rails, every tail length from 1 through 521,
  and bit-packed sign products with partial final words.
- Native CI8 amplitudes represented in int8 and int16 produce identical Q15 dot products.
- GPS L1 C/A PRN 7 replicas generated with Evo's production code NCO. Carrier is
  1375.25 Hz plus digital IF 0 or 4 MHz, Fs 16 MHz. Windows: 16000, 64000 and 16003 pairs.
- Double oracle, Q15 carrier/16-bit amplitude (12-bit ADC values), Q15 carrier/8-bit
  amplitude, corrected byte XOR with square carrier, and packed one-bit amplitude
  with square carrier. The packed path includes packing every input window.
- Clean and weak spread signal, DC, CW interference, clipped ADC input and half-chip
  replica displacement. Quality rows report relative complex prompt error and phase
  error against the double oracle, not position error or C/N0 accuracy.
- Complex harmonic replicas at carrier multiples -3 and +5, with integer-period
  separation over the window. This exposes square-carrier image responses.
- Sensitivity: 1024 independent paired noise/signal realizations, amplitude 0.003,
  Gaussian component sigma 0.14, 16000-pair integration. Deflection is squared mean
  prompt response divided by noise-only prompt variance. `loss_db` compares that
  statistic with the double oracle; negative sub-millidecibel results are finite-test
  effects, not demonstrated sensitivity gains. No detection threshold is calibrated.
- CI8 copy, CU8 centering, lossless widening and lossy Q11-to-CI8 conversion with
  adapter checks across all Q11 levels. Separate 32 MiB CI8/64 MiB IQ16 transfers
  expose memory traffic beyond the small hot kernel buffers.
- Concurrent 1/2/4 workers using Evo's production thread abstraction; every worker
  processes the same immutable input and prepared replicas. Outputs must agree.
  This checks arithmetic concurrency and shared-cache scaling, not an independent
  satellite workload or the production channel dispatcher and its queue/barrier costs.

## Timing boundaries and numerical conventions

The arithmetic kernels fuse rotation and one complex prompt correlation. No rotated
stream is materialized. Carrier/code arrays and masks are prepared outside timing;
there is no FFT search, replica generation, DC estimation, USB, recording or UI cost
inside the kernel numbers. Adapter and large-transfer timings are separate, not an
end-to-end sum or capacity guarantee. The 15 timing rounds rotate arithmetic mode order;
reported p95 uses nearest rank and refers to batch averages, not individual deadlines.
Parallel figures use seven batches; startup/join and dispatcher costs are excluded.

The 16-bit experiment stores Q11 ADC counts, while the 8-bit experiment stores Q7
counts from the same normalized waveform. Known fixed scale factors make outputs
comparable. The sine/cosine coefficients are Q15 with magnitude at most 32767.
Products widen before arithmetic and sums use int64 lanes; no int8 mixer output clips
the complex rotation. These prototypes therefore test 8-bit **storage**, not an
8-bit accumulator or an 8-bit precision limit at every intermediate step.

Square-carrier amplitudes are normalized by pi/4. Packed signs additionally use
the known synthetic Gaussian sigma and sqrt(pi/2) gain. Those factors are not a
calibrated gain estimator under CW/DC/clipping; those quality rows intentionally
expose departures from that Gaussian assumption. XOR/packed prototypes are new
candidate implementations inspired by the representation, not unchanged Classic kernels.

## Evidence and limits

`results/windows-x64-run1.csv` and `run2.csv` are repeated native MSVC x64 SSE2
measurements. `windows-x64-reference.csv` and `windows-x64-verify.csv` contain
matching numerical checks without/with explicit SIMD. `manifest.json` records
source/binary hashes and host identifiers. `check-results.cjs` checks the saved
evidence and reports its ranges; it does not replace running the executable.

No NEON implementation, ARM timing, Linux/macOS execution, live SDR reception,
full acquisition probability, DLL/PLL stability, navigation or PVT acceptance is
established here. Input packing reuse, hardware POPCNT, additional correlator taps,
low ADC occupancy, additional PRNs/modulations, sample rates, oscillator offsets,
independent channel replicas and sustained mixed workloads remain follow-up tests.
