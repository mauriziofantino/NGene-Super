# B8–B10 · Qualifica e interfaccia BeiDou B1C

26 novembre 2026, EVO `2ae930f1a62c762826114b4a6c013960a57f9a50` con le modifiche locali B8–B10.

## B8 · Replay riproducibile

Il banco usa il servizio HTTP locale della build operativa e un processo nuovo per
ciascuna prova. Registra gli SHA-256 di eseguibile, cattura, profilo, metadati e
riferimento, quando fornito. Conserva l'INI effettivo, la timeline e il riepilogo
JSON. I due avvii a freddo partono dal campione zero e leggono lo stesso
intervallo. Il terzo riavvolge lo stesso intervallo, genera un gap e continua per
almeno metà intervallo nell'epoca successiva. Il gap simula un reset di
continuità da replay, con contatore di campioni persi pari a zero.

```powershell
$capture = (Get-ChildItem build/performance-20261115/rtl-repro -Filter '*.cs8').FullName
node development/profiling/B8/replay.cjs build/f4-qualification-release/ngene.exe development/profiling/B8/gps-galileo-rtl.ini $capture build/b8-rtl-reproduction 60
```

Il comando richiede Node 18 o successivo e una build con Web attivo. Funziona
anche su Linux e macOS passando il percorso dell'eseguibile nativo. Il profilo
versionato riprende le impostazioni effettive della prova Windows. Per una
cattura con sidecar i parametri di flusso vengono letti dal ricevitore con
`--replay`. Per una cattura senza sidecar vanno dichiarati nel profilo.

L'ultimo argomento facoltativo è un JSON con posizione statica `position` oppure
traiettoria `epochs`. Ogni posizione contiene `latitude_deg`, `longitude_deg`
e `height_m` ellissoidica WGS84, e ogni record di traiettoria aggiunge `epoch` e
`sample` come stringhe decimali sull'asse campioni del ricevitore. Gli errori
sono distanze ECEF 3D sulle chiavi comuni, senza interpolazione. Le differenze
tra i due replay a freddo sono registrate a parte. TTFF e spaziatura massima tra
fix usano gli indici campione per epoca. Sono misure dei fix osservati dal
polling di stato nominale a 200 ms.

Per indagare le differenze tra avvii, usare una build separata con
`NGENE_PROFILE_ACQUISITION_JOBS=ON` e lo stesso profilo di replay. Al termine
di ogni prova, analizzare la cartella del singolo avvio con:

```powershell
node development/profiling/B8/acquisition-trace.cjs build/b8-rtl-traced-20261126/cold-1
```

Il rapporto per costellazione, PRN ed epoca riporta le decisioni F6, il
campione iniziale della cattura, le attese e la prima integrazione osservata
per ogni assegnazione di tracking. Le conferme presenti nella timeline si
associano alla decisione mediante il campione iniziale della cattura.
Il tempo fino alla decisione include le attese e le pause durante la ricerca.
Il polling rileva assegnazioni e integrazioni visibili al momento dello
snapshot. La timeline registra anche ricerche completate e attive, PRN in
ricerca e campioni in attesa all'ingresso.

Per la campagna B1C aggiungere `--require-b1c` dopo il percorso del riferimento.
Il banco richiede la capability operativa e almeno un fix osservato con
satelliti usati di tutti e tre i sistemi e riferimento corrispondente in ogni
avvio a freddo e in entrambe le epoche del replay con gap. Il riepilogo elenca
le evidenze mancanti e il comando termina con esito non riuscito se ne manca
una. Le metriche di accuratezza e continuità restano disponibili per la
valutazione del risultato. La prova con la build GPS/Galileo ha verificato
il rifiuto prima dell'avvio del replay.

### Risultati Windows x64

