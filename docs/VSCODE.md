# NGene da VS Code su Windows

Aprire la cartella del repository (`NGene-git`, con CMakeLists.txt), non la
cartella che contiene il bundle. La build usa MSVC nativo e i preset condivisi
con PowerShell. Non servono MinGW, MSYS2, WSL o percorsi SDK personali.

Prerequisiti: Visual Studio 2026 con C/C++, Windows SDK e CMake >=4.2 installato
(anche come componente Visual Studio). Per ARM64 occorre il toolset ARM64.

## Compilare e provare

**Ctrl+Shift+B** avvia `NGene: Build (Windows MSVC)`. I menu selezionano
Release/Debug, HackRF ON/OFF e x64/ARM64. Il task configura prima di compilare;
CMake, CTest e CPack vengono individuati automaticamente dalla stessa suite.

Da **Terminal → Run Task** sono disponibili:

| Attività | Operazioni |
| --- | --- |
| NGene: Configure (Windows MSVC) | Configura il preset selezionato |
| NGene: Build (Windows MSVC) | Configura e compila |
| NGene: Test (Windows MSVC) | Configura, compila ed esegue CTest |
| NGene: Package (Windows MSVC) | Configura, compila e genera il ZIP con CPack |
| NGene: Verify clean Windows checkout | Verifica completa OFF/ON × Debug/Release da un nuovo clone |

Gli errori del compilatore sono associati alla vista **Problems** mediante il
matcher MSVC; il terminale mostra l'output e l'esito completo. I log del flusso
quotidiano sono in `build/msvc-<arch>[-hackrf]/logs/`. CTest salva anche il report
JUnit. Il pacchetto è in `out/<build-preset>/`.

I task di sviluppo compilano il codice della working tree, comprese le modifiche
non committate. La verifica completa richiede invece un checkout Git pulito e
committato, per registrare il commit realmente verificato. I test runtime di una
cross-build ARM64 vanno eseguiti su un host ARM64; da x64 si può compilare e creare
il pacchetto ARM64.

## Gli stessi comandi da PowerShell

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Action Test -Configuration Debug -Hackrf OFF
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Architecture ARM64 -Action Package
```

Il punto di ingresso non richiede CMake nel PATH e non modifica il PATH
persistente. Non occorre avviare VS Code da una Developer Prompt.

## Estensioni e verifica

Le raccomandazioni `.vscode/extensions.json` includono Microsoft C/C++ e CMake
Tools per editing e navigazione. I task NGene funzionano anche senza CMake Tools:
usano `scripts/build-windows.ps1` direttamente. L'estensione può usare gli stessi
CMakePresets.json; la configurazione automatica all'apertura è disattivata per
evitare una seconda build involontaria o la selezione di un generatore diverso.
Nessun percorso `cmake.cmakePath` specifico del PC è versionato.

Questa integrazione è verificata tramite parsing dei file, piani degli otto
preset e strumenti di prova per il flusso configure/build/test e la propagazione
di un errore CTest. I test su PowerShell/Linux non certificano una sessione
interattiva VS Code o un'esecuzione MSVC su Windows.
