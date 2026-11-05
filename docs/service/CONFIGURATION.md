# INI configuration engineering notes

The [current configuration reference](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Configuration-Reference) lists every key,
default and bound from the implementation. This document retains the original profile
contract and versioned development notes. Older two-session and CI8-only descriptions are
historical; the current service has one active source and converts transport/replay formats
to one signed CI8 or CI16 Q15 common stream per run.

For native bladeRF precision, `bladerf.format=SC16_Q11` retains twelve ADC bits in
sixteen-bit components (default), while `bladerf.format=CI8` requests native SC8 Q7
transfer with one byte per I/Q component. See [native formats and ring buffering](NATIVE_IQ_PERFORMANCE.md).

The dependency-free C parser treats keys and section names as case-sensitive. Integers use
base 10 and frequencies use Hz. The GUI presents the GNSS signal reference in GHz,
SDR tuning and bandwidth in MHz, and sample rate in Msps; import/export and saved
profiles retain Hz with 1 Hz resolution. Doppler parameters remain in Hz.
Whitespace around `=` and full-line comments beginning
with `#` or `;` are allowed. Quoting, trailing comments, variable expansion, `~` expansion
and includes are not supported. Relative paths are resolved against the process working
directory, rather than the INI file's directory. Unknown/duplicate keys and non-finite
values are errors. ASCII paths are recommended for this version, including on Windows.

| Key | Meaning / values |
| --- | --- |
| `receiver.mode` | `capture` dispatches registered channels; `record` requires recording enabled and skips channels |
| `receiver.run_seconds` | Host-time limit; 0 runs until EOF/Ctrl+C; default 2 |
| `receiver.idle_timeout_ms` | Error if no samples arrive for this interval; 100..3600000; default 3000 |
| `frontend.stream_format` | `auto` (preserve source precision), `CI8` (may discard wider ADC precision), or `CI16` (signed Q15) |
| `frontend.type` | `file`, `hackrf`, `rtlsdr`, `bladerf` or `sige`, depending on compiled adapters |
| `frontend.serial` | Device selector; may be omitted with one device; multiple devices are not silently selected |
| `frontend.sample_rate_hz` | Complex pairs/s; default 16000000; HackRF range 2000000..20000000 |
| `frontend.center_frequency_hz` | SDR hardware tuning frequency; default 1575420000 |
| `frontend.signal_frequency_hz` | Signal centre; 0 follows hardware for compatibility; digital IF = signal − hardware, strictly within ±Fs/2 |
| `hackrf.bandwidth_hz` | Supported analogue filter; 0 selects the supported bandwidth immediately below the sample rate |
| `hackrf.lna_gain_db` | 0..40 in steps of 8; default 16 |
| `hackrf.vga_gain_db` | 0..62 in steps of 2; default 20 |
| `hackrf.rf_amp` | true/false; default false |
| `hackrf.antenna_power` | RF connector power; default false; enable only when required by the receiving setup |
| `buffer.window_samples` | Pairs per common window; 1..1048576; default 16000 |
| `buffer.ingress_blocks` | Transport queue slots; default 128 |
| `buffer.ring_blocks` | Central ring slots; default 16 |
| `buffer.overflow_policy` | `stop` (default) or `drop_newest` for the ingress queue |
| `recording.enabled` | Enable recording; default false |
| `recording.prefix` | Output prefix; existing parent directory and unused prefix required |
| `recording.queue_blocks` | Writer queue slots; default 128; overflow stops the run |
| `recording.segment_samples` | Pairs per raw file; 0 disables segmentation |

Queue capacities must be 2..4096. The allocation plan has a 512 MiB guard. These explicit
limits prevent accidentally unmanageable configurations and may be revised later. Replay
window size must not exceed one second at the configured sample rate.

HackRF parameters are validated but applied only to the HackRF frontend. Replay uses the
configured stream metadata and does not represent an active RF device.

HackRF setter return values are checked. Automatically selected filter bandwidth is reported
in the stream description and recordings. Rate and tuning are values configured/accepted by
the vendor library, rather than calibrated measurements of oscillator or actual frequency.

There are no inert `fast`, `precise`, `fft_backend` or `channels=24` keys. Such options must
arrive with their implementations. Historical CPU/channel counts are references, rather
than acceptance criteria for this prototype.

## Directory and web configuration (v0.2.0)

The optional `recording.directory` key specifies a folder on the receiver computer. When
present, `recording.prefix` must be a basename without path separators. The CLI requires an
existing directory. GUI saving creates the final directory if its parent exists and
normalizes the absolute path. Without this key, the CLI retains the original prefix
semantics, including prefixes containing a path.

In web mode, the basename is limited to 1–64 ASCII letters, digits, `_` and `-`; absolute
paths are limited to 240 bytes. Example:

```ini
[recording]
enabled=false
prefix=example_L1
directory=/data/gnss
queue_blocks=128
segment_samples=16000000
```

All profile parameters can be edited while stopped. `recording.enabled` requests automatic
recording on the next Start; recording buttons act on the current run without rewriting
that parameter. Plot refresh rate and pause are browser runtime choices and do not change
the sample stream.

The controller uses optimistic revisions: an outdated configuration is rejected so two
browser tabs cannot silently overwrite each other. Profile writing uses a temporary file
and atomic replacement. A `receiver.ini.tmp` left after a crash blocks another write until
it has been examined/removed. Atomic replacement is not a power-loss durability guarantee.

## v0.3 parameters

`recording.point=pre|post` is retained for profile compatibility. Both values now record
the common signed stream after transport conversion; the redundant UI selector is removed.
The `[analysis]` section
and its limits are described in [ANALYSIS.md](ANALYSIS.md). These settings can be applied at
runtime through the dedicated API; **Apply & save** persists the complete profile only while
stopped. Earlier versions provided an independent replay controller alongside the primary
controller; this has been removed from the current service. The default replay profile is
`config/replay.ini`; see [replay contracts](REPLAY.md).

The GUI provides LNA 0..40 dB in steps of 8 and VGA 0..62 dB in steps of 2, matching HackRF
validation. Settings depend on the frontend; HackRF fields are not shown for file replay.
The streaming DC remover is retired. Historical conditioning keys are accepted and ignored,
and omitted on save. SiGe hardware rate/RF/IF are fixed. See the
[frontend decision record](FRONTEND_STREAM_DECISIONS.md) for stream scaling, USB setup,
recording semantics and compatibility details.

## Replay parameters introduced in v0.6

| Key | Meaning |
| --- | --- |
| `frontend.type` | Initially `hackrf` or `file`; `synthetic` was removed. Current native adapters also include RTL-SDR, bladeRF and SiGe |
| `replay.path` | Closed I/Q file on the receiver computer; required before Start |
| `replay.format` | Recorded storage format: CI8, CU8, SC16_Q11 or CI16 (Q15) |
| `replay.start_sample` | Starting offset in complex pairs |
| `replay.sample_limit` | Pair limit; 0 reads the rest of the selected range |
| `replay.realtime` | Pace at the configured sample rate; default true |
| `replay.loop` | Repeat the range with an explicit epoch/gap; default false |

Old synthetic-frontend profiles must be replaced with a supported SDR or file profile.
Unknown keys remain errors; the retired conditioning keys described above are explicit compatibility exceptions. INI/JSON names preserve the protocol
format: lowerCamel C member names do not rename on-disk or WebSocket keys.
