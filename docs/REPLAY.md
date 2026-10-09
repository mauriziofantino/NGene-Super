# File replay e sessioni indipendenti

Il frontend `file` legge CI8: un byte **int8 I**, poi un byte **int8 Q**. Non interpreta uint8 come ampiezza positiva e non ricostruisce un segnale IF. File vuoti, dimensioni dispari e range fuori file sono rifiutati.

Un raw `.cs8` non contiene rate, tuning o tempo GNSS. Impostare i parametri frontend coerenti con la registrazione e con il relativo `.meta.ini`; questa versione non concatena automaticamente segmenti e non importa automaticamente i metadati.

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
