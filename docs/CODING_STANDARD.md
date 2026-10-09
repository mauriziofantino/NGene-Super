# Convenzioni del codice NGene — 0.6

Riferimento: ESA BSSC(2000)1 Issue 1 fornito dall'utente. La Rule 38 richiede convenzioni definite dal progetto. Il naming seguente si applica a tutto il codice C di proprietà NGene, inclusi moduli privati e test, non soltanto alle API pubbliche.

| Entità | Convenzione | Esempio | Riferimento |
|---|---|---|---|
| Funzione pubblica | Prefisso `Ng`, parole capitalizzate | `NgRingDestroy` | Rule 41 |
| Funzione privata | Parole capitalizzate | `SegmentOpen` | Rule 41 |
| Tipo, tag struct/enum, typedef | Prefisso `TNg`, parole capitalizzate | `TNgRecorder`, `TNgIqView` | Rules 42–43 |
| Variabile, parametro, membro | Prima parola minuscola, successive capitalizzate | `statsMutex`, `writerError`, `segmentOffset` | Rule 40 |
| Macro, guardia header | Maiuscole con underscore | `NG_ANALYSIS_MAX_FFT_SIZE` | Rule 39 |
| Enumeratore | Prefisso coerente `NG_`, maiuscole con underscore | `NG_FRONTEND_FILE` | Rule 45, convenzione del progetto |

`ng_ring_destroy` e `struct ng_recorder` non sono più simboli NGene. La migrazione interessa anche variabili, membri, parametri e simboli del confine assembly; non sono aggiunti alias. [IDENTIFIER_RENAMES.csv](IDENTIFIER_RENAMES.csv) e [API_RENAMES.csv](API_RENAMES.csv) permettono di aggiornare i consumatori esterni. Occorre ricompilare i moduli che usano gli header NGene.

Eccezioni necessarie: `main`, nomi/ABI delle librerie di sistema e delle dipendenze, relativi mock e macro richieste dal sistema operativo. `third_party/` non viene rinominato. Le chiavi INI/JSON, i percorsi API e le opzioni CMake conservano il contratto di configurazione; non sono identificatori C.

## Documentazione e layout

Ogni prototipo e definizione di funzione NGene ha un commento Doxygen adiacente. Gli header documentano formalmente parametri, unità, esiti/errori, ownership, durata dei buffer e vincoli di thread. Le definizioni descrivono operazione e impiego senza duplicare i tag dei parametri già associati alla dichiarazione: Doxygen unisce le due descrizioni. Nomi dei parametri identici fra prototipo e definizione. Membri delle strutture pubbliche e private documentati con significato, unità e sincronizzazione pertinenti.

Quattro spazi, massimo 100 colonne, parentesi graffe sui blocchi, dichiarazioni di variabili separate. `.clang-format` è il riferimento. Gli header contengono dichiarazioni; implementazioni nei sorgenti. Costanti condivise con unità esplicite in `include/ngene/limits.h`. Commenti tecnici inglesi; nessun nome autore aggiunto ai file NGene. Copyright e licenze esterne conservati.

```sh
cmake -S . -B build-docs -DNGENE_WITH_DOCUMENTATION=ON
cmake --build build-docs --target ngene_docs
```

HTML generato con Doxygen 1.18.0, warning trattati come errori: nessun warning. Doxygen rimane un tool facoltativo di sviluppo.

## Verifiche e perimetro

Un controllo AST dei sorgenti C propri ha rilevato zero violazioni delle convenzioni per tipi/tag, funzioni, membri, variabili e parametri. La build ARM64 verifica anche il ramo NEON. Il controllo `.clang-tidy` è stato eseguito con clang-tidy 22.1.8 su tutti i sorgenti C propri di produzione e test, incluso HackRF: uscita zero, nessuna violazione. I mock conservano esplicitamente i nomi ABI `hackrf_*`/`libusb_*`. Il job CI `naming` configura anche le dipendenze HackRF prima dell'analisi; la pipeline remota non è stata eseguita in questo ambiente.

L'uniformità del naming non equivale alla certificazione di tutte le regole ESA. Una matrice formale delle regole applicabili, delle deviazioni SIMD/OS e delle verifiche semantiche resta un'attività separata. Non rimane però una migrazione dei nomi di tipi/struct rimandata a un incremento successivo.
