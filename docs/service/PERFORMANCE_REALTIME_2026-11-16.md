# Real-time CPU budget, tracking kernels and search FFT

Validated on macOS 15 (Darwin 24.6), Apple M1 Pro, Apple Clang Release, 16 November
2026, branch `claude/realtime-budget`. x86_64 SSE2 paths were run under Rosetta 2
(unit/contract tests only, no timing). All evidence below is **replay or synthetic**.
No physical RF hardware was used, so no live-hardware real-time claim follows from it.

## Problem

Classic sustains HackRF at 16 Msps with one worker; EVO saturated every core. Profiling
found three causes:

1. Acquisition used every free core: the pool held `CPU - 1` threads, and admission
   was throttled only by the wall-clock tracking load. More tracking workers left
   more room for searches.
2. The tracking front end cost about 93 µs per GPS channel per millisecond at 16 Msps.
   It made seven or more passes per block: double carrier, CI8 widening, mixing,
   double code phases, then a sampled replica and a correlation per tap.
3. One barrier per millisecond of signal, and a second, empty wake-up wave for
   inactive slots.

## Changes

| # | Change | Arithmetic / decisions |
| --- | --- | --- |
| A | Signal-time CPU budget for the shared search pool: `acquisition.cpu_budget` (cores per signal second, default 1, 0 = previous adaptive behavior), charged with the thread CPU clock at every stage boundary, capped at 250 ms of signal's credit. With a budget, stage concurrency is `ceil(budget)` and the 0.55/0.75/0.85/0.95 delay steps are not used. Charging elapsed time instead was tried and rejected: it also charges strategies that wait, and it starved the other constellation in the slot contract test. | Unchanged. Searches are only delayed. A test checks the stalled and resumed results against the unbudgeted run. |
| B | `NgThreadSchedulingPriority` on POSIX: macOS QoS classes, Linux per-thread nice 10 for background work. Search lanes fixed at 4. | Unchanged. |
| C | Tracking bank batches 4 ms per dispatch, flushes on discontinuities, claims Galileo slots first. The dispatcher skips registrations that declare no ACQUISITION-state work (`idleInAcquisition`). | Unchanged. Tracking already accepts arbitrary window splits. |
| D | Fused front end (`NgSearchMixPrefixCi8`): carrier, CI8, mixing and running sums in one pass, with the same mixed samples as the separate kernels (bit-exact test). Each tap and Galileo probe sums whole chip runs of the running sums (`NgTrackingCorrelateRuns`). Chip boundaries are predicted and, within 1e-6 samples, settled with the exact per-sample replica arithmetic. | Same sample-to-chip assignment. Sums differ only by double rounding: test bound 1e-12 of the summed magnitudes. |
| E | Search FFT: radix-2^2 with contiguous per-pair twiddles. | Float rounding differs. Accuracy against a double DFT is unchanged (about 1.4e-7 relative RMS at every size 8..16384). |
| F1 | Opt-in `galileo_acquisition.fft_size` (0 automatic, 4096, 8192, 16384). 4096 is Classic's sizing: fixed-phase subcarrier wipe-off before compression and a primary-code grid replica. | **Changes the algorithm. Off by default. Not recommended (see below).** |
| F2 | Opt-in `tracking.fast_carrier`: four independent double carrier lanes and four-sample running sums (CI8 only). No tables or phase quantization, so this replaces the Classic int16/Q6 mixer idea. | Double-rounding differences. Off by default. |

## Evidence

**Tracking equivalence (D).** A golden harness ran 10 configurations × 10 channels over
1.5 s of a 16 Msps synthetic scene (8 GPS signals). Configurations covered XOR and
product correlation, square carrier, 5 taps, spectrum inversion, a seed offset, all-tap
observation, Galileo slots and window splits of 777–64000 samples. The output was
compared with the pre-change code: 126,805 completed integrations, 0 mismatches in
integration count, PLL, activity or Galileo peak verification. Doppler, code rate,
discriminators, C/N0 and code delay were bit-identical. E/P/L differed by at most
2.6e-16 relative. With `fast_carrier` the decisions were also identical, but loop
trajectories on noise-driven channels diverged by up to about 0.1 Hz of Doppler, as
expected from feedback loops with different rounding.

