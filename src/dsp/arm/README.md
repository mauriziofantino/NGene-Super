# AArch64 NEON

`fft_neon.S`: butterfly su interi per la FFT Q14. Due prodotti complessi in parallelo, intermedi signed32. Kernel leaf con registri volatili; simboli ELF/Mach-O. Nessun kernel ARM32 implementato.

`fft_neon.asm` conserva la stessa aritmetica in sintassi ARMASM64 per MSVC/Windows ARM64. Build predisposta e non ancora eseguita con il toolset Microsoft; vedere `docs/WINDOWS.md`. Le primitive di campioni in `src/dsp/sample_ops.c` usano intrinsics NEON e riferimento C.