Cattura storica RTL E4000, CI8 a 2,4 Msps, 1575,42 MHz, 155.978.080 coppie
I/Q. Profilo float, un worker di tracking e budget di acquisizione di un core.
La cattura ha SHA-256 `aa82593a27aee92cde07f964d9d4e41908bc129bf228a9f05a0245bafdf22620`
e il sidecar `44d1165edfa8357d0c0dd7cf0c7baab0915965d766fa7fc8e41660149dbb9cb0`.
Risultati locali in `build/b8-rtl-20261126`.

| Prova | Coppie I/Q | Gap | Campioni persi riportati | PRN con integrazioni GPS / Galileo | Fix osservati |
|---|---:|---:|---:|---:|---:|
| Freddo 1 | 144.000.000 | 0 | 0 | 6 / 5 | 0 |
| Freddo 2 | 144.000.000 | 0 | 0 | 0 / 0 | 0 |
| Riavvolgimento | 216.703.200 | 1 | 0 | 7 / 6 | 0 |

La seconda epoca del riavvolgimento ha integrato 7 PRN GPS e 4 Galileo. Il TOW
Galileo è stato osservato per 4 PRN nel corso della prova. In queste esecuzioni
sono variate le assegnazioni e sono rimasti indisponibili TTFF, continuità del
PVT ed errore su epoche comuni. La cattura non dispone di un riferimento
indipendente associato. [Misure storiche della cattura](../../../docs/service/PERFORMANCE_NAVIGATION_2026-11-15.md)

### Ripetizione con traccia per PRN

Sulla stessa cattura, con gli stessi parametri del profilo effettivo e un
eseguibile Release di hash diverso da quello storico, i due nuovi avvii
senza profilazione hanno integrato 8/7 e 6/8 PRN GPS/Galileo. La prima
integrazione è stata osservata rispettivamente a 0,314 e 5,701 s di segnale.
Entrambi hanno elaborato 144 milioni di coppie senza gap, perdite o fix.
I dati sono in `build/b8-rtl-followup-20261126`.

Con la traccia F6 attiva, i due avvii hanno elaborato lo stesso intervallo
integrando 7/4 e 6/4 PRN, con quattro fix osservati per avvio. I picchi
osservati della coda d'ingresso sono stati
57.600 e 304.800 coppie (24 e 127 ms di segnale). La prima integrazione
Galileo è stata osservata a 6,152 e 6,312 s di segnale.

| Sistema | Prova | Decisioni pubblicate | p95 attesa prima della presa in carico | p95 attesa dei permessi | p95 tempo fino alla decisione |
|---|---|---:|---:|---:|---:|
| GPS | Freddo 1 | 5993 | 0,147 s | 0,014 s | 0,163 s |
| GPS | Freddo 2 | 1924 | 1,674 s | 0,086 s | 1,718 s |
| Galileo | Freddo 1 | 48 | 5,287 s | 2,133 s | 7,790 s |
| Galileo | Freddo 2 | 48 | 7,210 s | 3,629 s | 9,562 s |

Le 71 decisioni con sistema, epoca, PRN e cattura in comune coincidono
per esito, conferma, Doppler, fase e rapporto del picco. Altre 5970 decisioni
sono presenti solo nel primo avvio e 1901 solo nel secondo. La traccia
quantifica l'attesa nella configurazione profilata. L'episodio storico con
zero integrazioni resta senza una traccia per PRN e la sua causa resta aperta.
I dati sono in `build/b8-rtl-traced-20261126`.
Il replay profilato con riavvolgimento ha elaborato 216.549.600 coppie
con un gap e zero campioni persi. In entrambe le epoche sono stati
osservati 6 PRN GPS e 4 Galileo con integrazioni.

La verifica sintetica B1C ha rieseguito anche il banco di acquisizione B2
con 28 ritardi e il banco di tracking B3 con PRN 1, 33 e 63. I 60 casi B3
con eco includono i falsi lock già osservati. I CSV sono in
`build/b8-synthetic-20261126`.

