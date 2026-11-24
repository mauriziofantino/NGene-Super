# F6 · Replay dello scheduling GPS/Galileo

Il confronto usa il pool condiviso e il batching già presenti in EVO. L'aritmetica,
la configurazione dei canali (8 GPS, 4 Galileo) e la cattura restano fisse. Si
variano i worker tracking (1 o 4) e il budget acquisizione (1 o 4).

## Riproduzione

```powershell
./development/profiling/F6/replay-matrix.ps1 -Executable <ngene.exe> -Capture <RTL-CI8.cs8> -Output build/f6-validation -Seconds 60 -Pairs 1
node development/profiling/F6/summarize.cjs build/f6-validation --events
```

Usare la cattura CI8 a 2,4 Msps con sidecar in
`build/performance-20261115/rtl-repro/`. Ogni esecuzione parte da un nuovo
processo e legge la cattura dall'inizio. Il runner controlla il numero esatto di
coppie processate. Registra timeline del servizio web ogni 250 ms circa, tempo
muro e CPU di processo. I campioni possono essere osservati in ritardo rispetto
all'elaborazione. Il p95 di tempo muro meno durata del segnale misura il ritardo
osservato, inclusi startup e polling. Le deadline dei singoli job non sono
esposte dal servizio. Le colonne per costellazione contano i poll in cui la
costellazione era attiva mentre il pool condiviso aveva più job assegnati che in
esecuzione. Un poll può contribuire a entrambe le colonne.

`--events` confronta le conferme osservate per costellazione, PRN, epoca e
primo campione della cattura di acquisizione. Confronta anche gli indici campione
PVT. Il polling può perdere eventi fra due richieste e non consente di dimostrare
parità esatta del flusso.

## Baseline a 30 secondi

Otto replay Windows conservati in `build/f6-matrix`, due per configurazione.
Tutti processano 72 milioni di coppie con zero gap, campioni scartati o persi e
un picco di un blocco nel ring. Nessun PVT osservato nei primi 30 secondi.

| Budget acquisizione | CPU media, core equivalenti | Poll con job assegnati in attesa | Prima conferma Galileo, tempo segnale osservato |
| ---: | ---: | ---: | ---: |
| 1 | 1,253–1,479 | 73–99 su 105–107 | 5,97–8,26 s |
| 4 | 1,941–2,582 | 6–10 su 106–110 | 0,97–2,69 s |

Il limite di esecuzione dell'acquisizione emerge anche con un solo worker di
tracking. Il budget 4 consuma più CPU e anticipa la prima conferma Galileo su
questa cattura. Gli insiemi di eventi osservati differiscono anche tra repliche
della stessa configurazione. Per questo confronto non si modifica il default.

## Verifica a 60 secondi

Windows x64 Release, cattura CI8 a 2,4 Msps. Tutti e quattro i replay leggono
l'intervallo `[0, 144000000)` con zero gap, perdite o scarti e un blocco di
picco nel ring. Il primo PVT osservato è al campione 122 416 000 in tutte le
prove. Coincidono anche i quattro indici PVT distinti osservati, e l'insieme
di 11 PRN che raggiungono la PLL durante il replay.

| Worker tracking | Budget acquisizione | CPU media, core equivalenti | Poll con contesa | Primo PVT, tempo muro | Poll con PVT |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1 | 1,425 | 172/220 | 51,502 s | 15/220 |
| 1 | 4 | 1,747 | 125/204 | 52,934 s | 15/204 |
| 4 | 1 | 1,427 | 147/216 | 51,190 s | 14/216 |
| 4 | 4 | 2,234 | 20/214 | 51,421 s | 14/214 |

Con quattro worker, alzare il budget ha ridotto i poll con contesa e anticipato
la prima conferma Galileo da 2,30 a 1,97 secondi nel tempo del segnale. Con un
worker e budget 4 è comparso un ritardo transitorio nel campione pubblicato
(p95 4,443 s contro 0,306 s con budget 1), recuperato entro fine replay.
Serve una replica del caso per attribuirlo allo scheduling.

Ogni variante condivide 10 delle 40 conferme osservate nella configurazione
di riferimento allo stesso indice di inizio cattura. La loro parità rimane
aperta. Il replay giustifica il mantenimento dei valori predefiniti. Restano
da misurare le deadline e le attese effettive per job con una traccia offline
allineata ai campioni.

Timeline in `build/f6-validation`. SHA-256 cattura
`AA82593A27AEE92CDE07F964D9D4E41908BC129BF228A9F05A0245BAFDF22620`.
SHA-256 del binario Release
`18B414A9DD59659E3175C99C0F3827F902735B62E59E8D3F8C45B91A9FAB731C`.
CTest Windows Release: 4/4 contratti superati (pool acquisizione, tracking,
replay e dispatcher).

## Traccia per ricerca e confronto F4 int32

Compilare una build di sviluppo con `-DNGENE_PROFILE_ACQUISITION_JOBS=ON`.
La traccia scrive su stderr una riga per decisione, prima dell'eventuale
calcolo della mappa per la GUI. Per ogni ricerca riporta costellazione,
epoca, primo campione e PRN, esito e tre tempi. La prima attesa va dalla
pubblicazione della cattura alla presa in carico del PRN. La seconda
somma le attese ai confini di stadio per mutex e permessi del pool,
escludendo la pausa esplicita. Il terzo tempo va dalla pubblicazione
della cattura al completamento della decisione.

Per ripetere il confronto, usare il runner con `-Int32Fft -Seconds 30
-Pairs 1 -OnlyTrackingWorkers 1 -OnlyAcqBudget 1` per ciascun binario, poi
eseguire `node development/profiling/F6/jobs.cjs <stderr-candidato>
<stderr-riferimento>`. Un terzo argomento opzionale in millisecondi conta
le decisioni oltre una soglia sperimentale. Il runner abilita la stessa
aritmetica int32 per GPS e Galileo. La build ordinaria usa l'opzione di trace
disattivata.

Su Windows x64 entrambi i binari elaborano 72 milioni di coppie senza gap.
Le 46 decisioni pubblicate con chiavi comuni coincidono in esito, conferma,
Doppler, fase e rapporto di picco. Le chiavi pubblicate solo da SSE2 sono
194 e quelle solo da AVX2 sono 140. Una sola coppia di replay fornisce
indicazioni sulle attese. Anche la scrittura della traccia può influire sullo
scheduling. Le differenze di ammissione, la ripetibilità
delle latenze e il rispetto delle deadline operative restano da verificare.

| Costellazione | Backend | Attesa presa in carico p95 | Attesa permesso p95 | Tempo decisione p95 |
| --- | --- | ---: | ---: | ---: |
| GPS | SSE2 | 5,58 s | 0,60 s | 6,01 s |
| GPS | AVX2 | 10,21 s | 0,76 s | 10,82 s |
| Galileo | SSE2 | 10,86 s | 4,46 s | 14,04 s |
| Galileo | AVX2 | 8,48 s | 3,47 s | 11,92 s |

Artefatti locali in `build/f6-job-trace-OFF/` e `build/f6-job-trace-ON/`.
