# B3 · Continuità dei correlatori del pilota B1C

Il banco usa il ramo BOC(1,1) del pilota primario. La fixture inietta questo ramo in Q, con ampiezza unitaria, IF 38 kHz e quattro tratti di Doppler pari a -2475, -1250, +375 e +2311 Hz. Ogni tratto dura 10 ms. La fase iniziale è a 0,173763 chip dal termine del primario e attraversa subito il confine. La fixture e l'oracolo usano trigonometria e accumulazione scalare in double.

Il percorso esercitato riusa i kernel SSE2/NEON di EVO per generare le fasi e le repliche, ruotare I/Q e correlare i tre tap. La replica anticipata avanza di 0,25 chip, la ritardata arretra di 0,25. Le fasi di codice e portante sono ancorate all'indice assoluto del campione al cambio di frequenza. La portante usa IF più Doppler. Il codice usa 1,023 MHz più Doppler diviso 1540, dal rapporto fra chip rate e 1575,42 MHz. Il cambio di pendenza preserva le due fasi. Le finestre contribuiscono allo stesso accumulatore complesso, in double, senza ripartire da chip zero.

Il banco confronta finestre nominali da 1 ms con finestre irregolari da 1 a 977 campioni sullo stesso segnale. Verifica che il puntuale domini gli altri tap, che le coordinate I/Q conservino la polarità e che un salto di indice scarti lo stato e impedisca di continuare con il seme invalidato. L'accumulo sui 40 ms serve alla prova numerica e presuppone la polarità costante della fixture. Il banco non stabilisce la durata coerente utile su segnale reale.

| Fs (Msps) | PRN | Campioni | Scarto massimo vs oracolo / N | Scarto massimo tra partizioni / N |
|---:|---:|---:|---:|---:|
| 2,048 | 1 | 81920 | 7,20e-9 | 1,83e-13 |
| 8,000 | 33 | 320000 | 7,32e-9 | 3,78e-16 |
| 8,1838 | 63 | 327352 | 7,34e-9 | 7,12e-16 |

Per riprodurre su Windows x64 Release con ambiente MSVC inizializzato:

```powershell
cmake --build build/f4-qualification-release --target ngene_beidou_b1c_tracking_lab
& ./build/f4-qualification-release/ngene_beidou_b1c_tracking_lab.exe
ctest --test-dir build/f4-qualification-release -C Release -R 'beidou_b1c_primary_contract|tracking_contract' --output-on-failure
```

Su Windows x64 Release sono passati il banco SSE2 e i due CTest mirati.

Rimangono l'aggancio a partire da una stima imperfetta, la dinamica di DLL/FLL/PLL, l'ambiguità BOC, la stima C/N0, il filtro di front-end e la riacquisizione dopo gap. Verifiche native Linux e macOS da eseguire.


## Prova in anello chiuso

Nella prima campagna, precedente al criterio misurato, il profilo `--closed-loop` esercitava la catena BOC(1,1) del pilota in sei prove sintetiche. Si usano i tre rate della prova precedente e, a ciascun rate, due semi con errori di segno opposto pari a 15 Hz di Doppler, 0,10 cicli di portante e 0,14 chip di codice. Il segnale ha IF 38 kHz, Doppler costante 1234,5 Hz e rumore uniforme I/Q di ampiezza 0,35 sulla componente di segnale unitaria. Il banco consuma 80 integrazioni da 10 ms ripartite in finestre irregolari.

I tap usano gli stessi kernel SIMD del banco di continuità. Il FLL misura la rotazione tra prompt successivi con il prodotto complesso e corregge la frequenza. Dal sedicesimo periodo il banco passa al PLL, che usa la fase residua del prompt e corregge fase e frequenza. Il DLL applica la differenza normalizzata delle potenze anticipata e ritardata. La stima Doppler influenza anche l'NCO del codice. I guadagni e l'istante di passaggio FLL→PLL appartengono alla fixture. Il banco conserva la misura dei tap ogni 10 ms e mantiene gli NCO tra periodi.

