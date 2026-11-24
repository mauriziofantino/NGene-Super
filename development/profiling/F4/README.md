# F4 — FFT int32 di acquisizione

## Profilo iniziale

Baseline EVO: `5de362f`, Windows x64 Release su Intel Core i7-1355U. Ripresa dal registro F3.
Il banco PCPS usa un worker, 4,096 Msps e IF 38,5 kHz. I timer
separano la permutazione, le butterfly e la normalizzazione all'interno
di ciascuna chiamata. Il controllo di headroom int32 precede la misura.

Valori centrali (elemento centrale superiore) in µs. Il [CSV](stages.csv)
contiene i campioni in secondi, con colonne identificatore, modalità, N,
inversa, permutazione, butterfly e normalizzazione.

| Modalità | N | Direzione | Campioni | Permutazione | Butterfly | Normalizzazione |
|---|---:|---|---:|---:|---:|---:|
| float | 4096 | diretta | 152 | 6,3 | 19,3 | 0 |
| float | 4096 | inversa | 120 | 6,5 | 19,4 | 2,5 |
| int32 | 4096 | diretta | 135 | 13,3 | 301,9 | — |
| int32 | 4096 | inversa | 120 | 12,5 | 210,0 | 14,5 |
| float | 16384 | diretta | 152 | 77,4 | 154,0 | — |
| float | 16384 | inversa | 120 | 81,9 | 177,9 | 20,7 |
| int32 | 16384 | diretta | 135 | 140,1 | 3193,9 | — |
| int32 | 16384 | inversa | 120 | 135,0 | 2769,7 | 131,8 |

La normalizzazione si applica solo all'inversa. La scrittura dei record
avviene dopo l'ultimo timestamp e altera il tempo complessivo del banco.
I campioni includono le trasformate di preparazione e ricerca. Usare
questo profilo per scegliere il primo kernel da valutare: le butterfly
int32. Per qualificare un candidato servono un confronto bit per bit
con il riferimento scalare in entrambe le direzioni e misure end-to-end
senza i timer. Dopo il profilo iniziale restavano da coprire il banco FFT diretto a N=1024 (il banco
PCPS rifiuta il rate GPS sotto 2,048 Msps), fallback C e NEON. I casi Q14
sono stati eseguiti separatamente e non producono record F4.

## Riproduzione

Dalla radice EVO:

```powershell
$build = 'build/msvc-x64-multisdr'
$generated = "$build/generated/pcps-profile"
node development/profiling/F3/generate-profile.cjs $generated
node development/profiling/F4/instrument.cjs $generated
cmake --build $build --config Release --target ngene_pcps_profile
& "$build/Release/ngene_pcps_profile.exe" gps 4096000 4096 38500 0 1 4
& "$build/Release/ngene_pcps_profile.exe" galileo 4096000 16384 38500 1 1 4
node development/profiling/F3/generate-profile.cjs $generated
cmake --build $build --config Release --target ngene_pcps_profile
cmake --build $build --config Release
```

Ripetere per entrambi i segnali con modalità 0 (float) e 1 (int32).
I record F4_STAGE sono su stderr.

Build operativa Windows x64 Release ripristinata dopo la misura.
CTest Release: 30/30 superati.

## Qualifica del candidato SSE2

Il candidato elabora due butterfly alla volta da `NgSearchFftExecuteInt32`.
Il backend operativo x64 usa SSE2 da 2048 punti in su. Il primo stadio
di ogni dimensione e le trasformate fino a 1024 punti usano il codice scalare.
Il limite di ingresso è ±16384 per componente, quindi il modulo complesso
iniziale è minore di 2^15 − 2. Il modulo dei twiddle quantizzati è al più
1 + 2^-30. La rotazione troncata aggiunge un errore di modulo minore di
√2. Prima dell’ultimo stadio il modulo è minore di 2^28, quindi l’errore
dovuto a twiddle e troncamento è minore di 2 a ogni stadio. Per induzione
dopo s stadi il modulo è al più 2^(15+s) − 2. A 16384 punti bastano 29
bit per il modulo. Ogni prodotto e la somma di due prodotti rientrano
in int64.

SSE2 moltiplica i dword pari come interi unsigned. Per due operandi a e b
si sottrae 2^32 × (a se b è negativo, più b se a è negativo), con aritmetica
modulo 2^64. Si ottiene così il prodotto signed esatto. Per un numeratore
Q30 negativo si aggiunge 2^30 − 1 prima dello shift. I 32 bit bassi del
quoziente conservano il troncamento verso zero della divisione C. La
normalizzazione inversa usa la stessa divisione signed per N.

