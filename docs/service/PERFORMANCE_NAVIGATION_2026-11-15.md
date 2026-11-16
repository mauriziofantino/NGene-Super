# Native-rate tracking, navigation and display work

Validated on Windows x64, Clang Release, 15 November 2026. Classic remains the
algorithmic reference. Acquisition stages, thresholds, Doppler grids, code
refinement and 3-of-4 confirmation were retained. Scheduling and optional display
work are separate from those decisions.

## Findings and measurements

The isolated production tracking probe reproduced the native-rate load problem.
At 16,367,600 pairs/s, twelve mixed GPS/Galileo channels and four workers, nine
correlator taps on every channel took 1.220 seconds per second of signal in the
final baseline measurement. Earlier baseline runs ranged around 1.26 seconds.
Three taps took approximately 0.64–0.70 seconds. Extra observational taps therefore
consumed the realtime headroom before acquisition, USB or GUI work was included.

The isolated kernel profile reports approximately these costs for 16,368 samples:

| Production operation | Microseconds per call |
| --- | ---: |
| Precise carrier generation | 18.942 |
| Square carrier generation | 18.866 |
| Complex rotation | 4.969 |
| Double code phases | 8.683 |
| One sampled replica | 21.712 |
| One product correlator | 7.496 |
| One XOR correlator | 6.913 |
| 4096-point search FFT forward + inverse | 46.110 |

These are separately timed production kernels, not a sampled CPU profile or a
decomposition of receiver wall time. The profile lives outside the receiver in
`benchmarks/tracking/profile.c`. Its input preparation is outside the timed kernels.
An initial fixture repeatedly mixed a square carrier and overflowed; it was corrected
to a unit-magnitude carrier before the final measurements above. The final checksum
is finite. No experimental instrumentation was added to the operational main.

## Changes

- All channels retain mandatory E/P/L correlation and Galileo half-chip probes.
  Extra taps, which never feed the DLL or acquisition decisions, run only for the
  selected channel. The request is latched at the start of a coherent integration.
  Unobserved telemetry contains the actual E/P/L values, rather than fabricated
  zero-valued extra taps. Double NCOs and double accumulators remain intact.
- The x64 SSE2 code upsampler now samples four signs per loop, retaining the same
  double wrapping and code-boundary rounding. Native x64 carrier generation,
  complex rotation, product/XOR correlators and floating search FFT already use
  SSE2. The separate Q14 FFT retains its existing selected SIMD backend. No ARM
  performance claim follows from these x64 measurements.
- A fused replica/correlator experiment produced identical checksums but worse
  throughput (about 0.84 versus 0.64 seconds for the three-tap probe). It was removed.
- Unobserved, unconfirmed searches do not reduce or copy display grids. A selected
  acquisition may produce its grid. On confirmation, the seed is published first;
  the acquisition worker can then build its archived coarse map from the same
  capture and original coarse threshold. This background calculation never feeds
  a search decision or delays seed admission behind map publication.
- Controller map copies are restricted to confirmed results and the selected
  acquisition. The browser publishes a two-second diagnostic lease for one channel;
  closing or leaving its page withdraws interest, and a lost browser expires naturally.
  HTTP writes only this interest value. Receiver-owned progress callbacks apply it;
  HTTP never accesses live tracking workspaces.
- Tracking and PVT complete before new capture work. A 100-ms moving load estimate
  samples one dispatch every 10 ms of signal. Below load 0.55 no cooperative wait is
  imposed. Above 0.55 the scheduler serializes new constellation admission and
  yields 1 ms at existing search stage boundaries; above 0.75 it yields 4 ms; above
  0.85 it pauses new admission; above 0.95 in-flight background work yields 20 ms.
  These are workload controls, not GNSS thresholds. Bounds and cancellation remain
  independent of the single-satellite algorithm.

