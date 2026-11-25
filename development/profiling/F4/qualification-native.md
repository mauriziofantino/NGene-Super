# F4 ? Verifiche native rinviate

La chiusura Windows e la decisione sul profilo operativo sono nel
[rapporto F4](README.md). Tutte le righe sotto sono **da eseguire
su macchine native**, con gli output conservati per host. Per ogni
sessione registrare commit, architettura, CPU, sistema, compilatore,
flag, hash dell'archivio e del binario. Identificare le catture con gli
SHA-256 del [manifest RF](ci8-pcps-rf-inputs.json) e del replay RTL
indicato nel rapporto.

| Prova | Linux nativo | macOS nativo | Verifica |
|---|---|---|---|
| Build operativa Release | x86-64 e AArch64 per le architetture distribuite | Apple Silicon e Intel se distribuito | Build e CTest con traccia F6 e backend sperimentali disattivati |
| Pacco kernel F4 | GCC o Clang su ciascuna ISA provata | Apple Clang su ciascuna ISA provata | Compilare e installare archivio e header. Collegare ed eseguire il banco usando i kernel dell'archivio |
| FFT CI8 scalare | Tutti gli host | Tutti gli host | Q7/Q15, 8?16384, 4 vettori storici e 2 direzioni. Confrontare componenti, esponente, saturazioni ed errori con il riferimento prodotto sullo stesso host |
| FFT CI8 AVX2 | x86-64 con AVX2 e stato vettoriale abilitato | Intel con le stesse capacit? | Parit? per wide Q15, adattivo, scratch e prodotti a coppie. Ripetere le misure accoppiate degli stadi sullo stesso binario |
| FFT Q14 | Backend operativo per ISA e AVX2 opzionale su x86-64 | Backend operativo per ISA e AVX2 opzionale su Intel | Eseguire fft_integer_contract con configurazione ordinaria e, dove disponibile, confronto Q14 AVX2 contro riferimento C e SSE2 |
| INT32 NEON | AArch64 nativo | Apple Silicon | Confrontare il candidato Q30 con il riferimento C nelle due direzioni. Misurare la FFT completa e il PCPS a parit? di input e opzioni |
| PCPS numerico | GPS e Galileo, entrambe le architetture qualificate | GPS e Galileo | Ripetere matrice sintetica, casi prossimi alla soglia, solo rumore e finestre RF. Confrontare mappe, soglie, picchi, esiti e stime finali rispetto al riferimento locale |
| Batch e scheduler sperimentale | Host nativo con cattura RTL | Host nativo con cattura RTL | Stessi budget e fixture Windows. Verificare equivalenza dei campi pubblicati a chiave comune, stop con job attivi, rilascio dei prestiti e assenza di risultati dopo la cancellazione |
| Prestazioni del ricevitore | Tre coppie alternate per ciascuna campagna | Tre coppie alternate per ciascuna campagna | Tracking a seed fissi con hash del lavoro identico, poi ciclo completo. Registrare job, conferme, CPU, RSS, p95 tracking, drain allo stop e carico esterno |

Esempio dalla radice EVO su ciascuna macchina:

```sh
cmake -S . -B build/f4-native-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DNGENE_PROFILE_ACQUISITION_JOBS=OFF -DNGENE_EXPERIMENTAL_INT32_NEON=OFF \
  -DNGENE_EXPERIMENTAL_INT32_AVX2=OFF -DNGENE_EXPERIMENTAL_Q14_AVX2=OFF
cmake --build build/f4-native-release
ctest --test-dir build/f4-native-release --output-on-failure

cmake -S development/profiling/F4 -B build/f4-native-kernels -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/f4-native-kernels
cmake --install build/f4-native-kernels --prefix build/f4-native-kernels/package
b=build/f4-native-kernels
"$b/ngene_f4_fft_lab" "$b/reference.bin" > "$b/reference.csv"
for mode in compact compact-adaptive; do
    "$b/ngene_f4_fft_lab" "$b/$mode.bin" "$mode" > "$b/$mode.csv" || exit
    cmp "$b/reference.bin" "$b/$mode.bin" || exit
    cmp "$b/reference.csv" "$b/$mode.csv" || exit
done
```

Sugli host x86-64 idonei ripetere il ciclo anche per `avx2`,
`compact-adaptive-avx2`, `compact-buffered` e
`compact-buffered-madd`. Salvare le misure di
`--timing-adaptive-avx2`, `--timing-buffered` e
`--timing-buffered-madd`. Sui sistemi ARM usare il pacco scalare
CI8 e la prova separata INT32 NEON. Per la verifica NEON compilare
due alberi, con `NGENE_EXPERIMENTAL_INT32_NEON` prima OFF e poi ON.
Usare `ngene_int32_fft_lab` e il confronto per dimensione e
direzione descritto nel rapporto F4.

La parte PCPS e scheduler richiede gli adattamenti di banco alle
piattaforme. Il banco RF corrente usa API Windows per seek esteso,
memoria e CPU. La configurazione CMake del replay sperimentale ?
limitata a Windows x64 e il runner usa `GetSystemTimes`. Su Linux
e macOS occorre abilitare i target sperimentali con gli hook
portabili dello scheduler, compilare i kernel ISA previsti e
adattare seek, CPU e RSS alle API native, con unit? esplicite.
Il campionamento del carico esterno deve sottrarre la CPU del
processo di replay dalla CPU aggregata dell'host nello stesso
intervallo. Conservare anche i campioni grezzi e il log della
cancellazione.

I test di sincronizzazione coprono fine file e cambio di
generazione durante il coarse. Verificare che il workspace
prestato torni disponibile e che un job annullato termini senza
pubblicare. Il confronto numerico ? per chiave composta da
costellazione, epoca, primo campione e PRN. Per promuovere il
riuso, richiedere un guadagno ripetibile sotto carico comparabile
senza peggioramento del tracking o del drain di cancellazione.
La compilazione o l'emulazione su un altro host resta evidenza
distinta da una prova nativa.

