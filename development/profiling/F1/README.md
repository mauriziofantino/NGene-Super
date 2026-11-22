# F1 · Baseline PCPS e verifica della ricerca Galileo E1-B

21 novembre 2026 · ramo `chatGPT/fast-arithmetic` · baseline `970c0dc2e8d22efb1ecff0db9ac416b7aa6a062f`. La misura dopo la modifica usa lo stesso HEAD con F1 nel working tree. Il driver è [acquisition_pcps_benchmark.c](../../../tests/acquisition_pcps_benchmark.c). Il [piano](../../../docs/service/PIANO_INCREMENTALE_EVO.md#13-criteri-di-chiusura-e-registro-degli-incrementi) registra lo stato dell'incremento.

## Prima e dopo

Il vecchio `--benchmark-pcps`, ricompilato in Release a HEAD prima delle modifiche, ha stampato 41,091 µs/periodo float, 78,559 Q14, 310,941 int32 e checksum 1855962,500. Usava un codice alternato a N=4096, 32 iterazioni e un accumulo diverso nei due rami. È la riproduzione del difetto del banco e i suoi tempi non costituiscono una baseline comparabile.

Il nuovo banco usa lo stesso PRN Galileo E1-B 7, carrier completo, otto periodi coerenti da 4 ms e una mappa di potenza double per tutti i ritardi. Prima dei tempi calcola l'oracolo per correlazione circolare diretta in doppia precisione, con sinusoide generata indipendentemente, a Doppler 1264 Hz e wrong-Doppler 22000 Hz. Confronta ogni bin, posizione del massimo, ampiezza equivalente da radice della potenza al ritardo di riferimento e guadagno aggregato. La scala d'ingresso intera è 4096 diviso il massimo modulo di una componente centrata sull'intera cattura. Il ripristino di scala è quello operativo di `NgSearchIntegerPcps`.

## Ambiente e riproduzione

- Windows x64, build 26300.10075. Core i7-1355U, 12 CPU logiche, 31,64 GiB RAM riportati dal sistema. Processo a un solo thread di misura, affinità verificata a CPU logica 2 (maschera `0x4`). Host interattivo, nessun test eseguito in parallelo ai tempi. Frequenza e turbo non bloccati. Nessuna misura di isolamento del core o di carico di fondo.
- MSVC 19.52.37029.0, CMake 4.3.1-msvc1, Release x64 multisdr con `/O2 /Ob2 /DNDEBUG`. Percorsi x64 SSE2 dove disponibili, Q14 nativo, PCPS int32 a butterfly scalari. Il processo ha toccato almeno 46,9 MiB di working set durante una rilevazione. È una misura aggregata del banco, non un confronto RAM tra modalità.
- PRN 7 da `tests/data/galileo-e1b-prn7.bin`, SHA-256 `4c2aa7faddcfc3860178fb74a149255cf9efc4c831cf4a2a7a4265ad1f803301`. Capture sintetica CI8 con BOC(1,1), IF 0, seme 20261121, fase carrier 0,71 rad, rumore uniforme e ritardi iniettati di 247/494 campioni. Fs=2,048/4,096 Msps per N=8192/16384. Ogni cattura copre 224 ms. N e checksum FNV-1a delle coppie CI8 sono nei record `fixture`.
- Tool da eseguire dalla radice, `build/msvc-x64-multisdr/Release/ngene_acquisition_tests.exe --benchmark-pcps`. `--verify-pcps` esegue il controllo a N=8192, anche in CTest. Driver misurato SHA-256 `78b1acacad3c3d78052ccb92b2091ebae3e3e2cb6ff3424544e941275f8a7927`. Cinque warm-up e 24 triplette accoppiate per dimensione, con tutte le sei permutazioni d'ordine bilanciate. Una chiamata per modalità e metrica in ogni tripletta.
- [Dati grezzi](measurements.csv) con record `fixture`, `map` e `time`, ciascuno con intestazione propria. I tempi grezzi sono in secondi. Mediane e rapporti appaiati sono calcolati sulle 24 triplette. L'intervallo riportato usa i ranghi 7 e 18 dei rapporti ordinati, copertura binomiale nominale 97,7% con coppie indipendenti. La numerosità basta per descrivere la mediana di questa campagna. p95/p99 restano aperti.

## Confini delle misure

- **Replica** include la costruzione dello spettro per un PRN a partire dai chip campionati. Esclude allocazione, generazione iniziale della replica e banca completa.
- **PCPS** include azzeramento della mappa e otto periodi. Carrier, spettro del codice e baseband sono pronti. La quantizzazione del carrier compiuta dal kernel intero rientra nel tempo.
- **Preparazione stimata** è il delta accoppiato fra le due chiamate `cold_prefix` e `warm_prefix` della stessa strategia. Entrambe si arrestano alla prima riga coarse dopo wrong-Doppler. La chiamata cold prepara una nuova epoca e può ricostruire lo spettro intero. La warm riutilizza la cattura e lo spettro. Il delta comprende quindi anche questo eventuale costo e l'effetto cache. La misura della replica è riportata a parte per leggerne l'ordine di grandezza. Il metodo vale per la fixture IF=0 e verifica il punto di arresto tramite il numero di chiamate al predicato di annullamento.
- **Ricerca completa** è la chiamata cold della strategia reale, fino alla conferma, su un PRN e senza mappa di visualizzazione. Esclude creazione del workspace, scheduler, ring e latenza di consegna.

## Risultati Release

| N | Modalità | Replica ms | PCPS ms | Preparazione stimata ms | Ricerca completa ms | PCPS / float | Prep / float | Ricerca / float |
|---:|---|---:|---:|---:|---:|---|---|---|
| 8192 | float | 0.0469 | 0.8619 | 9.1154 | 276.9065 | 1.000/1.000/1.000 | 1.000/1.000/1.000 | 1.000/1.000/1.000 |
| 8192 | q14 | 0.0494 | 1.6613 | 9.2662 | 384.2811 | 1.882/1.749/1.981 | 1.019/0.995/1.043 | 1.387/1.362/1.448 |
| 8192 | int32 | 0.5308 | 9.9315 | 9.8193 | 1380.1172 | 11.525/10.214/11.891 | 1.084/0.962/1.172 | 5.028/4.897/5.088 |
| 16384 | float | 0.1054 | 1.9648 | 18.7296 | 608.8642 | 1.000/1.000/1.000 | 1.000/1.000/1.000 | 1.000/1.000/1.000 |
| 16384 | q14 | 0.0885 | 3.3377 | 19.0265 | 776.5429 | 1.707/1.678/1.853 | 1.002/0.968/1.048 | 1.282/1.207/1.311 |
| 16384 | int32 | 1.1121 | 21.1780 | 20.2062 | 2868.9514 | 10.760/10.590/12.167 | 1.097/1.008/1.223 | 4.834/4.577/5.134 |

Per i rapporti, la forma è mediana / limite inferiore / limite superiore. Tutte le 144 ricerche cronometrate hanno confermato Galileo PRN 7 con 3 successi, Doppler raffinato 1267 Hz e fase 3968,620605 chip. Il float è risultato più rapido su entrambe le dimensioni nei confronti del kernel e della ricerca completa. Il confronto misura il costo delle implementazioni presenti e vale per questa configurazione.

| N | Doppler Hz | Modalità | Ritardo picco | Rapporto ampiezza | Rapporto potenza totale | Errore L2 % | Errore massimo / picco % | Tolleranza esplorativa |
|---:|---:|---|---:|---:|---:|---:|---:|---|
| 8192 | 1264 | float | 247 | 1.000000 | 1.000000 | 0.00002411 | 0.00002518 | within |
| 8192 | 1264 | q14 | 247 | 1.003314 | 1.032678 | 1.020 | 0.6638 | within |
| 8192 | 1264 | int32 | 247 | 0.990681 | 0.882167 | 1.967 | 1.855 | outside |
| 8192 | 22000 | float | 7212 | 1.000000 | 1.000000 | 0.00002124 | 0.00002476 | within |
| 8192 | 22000 | q14 | 7212 | 1.010173 | 1.025150 | 11.31 | 25.18 | outside |
| 8192 | 22000 | int32 | 7212 | 0.966398 | 0.858516 | 12.68 | 8.523 | outside |
| 16384 | 1264 | float | 494 | 1.000000 | 1.000000 | 0.00002287 | 0.00002378 | within |
| 16384 | 1264 | q14 | 494 | 1.004231 | 1.071703 | 1.347 | 0.8480 | within |
| 16384 | 1264 | int32 | 494 | 0.990670 | 0.837921 | 2.011 | 1.857 | outside |
| 16384 | 22000 | float | 2632 | 1.000000 | 1.000000 | 0.00002141 | 0.00002802 | within |
| 16384 | 22000 | q14 | 2632 | 1.047330 | 1.048691 | 15.83 | 33.31 | outside |
| 16384 | 22000 | int32 | 2632 | 0.960941 | 0.820649 | 15.20 | 7.694 | outside |

Il picco coincide con l'oracolo in tutte le mappe. A Doppler corretto, Q14 si discosta di circa +0,33/+0,42% in ampiezza di picco e +3,27/+7,17% in potenza totale. Int32 si discosta di circa −0,93% in ampiezza di picco e −11,78/−16,21% in potenza totale. A wrong-Doppler entrambe le modalità intere superano il criterio esplorativo L2. Il float resta entro 3×10⁻⁷ in L2 relativa e nel massimo errore normalizzato al picco.

I limiti esplorativi del banco sulle due metriche di errore sono 0,01% float, 5% Q14 e 0,2% int32 con controllo della posizione del massimo. Per il contratto automatico il float deve soddisfare il limite stretto. Per le modalità intere il test richiede potenze finite e non negative, picco del segnale corretto e scarta errori di scala gravi mediante un limite del 50% sulle due metriche. Gli scarti più piccoli restano visibili nei record e non modificano le soglie operative. Una conferma coincidente per questa cattura lascia aperte sensibilità, rumore e falsi allarmi su segnali deboli o in altre condizioni.

## Copertura

Tutti e 29 i CTest Release passano fra la [suite completa](ctest-release.txt) e la [ripetizione di build_dependency_contract](ctest-build-contract.txt). Quel test ha richiesto la riesecuzione fuori dalla sandbox a causa di un accesso negato di MSBuild alla directory temporanea della fixture. Il nuovo `acquisition_pcps_map_contract` è passato in 0,68 s nella suite completa. L'eseguibile misurato ha SHA-256 `3e094cc782a6c5aeb2410b5b3bf4cdf9abe1b51c6bf78330ca589a28525f2ac4`.

Build Release normale ricontrollata dopo il benchmark. `NGENE_TRACKING_BENCHMARK`, `NGENE_FFT_REFERENCE`, `NGENE_SAMPLE_REFERENCE` e `NGENE_SANITIZERS` risultano `OFF`. `ngene --version` ha restituito 0.6.4 Windows/x86_64 con FFT SSE2-assembly e i quattro frontend nativi. Il driver F1 è collegato solo al target `ngene_acquisition_tests`.

Il controllo della mappa usa la CI8 sintetica a IF zero. I test esistenti di campioni, ingresso, tracking e acquisizione coprono separatamente conversioni, IF/inversione, CI16, GPS/Galileo, rumore e conferma con cattura parziale. Questa campagna non costituisce un confronto mappe per ogni formato, una validazione RF, una prova di equivalenza con Classic o una misura su ARM. Il richiamo a Classic riguarda qui la sequenza e il percorso Q14 di riferimento documentati nel piano. Rimangono aperti i percentili di coda, la misura isolata di Prepare per altre IF e la ripetizione del confronto su altri host.
