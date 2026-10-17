# Architettura del kernel e della console

L’architettura recepisce lo schema e le istruzioni del 28 settembre 2026. Sono superate le proposte di implementazione C++/SoapySDR e di lettori indipendenti per ogni canale.

La revisione del 17 ottobre 2026 separa la matematica dal ricevitore e aggiunge un dispatcher
parallelo opzionale. Frontend, conditioning, registrazione e diagnostica sono operativi;
acquisizione satellitare, tracking, demodulazione e PVT restano moduli da integrare. CLI e
controller registrano attualmente zero canali GNSS: il nuovo pool è pronto per moduli reali,
ma non implica che questi algoritmi siano già presenti.

```mermaid
flowchart TD
    H[Frontend nativo o file] --> Q[Trasporto e buffer posseduti dal frontend]
    Q --> F[API frontend acquire e release]
    F --> C[Conditioning bypass o DC]
    C --> A[Assemblaggio finestra comune]
    A --> R[Ring centrale I/Q]
    R --> E[Motore: una sola vista]
    E --> G[Dispatcher seriale o pool di canali]
    G --> B[Completamento di tutti i job della finestra]
    B --> W[Coda di registrazione opzionale]
    W --> D[Writer su disco]
    B --> X[Rilascio unico della finestra]
```

La coda iniziale appartiene al trasporto del frontend: assorbe i burst USB e mantiene breve la callback. Non è un secondo stream GNSS né introduce cursori per satellite. Nella modalità di sola registrazione il motore salta il gruppo di canali e alimenta direttamente il writer.

Il thread che esegue `NgReceiverRun()` preleva i blocchi del frontend, applica il conditioner, assembla finestre e coordina i canali. Libhackrf ha i propri thread di trasporto; il recorder ha un thread aggiuntivo. Non esiste un thread per satellite.

## Confini dei moduli e librerie

| Responsabilità | Sorgenti/API | Confine |
|---|---|---|
| Motore | `src/core/`, `receiver.h`, `channel.h` | Ordine delle finestre, lease, dispatch, transizioni, stop e chiusura |
| Frontend | `src/frontend/`, `frontend.h` | Acquisizione dei campioni e adattatori nativi; nessuna logica di tracking |
| Conditioning | `src/conditioning/`, `conditioning.h` | Trasformazione condivisa prima del ring, stato temporale proprio |
| Algoritmi GNSS futuri | Acquisizione/tracking dietro `TNgChannelOps` | Stato e workspace per canale; nessun cursore autonomo sul ring |
| Matematica | `src/dsp/`, `include/ngene/dsp/` | Funzioni numeriche e piani; indipendente da driver, GUI e scheduler |
| Osservazione/registrazione | `src/analysis/`, `src/recording/` | Code/workspace limitati e worker indipendenti |
| Controllo/presentazione | `src/control/`, `src/web/`, `web/` | Comandi e snapshot; nessuna elaborazione GNSS nei thread HTTP |
| Piattaforma | `src/platform/` | Thread, mutex, condizioni, clock e filesystem portabili |

Le dipendenze CMake ora rendono riutilizzabile la matematica senza collegare frontend o controller:

```mermaid
flowchart TD
    App[CLI o Web] --> Control[ngene_control]
    App --> Engine[ngene_core / NGene::Engine]
    Control --> Engine
    Engine --> Dsp[ngene_dsp / NGene::Dsp]
    Engine --> Platform[ngene_platform / NGene::Platform]
    Dsp --> Base[ngene_base / NGene::Base]
    Platform --> Base
```

`ngene_base` contiene gli errori comuni; evita una dipendenza circolare DSP→motore.
`ngene_dsp` non dipende dai thread o dalle librerie dei dispositivi. Il test FFT collega
direttamente DSP e piattaforma di test, senza `ngene_core`. Gli altri moduli mantengono
interfacce C e directory separate, pur essendo ancora raccolti nel target `ngene_core`.
Separarli in ulteriori librerie sarà utile quando avranno consumatori indipendenti.

## Concorrenza e scelta dei core

La politica implementata distingue tre decisioni:

