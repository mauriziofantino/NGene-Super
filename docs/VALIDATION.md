# Verifiche del kernel, DSP e WebGUI

Risultati correnti **0.6** in [ITERATION_06.md](ITERATION_06.md): Linux HackRF SOURCE 10/10, ASan/UBSan 9/9, headless 8/8, ARM64 eseguito con QEMU 9/9, Doxygen senza warning e browser desktop/mobile. Build Windows nativa ancora da eseguire.

Le sezioni seguenti conservano lo **storico** delle verifiche 0.3 (1 ottobre 2026), 0.4 (2 ottobre) e 0.5. I riferimenti al sintetico e alle cross-build non descrivono le sorgenti o le verifiche correnti. Il perimetro comprende cattura/registrazione, FFT intera e osservazione; non comprende acquisizione satelliti, tracking o PVT.

| Configurazione | Risultato effettivo | Limite |
|---|---|---|
| Linux x86-64, GCC 13.3, Release | Compilazione, test e microbenchmark FFT eseguiti | Misure FFT in `FFT_REVIEW.md`, non prestazioni del ricevitore GNSS |
| Linux x86-64 con libhackrf reale | Compilazione/collegamento riusciti; 6/6 test CTest superati | Il test dell'adattatore usa un driver simulato, non trasferimenti USB reali |
| Linux x86-64, Debug, AddressSanitizer + UndefinedBehaviorSanitizer | 5/5 test superati (analisi, FFT, core, cattura, controller) | LeakSanitizer disabilitato; le istruzioni assembly non sono strumentate dai sanitizers |
| Linux x86-64, Web disabilitato | Build Release e 4/4 test superati | Nessuna dipendenza HTTP compilata |
| Linux x86-64, backend FFT C esplicito | Build Release e 4/4 test superati | Serve da riferimento; non prova le prestazioni di target a 32 bit |
| GUI Chromium, desktop 1512 px e mobile 390 px | Test end-to-end superato, screenshot verificati | Sorgente sintetica; non misura carico a 16 Msps |
| macOS Intel/ARM | Entrambi gli oggetti assembly FFT compilati in formato Mach-O | Applicazione completa e SDK Mac non verificati; nessuna esecuzione nativa |
| Windows x86-64, cross-compilazione Clang tramite Zig | Eseguibile con HTTP/WebGUI, controller e test compilati e collegati in formato PE32+ | Non eseguiti su Windows; MSVC non eseguito qui |
| Linux ARM64, cross-compilazione Clang tramite Zig | Eseguibile con HTTP/WebGUI, controller, NEON e test compilati e collegati in formato ELF AArch64 | Non eseguiti su ARM o QEMU; nessun benchmark NEON |
| Butterfly NEON, emulazione istruzioni AArch64 | 66.680 butterfly complesse confrontate con calcolo intero indipendente, incluse code singole | Emulatore Unicorn sul codice macchina reale, non esecuzione dell'intera FFT/receiver ARM |
| Adattatore HackRF per Windows x86-64 e Linux ARM64 | Oggetti compilati contro il vero header libhackrf | Non collegati a SDK HackRF per quei target; hardware non verificato |
| CLI con registrazione sintetica | 25.017 coppie I/Q, 26 finestre, nessun gap; file segmentati e metadati prodotti | Generatore deterministico, non segnale GNSS |
| Enumerazione USB reale nel workspace | Errore gestito all'inizializzazione libhackrf; uscita non zero | USB non disponibile; serve la prova con HackRF sul PC dell'utente |

Tooling: CMake 4.4.3, GCC 13.3.0, Zig 0.16.0/Clang 21.1, clang-format 21.1.8. Questi strumenti non sono inclusi. Adattatore compilato contro libhackrf host v2024.02.1, con header libusb v1.0.27 e libreria libusb del sistema.

## Contratti verificati dai test C

