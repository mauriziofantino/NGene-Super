# WebGUI e API di controllo · v1

La GUI in inglese è inclusa nel medesimo progetto ed eseguibile C del kernel. Il servizio locale resta acceso dopo Stop receiver. La chiusura della scheda del browser non interrompe ricezione o registrazione; riaprire la pagina riporta lo stato corrente. Non avviare più istanze sullo stesso dispositivo o directory di profilo.

## Parametri e salvataggio

Configurazione di frontend, buffer, durata, nome base e directory di registrazione: modifiche ammesse solo in stato `stopped` o `error`, con revisione corrente. Import/export INI permettono profili riutilizzabili. Validate controlla sintassi e limiti; Apply & save applica la normalizzazione del percorso e scrive il profilo. I permessi reali per i file di registrazione sono verificati all’apertura del recorder; Validate non certifica spazio disco o disponibilità USB.

Il campo Folder on receiver computer accetta un percorso del filesystem del processo NGene, non quello del browser. La cartella finale viene creata se il genitore esiste; per il replay è disponibile un navigatore di cartelle del ricevitore; la destinazione di registrazione conserva il campo cartella. Percorsi relativi sono risolti rispetto alla directory di lavoro del processo. Il percorso assoluto effettivo è mostrato in Recording session.

**Transfer** usa il download nativo del browser. Può copiare i dati a un computer diverso, oppure nella cartella download dello stesso PC. Gli originali restano sul ricevitore. Il tunnel SSH del README consente l’uso remoto senza cambiare binding del servizio; il codice non implementa un servizio LAN autenticato.

## Protocollo

Prefisso `/api/v1`. Risposte JSON; contatori e revisioni a 64 bit sono **stringhe decimali** per evitare arrotondamenti JavaScript. Richieste POST: `Content-Type: application/json`, corpo con Content-Length di massimo 16384 byte e header `X-NGene-Token` ottenuto da bootstrap.

| Metodo e percorso | Contenuto / effetto |
|---|---|
| GET `/bootstrap` | Versione, OS/architettura, disponibilità HackRF compilata, token della sessione server |
| GET `/config` | `{revision, ini, mutability:"stopped"}` |
| POST `/config/validate` | `{revision:"…", ini:"…"}`; sintassi e limiti senza applicare |
| POST `/config` | Stesso corpo; verifica revisione/stato, salva e restituisce configurazione normalizzata |
| GET `/status` | Stato, comandi, contatori, cartella, registrazione e snapshot I/Q |
| POST `/receiver/start`, `/receiver/stop` | `{revision:"…"}`; risposta 202 con command_id |
| POST `/recording/start`, `/recording/stop` | `{revision:"…"}`; stesso contratto asincrono |
| GET `/files` | Revisione/directory e fino a 200 file selezionati, dimensioni, closed e truncated |
| POST `/files/delete` | `{name, revision}`; elimina il solo file chiuso nella directory corrente |
| POST `/replay-files/browse` | `{path?, filter?}`; cartelle/file CI8 del ricevitore, massimo 500 elementi |
| POST `/replay-files/inspect` | `{path}`; sidecar e INI di bozza, nessuna modifica/avvio |
| GET `/files/<name>` | Allegato binario chiuso: raw, indice o metadati |
| POST `/shutdown` | `{}`; chiude HTTP, arresta ricevitore, drena il recorder e termina |
| WebSocket `/events?token=…` | Client invia il frame testo `status`; server risponde con lo snapshot JSON |

202 significa **accettato**, non completato. Attendere `completed_command_id >= command_id` e controllare `command_ok`, `state` e `message`. 409 segnala conflitto di stato, revisione o operazione pendente; 400 input non valido; 403 origine/token non validi. Un errore di salvataggio del profilo è riportato come conflitto 409 con messaggio esplicativo.

La telemetria WebSocket è richiesta dal client, con al massimo una richiesta pendente e un solo snapshot più recente da renderizzare. La GUI espone 1/2/4/10/20/30 Hz come limiti di refresh. Tre client sono ammessi; richieste più ravvicinate di 20 ms chiudono la connessione per limitare abusi. Il comando `status:MASK` rinnova per due secondi l'interesse a PSD (bit 1) e istogrammi (bit 2); `status` resta compatibile e attiva entrambi. Non sono inviati raw I/Q. JSON e rete sono fuori dai callback campioni. Ogni PSD include integrità e hash; vedere [ITERATION_04.md](ITERATION_04.md).

