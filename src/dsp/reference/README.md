# Riferimento numerico

`NgFftReference` resta nello stesso file del piano FFT (`../fft/fft.c`).
`NgIqMomentsReference` e `NgPowerAccumulateReference` sono nelle rispettive famiglie
`../statistics/` e `../power/`. Gli oracoli sono disponibili anche nelle build SIMD.
`NGENE_FFT_REFERENCE=ON` e `NGENE_SAMPLE_REFERENCE=ON` forzano C separatamente;
lo split in librerie e il parallelismo esterno non cambiano il contratto numerico.
