# F2b · Preparazione delle catture e memoria per worker

22 novembre 2026. Baseline EVO `970c0dc` con F1 e F2a nel working tree.
Confronto Windows x64 Release multisdr, MSVC, Core i7-1355U, backend Q14
SSE2-assembly. I quattro binari del banco compilano gli stessi driver con
revisioni successive dei sorgenti operativi.

| Variante | Intervento rispetto alla precedente |
|---|---|
| 0 | Baseline F2a. |
| 1 | In GPS e Galileo si esegue un solo percorso di compressione. Con IF non nulla si usa il percorso dopo il mixer e si controlla l'annullamento prima del primo periodo e dopo ogni periodo. La preparazione invalida il risultato precedente finché la nuova cattura è incompleta. |
| 2 | Il workspace seleziona l'aritmetica alla creazione. In modalità float salta la scansione per la scala intera e la quantizzazione. La prova mantiene le allocazioni precedenti. |
| 3 | La modalità float alloca il proprio scratch e la banca spettrale. Le modalità intere allocano i campioni quantizzati e il solo piano Q14 o int32. La baseband float resta disponibile per il raffinamento Doppler. Il costruttore FFT pubblico continua a fornire tutti i percorsi ai chiamanti che cambiano modalità. |

## Metodo

Il target esplicito `ngene_prepare_lab` è escluso da `all` e richiama
direttamente le funzioni statiche `Prepare` nelle copie compilate dal banco.
Il timer esclude acquisizione dei campioni, creazione dei thread, FFT PCPS e
refinement. La misura della creazione include in sequenza tutti i workspace
richiesti e le tabelle dei loro piani, come nella creazione dei workspace del
pool. I byte allocati sono la somma degli argomenti di `calloc` e `malloc`
per workspace, search FFT e FFT Q14, esclusi overhead dell'allocatore e
cattura condivisa. Il working set è la differenza rispetto allo stesso
processo con fixture già residente, misurata dopo la creazione e dopo
`Prepare` e una PCPS wrong-Doppler per workspace. Include pagine del
processo e del runtime raggiunte in questi passaggi.

La matrice temporale usa GPS N=4096 e Galileo N=16384 a 8.183.800 coppie/s,
IF 0 con CI8 e IF +38500 Hz con CI16 e inversione di spettro. Le differenze
tra profili includono queste convenzioni. Il confronto tra varianti tiene
fisse tutte le proprietà della cattura. PRN 7, Doppler +1234 Hz, fase
123,42 chip e seme 20261121. I worker ricevono gli stessi campioni già
decodificati e li elaborano su workspace esclusivi.

Per ogni profilo e variante, quattro processi freschi in ordine bilanciato.
Affinità di processo ereditata `0xF0`, CPU logiche 4–7. I processi di
misura sono sequenziali e ciascuna prova usa uno o quattro thread avviati
insieme. Frequenza e turbo seguono le impostazioni dell'host. Ogni
processo usa tre warm-up e dodici prove per worker. Le mediane di
`Prepare` e di annullamento sono mediane delle mediane di processo.
I rapporti sono accoppiati per ripetizione e includono minimo e massimo.
I p95 descrivono le osservazioni aggregate del banco.

Il banco simula una richiesta di annullamento dopo 100 µs per GPS e 400 µs
per Galileo, cronometra il tempo residuo fino a `NG_END` e ricontrolla il
retry. Un secondo indicatore registra il maggiore intervallo tra chiamate
al predicato, includendo ingresso, ultimo tratto e ritorno. La misura
riguarda `Prepare` isolata, senza coda del pool o preemption del ricevitore.

Per la parità numerica, la strategia reale viene chiamata su una cattura
già preparata. Il banco traccia ogni riga PCPS a risoluzione FFT originale
prima del bin successivo, verifica potenze finite e non negative e confronta
l'impronta a 64 bit dell'intera mappa, inclusa la riga wrong-Doppler.
Confronta inoltre somma del rumore wrong-Doppler, soglia, picco coarse,
Doppler e codice raffinati e successi di conferma. La matrice include
segnale completo, rumore e segnale troncato. I casi Galileo aggiuntivi
coprono 8192 con IF negativa e 4096 con rimozione fissa della sottoportante.

## Risultati float

Le sequenze riportano le varianti 0 → 1 → 2 → 3. La creazione e il
working set sono misure complessive dei workspace del processo. Il
tempo di `Prepare` è per worker.

