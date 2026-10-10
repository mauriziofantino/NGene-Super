# NGene 0.6.3 — profili, selezione file e metadati replay

## Correzione degli errori di configurazione

Il messaggio `Empty/long key or value at line 38` proveniva dal parser INI: il writer e la GUI emettevano `replay.path=` anche senza usare un file. Il parser ora accetta valori vuoti per i soli campi stringa opzionali (path replay, serial, directory); writer e GUI omettono il path replay vuoto. Sono leggibili anche i profili già salvati nelle revisioni precedenti. Numeri e booleani vuoti restano errori. Start di un frontend FILE senza path non cambia lo stato del controller.

## Registrazione e lettura

Ogni `.cs8` ha un sidecar `.cs8.meta.ini`, schema 2, con parametri I/Q e conteggio riferiti a quel segmento. Header/footer appartengono al writer: niente I/O aggiunto alla callback SDR. Le strutture sono copiate e possedute, senza riferimenti temporanei a nomi di conditioner. L’importazione verifica formato, conteggio, RF/digital IF e basename; un sidecar incoerente non modifica la bozza. Compatibilità con il sidecar di sessione schema 1. Fs è nominale; il tempo host non è tempo RF/GNSS.

Il file picker naviga cartelle/drive del ricevitore, filtra prima del limite di 500 e mostra i metadati prima della conferma. Il pulsante Replay nelle catture usa la sessione indipendente; il live continua. Importazione in bypass per preservare i valori salvati, range completo e registrazione automatica disabilitata. Applicazione/avvio restano espliciti. Risposte di navigazione obsolete vengono scartate. I file letti da un replay attivo sono protetti dalla cancellazione.

FFT/SIMD, ring, algoritmi di acquisizione/tracking e gestione dei campioni non sono stati riscritti. Le librerie esterne non sono modificate. Windows mantiene preset MSVC, warning come errori, baseline vcpkg e staging già esistenti. Il supporto narrow/ASCII dei percorsi Windows resta il contratto corrente.

## Prove eseguite

Linux x86-64, GCC 13, CMake 4.4.3, warning come errori:

| Configurazione | Build | CTest |
|---|---|---|
| HackRF OFF Debug | pass | 10/10 |
| HackRF OFF Release | pass | 10/10 |
| HackRF ON Debug | pass | 11/11 |
| HackRF ON Release | pass | 11/11 |

HackRF ON usa sorgenti libhackrf già disponibili localmente e la libusb di sistema. Non certifica il download/bootstrap da rete né il runtime USB Windows. PowerShell non è disponibile in questo ambiente; i due test PowerShell opzionali non sono conteggiati.

Prova Chromium reale: file picker con cartelle contenenti spazi, importazione dal sidecar appena registrato, sessioni parallele, protezione cancellazione, filtro oltre 500 file, sidecar rifiutato senza mutazione del profilo; mantenuti i controlli precedenti su PSD, histogram, ritmi GUI, diagnostica inattiva, trasferimento, mobile e spegnimento. Una richiesta GUI ritardata poteva essere seguita troppo presto dalla successiva: il pacing contava il tempo dall’invio, mentre il server conta il tempo fra richieste ricevute. La GUI ora distanzia la richiesta anche dalla risposta precedente; un test con invio ritardato di 80 ms riproduce il problema prima della correzione e passa dopo. Il cambio sessione disattiva anche i vecchi handler messaggio/errore, impedendo snapshot tardivi sul nuovo controller. Nessun algoritmo FFT o controllo di integrità è stato modificato.

CLI `--replay` con sidecar e percorso contenente spazi: 2000 coppie elaborate, digital IF corretto e bypass, exit 0. Build senza WebGUI verificata per l’API metadata usata dalla CLI. Nuovi metadata e helper filesystem compilati come oggetti ARM64 con warning come errori; anche recorder ARM64 compilato. Non è una prova runtime ARM64. Doxygen 1.18.0: documentazione completa con warning come errori, pass.

Le prove da checkout pulito e i relativi log sono in `validation/replay-063/`. Le fixture verificano anche CI8 signed e valori minimi, segmentazione, sidecar storico, count/basename errati e formato non supportato.

## Limiti da conservare

- Nessuna esecuzione nativa MSVC della 0.6.3: restano da eseguire OFF/ON × Debug/Release e staging su Windows. Il log utente della 0.6.2 copriva ON Release, non l’intera matrice.
- Nessun test USB/RF con HackRF collegata: l’enumerazione sul presente host restituisce un errore libusb, non una verifica hardware positiva.
- Il replay legge il segmento selezionato; concatenazione e ricostruzione dei gap/epoche dal CSV non sono ancora implementate.
- Raw e sidecar devono essere conservati/trasferiti insieme. Delete elimina sempre un singolo file, senza rimozione implicita degli associati.

## Aggiornamento di un clone esistente, Windows

Mettere `ngene-0.6.3.bundle` accanto alla cartella del clone; eseguire da quella cartella dopo avere chiuso NGene:

```powershell
git fetch ..\ngene-0.6.3.bundle main
git merge --ff-only FETCH_HEAD
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Action Build -Hackrf ON -Configuration Release
.\build\msvc-x64-hackrf\Release\ngene.exe --web --port 8080
```

Non occorre clonare di nuovo o eseguire il verificatore completo per una normale compilazione. Gli stessi task sono disponibili in VS Code. L’azione Build configura/compila senza eseguire i test.
