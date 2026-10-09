# NGene 0.5 — build nativa, Doxygen e configurazione GUI

9 ottobre 2026. Modifiche incrementali al progetto 0.4 esistente. NGene resta la base; frontend → conditioning condiviso → unico ring → canali invariato.

## Windows

Rimosso il generatore Visual Studio 2022 hardcoded. Preset MSVC x64/ARM64 con/senza HackRF; vcpkg individuato prima di project(), manifest incluso e triplet target coerente. Compilazione del solo libhackrf upstream; niente FFTW per hackrf-tools. SDK/sorgenti locali configurabili da comando. DLL risolte e raccolte dopo il link; installazione/CPack con runtime, configurazioni e licenze. [Procedura](WINDOWS.md).

Inizializzato lo snapshot del recorder nel percorso C4701. Policy `_CRT_SECURE_NO_WARNINGS` per target NGene MSVC, mantenendo `/W4` e UTF-8. Librerie esterne escluse dalla policy di stile/warning.

Il recap prova la precedente build nativa e cattura reale dell'utente. **La nuova automazione Windows non è stata eseguita qui con MSVC**. Download del tag predefinito e raccolta DLL restano da verificare nativamente; CI predisposta, non eseguita da qui.

## Stile e commenti

Funzioni globali `Ng...` e statiche con iniziali maiuscole senza underscore, secondo la convenzione Rule 41. Migrazione dei consumatori C necessaria. Prototipi, definizioni, callback e confini assembly documentati con Doxygen. Verificate 274 firme di produzione / 328 con test. [Convenzioni e limiti](CODING_STANDARD.md).

Nessuna riscrittura dell'aritmetica FFT/SIMD o dell'architettura. Simbolo assembly rinominato coerentemente in SSE2/NEON GNU/Microsoft; test FFT bit-exact superati.

## GUI e file

Configuration: unica scheda suddivisa in Input source, Receiver, Recording, Diagnostics, Display, Profile. Contenuto per argomento, indicazione before-start/runtime/display e footer comune Apply & save. Tema dark e layout mobile conservati.

**Delete** disponibile per file chiusi, con conferma del nome e cartella sul ricevitore. Annullamento senza effetti; si elimina il solo file scelto, conservando gli associati. Server richiede token e revisione corrente e rifiuta file attivi, traversal, estensioni estranee, symlink/reparse point. POSIX usa il descriptor di directory; Windows API native. Corretta anche la corsa tra annullamento/rapida riapertura e il vecchio evento close del dialogo.

## Verifiche effettive

| Prova | Esito | Limite |
|---|---|---|
| Linux GCC, SDK HackRF installato | Build, 9/9 CTest | Driver simulato, nessuna USB |
| Linux SOURCE, libhackrf locale v2024.02.1 | Build, 9/9 CTest, installazione | Tag predefinito 2026.01.3 non provato |
| Linux ASan + UBSan, senza HackRF | 8/8 CTest | LeakSanitizer disabilitato, assembly non strumentato |
| Linux senza WebGUI | Build, 7/7 CTest | Nessun HTTP |
| Chromium desktop/mobile | End-to-end superato | Sorgente sintetica a 100 ksps |
| Linux ARM64 | Eseguibile/test cross-compilati e collegati, NEON | Non eseguiti su ARM |
| macOS x86-64/ARM64 | Oggetti assembly Mach-O compilati | Nessuna applicazione completa eseguita |
| Rilevamento vcpkg/ARM64 | Script CMake con fixture superato | Nessuna installazione reale Windows |

Browser: conferma/annullamento, protezione file attivo, token/revisione/traversal/estensione/symlink, file associati conservati, download byte per byte, sezioni e mobile. Ripetuti i controlli PSD/waterfall e diagnostica precedenti. Screenshot in docs/images/.

Nessuna nuova SIMD o modifica DSP; benchmark restano quelli precedenti. Nessuna nuova misura hardware/CPU. Restano aperti Windows nativo della nuova procedura, Windows ARM64, macOS completo, percorsi Unicode Windows e audit ESA integrale.
