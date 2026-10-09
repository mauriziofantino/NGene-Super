# NGene 0.4 — consolidamento diagnostica, DC e tuning

2 ottobre 2026. Modifiche incrementali sul progetto 0.3; NGene originale resta la base. Frontend → conditioning condiviso → unico ring → canali resta il percorso del ricevitore. Nessuna modifica alle butterfly FFT esistenti per correggere il waterfall.

## Esito del problema waterfall

**Causa riproducibile individuata nella GUI:** `renderAnalysis()` aggiungeva `null` alla history quando mancava una sequenza, e `drawWaterfall()` dipingeva quel `null` come riga ambra `[230,173,88]`. Producer e refresh browser non erano sincronizzati: un salto normale di aggiornamento generava quindi una riga orizzontale artificiale. Questo spiega perché l'effetto cresceva con il rate, anche con una PSD valida.

La riga artificiale è eliminata. I salti sono contati come `display skips`; non diventano righe, zeri o frame sintetici. Il test browser esercita producer a 12,5 Hz e display a 1 Hz, verifica salti effettivi e una history composta soltanto da frame finiti. Una PSD alterata intenzionalmente è rifiutata e non modifica la history. Non si afferma che questo escluda ogni possibile artefatto RF o guasto USB sull'hardware dell'utente.

Un secondo problema è stato corretto: il server ignorava richieste WebSocket troppo ravvicinate senza risposta, lasciando il browser in attesa. Il nuovo pacing è coerente con le opzioni GUI fino a 30 Hz; una richiesta abusiva chiude la connessione invece di lasciarla sospesa. I rate sono limiti massimi, non scadenze garantite.

## Ownership e integrità

| Passaggio | Proprietà / verifica |
|---|---|
| Frontend/ring → diagnostica | Copia limitata in workspace privato, nessun puntatore RF trattenuto |
| Worker → snapshot | Array per valore pubblicati sotto mutex; il workspace in elaborazione non è riutilizzato dal producer |
| Snapshot → telemetry | Deep copy esplicita prima della serializzazione; mutex rilasciati prima di JSON e socket |
| WebSocket → browser | Un solo status richiesto in volo; un solo messaggio più recente in attesa di `requestAnimationFrame` |
| Browser → history | Copia dell'array PSD, congelata; history limitata, nessun riferimento al buffer di un frame successivo |

Nessuna corruzione di lifetime/ownership è emersa nelle prove eseguite. La telemetria mantiene una deep copy per rendere verificabile questo confine. Ogni PSD espone `sequence_id`, `produced_seconds` (clock monotonic del processo), min/max/mean, NaN/Inf count e checksum FNV-1a sui valori signed milli-dB in ordine little-endian. `telemetry_checksum` viene ricalcolato sulla copia. Il browser verifica lunghezza, finitezza, statistiche e hash prima di accettare il frame. L'hash è un controllo diagnostico, non una firma crittografica. La quantizzazione a 0,001 dB avviene una sola volta prima della pubblicazione.

## Istogrammi e statistiche

PSD e istogramma hanno raccolta, lunghezza, frequenza e selezione pre/post indipendenti. NFFT non determina più la lunghezza del burst dell'istogramma. Cambiare il profilo di analisi avvia una nuova revisione e azzera i risultati vecchi; sul segnale reale due finestre temporali diverse possono comunque avere statistiche diverse.

Per I e Q sono mostrati media, deviazione standard della popolazione, RMS, minimo/massimo, conteggio ai rail −128/+127 e conteggio degli zeri. La magnitudine DC è `hypot(meanI, meanQ)` in unità ADC. I rail sono un indicatore di clipping, non una diagnosi automatica di saturazione RF. Il bias rimane visibile.

Test con distribuzione nota: I alterna 20/22 → media 21, std 1, RMS √442; Q=12 → media/RMS 12 e std 0. Istogrammi pre invariati passando da NFFT 256 a 4096 sugli stessi dati; spettro post separato. Il test signedness copre tutti i 256 codici int8, code SIMD e potenze int16 incluso `(-32768)^2+(-32768)^2 = 2^31`. La build di riferimento usa anche `-funsigned-char`.

## DC offset

