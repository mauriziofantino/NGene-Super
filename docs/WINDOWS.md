# Windows — build MSVC nativa e dipendenze, v0.6.1

Il percorso standard parte da un checkout pulito: Visual Studio 18/2026 con workload C/C++, Windows SDK, Git e CMake >=4.2. Nessun file di progetto deve essere modificato. Il toolset ARM64 è richiesto soltanto per un target ARM64. Il minimo CMake generale resta 3.26 per gli altri generatori/OS.

## Procedura standard

```powershell
cmake --preset windows-msvc-x64-hackrf
cmake --build --preset windows-msvc-x64-hackrf --parallel
ctest --preset windows-msvc-x64-hackrf
.\build\msvc-x64-hackrf\Release\ngene.exe --web
```

Il preset richiede MSVC e imposta HackRF, provider AUTO, bootstrap vcpkg, web e test. Il generatore Visual Studio disponibile viene scelto da CMake; nessun riferimento a v143 o VS 2022 viene imposto nel progetto. Non sono necessari MinGW, MSYS, WSL, Node, Python, un SDK HackRF preparato a mano o una Developer Prompt per queste chiamate al generatore Visual Studio.

Se non è disponibile vcpkg, NGene lo scarica nella directory di build al commit fissato. Prima di `project()` CMake seleziona il toolchain e il triplet `x64-windows` oppure `arm64-windows`. La piattaforma esplicita ha priorità sull'host; `ARM64,version=10.0` e il default Visual Studio ARM64 sono gestiti correttamente. Con Ninja si considerano compiler target, processor e Developer Prompt. Un triplet esplicito resta invariato. Un vcpkg già incluso in Visual Studio viene individuato con vswhere; `VCPKG_ROOT` e `VCPKG_INSTALLATION_ROOT` sono override facoltativi. Il tool viene bootstrappato se manca l'eseguibile.

## Riproducibilità

`vcpkg.json` versiona:

- builtin baseline `0699a19d0c6386247ce50d4dedbb8217d484d536`;
- libusb `1.0.30`;
- pthreads `3.0.0`, port revision `14`.

`cmake/DependencyLock.cmake` contiene lo stesso baseline; una discrepanza fa fallire la configurazione. **Non eseguire `x-update-baseline`: il repository è già configurato.** Le versioni dei toolset/SDK restano quelle installate sul PC; il pin delle dipendenze non equivale a un ambiente interamente ermetico.

`AUTO` usa una libreria HackRF installata se individuabile. In un ambiente pulito, scarica `v2026.01.3` dal repository upstream e verifica SHA-256 `48238f3a21189fa8cbe67838584cc045b9e5433767db8c9c30c1da02a1489f2c`. Per imporre la stessa provenienza anche su PC con SDK preesistenti, usare `NGENE_HACKRF_PROVIDER=SOURCE`. Soltanto `hackrf.c` viene compilato; niente hackrf-tools, FFTW o pkgconf. Una release personalizzata richiede un proprio SHA-256 esplicito.

Il provider mantiene librerie libusb Release/Debug distinte quando sono disponibili in vcpkg. PThreads4W viene collegato tramite il target della sua configurazione CMake. Le dipendenze vcpkg utilizzano il toolset Microsoft, non una toolchain MinGW.

## Warning e header DLL

NGene mantiene `/W4 /WX /utf-8 /external:W0`. Le soppressioni sono **PRIVATE** sui target esterni `ngene_hackrf_native` e `ngene_http`: `/w` su MSVC, `-w` sugli altri compilatori. Gli include esterni sono SYSTEM. Gli stessi flag severi non vengono propagati a libhackrf/libusb/pthreads; vcpkg costruisce gli ultimi due separatamente secondo i loro port.

La release HackRF fissata definisce già `ADD_EXPORTS` nel proprio header: NGene non lo ridefinisce nella riga di compilazione. Per i consumatori della DLL genera una copia dell'header che elimina soltanto la definizione incondizionata, lasciando ABI, calling convention, licenza e ramo dllimport. Il target DLL continua a vedere l'header upstream originale. Anche il provider SYSTEM/AUTO con SDK installato usa un header consumer privato per NGene quando necessario; il target importato dello SDK rimane invariato. Nessun sorgente scaricato o SDK locale viene modificato. Questa trasformazione è coperta da test, anche con CRLF e header già corretti.

## DLL e pacchetto

Dopo il link, CMake usa dumpbin per risolvere la chiusura delle dipendenze e raccoglie le DLL non di sistema accanto a programma e test. Un errore nel pacchetto non viene silenziato. Per il percorso standard vcpkg non servono percorsi aggiuntivi o copie manuali.

```powershell
cmake --install build/msvc-x64-hackrf --config Release --prefix package
cpack --config build/msvc-x64-hackrf/CPackConfig.cmake -C Release
.\package\bin\ngene.exe --version
```

Per SDK personalizzati è possibile indicare `HACKRF_ROOT`, `CMAKE_PREFIX_PATH` e `NGENE_RUNTIME_DIRS`; sono override per installazioni esterne, non passaggi della procedura standard. Cambiando generatore o architettura si utilizza una nuova directory di build.

## ARM64 e verifica da checkout pulito

```powershell
cmake --preset windows-msvc-arm64-hackrf
cmake --build --preset windows-msvc-arm64-hackrf --parallel
```

Un host x64 può compilare ARM64 con MSVC/ARMASM64; i relativi test vanno eseguiti su ARM64. Le CI native usano runner Visual Studio 2026 x64 e ARM64. `scripts/verify-windows.ps1` configura con i sei flag richiesti, costruisce Debug/Release, esegue i test sul target nativo e estrae il vero ZIP prodotto da CPack e lo prova in una directory nuova con PATH privo delle directory SDK. Richiede una directory di build nuova e non modifica il checkout.

## Stato della validazione

Questa iterazione ha verificato localmente download/pin e build upstream HackRF su Linux, test C, browser, Doxygen e cross-build ARM64. **Non dispone di un host Windows/MSVC e non dichiara eseguita la nuova CI.** Il precedente recap Windows dell'utente non valida automaticamente questa revisione. Il risultato definitivo zero-touch Windows richiede il passaggio della CI o dello script sul target nativo.

Driver USB, dispositivo collegato e validazione RF restano prove sul computer ricevitore: `ngene --list-devices`. Il build non installa driver o firmware. Il supporto Unicode dei percorsi Windows rimane quello precedente, basato sulle API narrow: usare percorsi ASCII in questa revisione.