| Segnale | IF Hz | Worker | Prepare ms | Creazione ms | Allocati MiB, 0 → 3 | Residenti a caldo MiB, 0 → 3 |
|---|---:|---:|---|---|---|---|
| GPS | 0 | 1 | 6,67 → 6,63 → 2,99 → 3,00 | 2,80 → 2,71 → 2,70 → 2,07 | 4,89 → 3,80 | 4,95 → 3,84 |
| GPS | 0 | 4 | 6,58 → 6,62 → 2,97 → 2,98 | 10,85 → 10,87 → 10,86 → 8,62 | 19,56 → 15,19 | 19,66 → 15,27 |
| GPS | +38500 | 1 | 10,91 → 8,17 → 4,42 → 4,48 | 2,73 → 2,71 → 2,75 → 2,10 | 4,89 → 3,80 | 4,95 → 3,84 |
| GPS | +38500 | 4 | 10,38 → 8,11 → 4,48 → 4,81 | 11,87 → 11,39 → 12,37 → 9,07 | 19,56 → 15,19 | 19,66 → 15,27 |
| Galileo | 0 | 1 | 28,64 → 29,17 → 13,02 → 13,33 | 19,99 → 19,50 → 18,33 → 16,22 | 18,20 → 13,83 | 17,28 → 12,89 |
| Galileo | 0 | 4 | 28,61 → 29,87 → 14,04 → 13,07 | 81,49 → 110,04 → 85,38 → 88,47 | 72,81 → 55,31 | 68,94 → 51,41 |
| Galileo | +38500 | 1 | 72,94 → 62,01 → 33,03 → 32,67 | 31,20 → 38,44 → 40,48 → 30,03 | 18,20 → 13,83 | 17,28 → 12,89 |
| Galileo | +38500 | 4 | 80,55 → 60,48 → 34,84 → 34,36 | 133,88 → 142,04 → 130,79 → 125,57 | 72,81 → 55,31 | 68,94 → 51,41 |

Nel profilo Galileo a quattro worker e IF +38500 Hz, il p95 del maggiore
intervallo fra controlli passa da 37,64 a 10,55 ms. Il p95 del ritardo di
annullamento alla scadenza scelta passa da 7,40 a 8,47 ms. L'aumento
osservato in quest'ultima metrica resta visibile. La scadenza cade spesso
durante il calcolo iniziale della media, eseguito prima del primo
controllo in entrambe le versioni. Le misure a quattro worker risentono
anche degli intervalli di scheduling. La creazione float dei quattro
workspace Galileo a IF zero ha una mediana finale più alta della baseline.
La riduzione di memoria è misurabile in tutti i profili.

## Percorsi interi e interpretazione

| Segnale | Modalità | Worker | Prepare ms, 0 → 3 | Creazione ms, 0 → 3 | Allocati MiB, 0 → 3 | Residenti a caldo MiB, 0 → 3 |
|---|---|---:|---|---|---|---|
| GPS | int32 | 4 | 10,37 → 8,07 | 11,15 → 3,66 | 19,56 → 14,81 | 19,66 → 14,89 |
| GPS | Q14 | 4 | 10,50 → 8,13 | 11,20 → 3,44 | 19,56 → 14,56 | 19,66 → 14,64 |
| Galileo | int32 | 4 | 43,12 → 35,92 | 77,66 → 6,47 | 72,81 → 53,81 | 68,94 → 49,92 |
| Galileo | Q14 | 4 | 43,68 → 36,39 | 87,46 → 5,82 | 72,81 → 52,81 | 68,94 → 48,91 |

La tabella riguarda IF +38500 Hz. La rimozione della compressione
sovrascritta riduce il tempo di preparazione nelle mediane appaiate
dei profili con IF non nulla. Per Galileo int32 a quattro worker i quattro
rapporti sono 0,80, 0,79, 1,24 e 1,001. L'omissione della
quantizzazione float riduce il tempo in tutte le ripetizioni degli otto
profili float. La variante 3 riduce la memoria soprattutto evitando la
banca spettrale float in modalità intera e la baseband quantizzata in float.
I valori di creazione float fluttuano con il costo delle tabelle e il
carico dell'host.

