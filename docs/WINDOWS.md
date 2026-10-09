# Windows — build MSVC nativa e dipendenze, v0.6.2 (candidate)

Il percorso standard parte da un checkout pulito: Visual Studio 18/2026 con workload C/C++, Windows SDK, Git e CMake >=4.2. Nessun file di progetto deve essere modificato. Il toolset ARM64 è richiesto soltanto per un target ARM64. Il minimo CMake generale resta 3.26 per gli altri generatori/OS.

**Accettazione nativa ancora aperta.** Il log Windows della 0.6.1 ha evidenziato C4701 e CMP0207. Le correzioni sono nel sorgente della 0.6.2; questa revisione non è stata ancora eseguita su Windows. I test Linux e le fixture non certificano una build MSVC.

## Procedura standard

Per la build quotidiana da PowerShell o VS Code usare il punto di ingresso
condiviso, che rileva gli strumenti anche fuori dal PATH:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Action Test
```

Da VS Code aprire il repository e usare **Ctrl+Shift+B**; per i test scegliere
**Terminal → Run Task → NGene: Test (Windows MSVC)**. [Dettagli](VSCODE.md).
`-Configuration Debug`, `-Hackrf OFF`, `-Architecture ARM64` selezionano i preset.
Questa modalità accetta la working tree in sviluppo e non richiede un nuovo clone.

Da un normale PowerShell, il verificatore scopre automaticamente anche CMake
incluso in Visual Studio, senza modificare il PATH o aprire una Developer Prompt:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-windows.ps1
```

Cerca nel PATH, nelle installazioni standalone standard e nelle installazioni
Visual Studio individuate da vswhere (incluse Build Tools/preview). Richiede
una suite completa >=4.2 e usa i percorsi assoluti di cmake/ctest/cpack della
stessa versione, preferendo la più recente compatibile. Riporta i percorsi
in console e in acceptance.json. Se CMake non è installato, segnala precisamente
il componente «C++ CMake tools for Windows» da installare/aggiornare in Visual
Studio, oppure il prerequisito CMake standalone >=4.2. Non installa software
sistema né modifica il PATH persistente.

I comandi diretti seguenti sono disponibili quando CMake è già nel PATH:

```powershell
cmake --preset windows-msvc-x64-hackrf
cmake --build --preset windows-msvc-x64-hackrf --parallel
ctest --preset windows-msvc-x64-hackrf
.\build\msvc-x64-hackrf\Release\ngene.exe --web
```

Il preset senza suffisso seleziona Release. Debug usa gli stessi sorgenti e la stessa configurazione:

```powershell
cmake --build --preset windows-msvc-x64-hackrf-debug --parallel
ctest --preset windows-msvc-x64-hackrf-debug
```

Per HackRF OFF utilizzare `windows-msvc-x64`, anche per configure; i relativi build/test Debug sono `windows-msvc-x64-debug`. È impostato esplicitamente `NGENE_WITH_HACKRF=OFF`, non soltanto affidato al default del cache. Configure non impone né generatore né toolset di una specifica versione Visual Studio. Gli errori di policy CMake durante configure non vengono silenziati.

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

NGene mantiene `/W4 /WX /utf-8 /external:W0`. Le soppressioni sono **PRIVATE** sui target esterni `ngene_hackrf_native` e `ngene_http`: `/W0` su MSVC, `-w` sugli altri compilatori. Gli include esterni sono SYSTEM. Gli stessi flag severi non vengono propagati a libhackrf/libusb/pthreads; vcpkg costruisce gli ultimi due separatamente secondo i loro port.

`_CRT_SECURE_NO_WARNINGS` rimane limitato ai target C interessati: permette le API ISO C portabili (`fopen`, `snprintf`, `tmpfile`), invece di imporre sostituzioni `_s` specifiche di Microsoft. Non disabilita i controlli `/W4 /WX`, le conversioni, il flusso non inizializzato o i warning generali. Nessuna soppressione di C4701/C4703, C4244 o C4267 è stata aggiunta.

Il ramo HTTP della configurazione inizializza il draft e mantiene `NG_ERROR` esplicito dopo un errore di scrittura/flush/seek del file temporaneo. Il risultato di `NgFail` non viene più usato per decidere se esiste una configurazione letta. Questo evita sia il percorso ambiguo per MSVC sia l'applicazione di un draft dopo un errore I/O. I test verificano che configurazioni malformate non cambino profilo o revisione.

