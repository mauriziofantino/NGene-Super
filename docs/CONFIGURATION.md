# Configurazione INI

Parser C senza dipendenze. Chiavi e sezioni sono case-sensitive; numeri interi in base 10 e frequenze in Hz. Sono ammessi spazi attorno a `=` e commenti **su righe proprie**, iniziati da `#` o `;`. Non sono previste virgolette, commenti a fine riga, espansione di variabili, `~` o inclusioni. I percorsi relativi sono risolti dalla directory di esecuzione, non dalla posizione del file INI. Chiavi sconosciute/duplicate e valori non finiti causano errore. Si raccomandano percorsi ASCII per questa prima versione, anche su Windows.

| Chiave | Significato / valori |
|---|---|
| `receiver.mode` | `capture` esegue i canali registrati; `record` richiede registrazione abilitata e salta i canali |
| `receiver.run_seconds` | Limite in tempo host; 0 fino a EOF/Ctrl+C; default 2 |
| `receiver.idle_timeout_ms` | Errore se non arrivano campioni per questo intervallo; 100..3600000, default 3000 |
| `frontend.type` | `hackrf` o `file`; altri backend non ancora implementati |
| `frontend.serial` | Selettore HackRF; omettere con un solo dispositivo; nessuna selezione silenziosa con più dispositivi |
| `frontend.sample_rate_hz` | Coppie I/Q/s; default 16000000; profilo HackRF 2000000..20000000 |
| `frontend.center_frequency_hz` | Sintonia hardware SDR; default 1575420000 |
| `frontend.signal_frequency_hz` | Centro del segnale; 0 segue hardware per compatibilità. Digital IF = signal − hardware; deve essere entro ±Fs/2 |
| `hackrf.bandwidth_hz` | Filtro analogico supportato dal dispositivo; 0 seleziona quello immediatamente sotto il sample rate |
| `hackrf.lna_gain_db` | 0..40 a passi di 8, default 16 |
| `hackrf.vga_gain_db` | 0..62 a passi di 2, default 20 |
| `hackrf.rf_amp` | `true`/`false`, default false |
| `hackrf.antenna_power` | Alimentazione dal connettore RF, default false; abilitarla solo se richiesta dal proprio setup |
| `conditioning.module` | `bypass` (default) oppure `dc_iir` continuo |
| `conditioning.dc_shift` | 4..24, default 16; coefficiente estimatore DC 2^-shift |
| `buffer.window_samples` | Coppie per finestra comune, 1..1048576; default 16000 |
| `buffer.ingress_blocks` | Slot della coda di trasporto; default 128 |
| `buffer.ring_blocks` | Slot del ring centrale; default 16 |
| `buffer.overflow_policy` | `stop` (default) oppure `drop_newest` per la coda di ingresso |
| `recording.enabled` | Attiva la registrazione; default false |
| `recording.prefix` | Prefisso dei file; directory già esistente, prefisso non usato |
| `recording.queue_blocks` | Slot del writer, default 128; overflow ferma il run |
| `recording.segment_samples` | Coppie per file raw; 0 nessuna segmentazione |

I blocchi delle code devono essere 2..4096; il piano di allocazione ha una guardia di 512 MiB. Sono limiti espliciti di questo incremento per evitare configurazioni accidentali ingestibili, modificabili successivamente. La dimensione della finestra di replay non deve superare un secondo del sample rate configurato.

I parametri HackRF sono validati ma applicati soltanto al frontend HackRF. Il replay usa i metadati frontend configurati; non rappresenta un dispositivo RF attivo.

I setter HackRF vengono verificati. Il filtro scelto in automatico viene riportato nella descrizione dello stream e nei file. Rate e sintonia sono valori configurati/accettati dalla libreria: non misure calibrate dell’oscillatore o della frequenza reale.

Non sono presenti chiavi inerti `fast`, `precise`, `fft_backend` o `channels=24`: verranno introdotte insieme alle implementazioni. I numeri storici su CPU/canali rimangono riferimenti, non requisiti di accettazione di questo prototipo.


## Directory e configurazione web (v0.2.0)

`recording.directory` è una chiave opzionale: indica la cartella sul computer del ricevitore. Quando presente, `recording.prefix` deve essere un nome base, senza separatori di percorso. Con la CLI la cartella deve già esistere; con la GUI il salvataggio crea la cartella finale se il genitore esiste e normalizza il percorso assoluto. Omettendo la directory, la CLI mantiene la semantica del primo incremento (prefix anche con percorso).

In modalità web il nome base è limitato a 1–64 lettere ASCII, cifre, `_`, `-`; percorso assoluto fino a 240 byte. Esempio:

```ini
[recording]
enabled=false
prefix=prova_L1
directory=/data/gnss
queue_blocks=128
segment_samples=16000000
```

Tutti i parametri del profilo sono modificabili a ricevitore fermo. `recording.enabled` significa registrazione automatica al prossimo Start; i pulsanti di registrazione agiscono sul run corrente senza riscrivere quel parametro. Frequenza di aggiornamento e pausa del grafico sono opzioni runtime del browser e non modificano lo stream.

Il controller usa revisioni ottimistiche: una configurazione vecchia è rifiutata, così due schede del browser non si sovrascrivono silenziosamente. Il profilo viene scritto tramite file temporaneo e sostituzione atomica. Un eventuale `receiver.ini.tmp` lasciato da un crash impedisce una nuova scrittura finché non viene esaminato/rimosso. La sostituzione atomica non costituisce una garanzia di durabilità contro perdita di alimentazione.


## Parametri v0.3

`recording.point=pre|post` (default post), modificabile a ricevitore fermo. La sezione `[analysis]` e i suoi limiti sono descritti in [ANALYSIS.md](ANALYSIS.md). I valori di questa sezione possono essere applicati a runtime con l’API dedicata; `Apply & save` conserva il profilo completo su disco soltanto a ricevitore fermo. Il servizio può eseguire una sessione replay indipendente insieme al primario: [REPLAY.md](REPLAY.md). Il profilo iniziale dei file è `config/replay.ini`.


La GUI usa menu LNA 0..40 dB a passi di 8 e VGA 0..62 dB a passi di 2, coerenti con la validazione del backend HackRF. La scelta dei parametri dipende dal frontend; quelli HackRF non vengono presentati per il replay. Per offset tuning, DC e nuovi parametri diagnostici vedere [ITERATION_04.md](ITERATION_04.md).

## Parametri replay v0.6

| Chiave | Significato |
| --- | --- |
| `frontend.type` | `hackrf` oppure `file`; non esiste più `synthetic` |
| `replay.path` | File CI8 chiuso sul computer del ricevitore, richiesto prima dello Start |
| `replay.start_sample` | Offset iniziale in coppie complesse |
| `replay.sample_limit` | Limite di coppie; 0 legge tutto il range restante |
| `replay.realtime` | Pacing al sample rate configurato; default true |
| `replay.loop` | Ripetizione del range con epoca/gap espliciti; default false |

I profili precedenti con frontend synthetic devono essere sostituiti con un profilo HackRF o file; la lettura non ignora silenziosamente chiavi obsolete. I nomi INI e JSON conservano il formato del protocollo: il naming lowerCamel dei membri C non rinomina le chiavi sul disco o sul WebSocket.