Nel caso Galileo +38500 Hz a quattro worker, il ritardo Q14 passa da
2,34 a 2,46 ms come mediana e da 3,10 a 16,25 ms come p95. Per int32
passa da 2,34 a 2,49 ms come mediana e da 2,83 a 6,50 ms come p95.
I percorsi interi continuano a quantizzare dopo l'ultimo controllo
di `Prepare`. Il p95 del massimo intervallo fra controlli per Q14
passa da 33,63 a 48,83 ms nel profilo a quattro worker. La misura
include le sospensioni dei thread e la fase finale di quantizzazione.

La ripetizione mirata Q14 sullo stesso profilo e con la stessa affinità
ha alternato le varianti 0 e 3 in quattro coppie di processi. Su 192
osservazioni per variante, la mediana è passata da 2,60 a 2,61 ms e il
p95 da 3,31 a 3,60 ms. Lo scarto fra le due campagne mostra variabilità
di coda. La latenza sotto contendibilità resta una metrica distinta
dal costo di calcolo. [Ripetizione Q14](cancellation-repeat.csv)

La [tabella completa delle 96 combinazioni](timing-summary.csv) include
tempi di creazione, preparazione, memoria allocata e residente, mediana
e p95 di annullamento e rapporti temporali per ognuna delle quattro
varianti. I codici delle modalità sono 0 float, 1 int32, 2 Q14.
[Dati grezzi](timing.csv.gz) · [Impronte dei sorgenti e dei binari](manifest.csv)

## Verifica numerica e build

Tutte le 27 combinazioni di fixture e modalità coincidono tra le quattro
varianti, per un totale di 108 ricerche. I 15 casi con segnale completo
confermano con 3 successi. I sei casi con rumore e i sei con segnale
troncato producono 0 successi. Coincidono le impronte delle mappe
complete, la somma wrong-Doppler, la soglia, il picco coarse e gli
output dei raffinamenti e della conferma. [Log numerico](verify.csv)

Gli output di `--verify-pcps` F1 e dei casi di acquisizione preesistenti
coincidono con F2a dopo la normalizzazione della codifica del log.
Gli scarti Q14 e int32 di F1 rispetto all'oracolo double rimangono
la baseline per F3. [Misure F1](../F1/README.md)

Replay CLI float con sidecar a 8.183.800 coppie/s, IF +38400 Hz e PRN 7
in entrambi i sistemi. I binari F2a e F2b elaborano le stesse 13.087.200
coppie in 1600 finestre, con zero gap, drop noti e scarti allo stop.
La cattura locale ha SHA-256
`f559ce60f4dfeed9ce142dd61eb248a456a6d88dc7503a94ec3ff24cc1532b92`.
Il replay dura circa 1,6 s e verifica la continuità di questo input.

Build Release normale e 30/30 CTest Windows x64. Il target del banco
compare soltanto nella build esplicita.
`NGENE_TRACKING_BENCHMARK`, `NGENE_FFT_REFERENCE`,
`NGENE_SAMPLE_REFERENCE` e `NGENE_SANITIZERS` risultano OFF.
`ngene --version` restituisce 0.6.4 con i quattro frontend nativi e FFT
SSE2-assembly. Restano aperte le verifiche RF live e Linux.
[Log CTest](ctest-release.txt)

| Binario Release finale | SHA-256 |
|---|---|
| `ngene.exe` | `d00b2a314a83d14ec3e9f9b24f16c3e7c09320749cac4b5ba542ddd33717cf01` |
| `ngene_acquisition_tests.exe` | `c18a2c6a3be81e15da55fcd80c992dc886b3b3d912cece68a83bebcf54c5c87e` |

## Riproduzione

Compilare `ngene_prepare_lab` in Release per ogni variante e conservare il
binario come `ngene_prepare_0.exe` fino a `ngene_prepare_3.exe` nella
directory Release. I sorgenti operativi intermedi usati nella campagna
sono in `build/f2b/source-0` fino a `build/f2b/source-3` nel checkout di
misura. Il target finale si compila da un checkout con F2b.

```powershell
cmake --build build/msvc-x64-multisdr --config Release --target ngene_prepare_lab
$proc = [Diagnostics.Process]::GetCurrentProcess()
$proc.ProcessorAffinity = 0xF0
node development/profiling/F2b/run.cjs build/msvc-x64-multisdr/Release timing > build/f2b/timing.txt
node development/profiling/F2b/summarize.cjs build/f2b/timing.txt
node development/profiling/F2b/run.cjs build/msvc-x64-multisdr/Release verify
```