The measured clock pair and EMA update cost 0.141 microseconds per sample of the
budget: approximately 14.1 microseconds per second at 100 updates/s, excluding
ordinary scheduler bookkeeping. This does not time acquisition inside the budget
estimator. With nine taps configured and one channel observed, the isolated probe
took 0.691 seconds per signal second (precise/product), or 0.676 seconds
(square/XOR). Those modes differ numerically by the explicitly selected carrier;
they are not a sensitivity comparison.

## Physical HackRF result

Configuration: 16,367,600 pairs/s, 16,368-pair windows, twelve slots (8 GPS reserved,
4 Galileo reserved), four workers, nine configured taps, precise carrier/product,
LNA/VGA 40/42 dB, RF amplifier and antenna supply off. The 120-second run processed
1,963,999,216 pairs with zero gap events, zero known sample drops and zero transport
errors. Twelve slots reached tracking. At the final solution, ten satellites were
eligible for PVT: seven GPS and three Galileo. Residual RMS was approximately
2.79 m. This is a fit residual, not an absolute-position error or integrity bound.

The selected-channel check changed only channel 9 from three to nine taps; all
other channels stayed at three. After the diagnostic lease expired it returned
to three. The archived Galileo PRN4 map was available and referenced the seed's
original epoch/sample. The later zero-wait policy below load 0.55 and warning-cleanup
casts were not a separate repetition of that full RF run.

Artifacts are local under `build/performance-20261115/`: baseline executable,
kernel CSV, throughput outputs, live status snapshots, selection/expiration and
final receiver/PVT counters. Hardware evidence is separate from synthetic fixtures.

## Navigation and PVT scope

Classic GPS subframes 1–3 and Galileo I/NAV words 0–5 supply per-satellite orbit,
clock, health and time. Field maps/scales and the terminated Galileo Viterbi decoder
were ported from Classic with channel-private state and no disk/global receiver
side effects. I/NAV uses Classic deinterleaving and CRC24Q; alert pages are excluded.
Only consecutive even/odd halves are paired. Consistent issue-of-data parts commit
the operational orbit/clock cache. Full OSNMA, Reed-Solomon recovery, almanac, UTC
conversion and I/NAV word 6+ extensions remain separate work.

Transmit times count integer milliseconds from a decoded boundary, adding the
fractional code phase and propagating only the remaining fraction of a common
receiver window. Repeated additions to a large floating TOW are avoided. The
solver aligns observations to that common sample; rejects mismatched epochs,
incomplete/unhealthy or stale broadcast data; applies Classic Kepler/harmonic,
relativistic clock and Earth rotation equations; and solves separate GPS/Galileo
clock biases. Doppler velocity uses the rotated satellite frame as in Classic.

The initial C11 solver uses column-pivoted Householder QR and double arithmetic.
It implements unweighted broadcast least squares and Classic's default Hopfield
troposphere model. It does **not** yet reproduce Classic's full WLS/adaptive Kalman,
ionospheric corrections, carrier smoothing, outlier/geometry gates or integrity
qualification. The GUI and API label this model explicitly. A converged broadcast
solution is not a claim of full Classic accuracy or an integrity-qualified fix.

## Map and validation