La release HackRF fissata definisce già `ADD_EXPORTS` nel proprio header: NGene non lo ridefinisce nella riga di compilazione. Per i consumatori della DLL genera una copia dell'header che elimina soltanto la definizione incondizionata, lasciando ABI, calling convention, licenza e ramo dllimport. Il target DLL continua a vedere l'header upstream originale. Anche il provider SYSTEM/AUTO con SDK installato usa un header consumer privato per NGene quando necessario; il target importato dello SDK rimane invariato. Nessun sorgente scaricato o SDK locale viene modificato. Questa trasformazione è coperta da test, anche con CRLF e header già corretti.

## DLL e pacchetto

Dopo il link, CMake usa `dumpbin` del compilatore selezionato per risolvere la chiusura delle dipendenze e raccoglie le DLL non di sistema accanto a programma e test, **anche con HackRF OFF**. Il runtime MSVC della configurazione selezionata viene individuato tramite `InstallRequiredSystemLibraries` e collocato automaticamente accanto agli eseguibili prima della risoluzione; Debug comprende l'UCRT di debug del Windows SDK. Il package Release non include il CRT di debug. Gli archivi Debug servono per verifica/development locale, non per distribuzione del runtime di debug Microsoft.

`StageRuntime.cmake` dichiara il minimo CMake e imposta `CMP0207=NEW` quando disponibile. Il processo `cmake -P` ha un proprio scope di policy: la policy nel solo progetto principale non sarebbe sufficiente. I filtri rimangono compatibili anche con CMake precedenti a 4.3. Un lock per directory serializza le copie degli stessi runtime durante i link paralleli di programma e test. Un errore nel pacchetto non viene silenziato. Per il percorso standard vcpkg non servono percorsi aggiuntivi o copie manuali.

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

Un host x64 può compilare ARM64 con MSVC/ARMASM64; i relativi test vanno eseguiti su ARM64. Le CI definite usano runner Visual Studio 2026 x64 e ARM64; la nuova esecuzione non è stata avviata in questo ambiente.

Per la verifica completa su Windows nativo, dal checkout Git **con le correzioni già committate**:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-windows.ps1
```

Lo script rifiuta una working tree sporca per non certificare per errore una revisione precedente. Chi usa l'archivio consegnato può ottenere il checkout con `git clone ngene-0.6.2.bundle NGene-git`, poi eseguire lo script dentro `NGene-git`. Non occorre inizializzare o aggiornare il baseline.

Lo script:

1. Crea un nuovo clone e nuove build; conserva la revisione Git esatta nei risultati.
2. Rimuove temporaneamente override di SDK, toolchain, generatore e flag del compilatore, ripristinandoli alla fine.
3. Configura entrambi i preset OFF/ON e verifica generatore VS 2026+, compilatore MSVC e `ml64` per x64.
4. Compila **OFF/ON × Debug/Release**, esegue CTest e intercetta anche warning di compiler/linker/CMake nei log completi.
5. Prova `--help`, `--version` e, con HackRF ON, `--list-devices` da build, install e ZIP realmente estratto, con PATH limitato alle directory Windows di sistema.
6. Conserva log, build, pacchetti ed `acceptance.json` in una nuova directory temporanea indicata in console. `-WorkRoot` permette di sceglierne una nuova senza modificare file del progetto.
7. Continua sulle altre combinazioni in caso di fallimento e non restituisce accettazione positiva se una combinazione fallisce oppure se il vcpkg integrato di Visual Studio non è stato effettivamente usato.

Un Visual Studio senza vcpkg integrato utilizza comunque il fallback automatico previsto dal build. Lo script di accettazione segnala in quel caso la specifica prova "vcpkg integrato" ancora non effettuata, senza trasformare il fallback in una certificazione di quel requisito.

Su ARM64 nativo usare `-Architecture ARM64`. Il verificatore rifiuta l'esecuzione su un host di architettura diversa; la compilazione cross resta disponibile tramite i preset ma non sostituisce i test runtime nativi.

## Stato della validazione

Sono disponibili build/test reali Linux della 0.6.2 nelle quattro combinazioni e la regressione browser della configurazione. Il contratto dello staging usa output `dumpbin` simulato per testare policy, dipendenze transitive, spazi nei path, CRT, esclusione dei file OS e fallimento per DLL mancanti. Il test PowerShell verifica parsing e cattura di exit/warning, anche localizzati. **Nessuna di queste prove è un build nativo Windows.**

Mancano un host/runner Windows accessibile e i quattro risultati MSVC con smoke test e clone pulito. L'acceptance resta aperta; il report [WINDOWS_BUILD_062.md](WINDOWS_BUILD_062.md) distingue implementazione, prove eseguite e prove mancanti. La nuova CI è configurata, non dichiarata eseguita.

Driver USB, dispositivo collegato e validazione RF restano prove sul computer ricevitore: `ngene --list-devices`. Il build non installa driver o firmware. Il supporto Unicode dei percorsi Windows rimane quello precedente, basato sulle API narrow: usare percorsi ASCII in questa revisione.

### Verificatore: Windows PowerShell 5.1

Il percorso predefinito del checkout viene risolto dopo il binding dei parametri,
per evitare il valore vuoto di `$PSScriptRoot` osservato nel parametro della prima
consegna 0.6.2. Per continuare da quel bundle senza modificare file:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-windows.ps1 -SourceRepository "$PWD"
```

