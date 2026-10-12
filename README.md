# NGene — kernel e WebGUI · v0.6.4 (multi-SDR candidate)

**NGene è la base funzionale e algoritmica di questo progetto.** L'evoluzione resta incrementale: frontend I/Q, conditioning, ring condiviso, scheduler e librerie DSP interne. La [mappa di migrazione](docs/LEGACY_MIGRATION.md) collega questo codice al ricevitore originale.

Il kernel è C11, con FFT su interi e butterfly assembly SSE2/NEON, primitive SIMD e riferimento scalare. GUI HTML/CSS/JavaScript inclusa nello stesso eseguibile. Target Windows, Linux e macOS; architetture x86-64 e ARM64. Nessuna dipendenza da FFTW, SoapySDR, Python o Node per compilare e usare il ricevitore.

Questa versione riceve e registra I/Q e produce diagnostica. Acquisizione satelliti, tracking, demodulazione e PVT verranno innestati sulle interfacce già presenti.

## Incremento multi-SDR 0.6.4

Driver nativi HackRF, RTL-SDR e bladeRF RX0; formati CI8, CU8 e SC16 Q11 mantenuti attraverso ingress, conditioning, ring e registrazione. Istogrammi e PSD usano la risoluzione ADC nativa; FFT assembly e SIMD CI8 esistenti restano invariati. Una sola sorgente attiva nella GUI, scelta in Configuration. Contatori comuni di occupazione/high-water/overflow/trasporto, con disponibilità esplicita delle informazioni del device.

[Contratti, procedura di build, limiti e verifica hardware](docs/MULTISDR_064.md) · [Prove realmente eseguite](docs/VALIDATION_064.md).

Per compilare tutti i frontend da PowerShell, senza la matrice di test:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -MultiSdr
.\build\msvc-x64-multisdr\Release\ngene.exe --web --port 8080
```

Aprire la cartella del progetto in VS Code e usare il task **NGene: Build multi-SDR (Windows MSVC)** per la stessa compilazione. Il supporto native MSVC rimane configurato; questa iterazione è stata eseguita su Linux, non su Windows o schede fisiche.

## Consolidamento 0.6

- Manifest **vcpkg con baseline e versioni fissate**, bootstrap automatico se vcpkg non è installato; HackRF `v2026.01.3` scaricata con controllo SHA-256.
- Windows **MSVC nativo**, x64 e ARM64: nessuna modifica ai file CMake, aggiornamento manuale del baseline o copia manuale delle DLL. [Procedura e validazione Windows](docs/WINDOWS.md).
- Warning severi esclusivi dei target NGene; dipendenze `SYSTEM`, `/W0` solo sui target esterni. Header HackRF di import generato senza alterare upstream o duplicare `ADD_EXPORTS`.
- Naming ESA per **funzioni, tipi, tag delle struct, membri e variabili**: `NgRingDestroy`, `TNgRing`, `TNgRecorder`, `statsMutex`, `segmentOffset`. [Convenzioni e verifiche](docs/CODING_STANDARD.md), [migrazione degli identificatori](docs/IDENTIFIER_RENAMES.csv).
- **Replay di file CI8**, con range, pacing e loop; nessun frontend sintetico. La GUI usa una sola sorgente attiva, scelta in Configuration: file oppure frontend live.
- Doxygen verificato con warning trattati come errori; documentati anche ownership, lifetime e membri delle strutture. [Report dell'incremento](docs/ITERATION_06.md).

Ulteriore consolidamento 0.6.1: selezione corretta del triplet anche con piattaforma Visual Studio composta e host ARM64, riparazione del cache manifest, header import anche per SDK installati, controllo clang-tidy su produzione/test e verifica del vero archivio CPack. [Report finale](docs/ITERATION_06_COMPLETION.md).

Consolidamento del build 0.6.2: correzione del percorso HTTP C4701 e della policy CMP0207 nello staging, runtime Debug/Release anche senza HackRF, preset Debug e verificatore da vero clone per tutte e quattro le combinazioni. **Accettazione nativa Windows ancora aperta:** [stato e prove](docs/WINDOWS_BUILD_062.md).

## Windows con HackRF

Prerequisiti di sviluppo: Visual Studio 18/2026 con workload C/C++, Windows SDK, CMake che riconosca VS 2026 (4.2 o successivo) e Git. La prima configurazione scarica le dipendenze mancanti.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Action Test
.\build\msvc-x64-hackrf\Release\ngene.exe --web
```

Lo script rileva anche CMake incluso in Visual Studio fuori dal PATH e seleziona il generatore Visual Studio installato. `-Configuration Debug`, `-Hackrf OFF` e `-Architecture ARM64` selezionano gli altri preset senza modificarli. Da **VS Code**, aprire questa cartella e usare **Ctrl+Shift+B**, oppure **Terminal → Run Task → NGene: Test (Windows MSVC)**. [Istruzioni VS Code](docs/VSCODE.md). Per un cross-build MSVC da x64 ad ARM64 usare il preset `windows-msvc-arm64-hackrf` e il relativo toolset ARM64. Il build senza hardware è `windows-msvc-x64`; i test usano file registrati e fixture, senza un generatore nel ricevitore.

