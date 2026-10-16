# FFT NGene: analisi e migrazione · 1 ottobre 2026

## Sorgente esaminato

Archivio originale `ngene.tar` (identico a `ngene(2).tar`), SHA256 `c15c72e9e715e6a77ee6ccf694adb3e2d939a355b1f6ad3ff4e82121de8e1acb`. Esaminati integralmente `asmFolder/fft.c` (495 righe), `fft.h` (71) e le macro pertinenti di `simd.h` (520). Le copie originali restano inalterate; il progetto distribuisce il port adattato e questa tracciabilità, non l’intero archivio storico.

`PerformFft` implementa radix-2 DIT, permutazione bit-reversal, ingressi/uscite complessi interleaved signed16, twiddle Q14. I primi due stadi hanno kernel specializzati; gli altri usano SSE2, espandono le coppie e applicano `pmaddwd` ai coefficienti `[Re,-Im,Im,Re]`. Il parametro gain decide quanti stadi riducono il risultato di un bit. La dimensione massima predefinita è 16384.

## Problemi individuati e azione nell’incremento 0.3

| Evidenza nel codice storico | Conseguenza | Intervento |
|---|---|---|
| `paddsw` / `psubsw` precedono `psraw 1` | La saturazione distrugge informazione prima della divisione: 20000+20000 diventa 32767, poi 16383 invece di 20000 | Somma/sottrazione in signed32, shift, poi restringimento signed16 |
| Macro GNU asm separate senza dichiarazione completa dei registri modificati | Il compilatore non dispone del contratto necessario per ottimizzazione e allocazione registri | Funzioni assembly leaf con ABI esplicita in `.S` / `.asm` |
| Salvataggio XMM in `ngeneXmmsBackup` globale | Chiamate concorrenti possono condividere/sovrascrivere lo stesso backup | Nessuno stato globale mutabile; piano immutabile e workspace per chiamata |
| `posix_memalign`, VLA per seno/coseno, tipi/puntatori dipendenti dalla build | Accoppiamento Linux/GNU e requisiti di allineamento non uniformi | Tabelle heap con limiti, tipi espliciti, load/store non allineati SIMD |
| Controllo minimo della dimensione ma nessun controllo `log2Size > table->maxLog2FftSize` | Shift/indici non validi per richieste fuori tabella | Dimensione power-of-two controllata nell’API |
| Errori di libreria chiamano log/chiusura del ricevitore | Kernel matematico non riutilizzabile isolatamente | Codice di ritorno e `TNgError`, niente exit nel DSP |
| Guadagno variabile e saturazione dipendenti dai dati | Difficile calibrare lo spettro e confrontare backend | Contratto iniziale esplicito `/N`, input con headroom, stesso floor per C/SSE2/NEON |

Non viene dichiarata equivalenza bit per bit con tutte le modalità gain del vecchio `PerformFft`: correggere il punto di saturazione cambia proprio i casi patologici. Prima di inserire questo kernel in acquisizione/tracking occorrerà verificare l’intera catena NGene: gain, convenzione del prodotto complesso, eventuale trasformata inversa tramite coniugazione, soglie e quantizzazione delle repliche.

## Contratto implementato

- API C `NgFftCreate`, `NgFftExecute`, `NgFftReference`, `NgFftDestroy`.
- FFT complessa forward `exp(-j 2πkn/N)`, ordine naturale, output normalizzato per N, dimensioni 8..16384 power-of-two. La GUI espone 256..16384.
- Dati signed16 interleaved; componenti di ingresso in [-16384,16384], verificate. Piano condivisibile, ingresso/uscita disgiunti, almeno allineamento naturale C int16. Nessuna allocazione o virgola mobile durante `execute`.
- Q14 twiddles follow NGene; complex signed32 products use SSE2 `pmaddwd` or NEON `smull`/`addp`, followed by a 14-bit shift. NEON, the scalar oracle and SSE2 tails divide signed32 sums by two with arithmetic-floor rounding before saturating narrowing. The updated main SSE2 loop computes four independent complex butterflies with equivalent overflow-safe signed16 floor averaging; the first two stages are fused. See the October 2026 update below for the headroom proof and measurements.
- C gestisce piano, bit-reversal e sequenza degli stadi. Le butterfly di produzione sono assembly. Il riferimento C è un oracolo di verifica e una build esplicitamente selezionabile, non un fallback silenzioso.
- Il piano ha solo tabelle immutabili; due thread possono eseguire FFT simultanee con array separati. Il worker di osservazione usa un thread solo per limitare l’interferenza con il ricevitore.

L’headroom tiene il modulo iniziale sotto circa 23171 e i prodotti Q14 ampiamente entro signed32. Le potenze e le medie usano uint64. `floor` è definito anche per valori negativi e replicato nelle istruzioni aritmetiche dei due backend. Il Q14 resta una precisione finita: il fondo dello spettro può includere errore di arrotondamento/quantizzazione, soprattutto con FFT lunghe e segnali deboli. Non confonderlo con una misura calibrata del rumore RF.

## Target e assembler