Il profilo legacy continua a selezionare GPS e Galileo. Il ricevitore può
abilitare anche BeiDou B1C con `[beidou_acquisition]` e `enabled=true`.
La cattura HackRF consente di misurare separatamente conferma B1C, tracking,
trame B-CNAV1 e osservazioni BDT usate dal PVT. La qualifica completa
richiede il riferimento indipendente e le ripetizioni previste dal banco.
[Verifica tecnica](../../../docs/service/CONSOLIDAMENTO_GNSS.md)

Il decoder B4 ora espone anche l'ingresso streaming `NgBcnv1Push`. Riceve
simboli morbidi da prompt dati completi e conserva l'indice campione di SF1
per B5–B6. Il test ripete polarità, frame traslata, discontinuità e cambio di
epoca, con indici superiori a 2⁵³. I cinque CTest mirati a B1C, PVT e replay hanno
superato la verifica Windows x64 in `b1c-stream.xml`.

## B9 · RF sul dispositivo

La HackRF collegata all'antenna GNSS è disponibile su Windows. La cattura
è stata eseguita in sola ricezione a 1575,42 MHz, con campionamento nominale
di 16.367.600 coppie/s, filtro da 15 MHz e formato CI8 interlacciato. Il
controllo iniziale a 40/42 dB LNA/VGA ha rilevato campioni agli estremi
dell'ADC. La cattura di riferimento usa 32/34 dB, amplificatore RF e
alimentazione antenna dalla HackRF disattivati.

Il file `captures/b1c-hackrf-20261126-163658-reference/hackrf-gnss-l1_000000.cs8`
contiene 1.963.999.216 coppie I/Q, pari a 119,993 s al rate nominale.
L'indice è continuo e il ricevitore ha riportato zero gap e zero perdite
durante la cattura. All'arresto sono state scartate 114.704 coppie ancora
in coda, successive all'ultimo campione scritto. Su cinque finestre
distribuite lungo il file non sono stati osservati valori agli estremi
dell'ADC. Profilo, metadati, indice, log e SHA-256 sono nella stessa cartella.
Il replay Windows ha riletto tutte le coppie senza gap né perdite. Nel
riepilogo finale dell'acquisizione risultano confermati 11 PRN GPS e
12 Galileo.

La qualifica B9 della catena BeiDou B1C richiede una verifica di acquisizione,
tracking e decodifica sulla cattura e un riferimento indipendente per valutare
l'errore di posizione.

La build Windows con HackRF ha inoltre elaborato 130.944.000 coppie in ricezione
diretta con BeiDou abilitato, quattro ricerche completate, zero gap e zero
perdite durante la prova breve. Profilo e binario locali in
`build/b1c-hackrf-release`, stato del ricevitore in
`build/b1c-hackrf-live-smoke/status.json`.

### Replay della catena B1C

Il profilo versionato `beidou-hackrf.ini` riproduce i parametri della prova
Windows. Su Linux si può avviare una singola prova a freddo:

```sh
node development/profiling/B8/replay.cjs build/linux-b1c/ngene \
  development/profiling/B8/beidou-hackrf.ini /percorso/cattura.cs8 \
  build/b8-linux-hackrf 115 --single
```

Per ripetere la qualifica con due avvii a freddo e un riavvolgimento, omettere
`--single`. Usare la cattura HackRF con il suo sidecar per tutte e tre le prove.
Per partire più avanti nel file, passare `--start-seconds=N`. Il runner
verifica il campione iniziale nel profilo effettivo e lo salva come
`file_start_sample`. Le epoche, i campioni della timeline e le eventuali
chiavi del riferimento usano l'origine del replay.
Su Linux `summary.json` registra il tempo CPU
del solo processo ricevitore, inclusi i suoi thread, dalla richiesta di avvio
alla prima osservazione dello stato fermo. `receiver_mean_cores` è il rapporto
tra secondi CPU e secondi di muro e vale circa 1 per un core occupato in media.
La timeline contiene coda d'ingresso, gap, perdite e ricerche per costellazione.
Il conteggio `beidou_fixes` richiede un satellite BeiDou effettivamente usato.