Il banco diretto confronta 80 trasformate su otto vettori e cinque
dimensioni fra 1024 e 16384, dirette e inverse. Comprende zero, impulso,
tono fuori bin, estremi di ingresso, residui piccoli e sequenze
pseudocasuali con seed base 20261121. I **1.015.808** valori I/Q
coincidono bit per bit con il riferimento scalare. [Risultati](fft-verify.csv)

GPS e Galileo chiamano la FFT con buffer interi nativi. Per lo spettro
int32 il wrapper converte i campioni già centrati e finestrati all’ingresso,
poi ripristina la scala all’uscita. Il kernel fra questi due confini opera
in interi. Il test di analisi verifica frequenza e livello di un tono
di riferimento in questa modalità.

### Ripetere i confronti senza timer interni

Compilare `ngene_int32_fft_lab` e `ngene_pcps_lab` in Release con gli
stessi sorgenti del banco, prima con `src/dsp/search.c` della revisione
`5de362f`, poi con il kernel candidato. Copiare i due eseguibili per
variante come `ngene_int32_fft_before.exe` / `ngene_int32_fft_after.exe` e
`ngene_pcps_before.exe` / `ngene_pcps_after.exe` nella cartella di misura,
insieme alle DLL runtime necessarie. I target del banco richiedono una
compilazione esplicita e sono esclusi dalla build operativa.

```powershell
$lab = 'build/f4'
$benchProcess = [Diagnostics.Process]::GetCurrentProcess()
$benchProcess.ProcessorAffinity = 0x0f
node development/profiling/F4/run-fft.cjs verify $lab
node development/profiling/F4/run-fft.cjs timing $lab | Set-Content -Encoding ascii "$lab/fft-timing.csv"
node development/profiling/F4/run-fft.cjs summary "$lab/fft-timing.csv"
node development/profiling/F3/run.cjs verify $lab
node development/profiling/F3/run.cjs timing $lab | Set-Content -Encoding ascii "$lab/pcps-timing.csv"
node development/profiling/F3/run.cjs summary "$lab/pcps-timing.csv"
```

La misura diretta usa cinque riscaldamenti per dimensione e direzione,
poi 24 coppie di processi in ordine A/B e B/A. Ogni processo media più
trasformate del medesimo vettore pseudocasuale. La misura PCPS usa la
fixture del banco F3 a 4,096 Msps, quattro coppie di processi per
segnale, modalità e numero di worker. Il p95 della cancellazione è
descrittivo. Rapporti inferiori a 1 indicano una riduzione del tempo.

Il replay del 24 novembre usa il profilo `replay-int32.ini` e la
cattura CI8 locale con SHA-256
`f559ce60f4dfeed9ce142dd61eb248a456a6d88dc7503a94ec3ff24cc1532b92`.
Entrambi i binari hanno elaborato 13.087.200 coppie in 1600 finestre
con zero gap, drop noti e scarti allo stop. Il ricevitore ha completato
tre ricerche GPS nella baseline e cinque con SSE2, su epoche diverse.
La cattura ha raggiunto EOF prima di completare una ricerca Galileo
in entrambi i casi. Il confronto deterministico delle decisioni GPS e
Galileo è nei dati PCPS. [Replay scalare](replay-before.txt) ·
[Replay SSE2](replay-after.txt)

### Tempi della FFT

Windows x64 Release, Intel Core i7-1355U, MSVC 19.52.37029,
affinità `0x0f`. Le mediane delle FFT da 2048 a 16384 punti calano
del 64–75% nel confronto accoppiato. Tutte le 24 coppie per dimensione
e direzione favoriscono SSE2 in questa fascia. A 1024 punti il
candidato con SSE2 su tutte le dimensioni non ha mostrato un
guadagno ripetibile. La versione integrata usa ancora il percorso
scalare per quelle dimensioni. Nel confronto finale a N=1024 i
rapporti delle singole coppie sono a cavallo di 1 in entrambe le
direzioni (mediane 1,12 e 0,97).

[Riepilogo FFT finale](fft-summary.csv) · [Dati grezzi](fft-timing.csv) ·
[Prima campagna SSE2 anche a N=1024](fft-all-sse2-summary.csv) ·
[Dati della prima campagna](fft-all-sse2-timing.csv) · [Manifest](manifest.csv)

### Tempi della ricerca completa

