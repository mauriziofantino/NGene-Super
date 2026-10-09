# DSP NGene

La FFT Q14 intera è implementata in `src/dsp/fft/fft.c` con butterfly assembly x86-64 SSE2 e AArch64 NEON. API pubblica: `include/ngene/dsp/fft.h`. Piano immutabile, workspace per chiamata, riferimento C per verifica. Correlatori, NCO e repliche saranno migrati per incrementi. Vedere `docs/FFT_REVIEW.md`.

`sample_ops.c` implementa momenti CI8 e accumulo di potenza CI16 con intrinsics SSE2/NEON e riferimento C. Il limite di lunghezza dei momenti è dichiarato nell’header. Verifiche, benchmark e limiti della portabilità sono in `docs/ITERATION_04.md`.