La documentazione ufficiale [HackRF](https://hackrf.readthedocs.io/en/stable/troubleshooting.html) descrive il bias dei ricevitori in quadratura e l'offset tuning. Un DC block RF esterno non sottrae una componente introdotta internamente nella catena di conversione.

`conditioning.module=dc_iir` abilita un estimatore DC continuo Q32, separato per I e Q:

- `y[n] = x[n] - round(mean[n-1])`, con saturazione CI8;
- `mean[n] = mean[n-1] + 2^-dc_shift * (x[n] - mean[n-1])`;
- `dc_shift=4..24`, default 16; costante di tempo approssimata `2^shift/Fs`, frequenza di transizione circa `Fs/(2π 2^shift)`;
- stato mantenuto attraverso i blocchi, reset soltanto a inizio stream, gap, nuova epoch o discontinuità degli indici.

Il default rimane `bypass`. Non si azzera il bin DC della FFT e non si nascondono dati. L'IIR è scalare a stato continuo: non è stata forzata una vettorizzazione della ricorrenza. Il formato di uscita resta CI8, quindi esiste quantizzazione e può verificarsi saturazione dopo la sottrazione. La rimozione DC attenua anche segnali utili vicini allo zero hardware; il transitorio iniziale è reale. Registrare pre permette di conservare il segnale originale.

## Tuning, assi e futuro NCO

La chiave storica `frontend.center_frequency_hz` resta la **sintonia hardware**; nuova `frontend.signal_frequency_hz` è il centro del segnale. Il valore 0 segue la sintonia hardware per mantenere compatibilità con i vecchi profili.

```ini
[frontend]
type=hackrf
sample_rate_hz=16000000
center_frequency_hz=1576420000
signal_frequency_hz=1575420000
```

Risulta `digital_if_hz = −1000000`. Il setter HackRF riceve soltanto la frequenza hardware. Stream info, file metadata e telemetry riportano entrambe e la differenza signed. Si verifica che il centro del segnale cada nella banda campionata; la larghezza occupata dal segnale e il margine del filtro analogico restano da scegliere correttamente.

La GUI dispone di tre riferimenti: baseband hardware (`f_bin`), signal-centered (`f_bin − digital_IF`) e RF assoluta (`hardware_frequency + f_bin`). Cambiano assi e label, non i campioni. La frequenza di campionamento visualizzata è quella nominale applicata/accettata dal frontend, non una misura calibrata del suo oscillatore: libhackrf non fornisce qui un readback metrologico.

Gli helper C `NgCarrierFromDoppler()` e `NgDopplerFromCarrier()` fissano il contratto `NCO = digital_IF + Doppler` e `Doppler = carrier − digital_IF`. **Acquisition, tracking e PVT non sono ancora collegati nel progetto incrementale:** il riferimento è predisposto e testato, non viene dichiarata una ricerca satelliti implementata. Non è introdotto alcun mixer globale sui 16 MS/s.

## GUI e costo diagnostico

Restano Configuration/Observation e tema dark. FFT discrete 256..16384; overlap a qualsiasi percentuale intera 0..99; Welch 1..64 segmenti, nel limite di 262144 coppie per burst. Cinque finestre; rate PSD/istogramma separati 0,1..30 Hz; media temporale lineare 1..16 frame oppure EMA con coefficiente (0,1].

Scala dBFS/bin oppure dBFS/Hz con ENBW della finestra quantizzata. In dBFS/bin un tono coerente/DC ha livello normalizzato /N: NFFT maggiore non aumenta la sua ampiezza fisica; cambia Δf, fondo per bin e separazione visiva. In dBFS/Hz l'altezza spettrale di una riga coerente dipende dalla risoluzione, come per una densità. Nessuna calibrazione dBm. Oltre 1024 bin il display conserva i massimi dei gruppi: non integrarlo come una PSD completa per stimare potenza totale.

Controlli di scala: min/max spettro, min/max colore globale waterfall, limiti frequenza, fit una tantum, conteggi/percentuale e massimo Y istogramma. Le stesse soglie colore valgono per tutte le righe conservate. History 32..2048 righe e rate waterfall indipendente; non viene costruita una STFT continua di tutti i campioni.

Non è più trasmessa l'anteprima raw I/Q continua. Vengono inviati PSD, istogrammi, metriche e stato. Il browser rinnova l'interesse diagnostico soltanto in Observation con i relativi pannelli abilitati; Configuration, pausa, tab nascosta e chiusura della pagina lasciano scadere l'interesse dopo due secondi. Client multipli mantengono attivo ciò che almeno uno richiede. Il recorder e i canali continuano.

I tap diagnostici usano try-lock: se occupati saltano il blocco e incrementano `contention_blocks`; un eventuale burst incompleto viene scartato al successivo indice discontinuo. Il worker elabora il ready più recente e non accumula una coda grafica. Il lock del recorder è separato da quello della GUI. Rimangono sincronizzazione e I/O del ricevitore, quindi non viene dichiarato hard real-time.

## SIMD e benchmark

Nuove primitive in `src/dsp/sample_ops.c`: deinterleaving/sign extension e momenti I/Q, potenza complessa con accumulo uint64, intrinsics SSE2/NEON e riferimento C. Nessun AVX2 obbligatorio; `NGENE_SAMPLE_REFERENCE=ON` forza il riferimento. Istogramma, finestratura e IIR mantengono un percorso semplice; nuovi correlatori/mixer non vengono inventati in questo incremento.

Linux x86-64, GCC Release `-O3`, 16000 coppie per chiamata, 10000 ripetizioni per primitive, 1000 per DC:

| Operazione | Riferimento C | SSE2 / implementazione | Rapporto |
|---|---:|---:|---:|
| Momenti CI8 | 50,615 µs | 3,751 µs | 13,49× |
| Potenza CI16 e accumulo | 11,153 µs | 5,371 µs | 2,08× |
| DC IIR continuo Q32 | — | 110,141 µs / 16000 coppie | scalare |

Riproduzione: `ngene_sample_tests --benchmark` in Release. I risultati valgono per l'ambiente di prova; il C reference è quello distribuito, non ogni possibile implementazione scalare. Nessun benchmark NEON su hardware ARM, né percentuale CPU globale dichiarata.

## Windows e problemi ancora aperti

Build Windows prevista **nativa MSVC**, con MASM x64 e ARMASM64 per ARM64; preset e CI espliciti, nessuna dipendenza da MinGW. CMake minimo 3.26 per ASM_MARMASM. Il preset ARM64 consente cross-build da host Windows x64 con il toolset ARM64 installato. Vedere `WINDOWS.md`.

Test Linux, browser, sanitizers e cross-build Linux ARM64 descritti in `VALIDATION.md`. Questo ambiente non contiene Windows/MSVC: i nuovi percorsi MSVC/ARMASM sono predisposti ma non sono stati compilati/eseguiti nativamente qui. Rimangono da provare HackRF reale, Windows e ARM reali, longevità/throughput complessivo, piena gestione Unicode dei percorsi Windows e migrazione degli algoritmi GNSS. Le routine FFT aritmetiche di 0.3 sono inalterate; è aggiunta soltanto la sintassi ARMASM64 equivalente.
