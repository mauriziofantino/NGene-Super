# NGene 0.6.1 — report finale del consolidamento

Ripresa della revisione 0.6 consegnata, 9 ottobre 2026. Restano invariati motore, conditioning/ring, aritmetica FFT e separazione degli stream live/replay.

## Correzioni ulteriori

- Il triplet Windows viene scelto dalla piattaforma target, separando eventuali opzioni SDK (`ARM64,version=10.0`). Gestiti anche default Visual Studio ARM64, compiler target e Developer Prompt con Ninja. Non si forza più x64 su un host ARM64. I triplet espliciti restano invariati.
- Il bootstrap imposta il manifest NGene anche se il cache contiene mode/directory obsoleti, aggiunge la feature HackRF senza eliminare quelle preesistenti e rimuove i duplicati. Baseline e port rimangono fissati.
- L'header consumer Windows viene adattato anche quando AUTO/SYSTEM trova uno SDK installato, conservando immutati file e target importati esterni. SOURCE mantiene il sorgente/header originale per la DLL e l'header import per i consumer. Warning severi NGene e soppressioni PRIVATE third-party invariati.
- Il contratto CMake verifica ora selezione x64/ARM64, default ARM64, piattaforme composte, cross-target, scoperta di vcpkg in un percorso con spazi, manifest cache e rispetto degli override. Una fixture SDK verifica il ramo SYSTEM Windows senza compilare una DLL Windows.
- Il controllo clang-tidy è stato realmente eseguito su tutto il C proprio, inclusi test e HackRF. Corretta la CI naming: configura anche HackRF per avere compile command e header necessari. Esclusi dal rename soltanto main e nomi obbligatori dell'ABI esterna nei mock.
- Script PowerShell con architetture ammesse esplicite. La prova finale estrae il vero archivio ZIP di CPack e cerca il suo eseguibile, invece di copiare soltanto l'install tree. Rimangono Debug/Release, CTest e PATH ripulito.

## Verifiche di questa ripresa

| Verifica | Esito | Cosa dimostra |
|---|---|---|
| Linux Release, Web e HackRF SOURCE | 10/10 CTest, build senza warning | Sorgente HackRF v2026.01.3 scaricato/verificato, SDK collegato; driver USB simulato |
| Linux Release, HackRF AUTO con SDK disponibile | 10/10 CTest, build senza warning | Selezione del provider installato |
| Linux Release senza HackRF, Web attivo | 9/9 CTest | Percorso senza dipendenze USB |
| Contratto bootstrap/provider | Superato | Test di configurazione per i casi nuovi, compreso ramo Windows simulato |
| clang-tidy 22.1.8 | Uscita 0, zero violazioni naming | Sorgenti propri in src/apps/tests e relativi header |
| clang-format 21.1.8 | Dry-run senza errori | Formattazione di C/header propri |
| Doxygen 1.18.0 | Generazione riuscita, zero warning | Documentazione con warning trattati come errori |
| PowerShell 7.5.3 su Linux | Parsing senza errori | Sintassi dello script; non esecuzione MSVC |
| Chromium desktop/mobile | Test end-to-end superato | Sessioni parallele, registrazione/download/delete, diagnostica e layout |
| Clone Git pulito dal bundle consegnabile, Linux headless | 8/8 CTest | Configurazione/build da un vero clone con working tree pulito |

L'archivio finale comprende i sorgenti e `ngene-0.6.1.bundle`, clonabile con `git clone ngene-0.6.1.bundle NGene`. Il bundle contiene un unico repository con receiver e GUI. Nessun commit/staging della directory di lavoro originale è stato alterato. Le attribuzioni esterne restano nei loro file e le regole Git preservano i byte di `third_party/`, comprese le terminazioni di riga.

Le verifiche ARM64/QEMU e ASan/UBSan della revisione 0.6 sono conservate in `docs/validation/`; il codice C/assembly non cambia in questa ripresa. Non sono nuovi benchmark né prove di carico USB reale. Il generatore deterministico rimane esclusivamente una fixture dei test; nessun frontend synthetic nel prodotto.

## Stato del requisito zero-touch Windows

Implementazione e procedura automatica completate nel codice. **Esecuzione nativa Visual Studio 18/2026 ancora non effettuata**: questo ambiente è Linux, non offre MSVC/Windows SDK né accesso a un host Windows; il checkout non ha un remote Git sul quale eseguire la CI. Non si può certificare il requisito zero-touch Windows sulla base dei test Linux o delle fixture.

Lo script `scripts/verify-windows.ps1` e i job nativi x64/ARM64 sono pronti per questa prova. Nessun baseline update, modifica CMake o copia DLL manuale è prevista. Rimangono anche la prova HackRF fisica e una matrice formale ESA oltre alle convenzioni di naming applicate. Il replay attuale usa file CI8 chiusi e metadati configurati, non importa/concatena automaticamente registrazioni segmentate.
