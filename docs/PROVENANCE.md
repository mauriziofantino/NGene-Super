# Provenienza e dipendenze

Questo è un incremento dell’evoluzione di NGene, che resta la base funzionale e algoritmica del progetto. La [mappa di migrazione](LEGACY_MIGRATION.md) traccia il recupero dei moduli originali. Conserva e rende esplicita l’idea del buffer condiviso e della gestione per canale del ricevitore storico, recependo il nuovo avanzamento comune richiesto dall’utente. Non è una riorganizzazione meccanica dell’intero archivio, né include copie del vecchio assembly o dei driver USB dismessi.

Sono stati consultati `frontEnd/circularBuffer.h`, `frontEnd/circularBuffer.c` e `receiver/channelManager.c` dell’archivio fornito, oltre al manuale e allo schema dell’utente. Il codice storico non è stato modificato. La selezione dei moduli DSP/GNSS da riutilizzare sarà tracciata negli incrementi successivi, preservando eventuali obblighi di licenza.

Il codice di questo incremento è C; CMake e CI sono strumenti di build. Non sono richiesti Python o C++ per compilare o eseguire NGene. Le directory di build e gli SDK impiegati per la verifica non sono distribuiti nei sorgenti.

## Componenti esterni

- **libhackrf**, Great Scott Gadgets: API C nativa per HackRF; dipendenza opzionale esterna. Verifica dell’adattatore effettuata contro la versione host 2024.02.1, API library 0.9, commit `18b485e3b6d2031c15a79ba89cdb42b5fa245f24`.
- **libusb**: dipendenza del trasporto di libhackrf, gestita dalla distribuzione/SDK.
- Libreria C e primitive OS POSIX/Win32; nessuna dipendenza DSP esterna.

Fonti primarie utilizzate per l’adattatore:

- [API libhackrf](https://github.com/greatscottgadgets/hackrf/blob/v2024.02.1/host/libhackrf/src/hackrf.h).
- [Implementazione del client di acquisizione](https://github.com/greatscottgadgets/hackrf/blob/v2024.02.1/host/hackrf-tools/src/hackrf_transfer.c).
- [Installazione del software HackRF](https://hackrf.readthedocs.io/en/latest/installing_hackrf_software.html).

Non vengono copiate o cancellate intestazioni di autore/copyright di librerie esterne. Non viene applicata automaticamente una nuova licenza pubblica al progetto: il titolare dovrà decidere le condizioni di distribuzione prima della pubblicazione.

## Stile

API documentate con Doxygen, tipi a larghezza esplicita per dati/intervalli, indentazione a quattro spazi, gestione degli errori esplicita. Nessun autore nei file NGene nuovi. Non si dichiara una certificazione o una verifica completa di conformità ESA: sarà un’attività dedicata, distinta dalla verifica funzionale di questo incremento.


## Dipendenze web incluse (v0.2.0)

- **CivetWeb v1.16**, commit `d7ba35bbb649209c66e582d5a0244ba988a15159`: [sorgente ufficiale](https://github.com/civetweb/civetweb/tree/v1.16). Server HTTP/WebSocket in C, licenza MIT conservata in `third_party/civetweb/LICENSE.md`. Sono inclusi header, implementazione C e file `.inl`; TLS, CGI e scripting non sono abilitati. Una modifica locale in `read_websocket()` rifiuta payload maggiori di `NGENE_WS_MAX_PAYLOAD` prima dell’allocazione (4096 byte nella build). `BUILD_DATE` è fissato da CMake per evitare macro temporali non riproducibili.
- **cJSON v1.7.19**, commit `c859b25da02955fef659d658b8f324b5cde87be3`: [sorgente ufficiale](https://github.com/DaveGamble/cJSON/tree/v1.7.19). Parser/serializer C, licenza MIT conservata in `third_party/cjson/LICENSE`. Profondità massima 32 impostata in compilazione. Non vengono compilate le utility JSON patch.

Le attribuzioni degli upstream sono mantenute nei loro file. Le dipendenze web incluse non richiedono download durante CMake. Il provider HackRF SOURCE opzionale e vcpkg possono invece scaricare dipendenze mancanti. Playwright/Chromium e Node sono solo strumenti facoltativi del test GUI, non dipendenze dell’eseguibile o del browser dell’utente.


## FFT v0.3

La nuova FFT deriva dal disegno Q14/radix-2 di NGene originale, con analisi di `asmFolder/fft.c`, `fft.h`, `simd.h`. Le butterfly sono state riscritte per rendere esplicito l’ABI, eliminare il backup globale e correggere saturazione/scaling. Non viene importata alcuna libreria FFT esterna; [FFT_REVIEW.md](FFT_REVIEW.md) documenta cosa è mantenuto e cosa cambia. Unicorn è stato usato soltanto come strumento esterno di verifica delle istruzioni NEON, non è una dipendenza del progetto e non è incluso.

## Consolidamento v0.4

L’aritmetica FFT v0.3 resta invariata. Sono aggiunti il file ARMASM64 per Windows ARM64, primitive interne intrinsics SSE2/NEON e relativo riferimento C, conditioner DC continuo, integrità telemetrica e controlli GUI. Non sono aggiunte dipendenze DSP esterne. Il report `ITERATION_04.md` distingue le verifiche eseguite dai target ancora da validare nativamente.


## Consolidamento v0.5

API NGene rinominate e commentate; `third_party/` invariato. Il provider SOURCE costruisce soltanto hackrf.c upstream senza modificarlo, da checkout locale o tag v2026.01.3 configurabile. Il test locale ha utilizzato v2024.02.1 già disponibile; il nuovo tag non è stato scaricato/provato qui. libusb e pthreads sono dipendenze opzionali gestibili con il manifest vcpkg Windows. L'installazione conserva le licenze CivetWeb/cJSON e, per il provider SOURCE, COPYING di libhackrf. Il titolare deve ancora decidere la licenza del codice NGene, compatibilmente con le dipendenze usate.


## Consolidamento 0.6

Scaricato e compilato il tag upstream HackRF **v2026.01.3** con URL e SHA256 versionati in `cmake/DependencyLock.cmake`. La libreria riporta versione **0.9.2**; la data del tag non è la versione ABI. Manifest con baseline vcpkg `0699a19d0c6386247ce50d4dedbb8217d484d536`, libusb 1.0.30 e pthreads 3.0.0#14.

Il sorgente upstream non viene modificato. Su Windows si genera un header consumer separato correggendo il selettore export/import, conservando firme e licenza; la DLL compila l'header originale. Le 21 copie vendorizzate CivetWeb/cJSON rimangono byte per byte identiche all'incremento 0.5. Warning ridotti solo per target/include esterni, non per il codice NGene.

Naming, formattazione e documentazione sono applicati al codice NGene. File replay e fixture dei test sono implementazioni C interne; nessuna libreria DSP esterna aggiunta. Tooling AST, formatter, QEMU e Doxygen usati per le verifiche sono esterni al progetto e non sono inclusi nel pacchetto sorgente.
