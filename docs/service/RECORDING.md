# Recording format

Engineering reference. The original CI8 examples below also apply to native CU8 (`.cu8`)
and SC16 Q11 (`.sc16q11`) recordings with their own format metadata and bytes per pair.
See the [current recording manual](../wiki/Recording-and-Replay.md).

With `prefix=example` and CI8 input, the recorder creates:

- `example_000000.cs8`, `example_000001.cs8`, …: headerless interleaved signed 8-bit I/Q
  pairs; two bytes per pair, with no multibyte endianness issue.
- `example_000000.cs8.meta.ini`, `example_000001.cs8.meta.ini`, …: schema 2 sidecar for each
  segment, including automatic replay parameters and the number of pairs.
- `example.index.csv`: maps each written portion to sample time and position in its segment.
- `example.meta.ini`: configuration, source, host opening time and final result.

An existing prefix is never overwritten. If opening fails partway through, files already
created remain visible and incomplete. Choose another prefix or remove them deliberately.
The destination is an existing local directory.

## Index

```text
epoch,first_sample,count,segment,segment_offset_samples,flags,missing_known,missing_before
```

Every sample quantity counts complex I/Q pairs. For CI8, the byte offset is
`2 * segment_offset_samples`; use four bytes per pair for SC16 Q11. A window may occupy
several rows when it crosses a segment boundary.

`flags & 1` indicates a gap before this portion; `flags & 2` indicates a partial window.
`missing_before` is an exact quantity only when `missing_known=1`. No zeros are inserted
to fill losses. Do not concatenate files blindly while ignoring the index and epochs.

Relative time within a continuous epoch follows sample index/sample rate. The Unix host
time in metadata is not an RF or GNSS timestamp. The HackRF backend does not claim a
measurement of its sample-clock error.

## Write integrity

The writer uses a bounded queue with a dedicated copy. Accepted windows are written in
engine order. Queue overflow or a write error stops the run and causes a nonzero exit.
The implementation does not rely on silent loss or unbounded memory to keep up.

The `[result]` footer marks `complete=true` only when the run and writer close normally.
This establishes successful software closure, rather than absence of gaps: inspect the
index and counters. The footer includes written samples, blocks, segments, samples received
from the source, known losses and samples left in the frontend queue at termination.

A missing footer, `complete=false`, execution errors or inconsistent lengths require
inspection. An I/O failure may leave an incomplete final block, even a single byte; do not
interpret it as a complete pair. `fclose` and flush results are checked, but the format
does not promise transactions across files or durability against power loss. A future
container/checksum format can add these without changing the engine.

Segmentation occurs at exactly the configured pair count. Without segmentation, files may
exceed 4 GiB if supported by the filesystem: indices and counts are 64-bit and writes are
sequential.

## GUI-controlled recording

The selected folder is on the NGene computer. A browser can be local or remote through
the tunnel described in the manual. The operator chooses a basename; host time and a
random identifier distinguish runs, while `_000000`, `_000001`, etc. identify segments.
CLI operation retains the exact INI prefix without an automatically added session suffix.

**Transfer** downloads a single closed file over HTTP and preserves the original. Retain
raw segments, `.index.csv` and `.meta.ini` metadata to reconstruct the capture. The GUI
does not convert bytes or buffer the entire recording in browser RAM: it uses native
browser download handling. The browser chooses its local download folder. Transfers are
per file; automatic synchronization is not implemented. **Delete** removes a closed file
after confirmation and active-use checks.

`Closed` means the current recorder no longer has the file open. A recording interrupted
by an error can also be closed: check `complete` and gap metadata. The list displays at
most 200 files in the selected folder. Active files and symbolic links are not offered
for download.

## Pre/post taps (v0.3)

`recording.point` selects pre- or post-conditioning. Metadata records
`recording_point=pre_conditioning` or `post_conditioning`. The pre tap preserves original
frontend blocks; the post tap follows the assembled common windows. The operational
conditioner is bypass or continuous DC IIR. Bypass produces identical pre/post values;
with DC IIR, the chosen tap determines which values are retained. A transforming test
conditioner checks the contract without adding fictitious production filters.

The disk writer writes/finalizes each segment sidecar; RF callbacks perform no file I/O.
Parameters are copied when recording opens. Schema 1 session metadata remains available
for compatibility and summary. A schema 2 sidecar count applies only to its associated
raw file. [Import and validation](REPLAY.md).
