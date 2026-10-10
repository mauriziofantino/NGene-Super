# File replay e sessioni indipendenti

Il frontend `file` legge CI8: un byte **int8 I**, poi un byte **int8 Q**. Non interpreta uint8 come ampiezza positiva e non ricostruisce un segnale IF. File vuoti, dimensioni dispari e range fuori file sono rifiutati.

Un raw `.cs8` non contiene rate, tuning o tempo GNSS. Dalla 0.6.3 il lettore importa i parametri dal sidecar adiacente `FILE.cs8.meta.ini`. Per i segmenti storici `PREFIX_000000.cs8` cerca anche `PREFIX.meta.ini`. Non concatena automaticamente segmenti e non ricostruisce i gap dal CSV: ogni file è riprodotto come sequenza contigua delle coppie presenti. Conservare l’indice per la successiva ricostruzione temporale.

```ini
[frontend]
type=file
sample_rate_hz=16000000
center_frequency_hz=1576420000
signal_frequency_hz=1575420000
[replay]
path=D:/GNSS/capture_000000.cs8
start_sample=0
sample_limit=0
realtime=true
loop=false
```

`start_sample` e `sample_limit` contano **coppie complesse**, non byte. Limit zero legge fino al termine del range. `realtime=true` usa un clock monotono e il sample rate configurato; false legge al ritmo consentito dal motore. `loop=true` ripete il range, incrementa l'epoca e segnala un gap con perdita non quantificata: una ripetizione non è RF continua. Gli indici prodotti rimangono monotoni. Una finestra finale incompleta è segnalata esplicitamente; non viene aggiunto zero padding.

Ogni istanza possiede file handle, finestra, cursore e pacing. Il buffer ottenuto da `acquire` è immutabile fino a `release`; una seconda acquire durante la stessa lease viene rifiutata. EOF termina normalmente. La lunghezza viene fissata all'apertura: usare file chiusi e immutabili, senza modificarli o cancellarli durante il replay.

Il servizio web possiede due controller: primario e replay. La sessione primaria può usare HackRF o un file; la seconda è riservata ai file. Ciascuna ha motore, conditioning, ring, registrazione, diagnostica e revisione propri. Tutti i canali di **una** sessione continuano a condividere la medesima finestra; stream diversi non vengono miscelati o sincronizzati automaticamente. L'architettura consente futuri adapter nativi senza modificare il contratto del replay.

Il menu della GUI cambia la sessione comandata/osservata. Avvio, stop, configurazione, diagnostica, registrazione e trasferimento agiscono sulla selezione; l'altra sessione continua. Salvare o scartare le modifiche prima di cambiare sessione. Exit service e Ctrl+C fermano entrambe.

Le API della seconda sessione usano `/api/v1/replay/`: `config`, `status`, `receiver/start`, `receiver/stop`, `recording/start`, `recording/stop`, `analysis/config`, `files`, `files/delete`, `files/<name>`. Token e controllo origine sono gli stessi del servizio; le revisioni appartengono al controller selezionato. Le richieste WebSocket sono `status:<mask>` e `replay:<mask>`, con la politica di frame più recente già adottata.

I test verificano range, signedness, EOF, lease, loop, storage indipendente, replay insieme al vero adapter HackRF con driver simulato, e due controller contemporanei tramite browser. Non sostituiscono una cattura USB live reale. Il frontend sintetico e i suoi profili sono rimossi; la generazione di vettori deterministici esiste esclusivamente nei test.

## Selezione e configurazione automatica (0.6.3)

**Browse I/Q files…** mostra cartelle e file `.cs8` del computer ricevitore. **Roots / drives**, **Parent folder**, i pulsanti delle sottocartelle e il filtro per nome evitano di dover digitare il percorso. La risposta contiene al massimo 500 elementi; il filtro viene applicato sul server prima di questo limite. La selezione è una bozza: non avvia né riconfigura un run. Il pulsante **Replay** nella lista delle catture seleziona la sessione indipendente.

Il lettore controlla formato CI8 signed, interleaving I/Q, frequenze, associazione al basename e, per la versione 2, conteggio delle coppie effettivamente salvate. Metadati esistenti ma incoerenti vengono rifiutati; non sono sostituiti da valori presunti. Un sidecar assente consente la selezione manuale, con un avviso: impostare Fs e riferimenti RF prima di salvare. File attivi, vuoti, dispari, symlink/reparse point e percorsi oltre i limiti del profilo non sono selezionabili.

La configurazione importata usa `frontend.type=file`, Fs, frequenze hardware/segnale e bandwidth registrati, finestra iniziale da 1 ms, intervallo completo e registrazione automatica disabilitata. Il conditioning parte in **bypass**: non riapplica il DC remover a campioni già condizionati. È possibile cambiarlo consapevolmente nella bozza. Pacing/loop e diagnostica conservano le impostazioni del profilo selezionato. Il rate registrato è nominale, non una misura del clock HackRF. Nessuna traslazione complessa globale viene aggiunta.

CLI con sidecar:

```sh
ngene --replay "D:/GNSS/prova_000000.cs8"
```

Per file privi di sidecar:

```sh
ngene --config config/replay.ini --replay "D:/GNSS/legacy.cs8"
```

## Sidecar per file, schema 2

```ini
[capture]
version=2
format=CI8
layout=I,Q interleaved
component_bits=8
component_signed=true
data_file=prova_000000.cs8
sample_rate_hz=16000000
sample_rate_basis=configured_nominal
center_frequency_hz=1576420000
signal_frequency_hz=1575420000
digital_if_hz=-1000000
bandwidth_hz=15000000
conditioning=dc_iir
dc_shift=16
recording_point=post_conditioning
hardware_timestamp=false
host_start_unix_seconds=1791619200

[result]
complete=true
written_samples=16000000
gap_events=0
```

`data_file` è il solo nome, quindi raw e sidecar possono essere spostati insieme in un’altra cartella. Se il raw viene rinominato occorre aggiornare anche il nome del sidecar e `data_file`. Un footer mancante indica un file non finalizzato e impedisce l’importazione automatica dello schema 2. Un footer esplicito `complete=false`, con lunghezza coerente, viene mostrato come cattura interrotta da verificare. I segmenti precedenti già finalizzati possono essere completi anche se un segmento successivo ha fallito.

Trasferire raw e relativo sidecar; trasferire anche indice e metadati di sessione per conservare la storia dei gap. **Delete** continua a eliminare un singolo file; non cancella implicitamente i file associati. Il backend protegge dalla cancellazione il raw selezionato da un replay attivo, anche nell’altra sessione.

Le API `POST /api/v1/replay-files/browse` (`path` opzionale, `filter` opzionale) e `POST /api/v1/replay-files/inspect` (`path`) sono disponibili anche sotto `/api/v1/replay/`. Richiedono token/origin come le altre richieste POST; operano in sola lettura. L’inspect restituisce metadati, revisione e INI di bozza senza mutare il controller.