- Configurazione: default, valori validi, chiavi sconosciute/duplicate e limiti.
- Ring: wrap, coda piena, immutabilità della vista, rilascio, chiusura e svuotamento.
- Concorrenza produttore/consumatore: 5.000 blocchi da 7 coppie, ordine e contenuto controllati.
- Motore: ricomposizione dei frammenti; due canali in stati diversi ricevono lo stesso puntatore e intervallo; transizioni consentite.
- Discontinuità: finestre parziali prima del gap, conteggio dei mancanti, EOF e stop richiesto senza aggiunta di zeri.
- Recorder: 13 coppie in segmenti da 7 e 6, confronto esatto dei byte, indice e metadati, rifiuto della sovrascrittura e directory inesistente.
- Registrazione pre/post: conditioner di test che dimezza i valori; confronto byte per byte di entrambe le modalità, metadati e gestione gap.
- HackRF simulato: parametri API, copia prima del riutilizzo del transfer, overflow stop/drop_newest, indici dopo perdita, transfer non valido e cleanup dopo errore.
- FFT: dimensioni 8..16384, zero/DC/impulso/random ai limiti del contratto, confronto bit per bit SSE2/riferimento C, DFT indipendente sulle dimensioni piccole e tono complesso con controllo di segno e guadagno.
- FFT concorrenti: piano condiviso immutabile e workspace separati; input/output non allineati a 16 byte con sentinelle, rifiuto di overlap e range non valido.
- Worker: tono complesso a 4096 Hz con ampiezza attesa circa −6,02 dBFS, somma conteggi istogramma, cambio parametri runtime, burst interrotto da gap, rail e istogramma indipendente dallo spettro.

La suite non inietta ogni possibile guasto: per esempio, non simula un'interruzione di alimentazione o tutti i casi di filesystem pieno. Le chiamate di scrittura/flush sono controllate dal codice.

## Riproduzione

Dalla radice:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

Per il contratto HackRF su Linux, configurare NGENE_WITH_HACKRF=ON con un SDK di sviluppo disponibile. La scheda non serve per il test simulato.

Per ASan/UBSan su Linux:

    cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DNGENE_SANITIZERS=ON
    cmake --build build-sanitize
    ctest --test-dir build-sanitize --output-on-failure

Nell'ambiente della consegna è stato necessario ASAN_OPTIONS=detect_leaks=0. Su una macchina che supporta LeakSanitizer lasciare attivi i controlli disponibili.

I toolchain GNU forniti e la CI documentano build future Windows/Linux/macOS e ARM64. Le cross-build locali hanno usato Zig con CMAKE_C_COMPILER_ARG1 rispettivamente "cc -target x86_64-windows-gnu" e "cc -target aarch64-linux-musl". La CI GitHub è predisposta ma non è stata eseguita su un repository remoto.

## Prima prova hardware

Verificare sul sistema reale: identificazione del dispositivo, parametri, arrivo dei campioni, registrazione/arresto, contatori di perdita, ritmo sostenibile e tempi CPU. Ripetere su Windows e Linux prima di dichiarare il supporto hardware validato su entrambi.

Le copie e la sincronizzazione del primo incremento sono esplicite. Non è dimostrata una percentuale CPU o una capacità di canali; i valori storici NGene non sono misure di questa versione.


## Controllo e GUI verificati il 30 settembre e il 1 ottobre

`controller_lifecycle`: round trip INI, salvataggio atomico del profilo, revisione obsoleta rifiutata, configurazione bloccata durante ricezione, Start duplicato rifiutato, due sessioni di registrazione senza riavviare il ricevitore, conteggio byte, otto cancellazioni immediate di Start, EOF parziale con registrazione automatica chiusa.

`tests/web_ui.cjs`, eseguito con Chromium Headless 134 e Playwright: configurazione tramite campi GUI, scelta di nome e **cartella diversa da quella del profilo**, creazione cartella, persistenza INI, input invalido/token/origine errati rifiutati, configurazione bloccata durante ricezione, telemetria WebSocket reale, ricaricamento della pagina mantenendo il run, pausa grafico, registrazione indipendente, due sessioni, divieto download di file attivi, confronto byte per byte di tutti i sei file chiusi e del download nativo del browser, layout mobile senza overflow orizzontale, chiusura del servizio con exit 0. Nessun errore JavaScript rilevato. Screenshot in `docs/images/`.

Il test browser legge e confronta byte di sorgente sintetica; non esercita il tunnel SSH né la banda di rete remota. La configurazione persistente è verificata anche dal test C mediante rilettura dal disco. Il codice contiene il caricamento al riavvio, senza ripristinare automaticamente una ricezione in corso.

