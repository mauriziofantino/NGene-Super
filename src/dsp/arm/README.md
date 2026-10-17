# ARM NEON

`fft_neon_intrinsics.c` è il kernel FFT di produzione su ARM64 e ARMv7 NEON a 32 bit:
quattro butterfly per iterazione, codelet iniziale a otto punti e controllo SIMD del range.
Conserva esattamente prodotti Q14 e floor per stadio, con workspace posseduto dal chiamante.

`fft_neon.S` e `fft_neon.asm` conservano il precedente kernel ARM64 a due butterfly come
baseline dei confronti; non sono selezionati per il sorgente di produzione attuale.
Le primitive in `../statistics/` e `../power/` usano NEON su ARM64 e C su ARM32.
Per precisione, cross-compilazioni, emulazione e limiti delle misure native vedere
[FFT_REVIEW.md](../../../docs/FFT_REVIEW.md).
