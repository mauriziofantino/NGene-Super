# Isolated tracking throughput probe

Configure the ordinary root project with `-DNGENE_TRACKING_BENCHMARK=ON`, then build
`ngene_tracking_throughput`. It links the production tracking kernels and the same
bounded dispatcher. No instrumented receiver or hardware is used.

```
ngene_tracking_throughput 16000000 3 16 200 4
```

Arguments: sample rate Hz, odd tap count, channels (1..32), signal windows in ms,
worker count. Input allocation/generation and pool creation are outside the timer.
Every job receives the same immutable I/Q block, with independent NCO/loop state.
Output reports signal duration, elapsed seconds, their ratio, and a correlator
checksum. Compare equal arguments when changing kernels or worker count; do not
infer acquisition sensitivity, USB performance or position accuracy from this probe.

Measurements and limits are recorded in `docs/service/TRACKING.md`. Restore
`-DNGENE_TRACKING_BENCHMARK=OFF` for the normal build. The executable is never
installed or added to the default production build without this explicit option.