Massimi osservati negli ultimi 20 periodi, aggregando entrambi i segni del seme.

| Fs (Msps) | Fase residua (cicli) | Errore codice (chip) | Errore Doppler (Hz) |
|---:|---:|---:|---:|
| 2,048 | 0,000511 | 0,003315 | 0,003644 |
| 8,000 | 0,000316 | 0,001302 | 0,003153 |
| 8,1838 | 0,000243 | 0,001378 | 0,002769 |

```powershell
cmake --build build/f4-qualification-release --target ngene_beidou_b1c_tracking_lab
& ./build/f4-qualification-release/ngene_beidou_b1c_tracking_lab.exe --closed-loop
```

Questi casi provano l'aggancio da errori contenuti a Doppler costante. La campagna successiva qui sotto estende le prove alle rampe e al cambio di anello su criterio misurato.


## Rampe Doppler e transizione osservata FLL→PLL

Il profilo corrente `--closed-loop` incrocia cinque rampe continue (-35, -8, 0, +8 e +35 Hz/s), tre ampiezze relative del segnale (1, 0,12 e 0,04), due segni del seme e tre rate. La fase RF integra la frequenza lineare anche entro la finestra e l'NCO di codice riceve la deriva corrispondente. Restano 80 integrazioni da 10 ms, finestre SIMD irregolari e rumore I/Q uniforme di ampiezza 0,35. Si aggiunge un controllo con solo rumore per rate.

Per passare al PLL, il banco richiede quattro prompt consecutivi con residuo FLL inferiore a 2 Hz in valore assoluto, potenza del prompt almeno 1,5 volte quella del tap laterale più forte e potenza normalizzata |P|²/(N·Σ|IQ|²) superiore a 0,003. L'ultimo rapporto misura la coerenza relativa nel fixture. Le soglie sono sperimentali e non determinano C/N0 o lock operativo. La prova dinamica mantiene il segnale per tutta la durata. I profili di perdita sono descritti nella sezione seguente.

| Fs (Msps) | Casi con segnale | Periodo di ingresso PLL | Fase residua max (cicli) | Errore codice max (chip) | Errore Doppler max (Hz) |
|---:|---:|---:|---:|---:|---:|
| 2,048 | 30/30 | 8–12 | 0,043751 | 0,043979 | 1,587280 |
| 8,000 | 30/30 | 8–10 | 0,040529 | 0,025131 | 1,525849 |
| 8,1838 | 30/30 | 8–10 | 0,035027 | 0,029975 | 1,465112 |

Errori massimi negli ultimi 20 periodi. Per la frequenza il riferimento è il Doppler al centro della finestra. In tutti e tre i controlli di solo rumore il PLL resta inattivo. Esecuzione Windows x64 Release SSE2.

```powershell
cmake --build build/f4-qualification-release --target ngene_beidou_b1c_tracking_lab
& ./build/f4-qualification-release/ngene_beidou_b1c_tracking_lab.exe
& ./build/f4-qualification-release/ngene_beidou_b1c_tracking_lab.exe --closed-loop |
    Set-Content build/f4-qualification-release/b3-dynamic.csv
ctest --test-dir build/f4-qualification-release -C Release -R 'beidou_b1c_primary_contract|tracking_contract' --output-on-failure
```


## Perdita, riaggancio e veto sui lobi BOC

Il profilo `--recovery` esercita il ramo BOC(1,1) del pilota a 2,048, 8,000 e 8,1838 Msps. Prova ciascuno scenario con semi di entrambi i segni. Nei casi di calo e gap il livello relativo è 0,12 con la stessa fixture di rumore precedente e una rampa Doppler di ±8 Hz/s. Le integrazioni durano 10 ms e sono alimentate con finestre irregolari. Due tap di guardia a ±0,5 chip si aggiungono ai tap Early/Prompt/Late. Sono calcolati con gli stessi kernel SIMD. Il banco usa le regole seguenti.

