# Incremento 0.6 — Windows/HackRF, naming e replay

9 ottobre 2026. Revisione del progetto corrente, senza cambiare il percorso conditioning → ring → canali né l'aritmetica FFT. NGene originale rimane la base dei moduli GNSS da migrare.

## Build Windows da checkout pulito

- Manifest versionato: `builtin-baseline=0699a19d0c6386247ce50d4dedbb8217d484d536`; libusb 1.0.30 e pthreads 3.0.0, port-version 14. Nessun `x-update-baseline` richiesto.
- Bootstrap prima di `project()`: scoperta di vcpkg esistente oppure checkout fissato nel build tree; triplet x64/ARM64 automatico dal target del generatore.
- HackRF AUTO usa un SDK presente oppure scarica e costruisce la sola libreria C v2026.01.3, SHA256 `48238f3a21189fa8cbe67838584cc045b9e5433767db8c9c30c1da02a1489f2c`. Niente hackrf-tools/FFTW.
- Rimossa la definizione aggiuntiva di `ADD_EXPORTS`. DLL con header originale; copia consumer Windows con `dllimport`, senza modificare sorgenti upstream o ABI.
- Warning disabilitati soltanto sui target esterni, include SYSTEM. NGene mantiene `/W4 /WX` e warning severi GCC/Clang. Nessuna soppressione globale.
- Import library libusb distinta Debug/Release; pthreads tramite target CMake. Raccolta automatica delle DLL dopo link e nell'installazione.
- Preset senza toolset VS hardcoded; Microsoft C richiesto con `NGENE_REQUIRE_MSVC=ON`. Script e CI Visual Studio 18/2026: Debug/Release, test, installazione e package ricollocato con PATH ripulito. Nessun percorso MinGW.

Da PowerShell, con Visual Studio 2026 C++ e CMake ≥4.2 disponibili:

```powershell
./scripts/verify-windows.ps1
```

Lo script esercita HackRF ON/AUTO, bootstrap vcpkg ON, Web ON, test ON e REQUIRE_MSVC ON. Richiede una nuova directory di build per evitare che un cache precedente mascheri problemi. Prerequisiti in [WINDOWS.md](WINDOWS.md).

## Codice e GUI

Naming uniforme dei tipi/tag `TNg...`, funzioni capitalizzate, variabili/parametri/membri lowerCamel e macro maiuscole. `TNgRecorder` usa `statsMutex`, `writerError`, `segmentOffset`, ecc. Mapping versionati per i consumatori. Doxygen, documentazione dei membri, parametri identici fra dichiarazioni/definizioni, header senza implementazioni e formattazione uniforme. Le 21 copie vendorizzate sono identiche alla versione precedente.

Frontend prodotto `synthetic` e profili rimossi. Frontend FILE CI8 interleaved signed con seek a 64 bit, intervallo di coppie, pacing opzionale e loop con discontinuità. Il generatore deterministico è soltanto una fixture dei test.

Controller primario e replay indipendente: configurazioni, clock, ring, conditioning, diagnostica e recorder distinti per stream. I canali di ciascuno stream condividono una sola finestra. Nessuna miscelazione live/replay. GUI dark con selettore sessione e controlli replay; comandi/grafici sulla sessione selezionata. REST `/api/v1/replay/...`, WebSocket `replay:MASK`. [REPLAY.md](REPLAY.md).

## Verifiche eseguite

| Configurazione | Risultato | Limiti |
|---|---|---|
| Linux x86-64 Release, HackRF SOURCE fissato | Build senza warning; 10/10 CTest | SDK reale compilato; ingresso USB simulato |
| Linux x86-64 ASan + UBSan | 9/9 CTest | LeakSanitizer disabilitato per limite del container; assembly non strumentato |
| Linux x86-64 headless | 8/8 CTest; ripetuti sul sorgente estratto dall'archivio finale in un build tree nuovo | Senza Web/HackRF |
| Linux ARM64, cross-build Zig/Clang, QEMU | Build completa, 9/9 CTest eseguiti | Emulazione, non hardware ARM né benchmark |
| Adattatore HackRF ARM64 | Oggetto compilato con warning severi e header upstream | Nessun collegamento SDK ARM/USB reale |
| Chromium desktop/mobile | Test end-to-end superato | Live reale non disponibile |
| Doxygen 1.18.0 | HTML generato, zero warning | Non una certificazione semantica ESA |
| Controllo AST naming | Zero violazioni nel codice C proprio analizzato | CI clang-tidy predisposta, non eseguita sul repository remoto |
| Windows MSVC x64/ARM64, macOS nativi | Procedure e CI predisposte | Non eseguiti localmente; zero-touch Windows da confermare sul job nativo |

Test: baseline/hash, header import CRLF/idempotente con upstream immutato, lifetime lease, I/Q signed, EOF parziale, intervalli/loop, indipendenza delle sorgenti, replay durante lease HackRF simulata, due controller Web contemporanei, recording/download/delete, integrità PSD e GUI lenta. Screenshot aggiornati in `docs/images/`.

## Questioni aperte

Il build Windows è stato corretto e reso verificabile automaticamente; non viene dichiarato già validato senza eseguirlo. Servono prove native MSVC e HackRF reale. Restano percorsi Unicode Windows, algoritmi GNSS, ulteriori frontend e import automatico metadati/segmenti. Replay di file chiusi immutabili: non un file mentre viene scritto; Fs e metadati configurati esplicitamente.

Nessuna nuova SIMD o modifica FFT numerica in questa revisione; benchmark precedenti conservati, senza risultati nuovi. La conformità ESA completa oltre al naming richiede una matrice delle regole, incluse deviazioni OS/SIMD.