Il replay Windows di 115 s sulla cattura HackRF ha elaborato 1.882.274.000
coppie senza gap né perdite. Il profilo della prova assegna 20 slot (10
riservati a Galileo e 6 a BeiDou), 4 worker di tracking e 4 core di budget
per l'acquisizione. Le ricerche sono limitate ai PRN B1C 11–42 e Galileo
6–28 dopo la ricognizione RF iniziale. Il profilo operativo predefinito
ricerca i PRN B1C 1–63 quando abilitato.

- Acquisizione e tracking B1C osservati per i PRN 11, 27, 29, 30, 36, 39 e 42.
- Trame B-CNAV1 con CRC validi osservate sui PRN 11 e 42.
- Tredici soluzioni distinte per campione con una misura BeiDou marcata come
  usata. La prima a 30,005 s usa C11 con cinque Galileo. Le soluzioni
  successive con C42 usano otto Galileo.

Evidenze locali in `build/b1c-rf-operational-07` con hash degli ingressi,
profilo effettivo, timeline e snapshot PVT misto. La posizione di riferimento
e la verifica ripetuta di un fix a tre costellazioni restano aperte.

### Ripetibilità HackRF · 27 novembre 2026

Su Windows x64, stessa cattura e stesso profilo nei due avvii da 115 s
e nel replay con riavvolgimento. Artefatti in
`build/b8-hackrf-qualification-20261127`. La coda d'ingresso ha raggiunto
127 ms nel primo avvio, 47 ms nel secondo e 52 ms nel replay con gap.

| Prova | Fix PVT | Fix con BeiDou | Primo fix con BeiDou nel tempo dell'epoca |
|---|---:|---:|---:|
| Freddo 1 | 31 | 10 | 30,005 s |
| Freddo 2 | 0 | 0 | — |
| Riavvolgimento, prima del gap | 16 | 4 | 83,006 s |
| Riavvolgimento, dopo il gap | 14 | 0 | — |

Entrambi gli avvii a freddo hanno elaborato 1.882.274.000 coppie senza
gap. Il riavvolgimento ha elaborato 2.826.077.055 coppie con un gap
atteso. Il contatore delle perdite è rimasto a zero in tutte le prove.
Il secondo avvio ha confermato e tracciato otto PRN B1C e nessuno ha
prodotto una trama CRC valida. C11 ha dato lo stesso seme di acquisizione
in entrambi gli avvii, con inizio del tracking a 6,36 s e 13,83 s.
La trama valida osservata entro 29,31 s nel primo avvio era già iniziata
quando il secondo tracker ha preso in carico C11. La mancata decodifica
delle trame successive resta da indagare. Dopo il gap B1C ha ripreso
acquisizione e tracking. I 14 fix di quella seconda epoca usano Galileo.
Nessuna prova ha prodotto un fix con tutte e tre le costellazioni.
La campagna è stata eseguita senza una posizione di riferimento indipendente.

### Riproduzione isolata del tracking C11

Il replay del solo tracker usa i campioni CI8 della stessa cattura e il
seme originale. Si passa separatamente il campione di origine del seme e
quello da cui iniziare a elaborare. Dalla fine della cattura di acquisizione
da 570 ms al primo campione del tracker trascorrono 5,79 s di segnale nel
primo avvio e 13,26 s nel secondo. L'intervallo comprende attesa, ricerca e
consegna del seme.

| Condizione | Inizio tracking nella cattura | B-CNAV1 nel tracker isolato |
|---|---:|---|
| Primo avvio a freddo | 104.035.008 | Una trama valida, con SF1 al campione 171.116.200 e prima osservazione della decodifica al campione 477.644.517 |
| Secondo avvio a freddo | 226.336.704 | Zero trame valide nei 115 s esaminati |
| Seconda epoca del riavvolgimento | 445.553.327 | Zero trame valide entro il campione 943.803.055, corrispondente alla fine della prova B8 |

