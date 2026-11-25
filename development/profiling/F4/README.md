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

## Incremento AVX2 int32/Q30

Il candidato x86-64 processa quattro butterfly per iterazione con prodotti
signed allargati a 64 bit. Conserva il troncamento Q30 verso zero e la
divisione inversa per N. Compilare due build Release con
`-DNGENE_EXPERIMENTAL_INT32_AVX2=OFF` e `ON`. Con l'opzione attiva, il piano
controlla la disponibilità di AVX2 e dello stato YMM prima di selezionare il
candidato. Per le larghezze di stadio inferiori a 8 usa il percorso esistente.

Ripetere i comandi del banco sopra con le due build. Il banco diretto ora
copre tutte le potenze di due da 8 a 16384. Su Windows x64 con Core i7-1355U,
MSVC 19.52.37029 e affinità `0x0f`, 192 trasformate
dirette e inverse danno 1.048.320 componenti I/Q identiche. Nella fascia
2048–16384, il rapporto mediano accoppiato AVX2/baseline è 0,44–0,54
in 24 coppie per dimensione e direzione. Sotto 2048 la baseline del ricevitore
usa C scalare. Per 8 e 16 punti la misura non mostra un vantaggio.
Il comando `ngene_int32_fft_lab --plans` misura la creazione dei piani
separatamente, come media di 32 creazioni per lunghezza. Dati locali
`plans-OFF.csv` e `plans-ON.csv`.

Il banco PCPS conferma la parità dei 27 casi per variante, in tre modalità
aritmetiche. A 4,096 Msps e IF 38,5 kHz le misure int32 della ricerca completa
sono le seguenti, in rapporto mediano accoppiato AVX2/SSE2.

| Segnale | N | Worker | Rapporto |
| --- | ---: | ---: | ---: |
| GPS | 4096 | 1 | 0,849 |
| GPS | 4096 | 4 | 0,909 |
| Galileo | 16384 | 1 | 0,760 |
| Galileo | 16384 | 4 | 0,765 |

Nel caso GPS a quattro worker l'intervallo dei rapporti delle quattro
coppie è 0,779–1,555, quindi il beneficio in quella configurazione resta
da confermare. Dati locali in `build/f4-avx2-comparison/`.

### PSD comune a spettro e waterfall

Compilare il target `ngene_int32_psd_lab` nelle stesse due build ed eseguirlo
in quattro coppie con ordine alternato. Il banco invia burst CI8 al worker
PSD reale. Usa finestra Hann, quattro medie Welch e overlap del 50%.
Per tutte le sette potenze di due da 256 a 16384 coincidono checksum, primo
campione, frequenza e livello del picco su ogni frame verificato.

| N | Rapporto mediano accoppiato del tempo worker | Rapporto della latenza cattura–pubblicazione |
| ---: | ---: | ---: |
| 256 | 0,93 | 1,35 |
| 512 | 0,85 | 0,86 |
| 1024 | 0,70 | 0,71 |
| 2048 | 0,72 | 0,70 |
| 4096 | 0,75 | 0,76 |
| 8192 | 0,53 | 0,53 |
| 16384 | 0,99 | 0,96 |

I tempi del worker sono durate monotone. Le misure riguardano il produttore
comune delle tre viste. Restano da misurare il costo CPU con le viste attive
e la ripetibilità agli estremi della matrice. CSV locali `psd-*.csv` in
`build/f4-avx2-comparison/`. Il primo frame per dimensione viene scartato
dalla misura a piano riusato.

Il job macOS ARM64 esegue il banco diretto C/NEON sulla stessa macchina e
pubblica i CSV accoppiati nella prossima esecuzione CI della revisione.
La qualifica nativa ARM e le build native Linux e macOS restano da eseguire
per questo incremento.

Le due build Windows x64 per replay da file hanno superato 25/25 CTest.
La baseline include la ripetizione di tre test di build e scripting
con directory temporanea locale, Ninja e policy PowerShell di processo.
La traccia F6 è disattivata in entrambe le build di verifica.

## Incremento AVX2 Q14

Il candidato elabora otto butterfly per iterazione. Riusa la permutazione,
gli stadi fino a 8 punti e i blocchi da 512 campioni del backend SSE2.
Mantiene l'arrotondamento verso meno infinito, il dimezzamento per stadio
e la saturazione del contratto Q14. L'opzione
`-DNGENE_EXPERIMENTAL_Q14_AVX2=ON` abilita il candidato su x86-64 con
verifica AVX2 a runtime. Può essere valutata indipendentemente da int32.

Su Core i7-1355U e Windows x64 Release con MSVC 19.52, il contratto FFT
passa su ogni potenza di due da 8 a 16384. Il banco con SSE2 e AVX2 nello
stesso processo ha verificato gli stessi buffer contro C e ha alternato
l'ordine per 21 round. Tra 128 e 8192 punti il tempo mediano della FFT
completa cala di circa il 7–13%. A 16 e 32 punti sale rispettivamente
da 0,033 a 0,037 µs e da 0,053 a 0,057 µs.

Il worker PSD con `ngene_int32_psd_lab --q14` ha pubblicato le stesse
posizioni campione, checksum e picchi in quattro coppie di replay sintetici
da 256 a 16384 punti. Il tempo mediano accoppiato del worker peggiora del
42% a 256 e del 26% a 2048 punti, varia di circa il −1% a 4096 e
scende dell'8% a 16384. Il miglioramento del kernel varia quindi con
la dimensione e con il resto del lavoro del worker.

Le nove fixture Q14 PCPS coincidono in checksum e decisioni tra le
varianti. Nelle quattro coppie temporali a 4,096 Msps, i rapporti mediani
AVX2/SSE2 del tempo di ricerca completa sono i seguenti.

| Segnale | Worker | Rapporto mediano | Min–max |
| --- | ---: | ---: | ---: |
| GPS | 1 | 0,922 | 0,732–1,713 |
| GPS | 4 | 0,966 | 0,936–0,988 |
| Galileo | 1 | 0,945 | 0,882–1,149 |
| Galileo | 4 | 0,994 | 0,670–1,264 |

Solo GPS a quattro worker migliora in tutte le coppie. La PSD peggiora
in alcune dimensioni e il guadagno PCPS dipende dal profilo. Il candidato
resta disponibile come opzione sperimentale. Le prove native Linux,
macOS e ARM rimangono aperte.

La build Windows x64 con Q14 AVX2 ha superato 25/25 CTest con la traccia
F6 disattivata. Il banco int32 ricompilato dopo il riuso del controllo
AVX2 restituisce lo stesso buffer della baseline SSE2.

Per riprodurre, configurare `benchmarks/fft` con
`NGENE_FFT_BASELINE_ROOT` puntato all'albero SSE2 e abilitare
`NGENE_EXPERIMENTAL_Q14_AVX2` nella variante corrente. Per PCPS riusare la
procedura F3 con `verify <cartella> 2` e `timing <cartella> gps 2` o
`timing <cartella> galileo 2`. Per la PSD usare `--q14` in entrambe
le build. I dati grezzi locali sono in `build/f4-q14-results/`.
SHA-256 del kernel AVX2 misurato
`306b95ae86e9c3873cb8cdf8c52d2be6ee218ac47558219c10f617644a5c2381`.

[FFT diretta e hash dei sorgenti](q14-avx2-fft.csv) ·
[Riepilogo PSD](q14-avx2-psd-summary.csv) ·
[Parità PCPS](q14-avx2-pcps-verify.csv) ·
[Tempi PCPS](q14-avx2-pcps-summary.csv)

## Verifica della disponibilità AVX-512 e SVE/SVE2 · 2026-11-24

Il probe offline, compilato per l'ISA di base, controlla CPUID e lo stato ZMM
abilitato dal sistema operativo su Windows x64. Su Linux AArch64 legge
HWCAP/HWCAP2 e la lunghezza vettoriale SVE del thread.

```text
cl /W4 /WX /O2 /Fe:build/f4/isa_probe.exe development/profiling/F4/isa_probe.c
cc -O2 -Wall -Wextra -o build/f4/isa_probe development/profiling/F4/isa_probe.c
```

Con MSVC 19.52 su Intel Core i7-1355U il probe restituisce
`avx512f=0 avx512bw=0 os_zmm=0`. Le build Windows F4 SSE2 e AVX2 esistenti
producono entrambe SHA-256
`a170a57a2e75735ef77a10af0f613be7330f2cfc377c039095c064610ab47ca2`
sugli output del banco int32. La verifica Linux AArch64 del probe e i confronti
dei kernel AVX-512 e SVE/SVE2 restano aperti. Nessun tempo prestazionale nuovo.

## F4 · Inventario degli ingressi interi · 2026-11-24

I e Q sono interlacciati, con I per primo. Il riferimento di decodifica è
`NgIqComponent` in `src/stream/format.c`. Le parole a 16 bit sono little endian.

| Ingresso | Estremi e zero | Conversione verso CI16 |
|---|---|---|
| CI8, incluso il formato storico zero | -128, 0, 127 in unità ADC | Moltiplicazione esatta per 256 |
| CU8 | 0, 128, 255 sul filo. Sottrarre 128 per ottenere -128, 0, 127 | Centrare e moltiplicare per 256 |
| SC16 Q11 | -2048, 0, 2047 in un contenitore signed a 16 bit | Moltiplicazione esatta per 16. Il convertitore rifiuta campioni fuori range |
| CI16 | -32768, 0, 32767. La precisione effettiva dipende dalla sorgente | Identità |

- Acquisizione PCPS. Il callback accetta CI8, CI16 e SC16 Q11. CU8 entra
  attraverso la conversione dello stream comune. La copia della cattura
  converte interi in float esattamente in unità del buffer comune. Il
  profilo di ricerca determina la successiva aritmetica FFT e produce la
  mappa di potenza usata nella ricerca.
- Tracking. Il percorso byte configurabile richiede CI8. Il percorso wide
  legge i livelli signed tramite NgIqComponent e alimenta mixer e somme
  prefisse da cui ricava gli accumuli I/Q dei tap in double.
- Spettro e waterfall. Il worker usa fattori di ingresso 128, 8 e 1 per
  CI8, SC16 Q11 e CI16 rispettivamente, prima della finestra e degli
  adattamenti specifici del profilo FFT. Le potenze dipendono anche dalla
  normalizzazione applicata dal worker. Il confronto va eseguito alla
  stessa scala di uscita, includendo l'eventuale ampliamento dello stream.

Su Windows, build Release esistente f4-q14-ON, 4/4 CTest superati.
Eseguiti tracking_contract, sample_math_native, analysis_worker_contract e
common_frontend_stream. Quest'ultimo esercita conversioni e limiti dei
formati. La verifica copre i percorsi esistenti. Il riferimento FFT scalare
CI8 con twiddle quantizzati è il successivo incremento del banco.

## F4 · Banco FFT scalare CI8

Compilare `ci8_fft_lab.c` con MSVC 19.52, `/O2 /W4 /WX`. Il banco
stampa il CSV su stdout. Il [risultato](ci8-fft.csv) contiene 576 casi
da 8 a 16384 punti, nelle due direzioni, con impulso, valori estremi,
tono fuori bin e rumore a seed fisso.

Il riferimento double usa lo stesso schema radix-2 con twiddle double.
L'errore per componente è espresso in unità ADC dopo divisione per N,
ripristinando prima gli shift e la scala d'ingresso. I twiddle a 7 e 15
bit frazionari usano un contenitore che rappresenta entrambi gli
estremi ±1. I prodotti e le somme usano int64. Nel percorso wide la
magnitudine iniziale è inferiore a 47000. Ogni stadio cresce al più
di un fattore 2,01 con un'unità additiva di arrotondamento. Dopo 14
stadi la magnitudine resta sotto 10^9 e i prodotti sotto 2^47.

