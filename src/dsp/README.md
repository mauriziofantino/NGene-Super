# DSP NGene

`ngene_dsp` (`NGene::Dsp`) è la libreria matematica indipendente da motore, frontend e GUI.
Dipende soltanto da `ngene_base` per gli errori comuni e dalla libreria matematica di sistema
dove necessaria. Le API pubbliche non espongono tipi SIMD o dettagli dei dispositivi.

| Famiglia | API | Sorgente |
|---|---|---|
| FFT Q14 complessa | `ngene/dsp/fft.h` | `fft/fft.c`, kernel privati `x86/` e `arm/` |
| Statistica I/Q CI8 | `ngene/dsp/statistics.h` | `statistics/moments.c` |
| Potenza CI16 e accumulo uint64 | `ngene/dsp/power.h` | `power/power.c` |

`ngene/dsp/sample_ops.h` resta l'umbrella compatibile. `sample_backend.h` è privato e
centralizza la selezione SIMD delle primitive, senza duplicarla fra famiglie. Le formule,
i loop e gli oracoli sono preservati nello split.

`cmake/FftBackend.cmake` seleziona il kernel FFT secondo il target del compilatore, anche
nei benchmark: SSE2 assembly x64, SSE2 intrinsics x86-32, NEON intrinsics ARM64/ARMv7.
Le primitive statistiche/potenza mantengono SSE2 su x64, NEON su ARM64 e C sugli altri target.
Le opzioni reference restano disponibili per verifica. Piano FFT immutabile e workspace
disgiunti permettono chiamate concorrenti senza thread interni alla libreria.

Correlatori, NCO e repliche avranno famiglie proprie quando migrati e validati.
Vedere [architettura](../../docs/ARCHITECTURE.md),
[contratto FFT](../../docs/FFT_REVIEW.md) e [benchmark](../../benchmarks/fft/README.md).
