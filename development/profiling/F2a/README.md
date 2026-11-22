# F2a · Validazione della griglia di acquisizione

22 novembre 2026. EVO `970c0dc2e8d22efb1ecff0db9ac416b7aa6a062f` con la patch
F1 locale come baseline. La verifica finale usa lo stesso HEAD con F1 e F2a
nel working tree. Build Release Windows x64 multisdr, MSVC.

## Correzione

`Prepare` calcola il confine della cella `k` come
`floor(k × Fs × periodo_ms / (1000 × N))`. Il primo denominatore è zero
quando `floor(Fs × periodo_ms / 1000) < N`. Se il passo è almeno uno, i
confini di tutte le celle avanzano di almeno un campione. Il controllo usa
la parte intera del periodo nativo e condivide la regola di ammissibilità
delle dimensioni con il costruttore FFT. La capacità del buffer rimane
arrotondata per eccesso per ospitare anche i periodi frazionari.

Classic `CompleteAcqConfiguration` dimezza la FFT finché rientra nelle
coppie per periodo. EVO mantiene la propria scelta automatica e rifiuta
una richiesta manuale incompatibile. Entrambi i workspace nativi eseguono
il controllo prima della loro prima allocazione. La frequenza usata è
quella di `TNgStreamInfo` consegnata dal frontend aperto. La vista
Acquisition spiega il limite e mostra la diagnostica del ricevitore
all'avvio. La validazione sintattica dell'INI e il salvataggio del profilo
precedono l'apertura del frontend.

| Sistema | Evidenza F2a |
|---|---|
| GPS L1 C/A, 1 ms | Provati i confini di 1024, 2048 e 4096. Le ricerche operative usano l'auto già esistente, 2048 sotto 4,096 Msps e 4096 dalla soglia in poi. La selezione manuale di 1024 rimane in F7. |
| Galileo E1-B, 4 ms | Provati 4096, 8192 e 16384. N=16384 a 2,048 Msps o 4.095.999 coppie/s è rifiutato. A 4.096.000 e 4.096.001 è ammesso. Le ricerche manuali sono state esercitate con CI8/CI16 e IF nulla/non nulla. |
| BeiDou B1C, 10 ms | La verifica geometrica ammette 16384 con 20480 e 81838 coppie native per periodo. Le dimensioni native 20480/81838 e il candidato 32768 superano il limite del backend corrente. La prova B2 deve confrontare la correlazione circolare e l'aliasing con un oracolo a 10 ms, per la componente scelta. |

## Prima e dopo

- Il test del costruttore, compilato prima della correzione, fallisce
  sull'atteso `NG_ERROR` a 2,048 Msps con `N=16384`. Dopo la correzione,
  `acquisition_grid_contract` verifica geometria, avvio e ricerche valide.
  Galileo PRN 7 con CI16, IF −38500 Hz e N=8192 e con CI8, IF zero e N=16384
  ottiene in entrambi i casi Doppler 1237 Hz, fase 123,3794 chip e 3/4.
  Il caso manuale N=4096 con solo rumore produce una mappa finita e 0/4.
- Replay CLI su 458752 coppie CI8 nulle con INI a 4,096 Msps, sidecar
  a 2,048 Msps, IF zero e un PRN per sistema. Prima, `fft_size=16384`
  termina con codice 0 dopo aver elaborato il file. Dopo, termina
  all'avvio con codice 1 e indica 4096000 coppie/s minime e 2048000
  effettive. Con `fft_size=0`, il replay termina con codice 0, 458752
  coppie elaborate e zero gap. SHA-256 della fixture CI8
  `b527dab94f6bf43c3ccbea0f804d18604204064047f4efa0f7b6d859c2d706eb`.
  `file_replay_contract` ripete il controllo via API con un sidecar
  separato, IF +38500 Hz, spettro invertito e stream comune CI16.
- Verifica Playwright con Edge headless sul servizio locale avviato via
  `--web --config … --replay …`. La vista Acquisition mostra il limite
  della FFT e Start mostra nella notifica di errore la frequenza effettiva
  del sidecar.
- L'output dei casi GPS/Galileo preesistenti e l'output F1
  `--verify-pcps` coincidono byte per byte fra le due build. SHA-256
  rispettivamente
  `e0dd8c74e99b0c28a581a9a5c2dc1ed53e95a92084f47c7a5fa0b62c816b4fe5`
  e `9d8b75d81d3688891d2605220842d422a099cf822b81c49418b5fc930efcfcf3`
  con redirezione Windows PowerShell. Il confronto F1 riguarda il test
  delle mappe a N=8192. Gli scarti wrong-Doppler delle modalità intere
  rispetto all'oracolo double restano il punto di partenza per le fasi successive.

## Riproduzione e limiti

Con una build normale `windows-msvc-x64-multisdr` già configurata, il
contratto e il confronto F1 sono rieseguibili dalla radice EVO.

```powershell
cmake --build build/msvc-x64-multisdr --config Release
cmake -E env PSExecutionPolicyPreference=RemoteSigned ctest --test-dir build/msvc-x64-multisdr -C Release --output-on-failure
build/msvc-x64-multisdr/Release/ngene_acquisition_tests.exe --verify-grid
build/msvc-x64-multisdr/Release/ngene_acquisition_tests.exe --verify-pcps
```

La suite Release passa 30/30 test. I due contratti PowerShell usano
`RemoteSigned` per il solo processo CTest e i relativi figli.
[Log CTest](ctest-release.txt).

| Binario Release | SHA-256 |
|---|---|
| `ngene.exe` | `2c2333866c100f5ce3df40193961a00ba584be1695fa2f009bc74e9ee57303ae` |
| `ngene_acquisition_tests.exe` | `9da871fc32b5e0889f3af46d10787fb2e4d844b0b4b9cd01b66e2fa4c18ab02b` |

L'eseguibile operativo riporta 0.6.4
Windows/x86_64, FFT SSE2-assembly e i quattro frontend nativi.
`NGENE_TRACKING_BENCHMARK`, `NGENE_FFT_REFERENCE`,
`NGENE_SAMPLE_REFERENCE` e `NGENE_SANITIZERS` sono OFF. Le prove sono
sintetiche e su replay Windows. La misura prestazionale di Prepare,
la validazione live/Linux e la strategia B1C restano aperte.

## Conseguenze per F2b

Misurare direttamente `Prepare` in GPS e Galileo. In entrambi, con IF
non nulla, la compressione dopo il mixer sovrascrive il primo passaggio.
Separare questa rimozione dalla quantizzazione intera sempre eseguita,
compresa la modalità float, e dall'allocazione per modalità. Misurare
creazione, RAM allocata, memoria residente e latenza di annullamento
per worker. Confrontare mappe, rumore wrong-Doppler e decisioni prima
e dopo entro ogni modalità. La stima cold/warm di F1 include il costo
eventuale della replica intera e gli effetti della cache.
