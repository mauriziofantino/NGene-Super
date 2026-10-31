# Frontend and DSP precision options plan

Status: October 31, 2026. Candidate experiments completed; operational receiver and
UI unchanged. Evidence: [standalone DSP experiment](../../benchmarks/dsp-options/README.md).
This supplements the historical [native IQ report](NATIVE_IQ_PERFORMANCE.md).

## Findings that change the proposed architecture

Keep the 8-bit and XOR alternatives. There are three independent decisions:
storage width, carrier approximation, and correlator representation. XOR on bytes
does not imply one-bit samples. For a +/-1 replica, correcting the complement's
one-count offset makes the accumulated byte-XOR result exact. The loss of the
square-carrier candidate comes from approximating sine/cosine; one-bit amplitude
quantization is a separate loss.

The tested square carrier also responds to complex carrier harmonics -3 and +5,
at approximately 1/3 and 1/5 of its fundamental response. A simple amplitude
renormalization does not remove these images. Any use in acquisition needs an
explicitly validated ambiguity check; no existing acquisition decision or
threshold may be changed implicitly to accommodate the candidate.

For Gaussian sigma 0.14 in normalized ADC units, 1024 paired realizations gave
roughly 0.003–0.006 dB loss for Q15/8-bit amplitude, 0.97–1.18 dB for the corrected
byte-XOR square-carrier candidate, and 2.64–3.08 dB for packed one-bit amplitude
plus square carrier. These are prompt-deflection results at two digital IFs,
not universal quantization losses, C/N0 biases or PVT accuracy measurements.

In the two original x64 runs, hot-kernel medians for 16000 complex pairs were
approximately 4.5–5.0 us for IQ16/Q15, 5.0–5.8 us for IQ8/Q15, 1.7–1.9 us for
byte-XOR/square carrier, and 3.1–3.6 us for packed signs including packing.
Use the saved CSV and checker for the final measured ranges. Eight-bit storage
reduces memory traffic, but did not make the full-mixer kernel faster on this
host. Packing overhead also prevented the one-bit candidate from beating byte XOR.
These numbers exclude replicas, transport, queues and materialized mixer output.

## Proposed operator choices

Start with profiles, with advanced controls exposing only implemented combinations:

| Proposed profile | Shared stream | Carrier and correlation | Intended qualification |
|---|---|---|---|
| Native precision / Automatic | Common CI8 for 8-bit sources, common CI16 for wider sources | Complete carrier, exact SIMD correlation | Proposed default; preserve source precision |
| Compact 8-bit | Common signed CI8 from every adapter | Complete carrier, exact SIMD correlation; intermediates widened | Native 8-bit sources; explicit lossy conversion approval for wider ADCs |
| Fast XOR 8-bit | Common signed CI8 | Square carrier and corrected byte-XOR correlation | Explicit experimental selection; harmonic ambiguity and loop tests required |
| Packed one-bit / Experimental | Preserve selected common CI8/CI16 stream; derive private sign buffers | Sign packing, square carrier and XOR/popcount | Research profile; known sensitivity/image limitations |
| Advanced | One of the supported stream formats | Independently choose supported carrier/correlator implementations | Prevent unsupported combinations rather than expose a Cartesian product |

"Common" means every adapter emits the selected representation and agreed scale
for that run, not a different signedness/layout for every device. Choose the kernel
once at configuration time; no per-sample hardware or format dispatch. Widening
8-bit storage is exact and does not create ADC information. Narrowing bladeRF/USRP
must have explicit scale, rounding, clipping counters and a visible precision-loss
indication. Do not describe a higher ADC resolution as 8-bit merely because storage
was narrowed. Do not change hardware gains automatically to hide digital clipping.

The byte-XOR correlator can be selected independently wherever a validated
8-bit +/-1-replica boundary exists. Do not narrow a precise mixer's int16 output
merely to enable that correlator. A wider exact sign-change correlator is a
separate candidate to implement and benchmark.

UI should show source ADC bits, common storage width, arithmetic profile and backend.
SSE2/NEON/scalar are implementation selections, preferably automatic, not operator
"accuracy" settings. Automatic precision preserves native bits; it must not select
one-bit or square-carrier arithmetic silently because CPU load rises. Stream/profile
changes occur while stopped initially. Future changes within a run require coherent
state reset and continuity semantics; never change scale within an integration.

## First implementation stages

1. Define CI8/CI16 stream contracts and adapter conversions, preserving raw recording
   as a separate explicit choice. Add signedness, scaling, all rails, partial-transfer
   and ring-wrap tests. Assess buffer memory and copy count at maximum configured Fs.
2. Implement production block kernels with preallocated output, exact scalar oracle,
   SSE2 and NEON. Include conversion, NCO/code generation, output materialization,
   Early/Prompt/Late taps and real window boundaries in the next measurements.
3. Keep IIR DC removal out of the new design. Plan Off/Manual/Automatic DC policy,
   with RF/IF/Fs-aware decisions and Classic window-mean semantics as reference.
   Measure rounding, widened subtraction, transient behavior and zero/near-zero IF.
   No streaming DC removal is implemented by this study.
4. Integrate exact 8/16-bit paths first. Benchmark dedicated adapter/conditioning
   workers and the actual dispatcher with independent channel replicas, bounded
   queues, recording and diagnostics. More threads are not assumed to scale linearly.
5. Qualify XOR profiles using synthetic and recorded signals: Doppler/code sweeps,
   harmonic images, strong interferers, ADC under-use/clipping, DLL/PLL errors and
   reacquisition. Consider precise confirmation after approximate searches only as
   an explicit future acquisition design decision, preserving Classic's required
   coarse/refinement/M-of-N structure and existing decisions until authorized.
6. Add profile UI only after supported combinations and transitions pass validation.
   Expose measured tradeoffs and real counters; do not promise real time from kernel
   throughput or display smoothness.

## Required acceptance beyond this experiment

Test each device/format at its maximum requested sustained rate, not one universal
Fs. Preserve bladeRF native 12-bit precision by default and define future USRP
scaling from its actual host format. Run Windows/Linux/macOS and x86/ARM builds
with target-specific kernel checks; scalar parity on x64 does not establish NEON
performance or RF support.

Require stable queue occupancy under steady load, zero unexplained sample loss,
deadline distributions and explicit headroom under recording/diagnostic/channel
load. Report producer/consumer throughput separately from RF/USB/device loss.
Separate arithmetic error, detection probability, tracking stability and position
accuracy; a larger fix count or a faster microbenchmark proves none of the others.