| Profilo | Errore massimo Q7 | Errore massimo Q15 | Clipping per precisione |
|---|---:|---:|---:|
| Wide con 8 bit frazionari aggiunti all'ingresso | 0,371412 | 0,001521 | 0 |
| Buffer limitato a INT8 con shift fisso di un bit per stadio | 3,002887 | 3,784791 | 50 |
| Buffer limitato a INT8 con esponente comune adattivo | 3,002887 | 3,784791 | 0 |

Il profilo wide Q15 è il candidato di maggiore fedeltà tra quelli
provati. Il riscalamento adattivo evita i clipping osservati nel banco
con shift fisso. Gli intermedi limitati a INT8 richiedono ancora prove
su sensibilità e mappe PCPS. Il banco qualifica il confronto numerico
offline. Tempi, SIMD e replay operativo restano da misurare.

### Tempi dei profili scalari CI8

Windows x64, i7-1355U, MSVC 19.52 con /O2 /W4 /WX. Nove esecuzioni
del banco, con 96 trasformate per configurazione in ciascuna esecuzione.
Il cronometro include butterfly e riscalamento. Esclude tabelle,
permutazione iniziale, riferimento double e scrittura dei risultati.
Il CSV numerico delle 576 trasformate è identico byte per byte alla
serie precedente.

| Coefficienti | Wide | INT8 shift fisso | INT8 shift adattivo |
|---|---:|---:|---:|
| Q7 | 16,31 ms | 40,79 ms | 61,38 ms |
| Q15 | 15,79 ms | 39,61 ms | 63,30 ms |

I valori sono mediane delle nove somme per configurazione.
In questo banco i profili stretti aggiungono passaggi per riscalare
o determinare lo shift e costano circa 2,5–4 volte il wide.
Il banco usa workspace int64 per ogni candidato. I tempi descrivono
l'aritmetica scalare e non quantificano il beneficio di un workspace
compatto o di SIMD. I dati grezzi sono in [ci8-fft-timing.csv](ci8-fft-timing.csv).
Restano aperte le prove SIMD e la sensibilità delle mappe PCPS.

### Candidato AVX2 wide Q15

Compilare ci8_fft_avx2.c con /arch:AVX2 e il banco con
/DCI8_LAB_AVX2 /D_CRT_SECURE_NO_WARNINGS /Isrc, poi collegare gli oggetti.
Passare il percorso del dump binario in entrambe le prove e aggiungere
avx2 come secondo argomento per abilitare il candidato. La verifica ISA
riusa il controllo CPU e OS del progetto. Lo stadio largo 2 resta scalare.

Windows x64 con MSVC 19.52. Dump integrali identici sui 576 casi,
di cui 96 esercitano il candidato. SHA-256
512ee02a6931ddd679afde7c23fd2a2ea304ef5939355e6310cbfbff8b1c1411.
In nove coppie alternate la mediana delle 96 trasformate wide Q15
passa da 27,57 ms scalare a 13,99 ms AVX2. Le misure riguardano lo
stesso binario e includono gli stadi scalari. [Dati](ci8-fft-avx2-timing.csv).

La prova resta offline. Restano aperti workspace compatto INT8,
mappe e sensibilità PCPS, e qualifica nativa delle altre ISA.

### Workspace INT8 con shift fisso

Nel banco usare il percorso del dump seguito da 'compact'. Il percorso
compatto applica arrotondamento, shift fisso e saturazione prima di scrivere
ogni butterfly nel buffer a un byte per componente. I prodotti restano
allargati. A N=16384 il buffer attivo per gli stadi passa da 256 a 32 KiB.
Il banco conserva il buffer wide per preparazione, confronto e dump.

Su Windows x64, MSVC 19.52 /O2 /W4 /WX, gli output binari e i CSV
di errore coincidono con il banco scalare, inclusi i conteggi di clipping.
192 dei 576 casi esercitano il candidato. In nove coppie alternate
la mediana delle 96 trasformate passa da 34,16 a 19,28 ms per Q7
e da 33,96 a 19,35 ms per Q15. Il cronometro misura solo gli stadi.
[Dati grezzi](ci8-fft-compact-timing.csv).

Restano da verificare il workspace adattivo, il SIMD per il profilo
compatto e la sensibilità delle mappe PCPS.

### Workspace INT8 con shift adattivo

Ogni stadio esegue una prima passata in sola lettura con prodotti,
rotazioni e somme in int64. Gli estremi signed determinano il minimo
shift comune. L'arrotondamento del riferimento è monotono, quindi basta
controllare il minimo contro −128 e il massimo contro +127 dopo
l'arrotondamento. La seconda passata ricalcola ogni butterfly e scrive
in INT8. Le butterfly dello stesso stadio operano su coppie disgiunte.
La somma degli shift di stadio è l'esponente usato da `ldexp` per
ricostruire la scala.

Il buffer attivo vale 2N byte per entrambi i profili compatti e 16N
per wide e per il riferimento adattivo. A N=16384 sono 32 contro
256 KiB, una riduzione dell'87,5%. L'adattivo compatto usa quattro
intermedi int64 per butterfly, con spazio temporaneo indipendente da N.
Il processo del banco alloca anche le tabelle e i buffer comuni per
preparazione, oracolo double e dump. I valori sopra misurano i buffer
degli stadi.

**Parità su Windows x64.** I quattro vettori storici (impulso, estremi,
tono fuori bin e rumore a seed fisso), le 12 potenze di due da 8 a
16384, entrambe le direzioni e i twiddle Q7/Q15 danno 192 trasformate
per profilo. Il confronto byte per byte passa per le 1.048.320
componenti I/Q del candidato adattivo e per ciascun esponente cumulato
e conteggio di saturazione. Passa anche per lo shift fisso compatto.
Ogni dump contiene tutti i 576 casi del banco e ha SHA-256
`512ee02a6931ddd679afde7c23fd2a2ea304ef5939355e6310cbfbff8b1c1411`.
I CSV completi, che includono esponenti e saturazioni, coincidono
byte per byte e hanno SHA-256
`b0a7edbba6320ed36930653f194ce07b81726baa1344fadf64999c588e5c6455`.

**Confronto sullo stesso binario.** Windows x64, Core i7-1355U e MSVC
Release `/O2`. Dopo un round di riscaldamento, 12 round alternano
l'ordine per ciascun vettore. Ogni blocco di sei round usa tutte le
permutazioni. I tempi sono le mediane delle somme per 96 trasformate
per profilo e precisione. Il cronometro include butterfly, scansione
degli estremi e riscalamento, secondo il profilo. Tabelle, permutazione,
copie del banco, oracolo double e scrittura sono fuori misura.
Gli errori rispetto al double sono espressi in unità ADC dopo
ripristino della scala e divisione per N. Il valore RMS aggrega tutte
le componenti, pesando ciascun caso per N.

| Twiddle | Profilo | Stadi (ms) | Errore max | Errore RMS | Saturazioni |
|---|---|---:|---:|---:|---:|
| Q7 | Wide | 29,110 | 0,371412 | 0,00579150 | 0 |
| Q7 | INT8 fisso | 71,852 | 3,002887 | 0,705606 | 50 |
| Q7 | INT8 adattivo | 117,279 | 3,002887 | 0,378585 | 0 |
| Q15 | Wide | 27,488 | 0,001521 | 0,00002243 | 0 |
| Q15 | INT8 fisso | 73,139 | 3,784791 | 0,704281 | 50 |
| Q15 | INT8 adattivo | 119,078 | 3,784791 | 0,380392 | 0 |

A parità di buffer con INT8 fisso, l'adattivo elimina le saturazioni
osservate e riduce l'RMS aggregato del 46% circa in entrambe le
precisioni. L'errore massimo coincide. La doppia passata porta il tempo
mediano a circa 1,63 volte il fisso e 4,0–4,3 volte wide. Wide Q15
conserva la maggiore fedeltà nella matrice misurata. I risultati
riguardano il banco scalare offline. Restano aperte le verifiche native
Linux e macOS e la sensibilità PCPS.
[Tempi grezzi](ci8-fft-adaptive-timing.csv).

Dalla radice EVO, in un prompt MSVC x64 con PowerShell:

```powershell
$d = 'build\f4-ci8-adaptive'
New-Item -Force -ItemType Directory $d | Out-Null
cl /nologo /std:c11 /O2 /W4 /WX /D_CRT_SECURE_NO_WARNINGS /Fo"$d/" /Fe"$d/ci8_fft_lab.exe" development/profiling/F4/ci8_fft_lab.c
$exe = "$d\ci8_fft_lab.exe"
cmd /c "$exe $d/reference.bin > $d/reference.csv 2> $d/reference.log"
foreach ($profile in 'compact', 'compact-adaptive') {
    cmd /c "$exe $d/$profile.bin $profile > $d/$profile.csv 2> $d/$profile.log"
    if ($LASTEXITCODE) { throw "Esecuzione fallita: $profile" }
    foreach ($ext in 'bin', 'csv') {
        cmd /c "fc /b $d\reference.$ext $d\$profile.$ext"
        if ($LASTEXITCODE) { throw "Parità fallita: $profile.$ext" }
    }
}
& $exe --timing | Set-Content -Encoding ascii "$d/timing.csv"
```

Il CSV dei tempi usa i modi 0 (wide), 1 (INT8 fisso) e 2 (INT8 adattivo).
SHA-256 dell'eseguibile misurato
`6719c74d0948652034630899e64219d4f472b2d357494d822fe86665b4ff8714`.

### Effetto sulle mappe e sulle decisioni PCPS