Host/Origin sono limitati al listener locale; il token cambia al riavvio. Il server non invia CORS permissivo, non accetta path arbitrari per il download e non serve directory statiche dal filesystem. Questo modello è per uso locale e tunnel, non per un sito pubblico. Il nome della directory scelto tramite configurazione è comunque un’autorizzazione a scrivere lì con i permessi del processo.

## Stati

Receiver: `stopped → starting → running → stopping → stopped`; errori portano a `error`. Stop durante Starting cancella/chiude l’avvio. Record: `off → starting → recording → stopping → off`. Stop recording drena il writer mentre il ricevitore continua. Stop receiver chiude entrambi; la console resta attiva. Un errore del writer arresta il run e rimane visibile.

Il grafico e i contatori sono effettivi. Spettro, waterfall e istogrammi sono implementati; acquisizione/tracking/PVT restano da migrare. Il refresh e la pausa del grafico sono impostazioni runtime del browser; i parametri del modulo di osservazione hanno l’API runtime `/analysis/config` descritta in [ANALYSIS.md](ANALYSIS.md).

## Test browser opzionale

Da una macchina di sviluppo con Node >=18, Playwright e Chromium installati:

```sh
NGENE_EXECUTABLE=build/ngene node tests/web_ui.cjs
```

Il test crea directory temporanee e avvia un proprio server; configura nome/cartella dalla GUI, controlla persistenza e conflitti, registra, ricarica la pagina, ferma il recorder mantenendo il ricevitore attivo, confronta i download byte per byte e verifica il layout mobile. Variabili facoltative: `PLAYWRIGHT_MODULE`, `BROWSER_EXECUTABLE`, `NGENE_TEST_PORT`, `NGENE_SCREENSHOTS`. Nessun Node/Playwright è necessario all’utente per usare NGene.


## Due viste, un ricevitore

**Observation** mostra soltanto risultati e controlli operativi. **Configuration** separa i parametri statici del ricevitore (bloccati durante il run) da quelli runtime dell’analisi e dalle preferenze puramente grafiche. Lo stile è dark su entrambe le viste. I pulsanti Start/Stop restano comuni.

Lo snapshot status aggiunge l’oggetto `analysis`. Spettro e waterfall consumano lo stesso risultato, l’istogramma usa conteggi del backend. Le semantiche di scala, campionamento dei burst, revisione e persistenza sono in [ANALYSIS.md](ANALYSIS.md). Non usare il grafico ridotto a 1024 punti come output numerico completo per nuovi algoritmi: usare le API C e i bin FFT nativi.


## Controlli 0.4

Rate e sorgente pre/post separati per PSD e istogramma, overlap 0..99% intero, Welch nel limite del burst, densità per Hz, media temporale/EMA. Il display ha scale Y e colore globali regolabili, history/rate waterfall, limiti X e riferimenti hardware/signal/RF. Il fit colore è un comando esplicito una tantum e viene applicato a tutte le righe. Nessuna normalizzazione per riga. Nascondere un pannello, aprire Configuration o mettere in pausa fa scadere l’interesse diagnostico, senza fermare la ricezione.

The waterfall header provides **View → 2D waterfall / 3D waterfall**. Both views use the same bounded PSD history; switching projection also works while paused and preserves every stored snapshot. In the default 3D orientation, frequency runs from left to right, older observations recede from the newest row at the front, and height uses the Spectrum minimum/maximum settings. Colours use the same global Waterfall colour minimum/maximum in both views. The depth axis uses the snapshots' production times, including display skips. The history capacity remains 32–2048 snapshots; 3D drawing samples at most 48 history slices and 192 frequency buckets, retaining each bucket's peak so narrow carriers remain visible. Rendering uses the browser's existing Canvas API, without extra dependencies, raw I/Q transfers or receiver changes. These are browser display preferences and are not saved in the receiver INI profile.

**Spectrum height** and **Waterfall height** independently resize each canvas from 180 to 720 pixels. Drag the divider immediately below either canvas to resize it directly; with the divider focused, Up/Down changes height by 10 pixels, Page Up/Down by 100, and Home/End selects the minimum/maximum. The sliders also work on touchscreens. **Expand plots** uses the browser viewport for both plots, with scrolling if needed; **Restore plots** or Escape returns to the normal page and retains both selected heights. Resizing redraws existing data, including paused plots, without modifying the receiver configuration or the stored PSD history.

