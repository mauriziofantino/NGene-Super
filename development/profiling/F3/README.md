# F3 · PCPS e riduzione spettrale intera

22 novembre 2026. Baseline EVO `970c0dc` sul ramo
`chatGPT/fast-arithmetic`, con F1, F2a e F2b nel working tree.
Windows x64 Release multisdr, MSVC, Core i7-1355U, FFT Q14 SSE2-assembly.
La compensazione Doppler per integrazioni coerenti estese resta in F6a,
quando la deriva di codice è sufficiente a causare una perdita misurabile.

## Misura delle componenti

Il banco genera copie locali di `gps.c`, `galileo.c` e `search.c`
con timer attorno ai passaggi effettivi. Misura direttamente `Prepare`
e, per una riga wrong-Doppler, generazione portante, copia e mixer float
o quantizzazione e rotazione intera, FFT diretta, prodotto spettrale
con ricerca del massimo in modalità intera, scelta dello shift e
packing intero, FFT inversa e accumulo della potenza. I due tempi FFT
sommano le otto trasformate della rispettiva direzione. La differenza
tra riga intera e componenti include azzeramento, replica intera
ricostruita dopo `Prepare` e overhead del banco. La misura delle
componenti è una media per riga su 12 prove dopo 3 warm-up.
[Profilo per componente](profile.csv)

La fixture ha PRN 7, Doppler +1234 Hz, rumore a seme 20261121,
Fs 4,096 Msps, IF +38500 Hz, CI16 e spettro invertito. GPS usa N=4096
e Galileo N=16384. Affinità di processo ereditata `0x0F`, CPU logiche
0–3. Turbo e frequenza seguono le impostazioni dell'host. I confronti
prima/dopo dentro ciascun profilo verificano l'impronta dei campioni.

In Q14 la fase di scelta dello shift e packing occupava circa il
20% della riga GPS e il 19% della riga Galileo. Il cambiamento riguarda
le due divisioni per bin nel packing. Lo shift opera sul modulo
unsigned, seguito dal ripristino del segno. Per un divisore positivo
pari a `2^shift`, questo conserva il troncamento verso zero anche per
prodotti negativi con resto. Il massimo scelto con la riduzione
esistente vincola il risultato a ±8192. Prodotti coniugati, scelta
dello shift, scala `ldexp`, FFT e accumulo mantengono le operazioni
precedenti. Il packing Q14 passa da 147,0 a 67,4 µs per riga GPS e da
610,4 a 272,7 µs per riga Galileo. Nessuno scratch aggiuntivo.

## Tempi senza strumentazione interna

`ngene_pcps_lab` misura uno e quattro worker simultanei con workspace
esclusivi. Per profilo, quattro coppie di processi freschi in ordine
A/B e B/A. Dopo una preparazione e una riga di riscaldamento per worker,
ogni processo misura 12 righe warm, 24 annullamenti e una ricerca
completa per worker. La ricerca prepara una nuova epoca e procede
fino alla conferma, con opzione di visualizzazione spenta. Il timer
include preparazione, coarse e raffinamenti presso il worker e
esclude ring, lancio dei thread e consegna del risultato.

Per l'annullamento si usa una cattura già preparata. Il primo controllo
dopo wrong-Doppler fissa una scadenza a +10 µs e il banco misura da
quella scadenza al ritorno con `NG_END`. Questo campiona la coda di
annullamento durante una riga coarse. La distribuzione include attese
dello scheduler, soprattutto con quattro worker. I p95 sono descrittivi.

I rapporti temporali sono accoppiati fra le mediane di processo.
Nel CSV la riga comprende otto periodi e `N` bin per periodo.
Il test di memoria somma i byte richiesti all'allocatore dai workspace
e dai piani, con la cattura condivisa esclusa.
[Tempi e memoria](timing-summary.csv) · [Dati grezzi](timing.csv.gz)

### Risultati Q14

| Segnale | Worker | Riga PCPS ms | Ricerca completa ms | Annullamento p95 ms |
|---|---:|---|---|---|
| GPS | 1 | 0,800 → 0,710 | 41,79 → 40,12 | 0,867 → 0,797 |
| GPS | 4 | 1,358 → 1,162 | 61,54 → 56,04 | 1,375 → 1,255 |
| Galileo | 1 | 3,409 → 3,092 | 788,38 → 752,94 | 3,736 → 4,793 |
| Galileo | 4 | 4,940 → 4,795 | 1189,35 → 1170,89 | 6,935 → 6,741 |