Le correzioni sono verificate qui su Linux, inclusi build Debug/Release OFF/ON e regressioni GUI/staging. **La sua esecuzione nativa MSVC deve ancora essere verificata**: la CI comprende runner VS 2026 x64/ARM64 e uno script da checkout pulito, con Debug, Release e pacchetto rilocato.

## Linux e macOS

CMake >=3.26 e compilatore C11:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/ngene --web
```

Per HackRF aggiungere `-DNGENE_WITH_HACKRF=ON`. `AUTO` usa un SDK installato oppure compila libhackrf dai sorgenti fissati; per questa seconda opzione occorrono i pacchetti di sviluppo libusb. Non viene compilato hackrf-tools.

Per Linux ARM64 è disponibile `cmake/toolchains/linux-aarch64.cmake`, con compilatore cross GNU e test tramite QEMU se disponibile. Il target determina il backend DSP; non viene usata l'architettura del computer che ospita CMake. ARM/x86 a 32 bit richiedono per ora il riferimento C tramite `-DNGENE_FFT_REFERENCE=ON`.

## Console e sessioni parallele

Aprire **http://127.0.0.1:8080**. Il menu **Receiver session** seleziona la sessione primaria oppure il replay indipendente; le sessioni continuano a funzionare quando si cambia vista.

1. In **Configuration**, impostare frontend, rate, tuning, gain, conditioning e buffer. Con il build HackRF il profilo iniziale propone HackRF, senza avviare la ricezione.
2. In **Recording**, scegliere **Base filename** e **Folder on receiver computer**, poi **Apply & save**. Nome e cartella sono quelli del computer che esegue NGene.
3. **Start receiver** avvia la sessione selezionata; **Start recording / Stop recording** controllano la cattura senza riavviare il frontend. È possibile registrare pre o post conditioning.
4. **Transfer** scarica un file chiuso sul computer del browser. Trasferire anche il sidecar `nome.cs8.meta.ini` per il replay automatico. **Delete** elimina il solo file scelto, dopo conferma; i file utilizzati da un replay in corso sono protetti.
5. Per il replay, scegliere **Independent file replay → Input source → Browse I/Q files…** e navigare nelle cartelle del ricevitore. Il pulsante **Replay** di una registrazione chiusa apre la stessa selezione nella sessione indipendente. Il sidecar imposta Fs e riferimenti RF nella bozza: controllare, **Apply & save**, poi **Start receiver**. La sessione live continua.
6. **Stop receiver** agisce sulla sessione selezionata. **Exit service** oppure Ctrl+C arrestano entrambe e chiudono il programma.

Non viene inviato I/Q grezzo continuamente alla GUI. NGene calcola PSD e istogrammi opzionali; il browser disegna spettro, waterfall e statistiche. Le viste diagnostiche non osservate sospendono i relativi producer. Overlap 0–99%, FFT a dimensioni discrete, Welch, window, EMA, rate, sorgente pre/post e scala dei grafici sono configurabili. Il waterfall mantiene una scala globale.

Il profilo primario è `captures/receiver.ini`, quello replay è `captures/replay/receiver.ini`. `--output-dir` cambia la radice dei profili; `recording.directory` cambia la destinazione delle catture. I run non vengono riavviati automaticamente al caricamento dei profili.

Per una cattura da terminale:

```sh
./build/ngene --config config/hackrf.ini
./build/ngene --replay "captures/prova_000000.cs8"
```

`--replay FILE` importa il sidecar adiacente. Se assente, occorre anche `--config FILE` con i parametri del flusso. Il raw CI8 non contiene autonomamente rate o tuning. [Contratto del replay](docs/REPLAY.md).

Il servizio si lega a loopback. Per un browser su un altro computer usare un tunnel, ad esempio `ssh -N -L 8080:127.0.0.1:8080 utente@ricevitore`, e aprire la stessa URL locale. La destinazione dei download viene scelta dal browser.

## Documentazione

- [Architettura e ownership](docs/ARCHITECTURE.md)
- [Configurazione](docs/CONFIGURATION.md), [GUI e API](docs/WEBGUI.md)
- [Registrazione e formati](docs/RECORDING.md), [replay](docs/REPLAY.md)
- [Diagnostica](docs/ANALYSIS.md), [FFT e provenienza](docs/FFT_REVIEW.md)
- [Standard di codice](docs/CODING_STANDARD.md), [validazione](docs/ITERATION_06.md)
- [Correzioni e prove 0.6.3](docs/ITERATION_063.md)
- [Manuale di lavoro](docs/WORKING_MANUAL.md), [dipendenze e licenze](docs/PROVENANCE.md)

`NGENE_WITH_DOCUMENTATION=ON` abilita `ngene_docs` quando Doxygen è installato. `tests/web_ui.cjs` è una verifica opzionale browser: soltanto l'host di test richiede Node/Playwright. Gli screenshot inclusi provengono da una sessione reale del programma con un file di test, non da un mockup.
