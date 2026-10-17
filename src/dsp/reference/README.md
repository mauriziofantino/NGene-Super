# Numerical reference

`NgFftReference` remains in the FFT plan's source file (`../fft/fft.c`).
`NgIqMomentsReference` and `NgPowerAccumulateReference` are in their respective
`../statistics/` and `../power/` families. Oracles are also available in SIMD builds.
`NGENE_FFT_REFERENCE=ON` and `NGENE_SAMPLE_REFERENCE=ON` independently force C;
the library split and external parallelism do not change the numerical contract.