Il pacchetto aggiornato include la correzione e non richiede questo argomento.
Il test del punto di ingresso copre cartelle con spazi, directory corrente diversa
e repository espliciti; la sua esecuzione su PowerShell 7/Linux non certifica
Windows PowerShell 5.1 o una build MSVC.

### Continuare dal clone precedente con il verificatore aggiornato

Per evitare modifiche nella working tree, salvare la copia autonoma consegnata
`verify-windows.ps1` accanto alla cartella `NGene-git`, non dentro il repository.
Dal terminale già aperto in `NGene-git`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\verify-windows.ps1 -SourceRepository "$PWD"
```

Il verificatore esterno compila il commit del clone indicato e ne registra il
hash; non applica automaticamente modifiche al progetto. Il bundle aggiornato
contiene la stessa correzione per i checkout futuri.

### Warning D9025 delle dipendenze esterne

Il log nativo fornito durante questa iterazione mostra HackRF ON / Release con
zero errori e due warning D9025. La verifica resta fallita: il frammento non
certifica né CTest né le altre configurazioni. La precedente opzione `/w`
finiva in AdditionalOptions accanto al WarningLevel predefinito `/W1` di MSBuild.
Ora solo i target esterni ricevono `/W0`, che CMake mappa a
`WarningLevel=TurnOffAllWarnings`. NGene conserva `/W4 /WX`; il verificatore
continua a rifiutare D9025 e controlla i progetti .vcxproj dopo configure.

Per aggiornare un checkout del bundle precedente, salvare il nuovo bundle
nella directory che contiene `NGene-git` (sostituendo il vecchio bundle), quindi
eseguire dal checkout:

```powershell
git fetch ..\ngene-0.6.2.bundle main
git merge --ff-only FETCH_HEAD
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-windows.ps1
```

La nuova verifica esegue un clone pulito del commit aggiornato; non riutilizza
la build precedente. Non occorre modificare file CMake, warning level o DLL a mano.

### FTK1011 nei test CMake

Il nuovo log Windows mostra tre build visibili con 0 warning/0 errori, i test
funzionali (inclusa FFT e --list-devices) superati e il solo
build_dependency_contract fallito. I percorsi .tlog del probe del compilatore
nella fixture SDK erano lunghi 261–269 caratteri. Ora i progetti di prova SDK e
warning vengono creati in una radice temporanea breve e unica (`ngt-*`), fuori
dalla build del ricevitore. I test di compilazione/link di CMake restano attivi;
non si forza CMAKE_C_COMPILER_WORKS e non si disabilita FileTracker. In caso di
fallimento la directory resta disponibile; dopo un successo viene rimossa.
Il verificatore usa anche un nome temporaneo più breve (`ngm-*`). Nessuna
modifica a registro, LongPathsEnabled o file CMake del PC è richiesta.
La correzione mantiene aperta l'accettazione fino al nuovo test nativo completo.