Il banco F4 riusa la fixture PRN 7 e le strategie di acquisizione EVO.
Usa CI8 sintetico a 4,096 Msps e IF zero, con 4096 punti e 16 bin Doppler
per GPS L1 C/A e 16384 punti e 116 bin per Galileo E1-B. Ogni cella
somma otto periodi coerenti. Si confrontano cinque ampiezze del segnale
(0, 0,25, 0,5, 1 e 12 unità ADC prima dell'arrotondamento) con due seed
di rumore uniforme, per 20 fixture e 60 ricerche. L'ampiezza zero
conserva rumore e offset DC della fixture. L'hash verifica che i tre
profili ricevano la stessa cattura.

Il generatore sostituisce tre chiamate nella copia locale di
`search.c`: FFT della replica, FFT del dato dopo il mixer e inversa
del prodotto spettrale. Tutti e tre i profili usano twiddle Q15 e
lo stesso adattatore di ingresso INT8, con fattore comune potenza di due
scelto sul massimo assoluto. Wide aggiunge otto bit frazionari prima
degli stadi. All'uscita si ripristinano scala d'ingresso, esponente
cumulato e normalizzazione inversa prima del ritorno in INT32.
Il wide è il riferimento F4 sulla stessa rappresentazione d'ingresso.
Il prodotto spettrale, il criterio gamma e la probabilità di falso
allarme configurata, i raffinamenti e la conferma 3-su-4 restano quelli
di EVO. La soglia numerica viene ricalcolata, come nel ricevitore, dal
fondo a Doppler errato del profilo in prova.

Il runner confronta tutte le celle coarse in scala di potenza comune.
L'errore di mappa è la norma L2 della differenza divisa per la norma
L2 della mappa wide della stessa fixture. Il picco si confronta con
la soglia del profilo (>1 per il passaggio del coarse), e la potenza
del fondo è la media per cella del bin a Doppler errato, divisa per
otto. Il CSV include anche posizione coarse, stima finale, conferma
e conteggio delle prove riuscite.

Windows x64 Release, MSVC 19.52, Core i7-1355U. Ogni cella della
tabella conta le conferme sui due seed.

| Segnale | Ampiezza ADC | Wide Q15 | INT8 fisso Q15 | INT8 adattivo Q15 |
|---|---:|---:|---:|---:|
| GPS | 0 | 0 | 0 | 0 |
| GPS | 0,25 | 0 | 0 | 0 |
| GPS | 0,5 | 2 | 0 | 2 |
| GPS | 1 | 2 | 0 | 2 |
| GPS | 12 | 2 | 2 | 2 |
| Galileo | 0 | 0 | 0 | 0 |
| Galileo | 0,25 | 2 | 0 | 2 |
| Galileo | 0,5 | 2 | 0 | 2 |
| Galileo | 1 | 2 | 2 | 2 |
| Galileo | 12 | 2 | 2 | 2 |

L'adattivo concorda con wide nelle 20 decisioni coarse e nelle 20
decisioni finali. In tutti i 14 casi confermati coincidono Doppler
e fase codice sia al coarse sia al termine della ricerca. Il fisso
perde otto conferme del wide e si ferma in tutti e otto i casi al
coarse. Sul rumore senza segnale tutti i profili scartano tutte le
quattro fixture. Quattro prove di rumore consentono di riportare
il conteggio osservato, mentre una stima del tasso di falsi allarmi
richiede una campagna più ampia.

| Segnale | Profilo | Fondo / wide | Picco / wide | Errore relativo della mappa |
|---|---|---:|---:|---:|
| GPS | INT8 adattivo | 1,47–1,54 | 1,38–1,54 | 0,40–0,53 |
| GPS | INT8 fisso | 671–886 | 38–633 | 121–828 |
| Galileo | INT8 adattivo | 1,68–1,77 | 1,51–1,78 | 0,57–0,75 |
| Galileo | INT8 fisso | 11892–14125 | 194–11022 | 2584–13158 |

Vicino alla soglia, a 0,5 ADC per GPS, i rapporti picco/soglia
passano da 1,186–1,426 wide a 1,163–1,408 adattivo. A 0,25 ADC per
Galileo passano da 1,248–1,924 a 1,207–1,830. L'adattivo conserva
un margine positivo in questi casi, mentre l'aumento del fondo
modifica sensibilmente le mappe. Il fisso amplifica il fondo molto
più del picco nei casi deboli, riducendo il margine fino a far
scendere otto fixture sotto soglia. La matrice qualifica questo
confronto sintetico a una sola frequenza di campionamento, uno
scostamento Doppler e un PRN per segnale. Restano aperti i casi
più prossimi alla soglia, più semi di rumore, replay RF e la
verifica nativa su Linux e macOS.

[Risultati per fixture](ci8-pcps.csv). Il CSV è stato controllato
contro la dimensione delle mappe complete e contro il rapporto
picco/soglia prodotto dalla strategia. SHA-256 del binario di prova
`f3796c04ce60bb14c78856d5d40ac249cf59cc876c55c1f397de281b60410b4e`.
Dopo il riuso degli stadi, il dump e il CSV del banco FFT adattivo
coincidono ancora byte per byte con il riferimento. I cinque CTest
Release di acquisizione e FFT passano su Windows.

Per ripetere dalla radice EVO, con una build Ninja Release Windows x64
già configurata e il prompt MSVC attivo:

```powershell
$b = 'build/f4-q14-ON'
$generated = "$b/generated/pcps-profile"
node development/profiling/F4/generate-ci8-pcps.cjs $generated
cmake --build $b --target ngene_pcps_profile
node development/profiling/F4/run-ci8-pcps.cjs "$b/ngene_pcps_profile.exe" $b |
    Set-Content -Encoding ascii "$b/ci8-pcps.csv"
if ($LASTEXITCODE) { throw 'Campagna PCPS fallita' }
Remove-Item -LiteralPath "$generated/dsp/search.c", "$generated/profile_generated.h"
cmake --build $b --target ngene ngene_pcps_lab
ctest --test-dir $b -C Release -R 'acquisition|fft' --output-on-failure
```

Usare una cartella build dedicata o libera da altre copie generate
del banco PCPS. Il target ordinario e i sorgenti operativi usano
i kernel del ricevitore.

### Riduzione del costo dell'INT8 adattivo

Il candidato conserva un buffer attivo da 2N byte. Nelle butterfly
compatte usa intermedi INT32. Con componenti di modulo al più 128
e twiddle di modulo al più 32768 per componente, la somma di due
prodotti è in valore assoluto al più 8.388.608. Dopo l'arrotondamento
Q7 o Q15, ciascuna componente della rotazione è in `[-256, 256]`
e ogni uscita grezza è in `[-384, 384]`.

L'arrotondamento usa valore assoluto, somma di mezzo divisore,
shift logico e ripristino del segno. Preserva la regola di
arrotondamento al più vicino con i casi a metà lontano da zero.
Lo shift è zero per tutte le uscite in `[-128, 127]` e uno se
basta estendere l'intervallo a `[-256, 254]`. Fuori da quest'ultimo
intervallo lo stadio richiede due bit, sempre sufficienti per il
limite delle butterfly. In quel caso la prima passata termina
appena incontra il valore decisivo. La seconda passata ricalcola le
butterfly e scrive il risultato, già garantito in range, senza
divisioni intere né saturazione per componente.

Con MSVC 19.52 `/O2 /W4 /WX` su Windows x64, Core i7-1355U,
i dump e i CSV delle due varianti compatte coincidono byte per byte
con il riferimento adattivo int64. Il confronto esercita le 192
trasformate adattive e verifica 1.048.320 componenti, esponenti e
conteggi di saturazione. Una verifica locale aggiuntiva ha confrontato
la nuova routine di arrotondamento con la divisione su tutti i valori
interi negli intervalli dei numeratori Q7 e Q15 (16.842.754 confronti)
e ha confrontato i due kernel su tutte le 131.072 coppie di ingressi
reali INT8 allo stadio di larghezza 2, contando entrambe le precisioni.

Il microbenchmark usa lo stesso binario, un round di riscaldamento
e 16 round accoppiati con l'ordine prima/dopo invertito a ogni
round per ogni vettore. I tempi sono le mediane delle somme per
96 trasformate per precisione. La riduzione usa la mediana dei
rapporti accoppiati. La memoria degli stadi è 32 KiB a N=16384
in entrambe le varianti.

| Twiddle | Prima (ms) | Dopo (ms) | Rapporto accoppiato | Intervallo dei rapporti | Riduzione |
|---|---:|---:|---:|---:|---:|
| Q7 | 126,32 | 76,23 | 0,595 | 0,569–0,693 | 40,5% |
| Q15 | 120,54 | 75,28 | 0,617 | 0,586–0,662 | 38,3% |

[Tempi grezzi](ci8-fft-adaptive-shift-timing.csv). SHA-256 del binario
`9588826b9a9aff86d032fc951609f2977d0cb9c85c2f3a67ad887ec5d8e67570`.

Per riprodurre, compilare `ci8_fft_lab.exe` come sopra e usare i
comandi seguenti dalla radice EVO. L'opzione `--timing` mantiene il
confronto storico a tre profili della sezione precedente.

```powershell
$d = 'build\f4-ci8-adaptive'
$exe = "$d\ci8_fft_lab.exe"
cmd /c "$exe $d/reference.bin > $d/reference.csv 2> $d/reference.log"
if ($LASTEXITCODE) { throw 'Esecuzione del riferimento fallita' }
foreach ($variant in 'compact-adaptive-before', 'compact-adaptive') {
    cmd /c "$exe $d/$variant.bin $variant > $d/$variant.csv 2> $d/$variant.log"
    if ($LASTEXITCODE) { throw "Esecuzione fallita: $variant" }
    foreach ($ext in 'bin', 'csv') {
        cmd /c "fc /b $d\reference.$ext $d\$variant.$ext"
        if ($LASTEXITCODE) { throw "Parità fallita: $variant.$ext" }
    }
}
& $exe --timing-adaptive | Set-Content -Encoding ascii "$d/adaptive-shift-timing.csv"
```

La ripetizione delle 60 ricerche PCPS con il candidato produce
un CSV identico byte per byte alla campagna precedente, inclusi
gli hash delle mappe, le potenze, le soglie e le decisioni. Per
riprodurre usare i comandi della sezione PCPS e confrontare il
risultato con `fc /b build\f4-q14-ON\ci8-pcps.csv development\profiling\F4\ci8-pcps.csv`.
Anche la regressione del percorso wide Q15 AVX2 coincide con il
riferimento. I risultati riguardano Windows x64 e il banco offline.
Restano aperte le verifiche native Linux e macOS.

### PCPS in prossimità della soglia

La campagna `--near-threshold` riusa il banco PCPS Q15 di F4 e
confronta wide con INT8 adattivo ottimizzato. Il segnale resta PRN 7 a
4,096 Msps, IF zero e Doppler +1234 Hz. La matrice restringe l'ampiezza
alle regioni in cui il precedente confronto passava da scarto a
conferma. Le ampiezze, in unità ADC prima dell'arrotondamento, sono
0,39 / 0,43 / 0,46 / 0,50 per GPS e 0,17 / 0,19 / 0,21 / 0,24
per Galileo. Ogni ampiezza usa quattro seed, da 20261121 a 20261124.
Si aggiungono otto fixture di solo rumore per segnale, con seed da
20261125 a 20261132. In totale si eseguono 48 confronti accoppiati
e 96 ricerche. Ogni coppia verifica l'hash della cattura e confronta
le mappe complete alla stessa scala.

La regola di passaggio coarse resta potenza del picco maggiore della
soglia stimata dal fondo a Doppler errato di ciascun profilo.
Il banco conserva la conferma 3-su-4. Per leggere quanto una fixture
si avvicina al confine, il CSV riporta il rapporto picco/soglia e
identifica separatamente passaggio coarse e conferma finale.

Su Windows x64 Release con MSVC 19.52 e Core i7-1355U, dieci fixture
con segnale hanno rapporto picco/soglia fra 0,9 e 1,1 per almeno uno
dei due profili. I conteggi sotto si riferiscono alle singole fixture
accoppiate.

| Segnale e ingresso | Fixture | Coarse wide | Coarse adattivo | Conferme wide | Conferme adattivo |
|---|---:|---:|---:|---:|---:|
| GPS con segnale | 16 | 13 | 12 | 13 | 12 |
| Galileo con segnale | 16 | 9 | 8 | 5 | 5 |
| GPS solo rumore | 8 | 0 | 0 | 0 | 0 |
| Galileo solo rumore | 8 | 1 | 0 | 0 | 0 |

| Segnale | Seed | Ampiezza ADC | Picco/soglia wide | Picco/soglia adattivo | Esito finale wide / adattivo |
|---|---:|---:|---:|---:|---|
| GPS | 20261121 | 0,46 | 1,01666 | 0,97774 | Confermato / fermo al coarse |
| Galileo | 20261124 | 0,21 | 1,01066 | 0,99341 | Scartato in conferma / fermo al coarse |
| Galileo, rumore | 20261126 | 0 | 1,01120 | 0,94626 | Scartato in conferma / fermo al coarse |

Nei tre casi di cambio al coarse il massimo cade nella stessa cella.
Nelle 17 conferme comuni coincide la fase codice. Coincide anche il
Doppler finale in 16 casi. Nella fixture Galileo 20261121 a 0,24 ADC
il massimo coarse si sposta da 1264 a 1177 Hz e il Doppler finale
da 1237 a 1240 Hz.

Nelle fixture con segnale il fondo adattivo è 1,50–1,53 volte wide
per GPS e 1,69–1,75 per Galileo. L'errore relativo delle mappe è
0,511–0,524 per GPS e 0,731–0,739 per Galileo. L'aumento del fondo
può consumare il margine del picco anche quando la posizione del massimo
resta identica. In questa matrice l'adattivo perde una conferma GPS
rispetto al riferimento. Le 16 fixture di rumore non producono falsi
allarmi confermati. Il risultato vale per questi seed e non stima
il tasso di falsi allarmi. Restano aperti il replay RF e le verifiche
native Linux e macOS.

[Dati per fixture](ci8-pcps-near-threshold.csv). Le quattro righe
in comune con la campagna precedente coincidono byte per byte.
SHA-256 del CSV
`18e18d3cbee71440a5fe13f452a574e79f5f9685bc79fd4bf346643007bdff0c`.
SHA-256 del binario di prova
`ab8b74ce8650e7000e52a47c0678c057aee7aadd637ec4671621b075f9a2a4d5`.
Su Windows passano i cinque CTest Release di acquisizione e FFT.

Per riprodurre, dalla radice EVO con la stessa build Ninja Release
Windows x64 e il prompt MSVC attivo:

```powershell
$b = 'build/f4-q14-ON'
$generated = "$b/generated/pcps-profile"
node development/profiling/F4/generate-ci8-pcps.cjs $generated
cmake --build $b --target ngene_pcps_profile
node development/profiling/F4/run-ci8-pcps.cjs "$b/ngene_pcps_profile.exe" $b --near-threshold |
    Set-Content -Encoding ascii "$b/ci8-pcps-near-threshold.csv"
if ($LASTEXITCODE) { throw 'Campagna PCPS fallita' }
Remove-Item -LiteralPath "$generated/dsp/search.c", "$generated/profile_generated.h"
cmake --build $b --target ngene ngene_pcps_lab
```

### Replay RF a finestre appaiate

Il runner F4 accetta `--rf` con un [manifest delle catture](ci8-pcps-rf-inputs.json).
Verifica SHA-256, dimensione e intervallo dei campioni prima di avviare le
ricerche. Il banco legge coppie CI8 firmate nell'ordine I/Q, alla frequenza
e IF della cattura. Registra sia l'offset nel file sia il primo campione
della timeline. Per EVO l'indice ha 5453 righe nello stesso segmento ed
epoca, con offset e primo campione coerenti e senza gap segnalati. Per
Classic gli indici partono dall'inizio di ciascun file raw.

| Cattura | Rate (coppie/s) | IF (Hz) | Avvii finestre (s) | PRN GPS | PRN Galileo |
|---|---:|---:|---|---|---|
| EVO, CI8 da replay CU8 | 8183800 | 38400 | 0, 0,5, 1 | 7 | 7 |
| Classic, HackRF L1 VGA 48 | 8183800 | 0 | 0, 10, 40 | 5, 7 | 7, 26 |
| Classic, HackRF L1 diagnosi VGA 56 | 8183800 | 0 | 0, 20 | 5, 7 | 7, 26 |

Le finestre sono da 56 ms per GPS e 224 ms per Galileo, con griglie
da 4096 e 16384 punti. Le finestre diagnostiche coprono satelliti per
i quali il riferimento produce conferme. La cattura diagnostica è
fortemente saturata all'ingresso, come documentato nel report Classic.
Le altre due catture aggiungono finestre in condizioni di guadagno
diverse. La presenza o assenza dei satelliti nelle singole finestre
non è certificata da una verità di riferimento indipendente.

Ogni finestra e PRN viene cercata con tre profili. `native` chiama la
FFT INT32 precisa di EVO direttamente. `wide` è il riferimento F4
Q15 con packing iniziale INT8 e stadi allargati. `adaptive` usa
lo stesso packing e gli stadi compatti. Il runner confronta i tre
hash della finestra, verifica la dimensione delle mappe e controlla
il rapporto picco/soglia contro quello della strategia EVO.
Il CSV esprime l'errore di mappa rispetto al wide Q15. Il mixer,
le griglie, la stima di soglia, i raffinamenti e la conferma
restano quelli del ricevitore.

Il replay su Windows x64 Release (MSVC 19.52, Core i7-1355U) ha
completato 26 confronti per finestra/PRN, per 78 ricerche.
I tre valori nelle colonne dei risultati seguono l'ordine
INT32 operativo / wide Q15 / INT8 adattivo.

| Cattura | Segnale | Finestre/PRN | Passaggi coarse | Conferme |
|---|---|---:|---:|---:|
| EVO | GPS | 3 | 0 / 0 / 0 | 0 / 0 / 0 |
| EVO | Galileo | 3 | 1 / 1 / 0 | 0 / 0 / 0 |
| Classic VGA 48 | GPS | 6 | 0 / 0 / 0 | 0 / 0 / 0 |
| Classic VGA 48 | Galileo | 6 | 4 / 4 / 4 | 2 / 2 / 2 |
| Classic diagnosi | GPS | 4 | 4 / 4 / 4 | 4 / 4 / 4 |
| Classic diagnosi | Galileo | 4 | 4 / 4 / 4 | 4 / 4 / 4 |

I profili concordano sulle 26 decisioni di conferma. L'unica
differenza al coarse è Galileo PRN 7 all'inizio della cattura EVO.
Il massimo resta nella stessa cella e il rapporto picco/soglia vale
1,07035 con INT32, 1,04549 con wide e 0,94635 con l'adattivo.
I primi due profili arrivano alla conferma con 0 successi su 4.

Nelle dieci conferme, wide e INT32 coincidono in Doppler finale e
fase codice. L'adattivo coincide in nove. Per Galileo PRN 26 a 20 s
nella cattura diagnostica, il Doppler finale passa da 1404 a 1394 Hz
e la fase codice da 2939,751218 a 2939,751468 chip. I tre profili
registrano tre successi di conferma.

Il fondo adattivo rispetto al wide varia da 1,47 a 1,55 volte su GPS
e da 1,69 a 1,80 su Galileo. L'errore relativo di mappa va da 0,475
a 0,527 su GPS e da 0,656 a 0,764 su Galileo. La concordanza delle
conferme RF vale per le finestre e i PRN osservati. La perdita di
sensibilità GPS già osservata nella matrice sintetica prossima alla
soglia resta pertinente. Il replay usa finestre appaiate e copre
il DSP di acquisizione. L'esercizio operativo con scheduling e le
verifiche native Linux e macOS restano aperti.

[Risultati RF per finestra](ci8-pcps-rf.csv). SHA-256 del CSV
`1a1780d7bb7bc16913badc6b222232f848b33900a73d2384c00638acb1a417de`.
SHA-256 dell'eseguibile di prova
`abb4e12fae296f461f875cc1469c608865c48965acdd01d1a0a8017386f56fa5`.
I cinque CTest Windows di acquisizione e FFT sono passati.
La campagna sintetica standard di 60 ricerche, ripetuta dopo
l'aggiunta del replay, produce lo stesso CSV byte per byte.

Per riprodurre dalla radice EVO, con la stessa build Ninja Release
Windows x64 e i file locali indicati nel manifest:

```powershell
$b = 'build/f4-q14-ON'
$generated = "$b/generated/pcps-profile"
node development/profiling/F4/generate-ci8-pcps.cjs $generated
cmake --build $b --target ngene_pcps_profile
node development/profiling/F4/run-ci8-pcps.cjs "$b/ngene_pcps_profile.exe" $b --rf development/profiling/F4/ci8-pcps-rf-inputs.json |
    Set-Content -Encoding ascii "$b/ci8-pcps-rf.csv"
if ($LASTEXITCODE) { throw 'Replay RF fallito' }
Remove-Item -LiteralPath "$generated/dsp/search.c", "$generated/profile_generated.h"
cmake --build $b --target ngene ngene_pcps_lab
```

### Replay con lo scheduler reale

La configurazione autonoma in `scheduled/` compila il CLI, lo
scheduler e la traccia per job F6 di EVO in una build dedicata ai file.
Sostituisce solo le tre chiamate FFT del coarse usate anche dal banco
a finestre appaiate. `native` usa INT32 preciso, `wide` usa il Q15
allargato con packing CI8 e `adaptive` usa il Q15 compatto con lo
stesso packing. Buffer e cache del banco sono privati per thread.
La build da radice EVO continua a usare i normali sorgenti DSP.

La cattura è la RTL CI8 riutilizzata da F6, a 2,4 Msps e IF zero.
Si rilegge l'intervallo `[0, 28800000)` in tempo reale, con quattro
worker tracking, otto canali riservati a GPS, quattro a Galileo e
budget acquisizione 1 o 4. Ogni profilo usa un nuovo processo.
Due round sullo stesso binario invertono sia l'ordine dei profili
sia quello dei budget. Il runner verifica durata in campioni,
assenza di gap, drop noti e scarti in coda allo stop.

Per i job pubblicati, i p95 F6 misurano attesa di presa in carico,
attesa ai confini di stadio e tempo dalla pubblicazione della
cattura alla decisione. Il confronto fra profili si fa sulle
chiavi comuni di costellazione, epoca, primo campione e PRN.
La CPU è il tempo complessivo del processo. In tempo reale i
profili possono terminare quantità di lavoro diverse prima di EOF.

Windows x64 Release, MSVC 19.52, Core i7-1355U. Tutte le 12 prove
hanno elaborato 28,8 milioni di coppie, in 12,001–12,016 s, con
zero gap, drop noti e scarti in coda allo stop. La tabella conserva
i due round per ogni conteggio.

| Budget | Profilo | Job pubblicati (round 1 / 2) | Conferme (round 1 / 2) | CPU processo (s, round 1 / 2) |
|---:|---|---:|---:|---:|
| 1 | INT32 operativo | 55 / 27 | 10 / 1 | 14,47 / 10,61 |
| 1 | Wide Q15 | 18 / 14 | 0 / 0 | 11,89 / 11,52 |
| 1 | INT8 adattivo | 12 / 11 | 0 / 0 | 11,81 / 12,02 |
| 4 | INT32 operativo | 712 / 372 | 10 / 10 | 33,28 / 37,14 |
| 4 | Wide Q15 | 26 / 38 | 2 / 5 | 38,56 / 46,83 |
| 4 | INT8 adattivo | 15 / 27 | 0 / 2 | 37,02 / 43,53 |

L'adattivo concorda con INT32 sulle 65 chiavi pubblicate in comune.
Wide concorda sulle sue 96 chiavi comuni. Per le conferme a chiave
comune coincidono Doppler finale e fase. Il numero di job pubblicati
varia anche tra repliche dello stesso profilo. La differenza di
conferme complessive descrive dunque il lavoro raggiunto entro EOF
in queste prove. La sensibilità a parità di finestra va letta nelle
campagne appaiate.

Con budget 4, per INT8 adattivo il p95 della decisione è 10,52–11,11 s
su GPS e 10,08–10,17 s su Galileo. Per INT32 è 0,35–3,39 s su GPS e
4,41–8,02 s su Galileo. L'attesa p95 ai confini di stadio resta sotto
0,5 ms per tutti e tre i profili con questo budget. La differenza
osservata riguarda il tempo di presa in carico e di elaborazione,
con campioni di job diversi fra i profili. Con quattro permessi il
candidato usa più CPU di INT32 in entrambi i round e pubblica meno
decisioni. Il replay supporta il mantenimento dell'INT32 operativo
per questo carico. Restano aperte le verifiche native Linux e macOS.

[Riepilogo per round e costellazione](ci8-pcps-scheduled.csv).
Le tracce F6 grezze sono in `build/f4-scheduled/results/`.
SHA-256 della cattura
`aa82593a27aee92cde07f964d9d4e41908bc129bf228a9f05a0245bafdf22620`
e SHA-256 del binario
`741c2e1208e85351f80093d476d63848d30781aca2d38dfd20c0f98515b96e63`.
Il banco FFT compilato dopo l'introduzione del workspace per thread
conserva dump, esponenti e saturazioni identici al riferimento.
La build operativa Windows, con traccia F6 disattivata, supera
6/6 test di acquisizione, FFT e scheduling.

Per riprodurre dalla radice EVO, in un prompt MSVC x64 con Ninja:

```powershell
$b = 'build/f4-scheduled'
cmake -S development/profiling/F4/scheduled -B $b -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl
cmake --build $b --target ngene_f4_scheduled_replay
$capture = (Get-Item build/performance-20261115/rtl-repro/*.cs8).FullName
powershell -NoProfile -ExecutionPolicy Bypass -File development/profiling/F4/scheduled/run.ps1 -Executable "$b/ngene_f4_scheduled_replay.exe" -Capture $capture -Output "$b/results"
node development/profiling/F4/scheduled/summarize.cjs "$b/results" |
    Set-Content -Encoding ascii "$b/results/summary.csv"
```

### Kernel INT8 adattivo AVX2 e pacco librerie

Il target `ngene_f4_fft_kernels` conserva in un archivio statico
gli stadi wide scalare, INT8 a shift fisso e INT8 adattivo scalare.
Su x86-64 include anche gli stadi wide Q15 e INT8 adattivo AVX2.
L'header `ci8_fft_kernels.h` espone le chiamate per stadio.
Il codice AVX2 risiede in oggetti compilati con l'opzione ISA
dedicata. I banchi selezionano gli ingressi AVX2 solo dopo il
controllo di CPU e stato vettoriale del sistema operativo.

Il nuovo stadio elabora otto butterfly per gruppo. Legge I/Q
interlacciati dal buffer INT8, allarga a INT32 e riusa gli stessi
twiddle per tutti i blocchi a pari posizione nello stadio. La prima
passata riduce gli estremi in vettori e può uscire appena prova la
necessità di due bit. La seconda produce i risultati arrotondati
e li ricompatta nello stesso buffer. Gli stadi di larghezza
inferiore a 16 usano lo scalare. Il buffer attivo rimane 2N byte,
32 KiB a 16384 punti, e gli intermedi vettoriali stanno nei registri.

Le 192 trasformate adattive Q7/Q15, dirette e inverse da 8 a 16384
punti, coincidono bit per bit con il riferimento, inclusi esponenti
e saturazioni. Il dump e il CSV completi coincidono anche con
l'archivio scalare, lo shift fisso compatto e il precedente wide
AVX2, ciascuno confrontato con il proprio profilo di riferimento.

In 16 round alternati sullo stesso eseguibile Windows x64, Core
i7-1355U e MSVC 19.52 Release, i tempi mediani per 96 trasformate
sono i seguenti. La riduzione è calcolata dalla mediana dei
rapporti accoppiati. Sono inclusi gli stadi scalari iniziali e le
due passate adattive. Preparazione e oracolo sono fuori misura.

| Twiddle | Scalare (ms) | AVX2 (ms) | Rapporto accoppiato | Intervallo | Riduzione |
|---|---:|---:|---:|---:|---:|
| Q7 | 28,15 | 8,95 | 0,312 | 0,291–0,369 | 68,8% |
| Q15 | 27,44 | 8,42 | 0,307 | 0,298–0,349 | 69,3% |

[Tempi per round](ci8-fft-adaptive-avx2-timing.csv).
SHA-256 del banco `be8041cdda7dcd273056bbb7a842bf9fcfda210990f798a33558d5992d9a8ad6`.

Con lo scheduler reale, due round alternati con budget 4, ciascuno
da 28,8 milioni di coppie RTL CI8, danno i risultati seguenti.
Tutte le prove terminano senza gap, drop noti o scarti.

| Profilo | Job pubblicati (round 1 / 2) | Conferme (round 1 / 2) | CPU processo (s, round 1 / 2) |
|---|---:|---:|---:|
| INT32 operativo | 254 / 202 | 10 / 10 | 37,53 / 38,98 |
| INT8 adattivo scalare | 15 / 15 | 0 / 0 | 43,64 / 44,59 |
| INT8 adattivo AVX2 | 28 / 32 | 4 / 5 | 44,48 / 46,20 |

Nelle 30 ricerche pubblicate con stessa costellazione, epoca,
campione e PRN, scalare e AVX2 coincidono in stato, conferma,
Doppler, fase e rapporto picco/soglia. Le differenze fra conteggi
totali descrivono quante ricerche si completano prima di EOF.
AVX2 quasi raddoppia i job INT8 completati, con CPU di processo
simile e un carico utile maggiore. INT32 rimane avanti per
job pubblicati e conferme, con CPU inferiore in entrambi i round.

[Riepilogo scheduler](ci8-pcps-scheduled-avx2.csv).
SHA-256 del binario scheduler misurato
`0d657954fc5253fece1cc28a5e6b4937deb13042a35bfaa0c0ac695fbdf82b13`.

Per isolare il costo a parità di lavoro si usa un eseguibile
seriale separato, con quattro round alternati. Le due finestre
sono quelle iniziali della cattura Classic diagnostica, GPS
PRN 5 e Galileo PRN 7. I tre profili confermano il PRN e per ogni
profilo si eseguono 273 FFT GPS e 1873 FFT Galileo. Gli hash
delle mappe scalare e AVX2 coincidono. La misura separa packing,
preparazione delle tabelle e permutazione dalla FFT e dalla
conversione con ripristino di scala all'uscita. Per INT32 il
cronometro misura la chiamata intera, che include le relative
operazioni interne. Le altre fasi PCPS e i raffinamenti sono
fuori da questi tempi.

| Segnale | Profilo | Preparazione (ms) | Stadi INT8 (ms) | Ripristino (ms) | Totale FFT (ms) |
|---|---|---:|---:|---:|---:|
| GPS | INT32 operativo | — | — | — | 63,06 |
| GPS | INT8 adattivo scalare | 32,62 | 374,60 | 7,15 | 414,37 |
| GPS | INT8 adattivo AVX2 | 28,82 | 122,99 | 6,24 | 158,05 |
| Galileo | INT32 operativo | — | — | — | 1932,80 |
| Galileo | INT8 adattivo scalare | 916,48 | 14751,59 | 207,35 | 15875,43 |
| Galileo | INT8 adattivo AVX2 | 906,84 | 4487,23 | 206,45 | 5600,53 |

Le colonne riportano le mediane per fase e il totale è la mediana
delle somme per round. Il packing e il ripristino incidono per
circa il 22% del totale AVX2 GPS e per il 20% di quello Galileo.
Gli stadi adattivi AVX2 da soli impiegano già circa 1,95 volte
la chiamata INT32 GPS e 2,32 volte quella Galileo. In questo
incremento il divario residuo si concentra quindi negli stadi
adattivi, che effettuano due passate e allargano e ricompattano i
dati. La rappresentazione INT8 è la stessa nelle due varianti
compatte, mentre il cambio di implementazione riduce di circa
due terzi il tempo FFT nelle fixture. Il costo residuo va misurato
sulle singole operazioni prima di attribuirlo alla larghezza dei dati.

[Tempi delle fasi](ci8-pcps-avx2-phases.csv).
SHA-256 del banco seriale
`5566ee9586bdb04d80b01a33545c111a5b53a9aa09e24d424fd9faec5f1f4e76`.
I test Windows di acquisizione, FFT e scheduling passano. Restano
aperte le verifiche native Linux e macOS.

Per compilare, installare il pacco e ripetere i confronti, dalla
radice EVO con il prompt MSVC x64 e Ninja:

```powershell
$b = 'build/f4-kernels'
cmake -S development/profiling/F4 -B $b -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl
cmake --build $b
cmake --install $b --prefix "$b/package"
$lab = "$b/ngene_f4_fft_lab.exe"
& $lab "$b/reference.bin" | Set-Content -Encoding ascii "$b/reference.csv"
foreach ($profile in 'compact', 'compact-adaptive', 'compact-adaptive-avx2', 'avx2') {
    & $lab "$b/$profile.bin" $profile | Set-Content -Encoding ascii "$b/$profile.csv"
    if ($LASTEXITCODE) { throw "Banco FFT fallito: $profile" }
    foreach ($ext in 'bin', 'csv') {
        cmd /c "fc /b $b/reference.$ext $b/$profile.$ext"
        if ($LASTEXITCODE) { throw "Parità fallita: $profile.$ext" }
    }
}
& $lab --timing-adaptive-avx2 | Set-Content -Encoding ascii "$b/adaptive-avx2-timing.csv"

$s = 'build/f4-scheduled'
cmake -S development/profiling/F4/scheduled -B $s -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl
cmake --build $s --target ngene_f4_scheduled_replay ngene_f4_pcps_phases
$rtl = (Get-Item build/performance-20261115/rtl-repro/*.cs8).FullName
powershell -NoProfile -ExecutionPolicy Bypass -File development/profiling/F4/scheduled/run.ps1 -Executable "$s/ngene_f4_scheduled_replay.exe" -Capture $rtl -Output "$s/avx2" -AdaptiveSimd
node development/profiling/F4/scheduled/summarize.cjs "$s/avx2" |
    Set-Content -Encoding ascii "$s/avx2/summary.csv"
$diagnosi = (Resolve-Path '../NGene Classic/recordings/diagnosi/hackrf-l1-ci8.raw').Path
powershell -NoProfile -ExecutionPolicy Bypass -File development/profiling/F4/scheduled/run-phases.ps1 -Executable "$s/ngene_f4_pcps_phases.exe" -Capture $diagnosi -Output "$s/avx2/phases.csv"
```

### Stadio adattivo con buffer temporaneo e due thread

Il pacco conserva AVX2 con ricalcolo e aggiunge uno stadio AVX2 che
calcola ciascuna butterfly una sola volta. Scrive le uscite grezze
nel buffer temporaneo INT16 e riduce gli estremi nella stessa
passata. Una scansione vettoriale contigua legge l'INT16, applica
lo shift comune e scrive l'INT8. L'array INT8 da 2N byte resta
il dato fra gli stadi. Lo scratch richiede altri 4N byte. A
16384 punti il totale attivo è 96 KiB, contro 32 KiB con ricalcolo,
escluse le tabelle comuni.

L'archivio espone il calcolo per intervalli disgiunti di colonne
e il packing finale. Questo permette ai thread di condividere
l'ingresso in sola lettura e scrivere porzioni disgiunte dello
scratch. Il banco Windows ripartisce le colonne fra il thread
chiamante e un worker persistente a partire da N=8192 e
larghezza 128. Attende il worker, prende il massimo dei due shift
locali e compatta con un solo shift per stadio. La prova include
risvegli, attesa e riduzione nel tempo misurato.

I dump e i CSV della variante seriale con scratch e di quella con
worker coincidono byte per byte con il riferimento storico.
Sono coperte le 192 trasformate adattive da 8 a 16384 punti,
Q7/Q15, entrambe le direzioni, con 1.048.320 componenti, esponenti
e saturazioni identici. Nei 32 casi a 8192 e 16384 punti è stato
esercitato il worker negli stadi previsti.

Windows x64, Core i7-1355U, MSVC 19.52 `/O2 /W4 /WX`.
I due confronti usano ciascuno 18 round dopo riscaldamento
sullo stesso binario. I tempi sono mediane delle somme degli
stadi. I rapporti sono mediane accoppiate per round.

| Matrice | Twiddle | Ricalcolo (ms) | Scratch seriale (ms) | Scratch 2 thread (ms) | Scratch seriale / ricalcolo | 2 thread / scratch seriale |
|---|---|---:|---:|---:|---:|---:|
| 8–16384, 96 trasformate | Q7 | 27,27 | 24,13 | — | 0,855 | — |
| 8–16384, 96 trasformate | Q15 | 27,62 | 24,12 | — | 0,848 | — |
| 8192–16384, 16 trasformate | Q7 | 20,38 | 16,95 | 21,83 | 0,815 | 1,293 |
| 8192–16384, 16 trasformate | Q15 | 21,15 | 19,73 | 21,96 | 0,903 | 1,190 |

Lo scratch riduce circa il 15% del tempo degli stadi nella
matrice completa. Il worker aumenta il tempo mediano di circa
il 19–29% rispetto allo stesso algoritmo seriale nella fascia
8192–16384. Il risultato include il costo di coordinare due thread
a ogni stadio parallelizzato. L'opzione a due thread è stata ritirata dopo questa prova.
Le temporizzazioni storiche sono
durate a muro degli stadi.

[Tempi scratch](ci8-fft-adaptive-buffered-timing.csv) ·
[Tempi due thread](ci8-fft-adaptive-threaded-timing.csv).
SHA-256 del banco `b3b5d6e34eaddb64729048705f9bcdf2b843c120562d0c8fa8dbd91c858ac907`.

Nel banco PCPS seriale si ripetono le stesse finestre RF Classic
diagnostiche e i tre profili su quattro round alternati. GPS PRN 5
esegue 273 FFT e Galileo PRN 7 ne esegue 1873 per profilo in
ciascun round. Entrambe le varianti compatte producono gli stessi
hash di mappa e confermano entrambi i PRN.

| Segnale | INT32 totale FFT (ms) | AVX2 ricalcolo totale (ms) | AVX2 scratch totale (ms) | Scratch / ricalcolo accoppiato |
|---|---:|---:|---:|---:|
| GPS | 28,22 | 84,61 | 78,56 | 0,897 |
| Galileo | 1030,64 | 2737,27 | 2679,45 | 0,980 |

Il totale include preparazione, stadi e ripristino. Il rapporto
accoppiato GPS va da 0,791 a 0,980. Per Galileo va da 0,865 a 1,148
e due round su quattro sono più lenti. L'ottimizzazione riduce il
lavoro aritmetico e introduce traffico sullo scratch. Su queste
fixture il guadagno della FFT completa è circa il 10% su GPS e
variabile su Galileo. INT32 rimane il più rapido nelle due prove.
Il coordinamento a due thread provato qui è sfavorevole a uno
stadio e non è stato introdotto nello scheduler operativo.

[Risultati PCPS a lavoro fisso](ci8-pcps-buffered-phases.csv).
SHA-256 del banco PCPS
`123641d750a8d72c7e677bf9ab1788b00a9d12b7b6055a00816b4f17a2bacdbb`.
Il pacco Windows x64 contiene il kernel con scratch e le due
funzioni di calcolo per colonne e compattazione. L'archivio
installato ha SHA-256
`09ccb3325af5d7af0a5ee18393b4ed2f2875ec3954a26205c5a9d6a8a250f2dc`.
Su Windows passano 6/6 CTest di acquisizione, FFT e scheduling.
Verifiche native Linux e macOS aperte.

Per ripetere, compilare e installare i target F4 come sopra
da un prompt MSVC x64. Dopo aver rigenerato `reference.bin` e
`reference.csv` con il banco, eseguire:

```powershell
$b = 'build/f4-kernels'
$lab = "$b/ngene_f4_fft_lab.exe"
foreach ($profile in 'compact-buffered') {
    & $lab "$b/$profile.bin" $profile | Set-Content -Encoding ascii "$b/$profile.csv"
    if ($LASTEXITCODE) { throw "Banco FFT fallito: $profile" }
    foreach ($ext in 'bin', 'csv') {
        cmd /c "fc /b $b/reference.$ext $b/$profile.$ext"
        if ($LASTEXITCODE) { throw "Parità fallita: $profile.$ext" }
    }
}
& $lab --timing-buffered | Set-Content -Encoding ascii "$b/buffered-timing.csv"
$s = 'build/f4-scheduled'
cmake --build $s --target ngene_f4_pcps_phases
$diagnosi = (Resolve-Path '../NGene Classic/recordings/diagnosi/hackrf-l1-ci8.raw').Path
powershell -NoProfile -ExecutionPolicy Bypass -File development/profiling/F4/scheduled/run-phases.ps1 -Executable "$s/ngene_f4_pcps_phases.exe" -Capture $diagnosi -Output "$s/buffered-phases.csv" -Buffered
```

### Butterfly AVX2 con prodotti a coppie INT16

Dopo la prova precedente è stata rimossa dal banco l'opzione che
distribuiva uno stadio su due thread. Il pacco conserva i kernel
seriali, lo scratch e i dati storici del confronto. I worker dello
scheduler di acquisizione restano disponibili per lavori indipendenti.

Il nuovo kernel prepara due coppie di twiddle `(c,-s)` e `(s,c)`
per ciascun gruppo di otto butterfly. `VPMADDWD` combina in INT32
i due prodotti INT16 per ciascuna componente della rotazione.
Quando un coefficiente vale +32768, lo si rappresenta come +32767
e si somma il campione corrispondente prima di arrotondare.
L'estremo −32768 è rappresentabile direttamente. Lo shift comune,
l'arrotondamento e il buffer attivo da 96 KiB a N=16384 seguono
il contratto della variante AVX2 con scratch.

Su Windows x64, MSVC 19.52 e Core i7-1355U, i dump e i CSV
coincidono byte per byte con il riferimento sulle 192
trasformate adattive Q7/Q15, da 8 a 16384 punti nelle due direzioni.
Sono verificati campioni, esponenti e saturazioni. Il benchmark
usa 18 round alternati sullo stesso eseguibile. I tempi riportano
la somma mediana degli stadi su 96 trasformate per precisione.

| Twiddle | Scratch AVX2 precedente (ms) | Prodotti a coppie (ms) | Rapporto mediano accoppiato | Intervallo |
|---|---:|---:|---:|---:|
| Q7 | 23,70 | 22,74 | 0,916 | 0,870–1,091 |
| Q15 | 23,45 | 22,25 | 0,969 | 0,880–1,183 |

[Tempi per round](ci8-fft-adaptive-madd-timing.csv).
SHA-256 del banco
`63a38c3f051de2ba3012fb294d3b01f7e757c12de783d9e817eadd9c56721990`.

Una prova aggiuntiva ha confrontato 1536 stadi con lo scalare
usando coefficienti scelti fra gli estremi firmati, i loro
vicini e i valori prossimi a zero. Campioni ed esponenti
coincidono per Q7 e Q15. La prova collega l'archivio installato,
che esporta `Ci8FftCompactAdaptiveMaddAvx2`.

Nel confronto PCPS a lavoro fisso, quattro round alternati
sulle stesse finestre RF diagnostiche e sullo stesso binario,
gli hash delle mappe compatte coincidono. I tre profili
confermano GPS PRN 5 e Galileo PRN 7, con 273 e 1873 FFT per
prova rispettivamente. I valori sotto comprendono preparazione,
stadi e ripristino.

| Segnale | FFT INT32 (ms) | Scratch AVX2 precedente (ms) | Prodotti a coppie (ms) | Rapporto mediano accoppiato | Intervallo |
|---|---:|---:|---:|---:|---:|
| GPS | 77,32 | 172,85 | 160,04 | 0,946 | 0,831–0,999 |
| Galileo | 2039,88 | 4942,97 | 4771,02 | 0,971 | 0,899–1,001 |

Il guadagno è dell'ordine del 3–5% nella chiamata FFT del banco
PCPS. INT32 resta più rapido nelle due finestre. Il passo
successivo è misurare l'elaborazione a gruppi di FFT complete
su periodi o bin Doppler indipendenti, con budget di worker
fisso e riduzione in ordine deterministico. Il riferimento per
la decisione e le soglie resta la strategia EVO.

[Tempi PCPS per fase](ci8-pcps-madd-phases.csv).
SHA-256 banco PCPS
`44ffcd574d9842c2886c0abd0c34200b28cd111ee546b71969640f0952c194fe`.
SHA-256 archivio installato
`981f4f7d1f568a7537bb557a40ef21152cb511750c6294c644a201e452f8cca5`.
Su Windows passano 6/6 CTest. Restano aperte le verifiche native
Linux e macOS.

Dopo build e installazione dei target F4 secondo i comandi
precedenti, ripetere il confronto con:

```powershell
$b = 'build/f4-kernels'
$lab = "$b/ngene_f4_fft_lab.exe"
& $lab "$b/compact-buffered-madd.bin" compact-buffered-madd |
    Set-Content -Encoding ascii "$b/compact-buffered-madd.csv"
if ($LASTEXITCODE) { throw 'Banco FFT fallito' }
cmd /c "fc /b $b/reference.bin $b/compact-buffered-madd.bin"
if ($LASTEXITCODE) { throw 'Parità del dump fallita' }
cmd /c "fc /b $b/reference.csv $b/compact-buffered-madd.csv"
if ($LASTEXITCODE) { throw 'Parità dei metadati fallita' }
& $lab --timing-buffered-madd | Set-Content -Encoding ascii "$b/madd-timing.csv"
$s = 'build/f4-scheduled'
cmake --build $s --target ngene_f4_pcps_phases
$diagnosi = (Resolve-Path '../NGene Classic/recordings/diagnosi/hackrf-l1-ci8.raw').Path
powershell -NoProfile -ExecutionPolicy Bypass -File development/profiling/F4/scheduled/run-phases.ps1 -Executable "$s/ngene_f4_pcps_phases.exe" -Capture $diagnosi -Output "$s/madd-phases.csv" -Madd
```

### Batch di FFT complete per bin Doppler

Il target offline `ngene_f4_pcps_batch` usa copie generate delle
strategie GPS e Galileo. Estrae le frequenze dalla stessa
espressione usata dal ricevitore. Calcola la riga a Doppler
errato e la soglia sul chiamante, quindi assegna bin indipendenti
a uno, due o quattro worker, contando il chiamante nel budget.
Ogni worker ha un workspace e un piano FFT privati. Le otto
potenze di ciascun bin conservano l'ordine di accumulazione
del coarse originale.

Dopo il completamento dei worker, il chiamante ripristina
la riga di rumore per la traccia e legge le righe nell'ordine
Doppler originale. La strategia confronta le celle nello stesso
ordine e usa i raffinamenti e la conferma originali. Il banco
misura anche un'esecuzione diretta a un worker. Il batch a
un worker isola il costo di memorizzare e rileggere le righe.

Il buffer delle righe include la copia della riga di rumore.
Occupa 544 KiB per GPS a 4096 punti e 14,625 MiB per Galileo a
16384 punti, oltre ai workspace privati dei worker. La
preparazione delle copie di workspace è misurata separatamente.
La prova usa le primitive portabili dei thread solo nel banco. I
worker vengono creati e raccolti per ogni ricerca. Il
batch attende tutte le righe prima del consumo ordinato,
quindi il banco non qualifica la latenza di cancellazione
del ricevitore durante il calcolo del batch.

La verifica byte per byte copre 24 ricerche. Per ciascun profilo
si confrontano l'esecuzione diretta e tre budget batch, sulle
finestre RF diagnostiche GPS PRN 5 e Galileo PRN 7. L'adattivo
copre inoltre due fixture sintetiche vicine alla soglia, GPS
seed 20261121 a 0,46 ADC e Galileo seed 20261124 a 0,21 ADC.
In ciascun confronto coincidono tutte le righe di potenza,
compresa quella a Doppler errato, e tutti i campi della traccia
di verifica, inclusi soglia, picco, Doppler, fase codice,
conferma e numero di successi. Le due finestre RF confermano il
PRN e le due fixture sintetiche lo scartano.

Windows x64 Release, Core i7-1355U e MSVC 19.52. Quattro round
alternano l'ordine dei budget per ogni coppia segnale/profilo.
Le 64 prove di tempo verificano l'hash e i campi delle decisioni
e tengono la scrittura dei dump fuori dalla misura. La ricerca
misurata include la riga di rumore, il coarse, la verifica della
traccia in memoria, il raffinamento e la conferma. I valori
in tabella sono mediane in millisecondi.

| Finestra | Profilo | Diretto | Batch 1 worker | Batch 2 worker | Batch 4 worker |
|---|---|---:|---:|---:|---:|
| GPS | INT32 | 121,58 | 128,23 | 98,62 | 83,05 |
| GPS | INT8 adattivo AVX2 | 193,13 | 190,11 | 132,93 | 118,02 |
| Galileo | INT32 | 2798,20 | 2947,01 | 1983,40 | 1511,84 |
| Galileo | INT8 adattivo AVX2 | 4984,37 | 6927,81 | 3665,47 | 1993,12 |

Per l'adattivo AVX2 a quattro worker, la mediana del rapporto
diretto/batch è 1,73 su GPS e 2,40 su Galileo. I rapporti
vanno da 1,59 a 1,89 su GPS e da 1,00 a 5,66 su Galileo.
Includendo la preparazione dei workspace, le mediane accoppiate
scendono a 1,31 e 2,27. Su GPS il batch a due worker arriva
a 1,37 includendo la preparazione. La CPU di processo durante
la ricerca adattiva passa da 164 a 211 ms su GPS e da 4844 a
4891 ms su Galileo, in mediana. Sono misure di una ricerca
appaiata con lo stesso budget. Con quattro worker, l'adattivo
impiega ancora circa 1,36 volte l'INT32 sia su GPS sia su
Galileo come mediana dei rapporti per round.

Parallelizzare fra FFT complete riduce la latenza in questo
banco. Le verifiche sullo scheduler con carichi concorrenti
richiedono il riuso del budget e una verifica della
cancellazione durante la produzione delle righe. La spesa di
memoria del buffer pieno è particolarmente rilevante per
Galileo. Il ricevitore operativo usa la propria strategia.

[Risultati per round](ci8-pcps-batch.csv). SHA-256 del CSV
`c61c2d0da0c58f2a9743c675f9e4ac2fa29eaea591a5c37c7925de7377eb8e16`.
SHA-256 del banco
`25780d86471ceecadc595876171c466dcca7ab4e39294386463e1b86c431ccd1`.
La build operativa Windows, con trace F6 disattivata, ha superato
6/6 test di acquisizione, FFT e scheduling.
Le verifiche native Linux e macOS restano aperte.

Per ripetere dalla radice EVO, nel prompt MSVC x64 con la build
F4 offline configurata:

```powershell
$s = 'build/f4-scheduled'
cmake --build $s --target ngene_f4_pcps_batch
$diagnosi = (Resolve-Path '../NGene Classic/recordings/diagnosi/hackrf-l1-ci8.raw').Path
node development/profiling/F4/scheduled/run-batch.cjs "$s/ngene_f4_pcps_batch.exe" $diagnosi "$s/batch" |
    Set-Content -Encoding ascii "$s/batch-results.csv"
if ($LASTEXITCODE) { throw 'Verifica del batch PCPS fallita' }
```

### Gruppi Doppler limitati nello scheduler e misura del carico esterno

La parità numerica dei banchi precedenti è verificata su ingressi
deterministici. I tempi precedenti sono osservazioni sul PC durante
quelle sessioni, con ordine alternato dove indicato. Il carico
esterno variabile non era misurato. In particolare, i rapporti
di ricerca che cambiano molto tra round non isolano il costo
del kernel.

Il target offline `ngene_f4_group_replay` usa copie generate del
pool di acquisizione, delle strategie e del dispatcher. Alla
prima riga del gruppo, un job può chiedere un solo permesso
ausiliario. Il pool lo concede se ci sono credito CPU e capacità
libera dopo aver contato i job già attivi. Il worker persistente
ha la stessa priorità di fondo del chiamante, un piano privato e
calcola la seconda riga. La CPU dell'ausiliario viene addebitata
allo stesso credito del job. Il chiamante aspetta il risultato
prima di rilasciare il permesso. In mancanza di capacità, prosegue
con il bin singolo. Un gruppo attivo contiene al massimo due bin.
La seconda riga usa il buffer di potenza del workspace ausiliario.

La strategia usa la riga di rumore originale e conserva l'ordine
Doppler nel consumo. Mantiene il controllo della generazione
tra le righe, scarta una coppia annullata e attende che il worker
rilasci il prestito prima di uscire. Il worker può osservare
l'annullamento prima della preparazione e prima di iniziare la
propria riga. Una FFT o una riga di potenza in corso finisce
prima del ritorno. Il replay misura anche l'attesa all'arresto
di ciascuna costellazione, dal segnale di stop fino al
rilascio dell'ultimo job attivo.

Il runner Windows usa lo stesso binario, un processo alla
volta, e alterna l'ordine diretto/coppia per profilo. Campiona
la CPU aggregata di Windows e quella del processo circa ogni
250 ms. La differenza, espressa in core equivalenti, stima il
lavoro concorrente al replay. Include servizi, altri programmi
e il monitor. Il p95 della fase tracking misura le finestre in
cui c'è lavoro di tracking. Il CSV riporta anche il conteggio
di tali finestre, perché il numero dei canali attivi influenza
il carico. Il working set è il picco osservato dai campioni
del processo.

Su Windows x64 Release, Core i7-1355U e MSVC 19.52, i dodici
replay rileggono `[0, 28800000)` dalla cattura RTL CI8 di F6.
Usano quattro worker tracking e budget di acquisizione 4.
Tutti elaborano 28,8 milioni di coppie senza gap, drop noti
o scarti in coda. Il carico esterno medio varia da 2,15 a
6,32 core equivalenti per prova. Il p95 delle finestre da
circa 250 ms arriva a 10,69 core. Anche le repliche dello
stesso profilo vedono carichi diversi.

| Profilo | Round | Core esterni diretto / coppia | Job pubblicati diretto / coppia | Conferme diretto / coppia | Gruppi completati |
|---|---:|---:|---:|---:|---:|
| INT32 | 1 | 3,08 / 3,86 | 165 / 171 | 10 / 10 | 752 |
| INT32 | 2 | 3,05 / 3,21 | 420 / 528 | 10 / 11 | 3595 |
| INT32 | 3 | 5,19 / 2,15 | 379 / 634 | 11 / 11 | 4675 |
| INT8 adattivo | 1 | 5,01 / 3,22 | 23 / 32 | 0 / 5 | 0 |
| INT8 adattivo | 2 | 3,55 / 6,32 | 226 / 26 | 10 / 2 | 0 |
| INT8 adattivo | 3 | 2,48 / 3,37 | 152 / 71 | 10 / 9 | 118 |

Su 232 chiavi pubblicate in comune coincidono stato, conferma,
Doppler, fase codice e rapporto picco/soglia. Due job, uno GPS
INT32 e uno Galileo INT8, hanno un prestito senza una coppia
completata. Entrambi terminano con `NG_END` e pubblicazione
zero. Il drain di arresto delle costellazioni con job pendenti
va da 0,34 a 268,08 ms. L'estremo da 268,08 ms appartiene a
un replay INT8 in cui il contatore dei prestiti è zero.

I due round INT8 con zero prestiti mostrano quanto possa cambiare
il numero di conferme al variare del carico anche senza eseguire
una coppia. Nel round INT32 con medie e p95 di carico esterno più
vicini (round 2), i job pubblicati salgono da 420 a 528 e la CPU
di processo da 36,84 a 39,63 s. Il p95 della fase tracking sale
da 1,003 a 1,167 ms su 2953 e 2957 finestre rispettivamente.
Il working set cresce da 63,59 a 78,14 MiB. Le tre misure di
working set INT32 mostrano un incremento di 13,6–14,6 MiB.
Negli altri due confronti INT32 il p95 tracking scende in un
round e sale nell'altro, con carico esterno diverso.

La prova valida il prestito di una riga e la riduzione in ordine
sulle chiavi comuni. Le variazioni di carico e di tracking
lasciano aperto il beneficio netto di sistema. Il punto di
decisione per un'integrazione operativa è raggiunto. I kernel
restano nel pacco F4 e il gruppo resta sperimentale. Una
qualifica prestazionale successiva richiede più coppie sotto
carico comparabile e una misura del costo dei workspace
ausiliari a parità di lavoro del tracking.

[Riepilogo per replay](ci8-pcps-group-replay.csv) ·
[Campioni del carico esterno](ci8-pcps-group-ambient.csv).
Tracce grezze locali in `build/f4-scheduled/group-results/`.
SHA-256 del binario
`006a6fae27b32bc2f079d5798dca29f81b66902e786da0f417f957b8f76ce1fc`.
La build operativa Windows, con trace F6 disattivata, ha
superato 6/6 test di acquisizione, FFT e scheduling.
Il replay usa Windows x64. Restano aperte le verifiche native
Linux e macOS.

Per riprodurre, dalla radice EVO nel prompt MSVC x64:

```powershell
$s = 'build/f4-scheduled'
cmake --build $s --target ngene_f4_group_replay
$rtl = (Get-Item build/performance-20261115/rtl-repro/*.cs8).FullName
powershell -NoProfile -ExecutionPolicy Bypass -File development/profiling/F4/scheduled/run-groups.ps1 -Executable "$s/ngene_f4_group_replay.exe" -Capture $rtl -Output "$s/group-results"
if ($LASTEXITCODE) { throw 'Replay a gruppi fallito' }
node development/profiling/F4/scheduled/summarize-groups.cjs "$s/group-results" |
    Set-Content -Encoding ascii "$s/group-results/summary.csv"
```

#### Repliche mirate INT32

Quattro coppie aggiuntive usano lo stesso binario, la stessa
cattura, il budget 4 e la stessa raccolta del carico esterno.
Tutte le otto prove elaborano 28,8 milioni di coppie senza gap,
drop noti o scarti.

| Round | Core esterni diretto / coppia | Job pubblicati diretto / coppia | Conferme diretto / coppia | Tracking p95 diretto / coppia (ms) |
|---:|---:|---:|---:|---:|
| 1 | 2,53 / 3,45 | 182 / 280 | 10 / 10 | 1,130 / 1,332 |
| 2 | 2,52 / 2,46 | 170 / 180 | 10 / 11 | 1,122 / 1,225 |
| 3 | 2,54 / 2,13 | 176 / 629 | 10 / 11 | 1,307 / 1,338 |
| 4 | 1,81 / 2,23 | 643 / 325 | 11 / 12 | 1,209 / 1,730 |

Nei round 2 e 4 il p95 del carico esterno è quasi uguale
tra diretto e coppia. Il round 2 guadagna 10 job. Il round 4
ne perde 318 a fronte di una conferma in più. Il p95 tracking
cresce in tutte le quattro repliche, mentre il numero di
finestre tracking può variare con le conferme. Il working set
sale di 14,1–15,1 MiB. I risultati mostrano che il guadagno
del banco seriale non basta a prevedere la capacità complessiva
o l'effetto sul tracking.

Le 184 chiavi in comune coincidono in tutti i campi di
decisione. Tre ulteriori gruppi sono interrotti allo stop
e i relativi job terminano con `NG_END` senza pubblicazione.
Il totale delle due campagne è 20 replay, 416 decisioni
in comune identiche e cinque gruppi interrotti senza
pubblicazione. La prova supporta la correttezza del prototipo
sui casi esercitati. Il profilo operativo resta l'esecuzione
diretta. L'incremento major è l'eventuale integrazione del
lavoro ausiliario nella gestione dei workspace dello scheduler,
con qualifica prestazionale separata.

[Repliche INT32](ci8-pcps-group-native-repeat.csv).
Il CSV dei campioni di carico distingue `matrix` e `native-repeat`.
SHA-256 del riepilogo
`3c053921d805ace9cc5602007a30de654c4adfd9c2ab2970f527c418d369c7e6`.
Per replicare, usare il runner sopra con `-Rounds 4 -Profiles native`
e una directory di output separata.

### Riuso del workspace e del worker nello scheduler

Il profilo sperimentale `reuse` assegna la seconda riga a un
worker già presente nel pool. Al momento del prestito devono
essere liberi un worker, un workspace della stessa costellazione
e un permesso del budget di acquisizione. La coda delle righe
già prenotate precede una nuova ammissione PRN. Il pool addebita
la CPU misurata sul worker ausiliario. L'ammissione PRN salta
i workspace occupati da una riga, anche se non appartengono
a un job PRN completo.

Il worker legge i campioni già preparati dal chiamante in sola
lettura. Usa il proprio piano FFT, replica, portante e buffer
di potenza. Il chiamante aspetta il risultato e copia la sola
riga di potenza in un buffer privato prima di rendere
disponibile il workspace prestato. Il buffer aggiuntivo è
di N double per workspace chiamante, allocato su richiesta.
Con il rate di questa prova sono 16 KiB per un workspace GPS
e 64 KiB per uno Galileo. Il flusso torna al bin singolo
quando manca una delle risorse. La riduzione prosegue
nell'ordine Doppler originale e la cancellazione scarta
il risultato della coppia prima della pubblicazione.

La build sperimentale mantiene tre modalità sullo stesso
binario. `direct` elabora i bin sul chiamante. `pair` usa
il worker e il workspace ausiliari dedicati dell'incremento
precedente. `reuse` usa solo quelli già creati dallo
scheduler. Restano fissi budget 4, tracking a quattro worker,
RTL CI8 2,4 Msps e gli stessi 28,8 milioni di coppie per prova.
Si alterna l'ordine delle varianti e si registra il carico
esterno con il monitor del replay precedente.

Windows x64 Release, Core i7-1355U e MSVC 19.52. I 15 replay
hanno elaborato 28,8 milioni di coppie ciascuno, con zero gap,
drop noti e scarti. Il carico esterno medio è variato da 2,14
a 5,42 core equivalenti. Ogni cella con tre valori segue
l'ordine diretto / coppia dedicata / riuso.

| Profilo | Round | Core esterni | Job pubblicati | Working set (MiB) | Tracking p95 (ms) |
|---|---:|---:|---:|---:|---:|
| INT32 | 1 | 3,24 / 2,47 / 2,88 | 227 / 244 / 240 | 64,02 / 77,68 / 63,69 | 1,353 / 1,153 / 1,203 |
| INT32 | 2 | 2,71 / 3,02 / 2,55 | 312 / 274 / 199 | 64,02 / 77,65 / 63,66 | 1,110 / 1,227 / 1,177 |
| INT32 | 3 | 3,05 / 2,55 / 2,14 | 593 / 396 / 630 | 64,08 / 78,13 / 63,76 | 1,332 / 1,319 / 1,558 |
| INT8 adattivo | 1 | 2,14 / 2,38 / 3,78 | 112 / 111 / 53 | 63,09 / 77,13 / 63,28 | 0,949 / 0,892 / 0,778 |
| INT8 adattivo | 2 | 2,54 / 4,18 / 5,42 | 32 / 28 / 29 | 59,73 / 59,80 / 59,80 | 0,596 / 0,342 / 0,589 |

Il riuso completa 6720 coppie INT32 e 40 INT8, esercitando
entrambe le costellazioni. Sei coppie interrotte terminano
con `NG_END` e pubblicazione zero. Tutti i campi di decisione
coincidono sulle 212 chiavi in comune con il diretto e
sulle 211 in comune con la coppia dedicata. Il drain allo stop
con lavoro in corso raggiunge 7,65 ms fra le prove `reuse`.
Le chiavi confrontate sono costellazione, epoca, primo campione
e PRN. Il confronto riguarda i campi pubblicati della ricerca.

Nei quattro confronti in cui entrambe le modalità hanno
effettivamente elaborato coppie, il working set del riuso è
inferiore di 13,85–14,37 MiB rispetto ai workspace dedicati
e resta entro 0,4 MiB dal diretto. Nell'ultimo round INT8,
entrambe le modalità a gruppi restano senza prestiti e hanno
working set simili. I risultati con carico esterno variabile
mostrano un vantaggio di memoria. Throughput e p95 tracking
variano fra i round e non indicano un beneficio di sistema
ripetibile su questa campagna. Il profilo operativo resta
il percorso diretto. Su Windows la build operativa con trace disattivata ha superato 6/6 test di acquisizione, FFT e scheduling. Restano aperte le verifiche native Linux
e macOS.

[Riepilogo per replay](ci8-pcps-group-reuse.csv) ·
[Campioni del carico esterno](ci8-pcps-group-reuse-ambient.csv).
Tracce grezze nelle cartelle locali `group-reuse-native` e
`group-reuse-adaptive` sotto `build/f4-scheduled`.
SHA-256 del binario
`2ead2162e46423365f91ddcb591f61cd00a94d4f1b40f822e149c4aa82c3af17`.

Per ripetere, compilare il target sperimentale e avviare le
due matrici dalla radice EVO:

```powershell
$s = 'build/f4-scheduled'
cmake --build $s --target ngene_f4_group_replay
powershell -NoProfile -ExecutionPolicy Bypass -Command {
    $s = 'build/f4-scheduled'
    $capture = (Get-Item build/performance-20261115/rtl-repro/*.cs8).FullName
    $options = @{Executable="$s/ngene_f4_group_replay.exe"; Capture=$capture; Modes=@('direct','pair','reuse')}
    & development/profiling/F4/scheduled/run-groups.ps1 @options -Output "$s/group-reuse-native" -Rounds 3 -Profiles native
    & development/profiling/F4/scheduled/run-groups.ps1 @options -Output "$s/group-reuse-adaptive" -Rounds 2 -Profiles adaptive-madd
}
foreach ($part in 'native','adaptive') {
    node development/profiling/F4/scheduled/summarize-groups.cjs "$s/group-reuse-$part" |
        Set-Content -Encoding ascii "$s/group-reuse-$part/summary.csv"
}
```

### Chiusura della qualifica F4 su Windows

Le due campagne finali usano INT32 preciso, il profilo corrente di
riferimento, su un unico binario sperimentale Windows x64 Release.
Ciascuna alterna tre coppie diretto/riuso con budget acquisizione 4
e quattro worker di tracking. Tutti i 12 replay rileggono gli stessi
28,8 milioni di coppie RTL CI8 e terminano con zero gap, drop noti
e scarti in coda. Il carico esterno medio è compreso fra 1,57 e
2,96 core equivalenti per prova.

Nel carico fisso, il banco avvia il tracking al campione zero
con sei seed della stessa cattura RTL. Sono GPS 18, 26, 29, 31
e Galileo 10, 14, ricavati dalle decisioni confermate del
replay INT32 diretto precedente. Eventuali perdite dei canali
seguono il tracking originale. Le nuove conferme di acquisizione
non alimentano i seed in questa matrice. Le sei prove eseguono
3000 dispatch di tracking per un totale di 17.001 job e producono
lo stesso hash di lavoro `5e0fcee55b959bbf`. L'hash comprende
campioni, identità e stato dei canali passati al dispatcher.
Nel ciclo completo si usa l'ammissione normale del ricevitore.

| Campagna | Round | Core esterni diretto / riuso | Job acquisizione pubblicati | CPU processo (s) | Tracking p95 (ms) |
|---|---:|---:|---:|---:|---:|
| Carico fisso | 1 | 2,06 / 1,98 | 877 / 717 | 47,47 / 49,25 | 0,473 / 0,511 |
| Carico fisso | 2 | 1,57 / 2,81 | 637 / 700 | 47,95 / 47,92 | 0,565 / 0,521 |
| Carico fisso | 3 | 2,14 / 2,55 | 504 / 445 | 48,23 / 49,67 | 0,829 / 0,954 |
| Ciclo completo | 1 | 2,38 / 2,55 | 202 / 268 | 41,70 / 45,39 | 1,264 / 1,149 |
| Ciclo completo | 2 | 2,22 / 2,47 | 199 / 264 | 43,44 / 45,31 | 1,006 / 1,195 |
| Ciclo completo | 3 | 2,38 / 2,96 | 153 / 384 | 37,47 / 43,41 | 1,317 / 0,822 |

Coincidono tutte le **279 decisioni pubblicate a chiave comune**,
141 a carico fisso e 138 nel ciclo completo, inclusi rapporto
picco/soglia, fase e Doppler. Cinque gruppi interrotti
allo stop terminano con `NG_END` e pubblicazione zero.
A carico fisso il drain massimo con job pendenti è 5,96 ms
nel diretto e 13,11 ms nel riuso. Il working set del riuso
resta entro 1 MiB dal diretto in tutte e sei le coppie.

A lavoro di tracking identico, il riuso pubblica più job in
una coppia su tre e il p95 tracking cresce in due. Nel ciclo
completo pubblica più job in tutti i round, usa più CPU
e il numero di conferme passa rispettivamente da 10, 11, 11
a 10, 10, 10. Il lavoro di tracking in questa matrice varia
con le acquisizioni. Le misure in co-uso del PC descrivono
il carico osservato, compresa la variabilità fra round.

**Esito.** Qualifica F4 Windows conclusa. INT32 diretto è
il profilo operativo. Il riuso ha parità di decisione sui
casi comuni e un vantaggio di memoria rispetto alla coppia
dedicata. I criteri di promozione richiedevano un aumento
ripetibile della capacità a carico fisso e la tenuta del
tracking e della cancellazione. Il riuso, il batch e i
kernel INT8 restano sperimentali. Il pacco F4 conserva
i kernel wide, compatti e SIMD. Linux e macOS sono
rinviati alle macchine native con il
[piano di verifica dedicato](qualification-native.md).

Nella regressione finale del pacco Windows, sette selezioni
di kernel riproducono byte per byte dump e CSV del riferimento.
Il banco copre 576 casi, con Q7/Q15 da 8 a 16384 punti in
entrambe le direzioni. SHA-256 dell'archivio installato
`981f4f7d1f568a7537bb557a40ef21152cb511750c6294c644a201e452f8cca5`.
SHA-256 del banco FFT
`161b86e7744c7f366e079218b243eabaf45c26507e4e4c82f0b25eb1f7a61d91`.
SHA-256 del binario del replay
`88866268eeb27a8dee7270a606f2f5446019adcd47356cf1a48f709fe7faea00`.

[Riepilogo dei 12 replay](ci8-pcps-qualification.csv) ·
[Campioni del carico esterno](ci8-pcps-qualification-ambient.csv).
Tracce grezze locali in `build/f4-scheduled/f4-close-fixed` e
`build/f4-scheduled/f4-close-full`. Dalla radice EVO, dopo
aver compilato il target sperimentale come sopra:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command {
    $s = 'build/f4-scheduled'
    $capture = (Get-Item build/performance-20261115/rtl-repro/*.cs8).FullName
    $options = @{Executable="$s/ngene_f4_group_replay.exe"; Capture=$capture;
                 Modes=@('direct','reuse'); Rounds=3; Profiles='native'}
    & development/profiling/F4/scheduled/run-groups.ps1 @options -Output "$s/f4-close-fixed" -FixedTracking
    & development/profiling/F4/scheduled/run-groups.ps1 @options -Output "$s/f4-close-full"
}
foreach ($part in 'fixed','full') {
    node development/profiling/F4/scheduled/summarize-groups.cjs "build/f4-scheduled/f4-close-$part" |
        Set-Content -Encoding ascii "build/f4-scheduled/f4-close-$part/summary.csv"
    if ($LASTEXITCODE) { throw "Verifica $part fallita" }
}
```

La build operativa pulita `build/f4-qualification-release`
ha completato **25/25 CTest** con traccia F6, INT32 AVX2
sperimentale, INT32 NEON e Q14 AVX2 disattivati. SHA-256 di
`ngene.exe`
`68c84f47deb88710536efadb7de63660ea2f63130597f24d35b039d60f606ca7`.
Il log finale è in `build/f4-qualification-tmp/ctest-final.log`.

La regressione Windows si ripete nel prompt MSVC x64 con
`CMAKE_GENERATOR=Ninja` anche per le fixture che configurano
CMake figlio, e con TEMP/TMP in una cartella locale scrivibile.
La fixture dipendenze ha richiesto questo ambiente per trovare
il compilatore. La build locale usa la directory CRT installata
del compilatore per lo staging.
