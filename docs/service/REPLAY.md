# File replay contracts

For the current procedure, see [Recording and replay](https://github.com/mauriziofantino/nav-ngene-evo/wiki/Recording-and-Replay).
The older independent replay session described below is retained as explicitly historical
engineering material; the current GUI/service has one source and rejects `/api/v1/replay/`.

The `file` frontend supports the configured native I/Q format: CI8 uses one signed int8 I
followed by one signed int8 Q; CU8 uses unsigned bytes with the corresponding ADC midpoint;
SC16 Q11 uses signed 16-bit components containing twelve-bit ADC values. It does not
reconstruct a real IF signal. Empty files, lengths not divisible by the format's pair
size and out-of-file ranges are rejected.

Raw bytes do not contain sample rate, tuning or GNSS time. From 0.6.3, the reader imports
parameters from an adjacent segment sidecar, such as `FILE.cs8.meta.ini`. For historical
`PREFIX_000000.cs8` segments it also checks `PREFIX.meta.ini`. It does not concatenate
segments automatically or reconstruct gaps from CSV: each file is replayed as the
contiguous sequence of pairs present. Keep the index for later temporal reconstruction.

```ini
[frontend]
type=file
sample_rate_hz=16000000
center_frequency_hz=1576420000
signal_frequency_hz=1575420000
[replay]
format=CI8
path=D:/GNSS/capture_000000.cs8
start_sample=0
sample_limit=0
realtime=true
loop=false
```

`start_sample` and `sample_limit` count complex pairs rather than bytes. A zero limit
reads to the end of the range. `realtime=true` uses a monotonic clock and the configured
sample rate; false reads as fast as the engine permits. `loop=true` repeats the range,
increments the epoch and signals a gap with unknown missing count: a rewind is not
continuous RF reception. Produced indices remain monotonic. An incomplete final window
is marked explicitly; no zero padding is added.

Each frontend instance owns its file handle, window, cursor and pacing state. The buffer
returned by `acquire` is immutable until `release`; a second acquire during that lease
is rejected. EOF ends normally. Length is fixed when the file opens: replay closed,
immutable files and do not modify or delete them while active.

## Historical independent-session design (removed)

An earlier web service owned primary and replay controllers. The primary could use HackRF
or a file; the second was file-only. Each owned its engine, conditioner, ring, recorder,
diagnostics and revision. Channels within one session still shared the same window;
different streams were not mixed or synchronized automatically. This architecture allowed
new native adapters without changing the file frontend contract.

The former GUI selector chose which session received commands and supplied observations.
Start/Stop, settings, diagnostics, recording and transfer acted on that selection while
the other session continued. Unsaved changes had to be saved/discarded before switching.
Exit service and Ctrl+C stopped both. This selector is not in the current GUI; use separate
processes with different ports/profile directories for parallel operation.

The removed second-session routes used `/api/v1/replay/`: `config`, `status`,
`receiver/start`, `receiver/stop`, `recording/start`, `recording/stop`, `analysis/config`,
`files`, `files/delete` and `files/<name>`. They shared token/origin validation, with
controller-specific revisions. Former WebSocket requests were `status:<mask>` and
`replay:<mask>`, using the latest-frame policy. These historical routes are not an
instruction to use the current service's rejected independent-session API.

Historical tests covered ranges, signedness, EOF, leases, looping, independent storage,
replay alongside the actual HackRF adapter with a simulated driver, and two controllers
through a browser. These do not substitute for real USB/RF capture. The synthetic frontend
and its profiles were removed; deterministic vector generation belongs only to tests and
documentation capture tooling.

## File selection and automatic configuration (introduced in 0.6.3)

**Browse I/Q files…** lists receiver-side folders and supported raw I/Q files.
**Drives / roots**, **Parent folder**, subfolder buttons and filename filtering avoid
typing complete paths. Replies contain at most 500 entries; filtering occurs on the
server before that limit. Selection prepares a draft and neither starts nor reconfigures
a running receiver. The **Replay** action for a closed capture prepares the file source
in the current receiver.

The reader checks native format, I/Q interleaving, frequencies, basename association and,
for version 2 metadata, the actual stored pair count. Inconsistent existing metadata is
rejected rather than replaced with assumptions. A missing sidecar permits manual setup
with a warning: set sample rate and RF references before saving. Active files, empty or
misaligned files, symlinks/reparse points and paths exceeding profile limits cannot be
selected.

Imported settings use `frontend.type=file`, recorded format, sample rate, hardware/signal
frequencies and bandwidth, an initial 1 ms window, the entire range and automatic recording
off. Conditioning starts in **bypass**, avoiding a second DC-removal pass over already
conditioned samples. The operator may change that draft deliberately. Pacing, loop and
diagnostic options retain their profile values. Recorded rate is nominal, rather than a
measurement of hardware sample-clock accuracy. No common complex frequency translation
is introduced.

CLI with sidecar:

```sh
ngene --replay "D:/GNSS/example_000000.cs8"
```

Without sidecar:

```sh
ngene --config config/replay.ini --replay "D:/GNSS/legacy.cs8"
```

## Per-file sidecar, schema 2

```ini
[capture]
version=2
format=CI8
layout=I,Q interleaved
component_bits=8
component_signed=true
data_file=example_000000.cs8
sample_rate_hz=16000000
sample_rate_basis=configured_nominal
center_frequency_hz=1576420000
signal_frequency_hz=1575420000
digital_if_hz=-1000000
bandwidth_hz=15000000
conditioning=dc_iir
dc_shift=16
recording_point=post_conditioning
hardware_timestamp=false
host_start_unix_seconds=1791619200

[result]
complete=true
written_samples=16000000
gap_events=0
```

`data_file` is a basename, allowing raw and sidecar to move together. Renaming raw data
also requires renaming its sidecar and updating `data_file`. A missing footer means the
file was not finalized and prevents schema 2 automatic import. An explicit
`complete=false` footer with a consistent length is shown as an interrupted capture that
needs inspection. Earlier finalized segments can be complete even if a later one failed.

Transfer raw and its sidecar, plus index/session metadata to preserve gap history.
**Delete** removes one file and does not implicitly remove associated files. The backend
protects a file being used by the active receiver from deletion.

`POST /api/v1/replay-files/browse` accepts optional `path` and `filter`;
`POST /api/v1/replay-files/inspect` accepts `path`. They require token/origin validation
and only read files. Inspect returns metadata, revision and draft INI without mutating
the controller. Former duplicate routes under `/api/v1/replay/` have been removed.
