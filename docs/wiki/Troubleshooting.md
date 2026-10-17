# Troubleshooting

Check the receiver state and status message first. When reporting a problem, include `ngene --version`, the platform and architecture, the selected source/format, the relevant INI settings, and the exact error text. Do not include private paths, credentials, or unrelated capture data.

## The console cannot connect

Run the executable with `--web` and open `http://127.0.0.1:8080` on the same computer. For a custom port, use that exact port in the URL. The service binds to loopback and accepts the expected local host/origin. Opening the HTML directly from disk does not provide a running receiver service.

If a port is occupied, select another with `--port 8081`. If startup reports that web support is disabled, rebuild with `NGENE_WITH_WEB=ON`. After **Exit service**, restart NGene before reloading the browser. If rebuilding changed the embedded GUI, restart the process so it uses the new executable.

## Start receiver is unavailable

Apply or discard pending profile edits, wait for the current command to finish, and check the receiver state. For file input, choose an existing replay path. A source preset or an imported INI is a draft until **Apply & save full profile** succeeds. A saved configuration is separate from an open device or a running stream.

## No device appears, or the selected adapter is unavailable

Run `ngene --version` to confirm the required native adapter is compiled in, then `ngene --list-devices` to check discovery. Install the device's native library and operating-system USB driver, check the connection, and close other applications that exclusively own it. When multiple devices are connected, select the intended serial identifier.

**Query device limits** requires a stopped receiver and a device that can be opened. A build can include an adapter even when no corresponding hardware is connected. Native adapter support does not emulate the device.

## A sample rate, bandwidth, tuning, or gain is rejected

Use the correct source preset, then query the connected device's limits. HackRF accepts rates from 2 to 20 Msps; RTL-SDR accepts the two rate intervals listed in the [configuration reference](Configuration-Reference.md). RTL-SDR manual gain must be one of the tuner's discrete gain values. bladeRF gain bounds and steps depend on the selected device and frequency.

The generic profile validator is followed by native hardware checks. A value accepted as an integer by the form can still be unsupported by the device. Keep `signal_frequency_hz` inside the sampled band: its difference from hardware tuning must be strictly less than half the sample rate. Select SC16 Q11 if a bladeRF device reports that native CI8/SC8 transfer is unsupported.

## The receiver runs but plots do not update

Return to **Observation**, bring the browser tab to the foreground, and use **Resume plots** if paused. Enable at least one of spectrum or waterfall. The first Enable also restarts runtime spectrum computation if it was off. For histograms, check both **I/Q histograms** and **Diagnostics → Histogram enabled**.

A new plot waits for enough contiguous samples to form an observation. Large FFT/Welch settings, long histogram observations, slow file sample rates, discontinuities, or a short non-looping file can delay or prevent a complete burst. Choose smaller observations or a longer input range. Check the receiver state for EOF or a stream error.

The Configuration view and a hidden browser tab deliberately stop requesting optional diagnostics. Reception and recording can continue while the plots are inactive. Other interested consumers can keep a diagnostic producer active. See [Signal diagnostics](Signal-Diagnostics.md).

## The spectrum and waterfall are expensive

Disabling both **Enable** checkboxes switches runtime spectrum computation off. Re-enabling either restarts it. Histograms are independent: hide or disable them as well to stop their work when no other consumer requests it. For an occasional preview, reduce spectrum rate, Welch segments, and FFT size. Reduce histogram cadence and observation length separately.

Use 2D waterfall, shorter retained history, and a lower browser refresh rate to reduce display cost. Browser refresh and waterfall row rate do not directly set the receiver's FFT rate. Input acquisition, conditioning, and recording still consume CPU while the receiver runs.

## Input losses or queue overflow increase

Inspect frontend queue occupancy, its high-water mark, available buffering time, transport errors, and known input losses. Lower the input rate if feasible, reduce optional diagnostics, and check the USB path and sustained processing capacity. Increase ingress buffering only to absorb temporary stalls; it cannot correct a sustained throughput deficit.

`overflow_policy=stop` fails promptly when mandatory buffering fills. `drop_newest` reports known rejected input and discontinuities while allowing the stream to continue. Neither reconstructs the missing samples. Queued samples discarded during a normal stop are reported separately from samples lost during the run.

Hardware-only loss may be unmeasured for a particular device. A zero software drop counter does not establish perfect RF continuity. Diagnostic **skipped bursts**, **contended blocks**, and **display skips** are optional observation/display counters, not direct input loss counts.

## Recording fails or its queue remains full

Confirm free space, write permissions, and sustained disk throughput. At 16 Msps, native raw data require 32 MB/s for CI8/CU8 or 64 MB/s for SC16 Q11. Use a destination whose parent exists and a valid filename prefix. With `recording.directory` set, do not include a path in `recording.prefix`.

The CLI does not overwrite an existing capture with the same prefix. Choose a new prefix or destination. The GUI adds a unique session suffix. A larger writer queue may absorb a short disk stall but cannot compensate for a disk that remains too slow.

After stopping, wait for the writer to drain and files to close. Preserve incomplete sidecars and the sample index when diagnosing failures. A full queue or write failure is reported rather than silently accepting recording loss.

## Replay fails or the displayed rate is wrong

Use a nonempty raw file with complete pairs and the correct representation. CI8/CU8 lengths must be multiples of two bytes; SC16 Q11 lengths must be multiples of four. The start pair must be within the selected file. Keep its matching sidecar beside the raw segment and ensure the sidecar names that file.

Without metadata, set the sample rate, hardware tuning, signal centre, and sample format yourself. A suffix identifies a possible representation, not the RF rate or frequency. A malformed or contradictory sidecar requires correction; the application does not silently disregard it. Do not select a file still being written.

With real-time pacing disabled, the delivery rate can exceed the recorded nominal sample rate. This changes playback speed, not the meaning of the recorded frequency axis. Repeat mode creates explicit loop discontinuities. The GUI replays one raw segment at a time and does not restore historical gaps or timestamps from the capture CSV.

## INI validation fails

Check the exact section/key names, lowercase boolean words, and numeric units in the [configuration reference](Configuration-Reference.md). Unknown keys and duplicates are errors. Decimal counts contain digits only; do not append `MHz`, `ms`, commas, or inline comments. Set `recording.enabled=true` when using `receiver.mode=record`.

If the error concerns the 512 MiB buffer guard, reduce window or queue dimensions. If it concerns the Welch burst size, reduce FFT size or Welch segments, or increase overlap until the complete observation fits 262,144 pairs. Disabled diagnostics still require valid dimensions so that they can safely be enabled later.

## There is no satellite list or position fix

This application currently provides the GNSS receiver's input, conditioning, stream engine, diagnostic, and recording/replay functions. Satellite acquisition, tracking, navigation decoding, and PVT are not integrated. A running sample counter, L1 tuning, or spectral peak does not imply satellite lock. There is no configuration switch that enables a currently absent navigation solution.