- **Calo a timeline continua.** Il segnale è assente per sei periodi, da 300 a 360 ms. Durante i prompt incoerenti si sospendono gli aggiornamenti di fase, frequenza e codice, mentre gli NCO continuano ad avanzare. Dopo tre prompt deboli il PLL torna a FLL. La cronologia di fase del prompt viene invalidata, così il primo prodotto FLL dopo il calo usa due finestre valide contigue. Il PLL usa di nuovo il criterio dei quattro prompt consecutivi.
- **Salto di indice.** Si accumula mezzo periodo e un indice incongruente invalida gli NCO e l'accumulo. Le finestre successive risultano rifiutate fino a una nuova inizializzazione. La fixture fornisce un seme a 330 ms con lo stesso errore deliberato del seme iniziale. Questa misura riguarda il tracking dopo la consegna del seme. Il tempo di ricerca del satellite non è incluso.
- **Seme sul lobo.** Si prova un errore iniziale di ±0,5 chip con segnale unitario. Se il prompt è coerente ma una guardia ha potenza superiore a 1,5 volte quella puntuale, si sospendono le correzioni e si blocca l'ingresso in PLL. Quattro eventi consecutivi respingono il seme e richiedono una nuova acquisizione. Questa verifica copre le due posizioni iniettate e non una mappa completa dei falsi agganci.

| Scenario | Prove riuscite | Esito |
|---|---:|---|
| Calo di 60 ms | 6/6 | Rientro in PLL 50 ms dopo il ritorno del segnale |
| Gap con nuovo seme | 6/6 | Accumulo parziale scartato, ingresso in PLL 80 ms dopo il nuovo seme |
| Seme a ±0,5 chip | 6/6 | Seme respinto dopo 40 ms, PLL mai attivato |

```powershell
cmake --build build/f4-qualification-release --target ngene_beidou_b1c_tracking_lab
& ./build/f4-qualification-release/ngene_beidou_b1c_tracking_lab.exe --recovery |
    Tee-Object -FilePath build/f4-qualification-release/b3-recovery.csv
```

Nel CSV, `scenario` vale 1 per calo, 2 per gap e 3 per lobo. I campi `*_epoch` sono conteggi di periodi a partire da uno. Zero indica un evento assente. I criteri e le durate sono del banco. Restano la stima C/N0, la sensibilità del veto a lobi e multipath, la qualifica RF e le verifiche native Linux/macOS.


## C/N₀ di riferimento e selettività in multipath

Il profilo `--quality` misura il ramo BOC(1,1) del pilota a 2,048, 8,000 e 8,1838 Msps. Le fasi del codice e della portante sono fornite alla fixture. Questa prova separa lo stimatore e la geometria dei tap dagli errori degli anelli.

### C/N₀ in rumore gaussiano

Il banco inietta rumore gaussiano indipendente in I e Q prima dei correlatori SIMD. Con varianza per componente σ² e rate Fs, la densità complessa è N₀ = 2σ²/Fs. La potenza della replica utile è scelta per C/N₀ pari a 26, 35 o 45 dB-Hz. Ogni prova raccoglie 192 prompt da 10 ms.

Per prompt complesso normalizzato z si mediano M₂ = E[|z|²] e M₄ = E[|z|⁴]. La stima di potenza utile è S = √(2M₂² − M₄). La potenza residua è M₂ − S. Su T secondi il rapporto C/N₀ stimato è S/[(M₂ − S)·T]. Il controllo a fase nota ricava la potenza utile dalla media complessa. I momenti sono calcolati sull'intera prova. La media esponenziale del ricevitore e la validità a runtime richiedono verifiche proprie.

| Fs (Msps) | M₂/M₄ a 26 dB-Hz | M₂/M₄ a 35 dB-Hz | M₂/M₄ a 45 dB-Hz |
|---:|---:|---:|---:|
| 2,048 | 25,729 | 34,713 | 44,673 |
| 8,000 | 26,726 | 35,419 | 45,363 |
| 8,1838 | 27,123 | 35,625 | 45,495 |