**Isolated throughput** (`ngene_tracking_throughput`, 12 GPS channels, 3 taps,
CPU seconds per signal second):

| Rate | 1 worker before | 1 worker after |
| ---: | ---: | ---: |
| 2.048 Msps | 0.183 | 0.182 |
| 16.368 Msps | 1.379 | 0.446 |
| 20 Msps | 1.646 | about 0.73 (before the run-loop optimization; not re-measured) |

At 16.368 Msps, 4 workers went from 0.445 to 0.235 earlier in the work. That run
predates the last run-loop optimization.

Per 16,368 samples: front end 24.8 µs (carrier recurrence alone 20.6 µs; 19.0 µs
with `fast_carrier`); one GPS tap 2.1 µs; one Galileo BOC tap 3.8 µs. Before the
change, one tap cost a sampled replica plus a correlation: about 28 µs.

**Search FFT** (forward + inverse pair): 4096 points 49.1 → 25.5 µs; 16384 points
242.7 → 116.2 µs.

**Receiver replay**: 16 Msps synthetic CI8 file, real-time pacing, 12 slots (8 GPS / 4
Galileo), GPS and Galileo PRN 1–32. CPU was averaged over 15 s from t = 3 s:

| Configuration | Process CPU (cores) | Receiver thread busy | Ingress high water | Dropped |
| --- | ---: | ---: | ---: | ---: |
| before, 1 worker | 1.18 | 81 % | 128 / 128 | 0 |
| after, 1 worker | 1.22–1.24 | 22 % | 80, then 5 on repeat | 0 |
| before, 4 workers | 3.76 | — | 128 / 128 | 0 |
| after, 4 workers | 1.25 | — | 128 once, then 4 and 4 on repeats | 0 |
| after, 1 worker, `cpu_budget=0.5` | 0.76 | — | 12 | 0 |

All runs tracked the 8 simulated satellites with PLL lock. After the change, process
CPU is the tracking load plus the configured search budget, independent of the core
count. The single 128-block peak was not reproduced in two repeats. Its cause, for
example a cold file cache at start, was not isolated.

**F1, Galileo sizing.** Synthetic E1-B, PRN 7, 16 Msps, 8 code phases and Dopplers per
case, data bits constant:

| C/N0 | automatic 16384: detected / mean peak ratio | 4096 Classic sizing |
| ---: | --- | --- |
| 42 dB-Hz | 8/8, 6.81 | 5/8, 2.45 |
| 46 dB-Hz | 8/8, 16.58 | 6/8, 5.52 |

The 4096 sizing halves the search time per PRN. It misses satellites whose subcarrier
phase is near a quarter chip from the capture origin, which is inherent to a fixed-phase
wipe-off. Keep the default unless recordings show otherwise.

## Limits and follow-up

- Physical HackRF real-time behavior is unverified, as is Windows/Linux timing. The
  Linux nice and macOS QoS calls are compiled but their scheduling effect is unmeasured.
- Under Rosetta 2 the thread CPU clock reports about 1/40 of busy time, so the budget
  is effectively not enforced there. The pool test detects this and skips only its
  stall check. Native macOS/Linux report correctly; Windows `GetThreadTimes` has
  scheduler-tick granularity, which averages out over stages.
- `test_search_arm_emulated.py` was updated for the new FFT plan layout but not run:
  Unicorn is not installed here. NEON was exercised natively on the M1 Pro.
- With the default budget, the first full Galileo pass at 16384 points needs several
  seconds of signal. Raising `acquisition.cpu_budget` trades CPU for time to first fix.
- The native-rate Galileo refinement (`GalileoCode` with per-sample `fmod`) dominates
  the cost of a present Galileo PRN and was not changed.
