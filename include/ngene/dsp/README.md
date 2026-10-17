# NGene DSP

The `NGene::Dsp` library exposes APIs independent of the CPU backend:

- `fft.h`: complex Q14 FFT, immutable plan, per-call workspace, and scalar oracle.
- `statistics.h`: exact CI8 I/Q sums and sums of squares.
- `power.h`: complex CI16 power with exact uint64 accumulation.
- `sample_ops.h`: compatible umbrella for statistics, power, and backend name.

Application calls do not select SSE2/NEON: the compiler and target configuration select
the private implementation. Contracts declare ranges, overflow, alignment, and concurrency.
Correlators, NCOs, and replicas will be migrated incrementally; see the
[architecture](../../../docs/service/ARCHITECTURE.md) and [FFT](../../../docs/service/FFT_REVIEW.md).