Lo scarto assoluto massimo è 1,124 dB sui nove casi. Il controllo a fase nota ha uno scarto massimo di 0,613 dB. I nove casi rispettano il limite di 1,5 dB del banco. La prova non stabilisce bias e varianza su molte realizzazioni o con perdita di lock.

### Una riflessione coerente

Il banco aggiunge al cammino diretto un'eco con ritardo di 0,25, 0,5, 0,75 o 1,5 chip e fase relativa di 0°, 90°, 180° o 270°. Per ogni combinazione misura le due guardie a ±0,5 chip e il puntuale con seme diretto e semi a ±0,5 chip. Si isola il confronto di potenza guardia/prompt > 1,5 su una finestra. Il test usa una sola riflessione e nessun filtro analogico o rumore aggiunto.

A ogni rate il veto ha lasciato passare tutti i 16 semi diretti per ciascun guadagno. Ha respinto 32/32 semi laterali con guadagno 0, 0,25 o 0,50 e 27/32 con guadagno 0,90. Le cinque eccezioni sono le stesse ai tre rate. Si concentrano su 0,5 chip di ritardo dell'eco, con un'ulteriore eccezione a 0,25 chip e 180°. Il CSV registra la fase esatta e il segno del seme per ciascuna eccezione. Un seme che supera il veto resta un candidato e la prova non misura un falso lock del PLL.

```powershell
cmake --build build/f4-qualification-release --target ngene_beidou_b1c_tracking_lab
& ./build/f4-qualification-release/ngene_beidou_b1c_tracking_lab.exe --quality |
    Set-Content build/f4-qualification-release/b3-quality.csv
```

Verificati su Windows x64 SSE2 anche il banco di continuità, il recupero e 2/2 CTest mirati. Restano un confronto sui cinque casi critici con geometrie alternative dei tap, una prova in anello sotto eco, le misure RF e le verifiche native Linux/macOS.


## Anello chiuso con eco forte

Il profilo `--echo-loop` riprende le cinque eccezioni a guadagno 0,90 già riportate. A ciascun rate e per ciascun caso prova un seme diretto e il seme laterale critico. Oltre alle guardie a ±0,5 chip, il secondo profilo aggiunge ±0,375 chip e usa la massima potenza delle quattro guardie. La soglia sperimentale guardia/prompt resta 1,5. I tap in più portano da cinque a sette le correlazioni per finestra, due in più con lo stesso mixer SIMD. Questa prova conta il lavoro per finestra e non misura un rapporto di tempi.

Il banco inietta entrambi i cammini in I/Q prima del tracking, con rumore uniforme additivo e guadagno diretto unitario. Il PLL usa il criterio dei quattro prompt e il DLL riceve Early/Late. Si classifica come PLL fuori dal cammino diretto un periodo in PLL con errore codice maggiore di 0,25 chip rispetto alla verità iniettata.

| Geometria | Semi diretti in PLL a fine prova | Semi laterali che entrano in PLL oltre 0,25 chip |
|---|---:|---:|
| 5 tap, guardie ±0,5 chip | 12/15 | 15/15 |
| 7 tap, guardie ±0,5 e ±0,375 chip | 12/15 | 15/15 |

In entrambe le geometrie l'eco a 0,5 chip e fase relativa zero impedisce l'ingresso in PLL dal seme diretto ai tre rate. I quindici semi laterali mantengono il PLL fino a fine prova. Il controllo aggiuntivo non migliora la classificazione su queste fixture e resta nel banco. La selezione robusta del cammino diretto rimane aperta. Qualsiasi tecnica proposta a partire dalla letteratura va concordata prima dell'implementazione.

```powershell
cmake --build build/f4-qualification-release --target ngene_beidou_b1c_tracking_lab
& ./build/f4-qualification-release/ngene_beidou_b1c_tracking_lab.exe --echo-loop |
    Set-Content build/f4-qualification-release/b3-echo-loop.csv
```
