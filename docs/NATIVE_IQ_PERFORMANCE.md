# Native bladeRF precision, ring buffering and waterfall views

## Storage and native transfer formats

| Frontend / setting | Native transfer | I storage | Q storage | Bytes per complex pair |
|---|---|---|---|---|
| HackRF | CI8 | signed 8 bits | signed 8 bits | 2 |
| RTL-SDR | CU8 | unsigned 8 bits | unsigned 8 bits | 2 |
| bladeRF `format=SC16_Q11` (default) | SC16 Q11 with metadata | signed 16 bits, 12 significant ADC bits | signed 16 bits, 12 significant ADC bits | 4 |
| bladeRF `format=CI8` | SC8 Q7 with metadata | signed 8 bits | signed 8 bits | 2 |

An eight-bit component is stored in one byte, not in a sixteen-bit container.
The bladeRF twelve-bit mode retains values from -2048 through 2047 in each
little-endian sixteen-bit component. Native eight-bit mode carries -128 through
127 in each component. It reduces transfer and sample storage bandwidth by half,
with reduced quantization precision; it does not make the physical ADC eight-bit.

Select the format while stopped, either in **Configuration → Input source →
bladeRF → Sample precision**, or in the receiver profile:

```ini
[bladerf]
format=CI8
```

Use `format=SC16_Q11` to retain all twelve ADC bits. Existing profiles without the
new key retain that default. This setting selects `BLADERF_FORMAT_SC8_Q7_META` or
`BLADERF_FORMAT_SC16_Q11_META` directly in `bladerf_sync_config`; there is no host
narrowing or automatic fallback. An incompatible SDK is rejected at configuration
time; an unsupported device/FPGA is rejected before RX. Native eight-bit support
requires a compatible FPGA (the vendor capability was introduced in FPGA 0.15.0)
and libbladeRF with SC8 metadata support (2.5 or later). Actual support is checked
by the native configuration call, not inferred solely from a version string.
`TNgConfig` adds the `bladeFormat` member; rebuild clients using this public C
structure. The frontend lifecycle and ring API are unchanged.

The pinned source provider uses libbladeRF 2.6.0. Its synchronous RX removes
transport metadata and returns contiguous I/Q plus a separate metadata structure.
For RX_X1, `actual_count`, hardware timestamps and missing-sample counts are all
complex-pair units in both modes. Valid prefixes returned with a timeout are
preserved. Device overruns annotate the following interval; timestamp deltas
measure missing pairs without confusing software queue overflow with device loss.

