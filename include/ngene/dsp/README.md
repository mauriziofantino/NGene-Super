# DSP NGene

La libreria `NGene::Dsp` espone API indipendenti dal backend CPU:

- `fft.h`: FFT complessa Q14, piano immutabile, workspace per chiamata e oracolo scalare.
- `statistics.h`: somme e somme dei quadrati I/Q CI8 esatte.
- `power.h`: potenza complessa CI16 con accumulo uint64 esatto.
- `sample_ops.h`: umbrella compatibile per statistica, potenza e nome del backend.

La chiamata applicativa non seleziona SSE2/NEON: compilatore e configurazione del target
scelgono l'implementazione privata. I contratti dichiarano range, overflow, allineamento e
concorrenza. Correlatori, NCO e repliche saranno migrati per incrementi; vedere
[architettura](../../../docs/ARCHITECTURE.md) e [FFT](../../../docs/FFT_REVIEW.md).
