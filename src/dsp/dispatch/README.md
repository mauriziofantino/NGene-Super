# Selezione backend

`cmake/TargetArchitecture.cmake` verifica le macro del compilatore target.
`cmake/FftBackend.cmake`, condiviso da ricevitore e benchmark, sceglie sorgente e opzioni
locali: SSE2 assembly x64, SSE2 intrinsics x86-32 e NEON intrinsics ARM64/ARMv7.
Gli algoritmi chiamano l'API matematica comune; non includono i kernel o header SIMD.

Su x86-32 il piano controlla SSE2 una volta e usa C sulle CPU prive dell'ISA. ARM32 NEON
richiede hardware e ABI adeguati; il reference resta selezionabile esplicitamente.
Non esiste dispatch AVX e non vengono abilitate ISA native globalmente.