I primi due replay usano il seme al campione zero con Doppler 2820 Hz e
fase 5537,024900816342 chip. Per la seconda epoca gli indici sono riportati
all'origine del file. Il seme viene dal campione 248.860.235, con Doppler
2850 Hz e fase 10109,679676808146 chip. Al termine dell'intervallo della
seconda epoca, il tracker isolato era ancora in PLL. I risultati riproducono
l'esito della decodifica C11 senza scheduler. La causa della mancata
validazione delle trame successive resta aperta.

CSV locali in `build/b8-c11-isolated`. Per costruire lo strumento usare
`-DNGENE_TRACKING_BENCHMARK=ON` e il target `ngene_tracking_replay`.
Esempio del primo avvio:

```sh
ngene_tracking_replay cattura.cs8 16367600 3 11 2820 5537.024900816342 104035008 0
```

### Confronto su nuove catture e partenze diverse

Prove Windows x64 del 27 novembre 2026. Cinque finestre di 100 s, ciascuna
con processo nuovo. Stesso profilo di ricevitore con ricerca B1C sui PRN
1–63 e Galileo sui PRN 6–28. Ogni replay usa il proprio sidecar per il
formato e i parametri RF. Le registrazioni sono state raccolte in
successione e differiscono per condizioni RF, frontend e campionamento.
I dati, i profili effettivi, gli hash e le timeline sono locali in
`build/b8-frontend-matrix-20261127-123047`. Gli stream I/Q originali sono
in `captures/`, esclusa da Git.

| Frontend e cattura | Partenza nel file | PRN B1C in tracking / con trame valide | Fix con B1C | Fix a tre costellazioni | Primo fix con B1C |
|---|---:|---:|---:|---:|---:|
| HackRF 1 | 0 s | 9 / 4 | 13 | 12 | 87,006 s |
| HackRF 1 | 150 s | 8 / 6 | 37 | 31 | 36,005 s |
| HackRF 2 | 75 s | 6 / 5 | 39 | 37 | 56,005 s |
| RTL-SDR | 75 s | 6 / 6 | 49 | 49 | 33,004 s |
| SiGe | 75 s | 6 / 6 | 59 | 59 | 41,005 s |

Zero gap e zero campioni persi in tutte le finestre. Il tempo al primo fix
è misurato rispetto al campione iniziale del replay. L'uso di BeiDou nel
PVT è stato osservato su più PRN e sui tre frontend. Questa campagna copre
partenze diverse a freddo.

### Confronto con la posizione indipendente LNIKS

Per le catture del 27 novembre si usa il riferimento WGS84 dell'antenna
LNIKS fornito dall'utente. Le coordinate e i risultati per campione sono
conservati localmente in `build/b8-rtl-repeat-gap-20261127`. Il confronto
calcola l'errore ECEF 3D di ogni fix che usa effettivamente BeiDou.

| Finestra | RMS 3D, tutti i fix con B1C | RMS 3D, fix con GPS/Galileo/B1C | Massimo 3D, tutti i fix con B1C |
|---|---:|---:|---:|
| HackRF 1, 0 s | 79,80 m | 13,86 m | 283,70 m |
| HackRF 1, 150 s | 564,85 m | 37,28 m | 2.137,33 m |
| HackRF 2, 75 s | 33,84 m | 30,51 m | 168,76 m |
| RTL-SDR, 75 s | 14,01 m | 14,01 m | 39,66 m |
| SiGe, 75 s | 27,03 m | 27,03 m | 80,45 m |

Nel replay HackRF 1 con partenza a 150 s, i fix a tre costellazioni
restano entro 95,50 m. L'errore aumenta dopo l'uscita di GPS dalla
soluzione, fra 67 e 72 s dal via. Il massimo è nel fix con sei satelliti
BeiDou e nessuna altra costellazione. Le ultime trame accettate per quei
PRN precedono l'aumento di errore. Al campione 1.178.561.472 il solver
usa quattro satelliti BeiDou per quattro incognite, con residuo quasi
nullo e quota errata di circa 1,30 km. La causa nelle osservabili o nel
modello di misura resta da individuare.