A un worker tutte le quattro coppie Q14 migliorano sia nella riga
sia nella ricerca completa per entrambi i segnali. A quattro
worker tutte le coppie della riga GPS e della ricerca Galileo
migliorano. La riga Galileo e la ricerca GPS a quattro worker
mostrano anche coppie più lente. Il p95 di annullamento peggiora
nel profilo Galileo Q14 a un worker durante la prima campagna.
Per int32 il packing strumentato diminuisce del 51–56%. La ricerca
int32 mostra differenze piccole e variabili nella campagna completa,
con un peggioramento della mediana in Galileo a un worker.
I profili float forniscono un controllo della variabilità dell'host.
I byte allocati coincidono in ogni abbinamento di segnale, modalità
e numero di worker.

## Verifica

Le 27 combinazioni di fixture e modalità coincidono prima/dopo, su
54 ricerche. Il confronto comprende le impronte delle mappe complete
a risoluzione FFT, la potenza ripristinata, la somma wrong-Doppler,
la soglia e il picco coarse, Doppler e codice raffinati e la conferma.
I 15 profili con segnale completo confermano con tre successi.
I sei profili di solo rumore e i sei con segnale troncato registrano
zero successi. I test includono CI8 e CI16, IF nulla e non nulla,
IF negativa e la configurazione Galileo N=4096. Gli output della
regressione di acquisizione e di `--verify-pcps` F1 coincidono con F2b,
inclusi gli scarti interi misurati rispetto all'oracolo double.
[Log numerico](verify.csv)

Replay CLI con Q14 per entrambi i sistemi su una cattura locale da
13.087.200 coppie. Stesso sidecar a 8.183.800 coppie/s con
IF +38400 Hz e stesso PRN 7 prima/dopo. Entrambi i binari elaborano
tutte le coppie in 1600 finestre con zero gap, drop noti e scarti allo
stop. SHA-256 della cattura
`f559ce60f4dfeed9ce142dd61eb248a456a6d88dc7503a94ec3ff24cc1532b92`.

Build normale Windows x64 Release, **30/30 CTest**. `ngene --version`
riporta versione 0.6.4, FFT SSE2-assembly e i quattro frontend nativi.
`NGENE_TRACKING_BENCHMARK`, `NGENE_FFT_REFERENCE`,
`NGENE_SAMPLE_REFERENCE` e `NGENE_SANITIZERS` sono OFF.
Il log della build normale esclude i target del banco.
Restano aperte la validazione Linux e la verifica RF live.
[Log CTest](ctest-release.txt) · [Hash dei sorgenti e degli eseguibili](manifest.csv)

### Indicazioni per F4

Nel profilo F2b, le FFT diretta e inversa coprono circa 75–81% della
riga float e 80–82% della riga int32. In Q14 la quota è circa 19–20%
e la quantizzazione con rotazione della portante pesa circa 45–47%.
In F4 distinguere butterfly, permutazione e normalizzazione, iniziando
da un collo misurato in float o int32. Qualificare la FFT int32 contro
il percorso scalare con gli stessi arrotondamenti signed Q30 e la
stessa scala dell'inversa. Tenere separata la prova Q14.

## Riproduzione

Il sorgente operativo F2b usato prima della modifica è conservato
in `build/f3/search-f2b.c` nel checkout di misura. Per entrambe le
revisioni generare i sorgenti del profilo, compilare i due target
espliciti e copiare i binari con suffisso `_before` o `_after`.
I target sono esclusi dalla build normale.

```powershell
node development/profiling/F3/generate-profile.cjs build/msvc-x64-multisdr/generated/pcps-profile
cmake --build build/msvc-x64-multisdr --config Release --target ngene_pcps_lab ngene_pcps_profile
$proc = [Diagnostics.Process]::GetCurrentProcess()
$proc.ProcessorAffinity = 0x0f
node development/profiling/F3/run.cjs profile
node development/profiling/F3/run.cjs timing > build/f3/timing.txt
node development/profiling/F3/run.cjs summary build/f3/timing.txt
node development/profiling/F3/run.cjs verify
```