Primary references: [Nuand sample formats](https://www.nuand.com/libbladeRF-doc/v2.6.0/group___s_t_r_e_a_m_i_n_g___f_o_r_m_a_t.html),
[pinned synchronous RX implementation](https://github.com/Nuand/bladeRF/blob/libbladeRF_v2.6.0/host/libraries/libbladeRF/src/streaming/sync.c),
[RX metadata and overrun semantics](https://www.nuand.com/libbladeRF-doc/v2.6.0/sync_rx_meta.html).

## Ring-buffer contract and real-time limits

The existing receiver architecture is retained: vendor RX → native-format ingress
ring → conditioning and common-window assembly → shared engine ring → synchronous
channel hooks / asynchronous recorder. The ring stores native bytes and one metadata
record per slot; a format selected at creation fixes its byte stride for the run.

- CI8 and CU8 use `2 * windowSamples` bytes per slot; SC16 Q11 uses
  `4 * windowSamples`. The same format flows through ingress, the common ring and
  recording. The diagnostic FFT's selected sixteen-bit workspace does not widen
  the continuous receiver stream.
- All slots and metadata are allocated before reception. Push copies borrowed
  vendor bytes before they expire; acquire returns a borrowed immutable slot,
  without copying; release makes it reusable. A full ring never overwrites an
  unread or leased block. Format mismatches are rejected.
- The implementation has one logical producer, one reader and one outstanding
  lease. A mutex protects slot bookkeeping and the push copy. It is bounded by
  configured storage, but is neither lock-free nor a hard real-time guarantee.
- Occupancy and high-water counters count slots, including a leased slot.
  Pending/accepted/consumed counters count complex pairs. Partial blocks occupy
  full slots, so reported free milliseconds describe maximum pair capacity,
  rather than a guaranteed time to overflow for arbitrary partial transfers.
- bladeRF keeps its internal vendor buffer at 16384 pairs, while requesting the
  largest whole number of receiver windows that fits that bound. The default
  16000-pair window therefore requests 16000 pairs, avoiding a second nearly-empty
  ingress slot for the remaining 384 pairs. Windows larger than 16384 pairs,
  hardware gaps and partial timeouts can still produce partial slots.
- Stop-on-overflow fails reception explicitly; drop-newest advances indices and
  marks the next accepted interval with the known gap. Channel processing must
  reset coherent integration when the epoch or continuity changes.

At 16 M complex pairs/s, sample bandwidth is 32 MB/s for CI8/CU8 and 64 MB/s for
SC16 Q11, before transport metadata and other copies. A full 16000-pair window is
1 ms in either representation. A 128-slot ingress stores 4,096,000 bytes in eight-bit
mode or 8,192,000 bytes in twelve-bit mode, excluding metadata. Capacity expressed
in samples is identical; byte traffic differs. The configuration's 512 MiB sample
queue guard now uses the selected bladeRF format.

The concurrent native-format tests exercise CI8, CU8 and SC16 Q11 with 20,000
varying-size producer blocks each, preserving byte values, sequence, occupancy
and `accepted - consumed = pending`. Recording/replay, DC estimation and
diagnostic scaling are tested in all three formats. The bladeRF native API fixture
additionally writes independent vendor layouts, including signed endpoints and
nonzero low ADC bits, for both transfer selections.

The ring is suitable for delivering immutable native blocks to future acquisition
and tracking kernels. Those kernels are not implemented by this task: the existing
channel API still rejects non-CI8 channel processing explicitly. Twelve-bit/CU8
tracking requires appropriate format-specific kernels; no silent quantization is
introduced to bypass that restriction. Sustainable real-time operation must be
measured with the actual device, selected sample rate, recording disk, acquisition
schedule and active channel count. A copy benchmark alone cannot certify it.

Run the existing native ring microbenchmark in a Release build:

```powershell
.\build\msvc-x64-multisdr\Release\ngene_sdr_tests.exe --benchmark
```

One Windows x64 Release run on an Intel Core i7-1355U measured 1.148 µs/block
for CI8, 1.108 µs/block for CU8 and 2.654 µs/block for SC16 Q11 (16000 pairs,
20000 push/acquire/release iterations). These are single-thread, warm-memory
copy measurements including ring bookkeeping, not concurrent throughput,
latency guarantees or hardware acceptance results.

## FFT comparison

See [the FFT benchmark](../benchmarks/fft/README.md) for a reproducible comparison
with the original NGene implementation and the scalar oracle. Results distinguish
execution time, plan setup, numerical differences and host/compiler conditions.
They do not establish a universal fastest implementation or full receiver throughput.

## Waterfall display

The **Waterfall view** selector switches between the original **2D** waterfall
and a **3D** projected surface. Both use the same received PSD history, frequency
interval and global colour scale. Height uses the existing power-axis range;
depth uses producer timestamps. Switching views does not change receiver settings,
restart RX, run additional FFTs or transfer raw I/Q.

The projected surface renders at most 48 history slices and 192 frequency buckets,
retaining the peak within each frequency bucket. It is a display reduction, not
additional spectral resolution. See [GUI documentation](WEBGUI.md).

## Validation and changed areas

Native Windows x64 MSVC builds completed with the project's strict warning policy.
CTest passed all 14 tests in Release and all 14 in Debug, including native-format
concurrency, recording/replay, bladeRF precision/metadata fixtures and the expanded
FFT oracle/DFT/headroom/alignment/concurrency cases. The Edge browser suite passed
the 2D/3D, history, mobile, bladeRF controls and existing file/recording regressions.
JavaScript syntax, C formatting and `git diff --check` were also checked.

Windows test tools need normal host permissions: sandbox execution denied
`GetLongPathNameW` and caused spurious file-download 404 responses; the complete
browser suite passed outside that sandbox. PowerShell test execution policy was
scoped to the test process. No system policy or production file-serving code was
changed to accommodate the tests.

No SDR devices were connected during enumeration. Native RF reception, firmware/FPGA
interoperability and acquisition/tracking under sustained RF load remain hardware
acceptance work. GNU SSE2 assembly was additionally assembled for Windows, Linux
and macOS, with Windows execution checked; Linux/macOS runtime and ARM64 performance
were not measured here.

| Area | Files |
|---|---|
| Native precision and profile | `src/frontend/bladerf.c`, `src/frontend/bladerf_adapter.h`, `include/ngene/config.h`, `src/config/config.c`, `config/bladerf.ini` |
| SDK and Windows native fixture | `CMakeLists.txt`, `cmake/MultiSdrProvider.cmake` |
| FFT implementation | `src/dsp/fft/fft.c`, `src/dsp/fft/butterfly.h`, `src/dsp/x86/fft_sse2.S`, `src/dsp/x86/fft_sse2.asm`, `src/dsp/x86/README.md` |
| GUI | `web/app.js`, `web/index.html` |
| Regressions | `tests/test_bladerf.c`, `tests/test_core.c`, `tests/test_fft.c`, `tests/web_ui.cjs` |
| Benchmark and evidence | `benchmarks/fft/` |
| Documentation | `README.md`, `docs/CONFIGURATION.md`, `docs/FFT_REVIEW.md`, `docs/WEBGUI.md`, this report |
