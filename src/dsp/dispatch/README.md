# Selezione backend

CMake seleziona il backend FFT secondo le macro del target del compilatore. Nessun dispatch AVX runtime in questa versione. SSE2 per x86-64, NEON per AArch64; riferimento C solo se richiesto esplicitamente.