| Target | Implementazione | Verifica locale |
|---|---|---|
| Linux x86-64 GCC/Clang | `fft_sse2.S`, ABI System V | FFT complete eseguite e confrontate con riferimento/DFT |
| Windows x86-64 GNU/Clang | `fft_sse2.S`, ABI Win64 | Programma completo cross-compilato e collegato |
| Windows x86-64 MSVC | `fft_sse2.asm`, MASM | Sorgente/CI predisposti; MSVC non eseguito qui |
| Linux AArch64 | `fft_neon.S`, AAPCS64 | Programma completo cross-compilato; butterfly emulata a livello istruzioni |
| macOS x86-64 / Apple Silicon | `.S` SSE2 / NEON, simboli Mach-O | Entrambi gli oggetti assembly compilati; applicazione/SDK Mac non disponibili |
| Windows ARM64 | `.S` con toolchain Clang/GNU appropriata | Non verificato in questa consegna; MSVC ARMASM non implementato |
| x86/ARM a 32 bit | Build `NGENE_FFT_REFERENCE=ON` | Nessun kernel assembly 32 bit portato in questo incremento |

CMake riconosce il **target**, non l’host. Nessun `-march=native` o AVX obbligatorio. SSE2 è baseline x86-64; NEON è il backend AArch64. Non c’è dispatch runtime SSE2/AVX in questo incremento. Windows x64 preserva i registri non volatili evitando di usarli: il kernel tocca soltanto GPR volatili e XMM0..5. Il kernel ARM usa x0..3 e v0..4.

Riferimenti ABI: [Microsoft x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention), [Arm AAPCS64](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst). Le verifiche numeriche sono riproducibili con i test C inclusi, non dedotte soltanto dai manuali delle istruzioni.

## Misure locali

GCC 13.3, Linux x86-64, Release `-O3`, 300 chiamate per misura, piano già inizializzato; inclusi controlli input, permutazione e stadi, esclusi finestratura, potenza, rete e I/O. Due esecuzioni nello stesso ambiente hanno dato:

| N | Assembly SSE2 | Riferimento C scalare | Rapporto C / assembly |
|---|---:|---:|---:|
| 1024 | 5,8–6,4 µs | 17,9–19,8 µs | 2,8–3,4 |
| 4096 | 26,9–29,0 µs | 85,5–93,6 µs | circa 3,2 |
| 16384 | 139,9–150,8 µs | 469,8–476,4 µs | 3,1–3,4 |

Sono microbenchmark del nuovo kernel, **non un confronto con il vecchio assembly NGene**, non misure dell’intero ricevitore e non garanzie su altre CPU. Il riferimento C usa divisione floor portabile per garantire la semantica; non rappresenta ogni possibile FFT C ottimizzata. Riprodurre con `ngene_fft_tests --benchmark` in Release.

## Miglioramenti successivi sensati

Misurare prima i costi su hardware reale. Poi valutare fusione/specializzazione dei primi stadi, radix-4, batching di FFT indipendenti, AVX2 con dispatch e block-floating scaling con esponente restituito. Quest’ultimo può recuperare dinamica sui segnali deboli ma cambia il contratto numerico; non è inserito implicitamente nel kernel attuale. Manteniamo FFT interne e su interi, senza FFTW.


Aggiornamento 0.4: le butterfly qui descritte restano inalterate. È aggiunta la sintassi Microsoft equivalente `src/dsp/arm/fft_neon.asm`, selezionata da CMake ASM_MARMASM per Windows ARM64. La verifica nativa di questo assembler è ancora da eseguire. La correzione waterfall riguarda la history GUI e il trasporto diagnostico, non l’algoritmo FFT.

## Original NGene comparison and SSE2 update — 16 October 2026

The reproducible [benchmark](../benchmarks/fft/README.md) now measures EVO against the untouched
original NGene FFT supplied separately. It records source hashes, validates the compiled baseline,
uses the same `/N` gain, alternates backend order and reports repeated batch statistics. The
original is still faster on the tested Intel i7-1355U; this update makes no claim that EVO is the
fastest available FFT. EVO latency improves by about 35–38% at 1024, 4096 and 16384 points versus
the previous EVO implementation. The raw before/after CSV files retain host, compiler and flags.

The x86-64 implementation now fuses its first two stages, executes four butterflies per main
iteration, validates input headroom with SSE2 and copies complete complex pairs during bit reversal.
The four-butterfly path uses `S=(U&T)+((U^T)>>1)` and `D=S-T`, which are exactly
`floor((U+T)/2)` and `floor((U-T)/2)` for signed16 values without saturating the sum before scaling.
The scalar and NEON paths retain their signed32 sum/scaling implementation; all outputs are unchanged.

Packing Q14 products before averaging is lossless in the public headroom contract. Initially the
complex norm is at most `16384*sqrt(2) < 23171`. A quantized Q14 rotation has norm at most
`1+1/16384`; component floor rounding adds norm less than `sqrt(2)`. If `M_s` bounds a complex
norm after stage `s`, product rounding followed by per-stage floor averaging gives
`M_s <= (1+1/32768)*M_(s-1) + 3*sqrt(2)/2`. Induction yields
`M_s < 23171+4*s <= 23227` through all 14 stages. A rotated lower operand consequently has
norm less than `(1+1/16384)*23227+sqrt(2) < 23230`, safely inside signed16. SSE2 `packssdw`
therefore does not clip these products, and the average/difference identity is bit-exact against
the signed32 oracle. This bound depends on plan-generated twiddles and the enforced input range;
it is not permission to feed arbitrary full-range int16 samples to the FFT.

The updated tests additionally exercise full-scale independent I/Q signs, odd values and rotating
corners that drive valid intermediate components above 16384. The public API, `/N` gain, rounding,
immutable-plan concurrency and input/output ownership remain unchanged. Original NGene still has
different high-level clipping behavior and unsafe shared register backup, so these timing results
do not establish numerical interchangeability in acquisition or tracking.