In 3D, drag the waterfall with a mouse or one finger to rotate its azimuth and elevation. The four arrow buttons provide the same controls; when the canvas has keyboard focus, Left/Right rotates and Up/Down tilts. Elevation is bounded to 12–78 degrees. **Reset view**, Home with canvas focus, or a double-click restores the default perspective. Frequency, observation-age and power axes rotate with the surface, and facets are sorted by camera depth so rear rows remain correctly ordered when viewing the history from another side. The 2D waterfall retains normal touch scrolling. Rotation works while paused and keeps the same PSD samples and global scales.

`tests/web_plot_interactions.cjs` is a focused display regression that serves the current web assets under the production Content Security Policy and supplies deterministic PSD frames. Run it with Node and Playwright installed on the test host (the same `PLAYWRIGHT_MODULE` and optional `BROWSER_EXECUTABLE` overrides as `tests/web_ui.cjs`). It verifies plot size changes, mouse and keyboard divider resizing, expand/restore and keyboard focus, mouse/touch/button/keyboard 3D rotation, reset, mesh/elevation bounds, mobile layout and preservation of paused history. The receiver integration and live transport remain covered by `tests/web_ui.cjs`.

Profile and diagnostic validation errors appear next to their fields, with an accessible association, and in the form summary. They remain visible across telemetry updates and request completion; correcting a field or successfully retrying validation updates the feedback. The GUI does not depend on transient browser validation bubbles. RTL-SDR sample rates must be in `(225000,300000]` or `(900000,3200000]` pairs/s; the clock interval `(300000,900000]` is rejected locally as well as by the backend. Integer, buffer and timeout controls follow the backend bounds; server validation remains authoritative. A delayed device-limits response is discarded if its source, serial, tuning or precision no longer matches the draft.

`tests/web_validation.cjs` checks persistent feedback across live WebSocket replies, the RTL-SDR ranges, required/integer fields, delayed backend failures and retry/edit recovery, diagnostic errors and stale device-capability responses against the actual receiver executable. Use the same test-host environment overrides as `tests/web_ui.cjs`; no RF hardware is needed.

For bladeRF, **Input source → Sample precision** selects `SC16_Q11` (12-bit ADC data stored in 16-bit I and 16-bit Q components) or `CI8` (native signed 8-bit I and 8-bit Q transfer, requiring compatible device/FPGA support). The status line displays the actual stream format reported by the receiver. Changing precision invalidates previously queried device limits; query them again before setting gain.


## Configurazione e cancellazione 0.5

Configuration usa sei sezioni: Input source, Receiver, Recording, Diagnostics, Display e Profile. Una sola area di contenuto e un footer comune sostituiscono le colonne diseguali. Le indicazioni before-start/runtime/display spiegano quando i parametri sono modificabili; i vincoli del backend restano applicati.

**Delete** richiede conferma esplicita di filename e directory sul computer del ricevitore. Elimina un solo file chiuso; segmenti/metadati associati restano. Il backend ricontrolla revisione e file attivo prima della rimozione, anche se la lista GUI è diventata obsoleta. Rifiuta nomi estranei ai file di cattura, traversal e symlink/reparse point. File assente: 404; revisione obsoleta/file attivo: 409; nome/operazione non validi: 400. Anche Delete usa token/origin come gli altri POST. Un errore del filesystem viene mostrato; il file non viene dichiarato eliminato in anticipo.


## Sessioni 0.6: primario e replay parallelo

Il selettore **Receiver session** sceglie `Primary / live stream` oppure `Independent file replay`. Entrambe possono essere avviate contemporaneamente; la sessione non visualizzata continua a lavorare. Configurazione, Start/Stop, Recording, file e diagnostica agiscono soltanto sulla selezionata. Lo spegnimento del servizio arresta entrambe.

La sessione replay usa un frontend FILE distinto, profilo `replay/receiver.ini` e API `/api/v1/replay/...`; stessa semantica delle revisioni/comandi del primario. Bootstrap e shutdown restano globali. Sul medesimo WebSocket si invia `replay:MASK` invece di `status:MASK`. Il cambio sessione svuota lo stato grafico e riconnette il socket; è disabilitato durante modifiche non salvate o operazioni pendenti per evitare risposte applicate al controller sbagliato.

**Input source** offre `Recorded I/Q`: percorso sul computer NGene, prima coppia, lunghezza dell'intervallo, pacing e ripetizione. Il pulsante **Browse I/Q files…** permette la selezione da cartelle/elenco e importa Fs e riferimenti RF dal sidecar validato. Se assente, la GUI richiede di impostarli manualmente. Il profilo replay iniziale non ha un file e non parte automaticamente. Dettagli in [REPLAY.md](REPLAY.md).