La prova del 1 ottobre aggiunge tema dark, separazione Configuration/Observation, spettro con picco sintetico a 10 kHz, storia waterfall, istogrammi con conteggi attesi, modifica runtime a FFT 4096/Blackman e istogramma 128 bin/8192 coppie senza cambiare run, selezione recording pre e controllo del metadato scaricato. Nessun errore JavaScript; screenshot desktop, configurazione e mobile aggiornati. La prova GUI usa 100 ksps sintetici per contenere il tempo del test, non il sample rate operativo HackRF.

Per riprodurre la prova browser con Playwright già disponibile:

    PLAYWRIGHT_MODULE=/path/to/playwright BROWSER_EXECUTABLE=/path/to/chromium \
      NGENE_EXECUTABLE=build/ngene NGENE_SCREENSHOTS=docs/images node tests/web_ui.cjs

I benchmark FFT confrontano il nuovo assembly con il riferimento C, non con il vecchio assembly NGene. Non sono stati eseguiti benchmark di throughput/CPU dell'intero ricevitore, test di longevità, USB HackRF reale, test Windows/MSVC o macOS nativi. Il porting riguarda la FFT: correlatori e altre routine storiche restano da migrare. La build ARM64 non dimostra supporto bare-metal o prestazioni real-time su schede embedded specifiche. La conformità ESA completa non è certificata.


## Verifiche 0.4 — 2 ottobre 2026

- Linux GCC, SDK libhackrf collegato: **7/7** test CTest, inclusi nuovi sample/DC e diagnostica. Driver HackRF simulato, nessuna USB reale.
- ASan/UBSan: **6/6** (senza adattatore HackRF); LeakSanitizer disabilitato e assembly non strumentato. Un oggetto di build precedente era troncato; la ricompilazione pulita ha risolto il link e tutti i test sono passati.
- Backend C reference/headless con `-funsigned-char`: **5/5**; verifica della rappresentazione signed dei campioni indipendente da `char`.
- Linux AArch64: programma completo e test cross-compilati con Clang, inclusi nuovi intrinsics NEON. Nessuna esecuzione nativa ARM di questa iterazione.
- Test C: statistiche esatte su distribuzioni note, pre/post indipendenti, istogramma invariato al solo cambio NFFT sugli stessi dati, PSD/Hz e ENBW, media temporale ed EMA in potenza lineare, checksum e NaN/Inf, DC continuo invariante alla suddivisione dei blocchi, metadata tuning/NCO.
- Browser: producer a 12,5 Hz, GUI rallentata a 1 Hz, salti effettivi senza righe sintetiche; rifiuto di un frame intenzionalmente alterato, checksum producer/telemetry/browser, opzione 30 Hz senza blocco richieste, pausa dei producer nascosti con ricevitore ancora attivo, assi con digital IF e menu guadagni. Passano inoltre registrazione, persistenza, download byte per byte, layout mobile e shutdown. Nessun errore JavaScript.
- Microbenchmark primitive e DC in `ITERATION_04.md`, riproducibili con `ngene_sample_tests --benchmark` Release. Nessuna misura del throughput globale del ricevitore.

La build Windows richiesta usa ora i preset **MSVC** espliciti, MASM/ARMASM64 e CI relativa. In questo ambiente Linux non sono disponibili `cl.exe`, `ml64.exe` o Windows SDK: **nessuna compilazione/esecuzione MSVC locale viene dichiarata**. La compilazione MinGW del precedente incremento non viene usata come validazione MSVC. I test ARMASM64 e il collegamento HackRF per Windows restano da eseguire sulla piattaforma.


## Verifiche 0.5 — 9 ottobre 2026

[ITERATION_05.md](ITERATION_05.md) riporta la matrice corrente: Linux con SDK 9/9, provider SOURCE locale 9/9, ASan/UBSan 8/8, senza web 7/7, browser end-to-end, cross-build ARM64 e oggetti FFT Mach-O. Nessuna esecuzione nativa MSVC della nuova automazione qui; il recap dell'utente valida il precedente incremento. Nessun nuovo benchmark o test USB reale.

Controllo strutturale Doxygen/naming: 274 firme di produzione, 328 con test; brief/details e copertura dei parametri adiacenti. HTML Doxygen non generato localmente; target e CI predisposti. Le API esterne e i loro mock mantengono la ABI upstream.