PVT has both numerical values and an interactive Web Mercator map with current/last
position, bounded solution trace, zoom, pan and follow controls. The map requests
only visible viewport tiles when its page is visible and a solution exists, uses
browser caching, and displays OpenStreetMap attribution. Tiles need internet.
See the [OSM tile policy](https://operations.osmfoundation.org/policies/tiles/).
Automated map tests mock tiles and make no requests to the public tile service.

Deterministic cases cover GPS bit polarity/parity, Galileo convolutional encoding
through deinterleaving/FEC/CRC/time anchoring, analytic orbit derivatives, mixed
clock solves, Sunday rollover, gaps, and exact equality of mandatory tracking
values with observational taps enabled/disabled. Native replicas are checked at
code boundaries and tails against the retained double-fmod reference. All four
carrier/product mode combinations remain independently configurable.

The full local CTest invocation passed 21/24 cases. Three environment checks failed:
the MSBuild temporary-directory fixture was denied access, and two PowerShell
helper checks were blocked by host execution policy. They are not reported as
passed. Browser workspace/map tests and the mixed satellite-page replay have their
individual results in the work log. Final normal configuration has
`NGENE_TRACKING_BENCHMARK=OFF`.

## Current pool, UI and recovery update � 16 November 2026

The earlier HackRF RF result above predates the shared adaptive acquisition pool.
It is historical evidence, not acceptance of the current scheduler. With that
pool an MSVC live run reproduced ingress overflow at10.23s despite seven tracking
channels. Native isolated twelve-channel/three-tap throughput was0.736s per signal
second at16.3676M; admission control alone could leave already borrowed acquisition
jobs contending with tracking. MSVC kernel timings were higher than Clang timings
(e.g. precise carrier27.7us, one product13.1us, FFT pair54.1us).

The current scheduler separately bounds admitted jobs and executing stages.
Every existing cancellation boundary releases its permit and waits for the current
execution allowance; reducing the budget therefore also reaches in-flight jobs.
An independent cap allows deferred maps to finish even when no receiver slots are
free. Generation/stop breaks all waits. A paused-budget test freezes already
borrowed native jobs, resumes at one execution lane, and compares every search
result exactly with serial execution. Acquisition decisions/thresholds are unchanged.

A wall-progress-debt experiment was rejected by slow-source tests: waiting for
input is not CPU overload. Final backpressure uses the actual ingress occupancy
with75/50% hysteresis, sampled at most200Hz using the engine clock already read
for lifecycle checks. Frontend/ring counter snapshots take existing short locks;
no additional clock reads, DSP, GUI work or allocations occur in this callback.
File/unavailable queues never throttle. Tracking wall-load sampling remains100Hz.
Optional diagnostics pause under the same portable headroom; OS priorities are
advisory. The14.1us/sec clock/EMA measurement does not include the final queue
snapshots or all scheduler bookkeeping.
Budget pauses preserve the PRN cursor instead of restarting at low PRNs, fixing
Galileo starvation. Small transport windows are accumulated into a bounded native
millisecond before tracking dispatch; no resampling, quantization or precision
change occurs. A37-pair versus2048-pair test compares final tracking values exactly.

A physical RTL E4000 calibration tested2268states: tuner29dB/IF41dB, RMS9.31%,
no rail clipping. These gains were used only in temporary test profiles. A65.403s
recording saved155978080CI8pairs with zero reported software gaps/drops. GPS16/18
lose correlation together near19.1s even in the isolated replay tool; fresh
acquisition later decodes GPS and Galileo again. This excludes the multi-worker
scheduler as a necessary cause of that recorded loss, but does not prove a hardware
loss: RTL hardware continuity is unmeasured. Full Classic comparison remains open.

PLL power qualification now releases lost assignments after the bounded Classic
relock deadline and restores per-satellite broadcast caches on reacquisition.
FLL/timing reset preserves orbit/clock parts while discarding framing and TOW.
On the recovery replay, PVT is available around51�54s with six GPS and two Galileo:
GDOP3.801, HDOP1.275, VDOP2.303, residual RMS0.94m at the saved sample129602400.
These are geometry/fit values, not absolute accuracy. The solution is interrupted
again; continuous PVT and full Classic false-lock/holdover are not established.

The UI removes static menu dots, fixes channel-column widths, adds numeric+bar
C/N0, and displays real solved satellite azimuth/elevation plus DOP and the map.
Browser fixtures verify sky orientation, DOP, no fabricated geometry, stable
columns, meter values and map behavior with mocked tiles. Mixed GPS/Galileo channel
pages and the workspace suite pass. Search kernels additionally have AArch64 NEON
carrier/rotation, code, product/XOR and floating FFT paths; production execution
under Unicorn passes independent arithmetic/DFT checks. This is not ARM performance.

Optional traces live only in benchmarks/tracking/profile.c and replay.c and the
benchmark configuration is restored OFF in both build directories. The current
normal executable is the MSVC multi-SDR Release build. See the work log for final
live counters and remaining platform/RF limitations.