1. **Numero di worker fisso per run**, esplicitamente scelto dal chiamante. Thread e scratch
   dei risultati sono preparati prima dello start del frontend; nessuna allocazione per finestra.
2. **Assegnazione dinamica dei job**: ogni worker libero preleva il prossimo canale della
   finestra. Un canale impegnativo non resta legato a uno specifico worker fra finestre.
3. **Assegnazione ai core affidata al sistema operativo**. Il ricevitore non impone affinity
   o priorità speciali. L'affinity limita le scelte dello scheduler e va valutata soltanto
   con misure sulla macchina di destinazione; anche la
   [documentazione Windows](https://learn.microsoft.com/en-us/windows/win32/procthread/multiple-processors)
   raccomanda generalmente di lasciare questa scelta al sistema.

`NgReceiverRun()` e `NgReceiverRunEx()` conservano l'ordine sequenziale precedente.
`NgReceiverRunScheduled()` accetta `TNgReceiverExecution { workerCount }`: NULL o 1 usa il
percorso seriale; 0 è invalido; il numero effettivo non supera quello dei canali. Con zero
o un canale e in modalità record-only non viene creato alcun pool. La scelta è un'API del
motore, non un parametro GUI che avvii algoritmi GNSS assenti.

```c
TNgReceiverExecution execution = {4};
int result = NgReceiverRunScheduled(&config, &frontend, &conditioner,
                                    channels, channelCount, StopRequested, context,
                                    &stats, &execution, &hooks, &error);
```

Tutti i job ricevono gli stessi campioni immutabili, indici e gap. Il motore mantiene
l'unica lease del ring fino alla barriera di completamento. Lo stesso canale non elabora
due finestre contemporaneamente. I contesti mutabili condivisi richiedono sincronizzazione
esplicita; un callback parallelo non può dipendere dall'ordine degli altri canali.

In parallelo tutti i job della finestra terminano anche in presenza di un errore. Il motore
valida e applica le transizioni riuscite dopo la barriera; il primo errore nell'ordine di
registrazione determina la diagnostica restituita. Chiude e unisce i worker prima di
distruggere i campioni. Il percorso seriale conserva lo stop al primo errore.

Frontend, conditioning e hook restano sul thread coordinatore; recorder e diagnostica
mantengono i propri worker. La barriera fa dipendere l'avanzamento dal canale più lento:
il pool non modifica le politiche di overflow e non garantisce hard real-time. Non vengono
avviati thread interni per ogni FFT. Se due sessioni live/replay condividono la macchina,
il chiamante deve distribuire fra loro un budget complessivo di worker.

## Libreria matematica e selezione del target

| Famiglia | Header pubblico | Implementazione |
|---|---|---|
| Trasformate | `ngene/dsp/fft.h` | `src/dsp/fft/` e kernel privati in `x86/`, `arm/` |
| Statistica I/Q | `ngene/dsp/statistics.h` | `src/dsp/statistics/moments.c` |
| Potenza complessa | `ngene/dsp/power.h` | `src/dsp/power/power.c` |

`ngene/dsp/sample_ops.h` resta l'umbrella compatibile. Formule, reference e loop SIMD delle
primitive sono stati separati senza modificarli. Correlazione, NCO/mixing, finestre,
filtri e operazioni fixed-point avranno famiglie proprie quando verranno portate e validate;
non sono aggiunte implementazioni vuote.

Il chiamante usa sempre `NgFftExecute`, `NgIqMoments` o `NgPowerAccumulate`, senza tipi SIMD
né selezioni ISA negli algoritmi GNSS. `TargetArchitecture.cmake` interroga le macro del
compilatore target; `FftBackend.cmake`, condiviso fra ricevitore e benchmark, sceglie un kernel
e le relative opzioni locali. Nessun rilevamento dell'architettura del computer host e
nessun `-march=native` globale.

La FFT usa SSE2 assembly su x64, SSE2 intrinsics su x86-32 e NEON intrinsics su ARM64/ARMv7.
Su x86-32 resta il controllo SSE2 al momento della creazione del piano, necessario per
eseguire il fallback su CPU prive dell'ISA. Questo controllo di sicurezza non cambia l'API.
Le primitive di statistica/potenza mantengono SIMD su x64/ARM64 e C su x86-32/ARM32.
`NGENE_FFT_REFERENCE` e `NGENE_SAMPLE_REFERENCE` selezionano gli oracoli in compilazione.

Ogni famiglia dichiara range, scaling, allineamento, aliasing, remainder e overflow negli
header. I piani FFT sono immutabili e condivisibili; ogni chiamata usa buffer disgiunti.
Precisione e parallelismo sono separati: più worker non cambiano l'ordine aritmetico interno
di una FFT né il suo contratto bit-exact.

## Struttura proposta per le prossime integrazioni

```text
src/
  core/           motore, finestre, stati e dispatcher
  frontend/       adattatori di trasporto e file
  conditioning/   stadi condivisi prima del ring
  stream/         formati, indici, ring e lease
  dsp/            matematica per famiglia e kernel CPU privati
  acquisition/    futuro: ricerca, raffinamento, conferma
  tracking/       futuro: correlatori, loop, holdover e riacquisizione
  navigation/     futuro: decodifica e dati di navigazione
  observables/    futuro: misure su epoche comuni
  pvt/            futuro: soluzione di navigazione
  analysis/       diagnostica asincrona
  recording/      registrazione asincrona e metadati
  control/        ciclo di vita delle sessioni
  platform/       servizi OS
  web/            adattatore HTTP/WebSocket
```

L'adattatore di canale riceverà la finestra dal motore e invocherà acquisizione o tracking
in base allo stato. Il motore manterrà scheduling, tempo e transizioni; i moduli GNSS
manterranno algoritmi, accumulatori e workspace. La migrazione parte dai kernel e dagli
algoritmi originali documentati in [LEGACY_MIGRATION.md](LEGACY_MIGRATION.md), con test su
replay prima di collegare gli osservabili e il PVT.

## Memoria e tempo

- Il frontend possiede i campioni fino a `release()`. La callback USB copia in memoria preallocata prima che libhackrf riutilizzi il transfer.
- Il conditioner produce una vista valida fino alla chiamata successiva e non trattiene quella di ingresso. Il bypass non copia. I percorsi attuali conservano formato nativo CI8/CU8/SC16 Q11, rate, conteggio, indici ed eventi. Il contratto dei canali GNSS resta per ora CI8.
- Il motore assembla finestre di `window_samples` coppie; confini USB e confini delle finestre non devono coincidere.
- Il ring è **a blocchi**, con slot contigui riutilizzati circolarmente. Una vista non attraversa fisicamente due slot; il wrap avviene fra finestre. È un ring di segnali complessi, non due ring indipendenti I/Q.
- Una sola lease è attiva per il consumatore del ring. Mentre è attiva, i dati restano immutabili e non sovrascrivibili. Tutti i canali vedono lo stesso puntatore, intervallo ed evento.
- Il motore produce e acquisisce il ring centrale nello stesso thread, una finestra alla volta. Il dispatcher opzionale parallelizza i canali sulla finestra corrente; non sovrappone finestre successive né introduce più consumatori logici del ring. La coda USB è concorrente e indipendente.
- Le copie correnti sono esplicite: ingresso USB→coda frontend, assemblaggio della finestra, pubblicazione nel ring centrale, eventuale coda del recorder. Non sono copie private dello stream per canale. Ridurle è un’ottimizzazione successiva da misurare.
- Gli indici contano **coppie** I/Q. `sample_rate_hz` è il numero di coppie/s. L’indice assoluto nella coppia `(epoch, first_sample)` non è la posizione fisica nello slot.
- HackRF, nell’interfaccia usata qui, non fornisce un timestamp RF per blocco. L’orario host del file è informativo; non viene presentato come tempo GNSS né come istante esatto dell’antenna.

Quando il frontend perde campioni nella propria coda, continua il conteggio dei campioni ricevuti dalla callback: il salto risulta visibile al motore. La durata di perdite non osservabili dal software non viene inventata. La disconnessione arresta il run con errore; non viene implementato un riavvio automatico che nasconda la discontinuità.

Un gap interrompe la finestra in costruzione. L’eventuale prefisso valido viene consegnato come `NG_BLOCK_PARTIAL`; la prima nuova finestra porta `NG_BLOCK_GAP`. Anche EOF e arresto richiesto possono produrre una finestra parziale: nessuno zero padding. Il modulo GNSS deve decidere come invalidare o completare i propri accumuli. Le epoche e gli indici non possono regredire senza una nuova epoca.

## Interfacce

| Header | Contratto |
|---|---|
| `frontend.h` | Apertura/configurazione, start, acquire/release, stop, statistiche, destroy |
| `conditioning.h` | Trasformazione sincrona a rate costante prima del ring |
| `ring.h` | Pubblicazione bounded, acquire/release unico, close e drain |
| `channel.h` | Stato privato per istanza, stessa finestra per tutti, richiesta di transizione |
| `receiver.h` | Coordinamento, stop, assemblaggio, dispatch e statistiche |
| `recording.h` | Writer asincrono a memoria limitata, segmentazione e chiusura |

Le tabelle di funzioni C consentono l’iniezione di moduli. Le strutture interne dei backend restano private; nessun tipo libhackrf compare negli header NGene pubblici. L’ABI è iniziale e versionabile, non congelata per plugin binari indipendenti. Il contratto attuale dei canali è volutamente piccolo: non definisce ancora tutta la banca correlatori o l’assistenza alla navigazione.

## Stati del canale

```mermaid
stateDiagram-v2
    [*] --> ACQUISITION
    ACQUISITION --> TRACKING
    TRACKING --> HOLDOVER
    HOLDOVER --> TRACKING
    HOLDOVER --> FAST_REACQUISITION
    FAST_REACQUISITION --> TRACKING
    FAST_REACQUISITION --> ACQUISITION
```

È implementata la validazione delle transizioni, non la decisione GNSS che le provoca. Permanere nello stesso stato è sempre consentito. `HOLDOVER` indica propagazione del tracking ad anello aperto; non dead reckoning della posizione. Demodulazione e formazione degli osservabili saranno attività durante il tracking, non stati esclusivi successivi.

I canali non fanno avanzare il ring. Un’integrazione lunga conserva accumulatori/stato propri; un’acquisizione che richiede storia completa dovrà dichiarare un workspace limitato e la finestra richiesta. Lo stato del canale non deve contenere puntatori persistenti a una vista del ring.

## Errori e chiusura

Il default `stop` interrompe su overflow della coda di ingresso. `drop_newest` mantiene i dati precedenti e conta quelli scartati. Una coda del recorder piena arresta sempre il run: non vengono eliminate registrazioni in silenzio. Gli errori dei moduli fermano il gruppo e non vengono trasformati in successo parziale.

Stop e destroy si eseguono fuori dalla callback. Il recorder svuota la propria coda e chiude i file prima di terminare; la chiusura può attendere l’I/O del filesystem. I campioni ancora nella coda del frontend al termine sono contati separatamente. Le statistiche misurano eventi noti al software, non certificano l’assenza di ogni perdita RF/USB.

## Confini di piattaforma e formato

`src/platform/` isola i servizi Win32/POSIX usati dal motore. Gli adattatori dei dispositivi e i kernel CPU mantengono le proprie dipendenze private. Il codice operativo è C11. Le API DSP esistenti dichiarano già allineamento naturale, remainder, scaling e accumuli; le famiglie future dovranno fare altrettanto.

CI8 appartiene al percorso HackRF; RTL-SDR usa CU8 e bladeRF conserva CI8 o SC16 Q11. Il formato nativo è esplicito in descrittori e viste, con unità in coppie complesse. Filtri, mixer e canali multibit futuri richiederanno un contratto numerico e temporale proprio.


## Controllo e osservazione (v0.2.0)

`apps/main.c` seleziona CLI o Web nello stesso eseguibile. `src/control/` implementa il ciclo di vita senza tipi HTTP; `src/web/` traduce REST/WebSocket in chiamate C; `web/` contiene soltanto presentazione e comandi. Le risorse web sono incorporate in compilazione da CMake. `ngene_core`, `ngene_control` e `ngene_http` sono target di libreria, non processi separati.

Il controller ha un thread manager e al massimo un thread motore. I thread HTTP non eseguono i cicli di ricezione. Il manager serializza start, stop e apertura/chiusura del recorder. Gli hook di `NgReceiverRunEx()` comunicano avvio, progresso e finestre; `NgReceiverRun()` resta il wrapper originale. La callback di finestra copia i dati nella coda del recorder e sottopone i tap pre/post alla diagnostica. Non esegue I/O su disco o rete. Il recorder ha un mutex di lifetime separato; diagnostica e progresso usano try-lock. Il browser legge snapshot fino a 30 Hz senza ricevere raw I/Q.

Lo snapshot JSON è costruito dopo aver copiato lo stato fuori dal mutex. Un browser lento non trattiene una lease del ring. Restano costi di copie e sincronizzazione da misurare: non è una garanzia hard real-time.

Start recording pubblica il nuovo recorder fra finestre complete. Stop recording lo scollega fra finestre, poi attende il drain nel manager mentre il motore continua. Stop receiver chiude anche il recorder; la conferma arriva dopo il join/flush. La richiesta di Stop durante Starting cancella l’avvio o ne attende la chiusura. Errore di scrittura/overflow non viene nascosto come perdita accettabile.

La destinazione scelta nella GUI appartiene al computer del processo NGene. La directory del profilo persistente rimane separata. La lista file riguarda la destinazione attualmente configurata e sopravvive al riavvio perché legge il filesystem. Per tornare a registrazioni di un’altra cartella, selezionarla a ricevitore fermo. Il trasferimento HTTP legge soltanto file di registrazione chiusi; non è un consumatore dello stream live.

Il modulo `src/analysis/` implementa ora l’osservazione spettrale con FFT intera interna, worker dedicato e configurazione runtime. La GUI presenta risultati e waterfall. `docs/ANALYSIS.md` descrive la semantica dei burst e delle statistiche.


## Aggiornamento v0.3

Ordine confermato dall’utente: conditioning condiviso **prima** del ring centrale. Tap di registrazione selezionabile pre/post; nessun secondo ring GNSS. Le code del recorder e i workspace temporanei dell’osservatore hanno scopi e proprietà distinti dal ring che alimenta i canali.

`HOLDOVER → TRACKING` è consentito e verificato: il recupero del segnale può richiudere direttamente la retroazione. La fast reacquisition è un tentativo successivo in caso di mancato recupero, non un passaggio obbligato. HOLDOVER non indica dead reckoning della posizione.


## Aggiornamento 0.4

La precedente anteprima raw I/Q è rimossa dalla telemetria. Il receiver non attende i lock GUI: tap diagnostici e progress usano try-lock; il recorder ha un lock di lifetime separato, condiviso soltanto con il manager. PSD e istogramma hanno workspace e cadenze indipendenti; il ring GNSS resta unico. L’ownership della PSD e i controlli end-to-end sono descritti in [ITERATION_04.md](ITERATION_04.md). Il conditioner opzionale DC mantiene lo stato fra blocchi. Il riferimento digital IF appartiene allo stream ed è disponibile ai canali: nessun mixer è aggiunto al flusso comune.


## Sorgenti indipendenti 0.6

Il frontend FILE implementa la stessa interfaccia acquire/release di HackRF, con memoria posseduta dal suo backend e lease esplicita. Niente frontend sintetico nel prodotto. [REPLAY.md](REPLAY.md) definisce formato, EOF, pacing e loop.

Un server può ospitare due controller: primario e replay parallelo. Ciascuno possiede un motore e uno stream indipendente, con ring, conditioner, diagnostica e recorder propri. Questo non duplica il ring per i canali di uno stream: tutti i satelliti di quel motore continuano a vedere la stessa finestra. Clock e campioni delle sorgenti non vengono mescolati. La GUI seleziona il controller da osservare/comandare; i thread di ricezione non attendono il browser.