Il backend finale è stato confrontato quattro volte per configurazione.
A 4096 punti per GPS e 16384 per Galileo, tutte le coppie favoriscono
SSE2 per la ricerca completa. La riduzione percentuale si calcola dal
rapporto mediano delle coppie. Configurazione dei worker, cattura sintetica e byte allocati dal
banco coincidono fra le varianti.

| Segnale | Worker | Mediana prima / dopo (ms) | Riduzione accoppiata | p95 cancellazione prima / dopo (ms) |
|---|---:|---:|---:|---:|
| GPS | 1 | 137,1 / 88,2 | 36% | 8,38 / 3,69 |
| GPS | 4 | 185,8 / 130,5 | 30% | 12,66 / 4,94 |
| Galileo | 1 | 4684,8 / 2214,0 | 53% | 45,00 / 19,02 |
| Galileo | 4 | 4916,5 / 3181,6 | 35% | 56,12 / 47,01 |

Il banco ha confrontato 54 ricerche fra riferimento e versione finale,
in float, int32 e Q14. I record coincidono, compresi i checksum delle
mappe e le decisioni. Gli scarti fra famiglie aritmetiche rilevati da
F1 restano pertinenti. Nella prima campagna a pari fixture, int32 con
SSE2 ha conservato un costo di ricerca maggiore di float e Q14 nelle
combinazioni misurate. Il nuovo percorso riguarda l’opzione int32
già selezionabile.

[Riepilogo PCPS finale](pcps-summary.csv) · [Tempi grezzi compressi](pcps-timing.csv.gz) ·
[Confronto delle ricerche](pcps-verify.csv) · [Campagna a tre modalità](pcps-all-sse2-summary.csv) ·
[Tempi grezzi delle tre modalità](pcps-all-sse2-timing.csv.gz)

La prova riguarda Windows x64. Per NEON resta da implementare e
qualificare un candidato separato su ARM. Il replay descritto sopra
copre la continuità del flusso registrato.

La build operativa Windows x64 Release passa 30/30 CTest, incluso il
tono nello spettro int32 a 2048 punti. Le opzioni FFT reference,
sample reference, sanitizer e tracking benchmark risultano OFF.
[Log CTest](ctest-release.txt)

## Candidato NEON ARM64

Il candidato esegue due butterfly in parallelo. Usa prodotti signed
32 × 32 allargati a 64 bit, tronca la somma Q30 verso zero e conserva
la divisione inversa per N. Il limite numerico dimostrato sopra vale
anche per NEON. Il candidato si abilita con
`-DNGENE_EXPERIMENTAL_INT32_NEON=ON`. Il primo stadio resta scalare.
Il banco esercita NEON anche a 1024 punti per misurare la soglia
di convenienza su ARM.

- Unicorn ha eseguito 60 FFT dirette e inverse a 1024, 2048, 4096,
  8192 e 16384 punti, confrontando 761.856 componenti I/Q con
  un oracolo scalare indipendente. Tutte coincidono. [Log](neon-emulated.txt)
- Clang ha compilato `src/dsp/search.c` con NEON attivo per
  `aarch64-pc-windows-msvc`, `aarch64-linux-gnu` e
  `arm64-apple-macos14`, usando gli header freestanding del banco.
  Il link e i test nativi ARM restano da verificare. La configurazione
  MSBuild locale si ferma al rilevamento della piattaforma ARM64.
- La CI prevede il confronto dei buffer su Windows e macOS ARM64 nativi
  e su Linux ARM64 con QEMU. I job della revisione locale sono da eseguire.

Per ripetere l’emulazione usare
`python tests/test_search_arm_emulated.py --clang <clang>` con Unicorn
e pyelftools. Su un host ARM, compilare `ngene_int32_fft_lab` in Release
con l’opzione prima OFF e poi ON. Conservare gli eseguibili come
`ngene_int32_fft_before` e `ngene_int32_fft_after`, con estensione
`.exe` su Windows. Lanciare `run-fft.cjs verify` e `run-fft.cjs timing`
con la loro directory. Su Windows ARM64 `ngene_pcps_lab` accetta la
stessa opzione per misurare la ricerca completa GPS/Galileo con uno e
quattro worker. La misura su hardware ARM è richiesta per la scelta
del backend operativo.

La build operativa Windows x64 Release con il candidato disattivato
passa 30/30 CTest. Il controllo dello script di verifica Windows
passa anche dopo l’aggiunta del confronto ARM64.
[CTest Windows](ctest-neon-patch-windows.txt) · [Oggetti ARM64](neon-cross-build.txt)