### Ripetizione RTL-SDR con gap e riferimento LNIKS

Stessa finestra del file RTL-SDR, da 75 a 175 s, in due avvii a freddo e
in un riavvolgimento. Zero perdite in tutte le prove e un solo gap atteso
nel riavvolgimento. Risultati e posizione di riferimento locali in
`build/b8-rtl-repeat-gap-20261127`.

| Prova | Fix a tre costellazioni | Primo fix B1C nell'epoca | RMS 3D dei fix B1C | Massimo 3D dei fix B1C |
|---|---:|---:|---:|---:|
| Freddo 1 | 49 | 33,004 s | 12,67 m | 39,82 m |
| Freddo 2 | 49 | 33,004 s | 12,64 m | 39,66 m |
| Prima del gap | 40 | 42,004 s | 164,80 m | 176,98 m |
| Dopo il gap | 27 | 24,001 s | 256,76 m | 474,49 m |

La distanza 3D fra i due avvii a freddo non supera 0,913 m sui
67 campioni PVT comuni. Al campione 120.009.600, prima del gap, il
riavvolgimento usa gli stessi PRN del primo avvio e il residuo sale da
3,18 a 37,97 m. La quota passa da 325,35 a 444,47 m.

Sul PRN C32, il tracking isolato con i due semi e le due partenze
effettivamente osservati riproduce una differenza di circa 198 m
nell'equivalente di distanza del tempo di trasmissione all'epoca allineata
corrispondente. I campioni dei due replay usano la stessa cattura e la
stessa origine su disco. Il confronto localizza una discrepanza nelle
osservabili. Nella terza prova isolata, con la stessa partenza del
riavvolgimento e il seme dell'avvio a freddo, la differenza rispetto
all'avvio a freddo scende a 6,68 m alla medesima epoca. Questo confronto
mostra che, per C32 in questa finestra, cambiare il seme cambia
l'osservabile anche a parità di campione iniziale del tracker. Il test di
recupero PVT dopo gap è completato, con anomalie di accuratezza ancora
aperte.

## B10 · Canali, messaggi e PVT

La console mostra acquisizione, tracking, decodifica e appartenenza alla
soluzione PVT in colonne distinte e nella pagina satellite. L'appartenenza
considera sistema, PRN, epoca di continuità e inizio dell'assegnazione corrente.
La pagina Navigation data distingue messaggio decodificato, TOW e consistenza
dell'issue. Il PVT mostra conteggi, bias per costellazione e skyplot.

Playwright con Edge su Windows verifica PRN 63, omonimia tra costellazioni,
stato parziale, telemetria storica, reset di epoca, nuova assegnazione e il
layout mobile in `web_pvt_map.cjs` e `web_workspace.cjs`. La scelta Preview BeiDou B1C attiva una fixture a tre
costellazioni con etichetta visibile. La scelta operativa **BeiDou B1C** è in Receiver configuration, Channel allocation. [Contratto web](../../../docs/service/WEBGUI.md#states)

## Verifiche native

| Piattaforma | Evidenza |
|---|---|
| Windows x64 | MSVC Release, 28/28 CTest in 54,15 s, profiling dei job disattivato. Evidenze locali in `build/f4-qualification-release/b8-b10-windows-final.xml`. |
| Linux | In attesa del job nativo sulla revisione delle modifiche. WSL non dispone di distribuzioni installate. |
| macOS | In attesa del job nativo sulla revisione delle modifiche. Nessun host macOS disponibile nella sessione. |

La matrice nativa configurata nel repository copre Linux e macOS sia con Web
attivo sia disattivato. [Build CI](../../../.github/workflows/build.yml)
