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
