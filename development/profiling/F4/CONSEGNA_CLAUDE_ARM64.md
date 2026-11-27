# Consegna a Claude Code per la verifica nativa delle aritmetiche ARM di NGene EVO

## Mandato

Lavora dalla radice di nav-ngene-evo su una macchina AArch64 fisica. Prepara ed esegui un runner riproducibile per il sistema del collega. Consegna il runner, i dati grezzi e una valutazione breve che un secondo revisore possa ricalcolare. Il riferimento ispezionato per questo mandato è il commit 6e605ba. Registra il commit realmente provato e l'eventuale diff. Leggi AGENTS.md, le sezioni ARM di docs/service/PROJECT_STATUS.md e development/profiling/F4/README.md. Limita le modifiche agli strumenti di verifica. Conserva i criteri numerici, le soglie GNSS e la configurazione operativa.

Modello suggerito per Claude Code: **Sonnet 5.5 con effort medio**. Salva i log su disco e riporta in chat soltanto esiti, anomalie e percorsi. Alza l'effort per il singolo caso se un confronto produce una divergenza riproducibile. [Documentazione Anthropic](https://platform.claude.com/docs/en/models/sonnet-5-5/overview)

## Quali implementazioni misurare

- La FFT Q14 ha un percorso ARM NEON. La FFT di ricerca int32/Q30 ha un candidato NEON AArch64 selezionabile in compilazione. Sono due verifiche distinte. Per conversione campioni, correlazioni e tracking controlla il dispatch effettivo e i contratti esistenti.
- NEON usa vettori da 128 bit. SVE è una diversa estensione ARM a lunghezza vettoriale scalabile. Nei sorgenti operativi della revisione ispezionata manca un backend SVE/SVE2 esplicito. Registra il supporto hardware separatamente dal backend compilato ed eseguito. Eventuale autovettorizzazione del compilatore va indicata come tale.
- Il laboratorio F4 CI8 adattivo comprende C portabile e candidati espliciti x86. Anche benchmarks/dsp-options/experiment.c usa una selezione SSE per i suoi kernel espliciti. Una coppia di esecuzioni scalari su ARM misura il riferimento portabile. Indica nella matrice quali aritmetiche dispongono davvero di un confronto NEON.
- Per int32 esiste già evidenza emulata bit per bit. Il lavoro aperto riguarda esecuzione nativa, correttezza delle build reali e costo sul processore del collega.

## Procedura

1. Registra CPU/SoC, OS, architettura del processo e dei binari, compilatore, CMake, Node, generatore e modalità energetica. Su Linux acquisisci uname -m e lscpu. Su macOS acquisisci uname -m e verifica Rosetta con sysctl -in sysctl.proc_translated. Su Windows usa una shell ARM64 e verifica il machine type PE con dumpbin /headers. Registra eventuale VM o traduzione. Esegui i timing solo su hardware nativo con carico stabile.
2. Usa CMake 3.26 o successivo, compilatore nativo, Ninja e Node. Crea directory nuove per tre build Release: before = backend standard, after = candidato int32 NEON, q14ref = riferimento C della sola FFT Q14. Mantieni identici compilatore e flag generali. Su Windows ARM64, se usi Visual Studio, sostituisci Ninja con il generatore installato e -A ARM64, aggiungi --config Release alle build e -C Release a CTest. I binari dei generatori multi-config sono in Release/.
3. I seguenti comandi Bash sono eseguibili su Linux/macOS dalla radice del checkout. Il runner deve catturare stdout, stderr ed exit code per ogni fase. Su Windows implementa gli stessi passi in PowerShell, controllando LASTEXITCODE e conservando l'estensione .exe.

~~~bash
set -eu
root=build/arm-assessment
mkdir -p "$root/pair"
for v in before after q14ref; do
  neon=OFF
  ref=OFF
  case "$v" in after) neon=ON ;; q14ref) ref=ON ;; esac
  b="$root/$v"
  cmake -S . -B "$b" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DNGENE_BUILD_TESTS=ON \
    -DNGENE_WITH_HACKRF=OFF -DNGENE_WITH_RTLSDR=OFF -DNGENE_WITH_BLADERF=OFF \
    -DNGENE_WITH_SIGE=OFF -DNGENE_EXPERIMENTAL_INT32_NEON="$neon" -DNGENE_FFT_REFERENCE="$ref"
  cmake --build "$b"
  cmake --build "$b" --target ngene_int32_fft_lab ngene_int32_psd_lab
  "$b/ngene" --version
  ctest --test-dir "$b" --output-on-failure --no-tests=error \
    -R '^(fft_integer_contract|sample_math_native|tracking_contract|gps_acquisition_contract|acquisition_pcps_map_contract|acquisition_grid_contract|analysis_worker_contract)$'
done
cp "$root/before/ngene_int32_fft_lab" "$root/pair/ngene_int32_fft_before"
cp "$root/after/ngene_int32_fft_lab" "$root/pair/ngene_int32_fft_after"
f4=development/profiling/F4/run-fft.cjs
node "$f4" verify "$root/pair" > "$root/fft-verify.csv"
node "$f4" timing "$root/pair" > "$root/fft-timing.csv"
node "$f4" summary "$root/fft-timing.csv" > "$root/fft-summary.csv"
~~~

4. Verifica nel log di compilazione il backend Q14 e la definizione NGENE_EXPERIMENTAL_INT32_NEON sull'unità src/dsp/search.c della build after. Ispeziona il disassemblato del relativo kernel e conserva un breve estratto con simbolo e istruzioni vettoriali. Controlla nel codice quali taglie attivano quel ramo. Registra separatamente il nome stampato da ngene --version, che da solo non identifica il candidato int32. Se un backend atteso ricade su C, classifica la misura come fallback.
5. Per int32, verify confronta ogni parola dei buffer su otto vettori, FFT diretta e inversa, potenze di due da 8 a 16384. Il criterio è zero differenze. Conserva i due dump con SHA-256. Al primo mismatch registra N, direzione, vettore, indice I/Q, atteso e ottenuto. Sospendi i timing della variante divergente e completa i controlli indipendenti.
6. Per Q14, oltre al contratto FFT, esegui ngene_int32_psd_lab --q14 in before e q14ref. Per int32 esegui lo stesso banco senza argomenti in before e after. Salva quattro CSV e confronta le righe con uguali N e first_sample. Conta anche righe mancanti o duplicate. Verifica checksum, peak_hz e peak_dbfs, tenendo separati compute_ms e latency_ms. Una differenza di checksum richiede diagnosi anche quando il picco coincide. Per Q14, se il contratto esistente verifica ogni backend contro una tolleranza, riporta tale criterio e il massimo errore disponibile. Riserva “identico bit per bit” ai buffer confrontati integralmente.
7. Per i modi di correlazione e aritmetica sperimentali, configura il progetto autonomo con cmake -S benchmarks/dsp-options -B build/arm-dsp-options -DCMAKE_BUILD_TYPE=Release, compila ed esegui dsp_options --verify. Ripeti in una directory distinta con -DREFERENCE_ONLY=ON. Conserva le righe di qualità, armoniche, clipping e l'intestazione del backend. Per CI8 configura analogamente development/profiling/F4 e usa ngene_f4_fft_lab seguendo il README F4 per i modi portabili disponibili. Riporta separatamente aritmetiche esatte, approssimate e candidati disponibili solo su x86. Per i modi approssimati applica i limiti già codificati nel banco e riporta lo scostamento dall'oracolo.
8. Il runner F4 esegue 24 coppie in ordine alternato. Riporta la mediana dei rapporti after/before per N e direzione, insieme alla dispersione. Un rapporto sotto 1 indica meno tempo per trasformata. Evidenzia inversioni di convenienza e variabilità fra coppie. Su Windows ARM64 aggiungi il banco integrato ngene_pcps_lab per GPS/Galileo con 1 e 4 worker secondo i banchi F3/F4. Confronta le decisioni numeriche prima dei tempi. Su altri OS registra la limitazione del target WIN32. Una misura isolata della FFT permette una conclusione su quella FFT e quella CPU.

## Materiale da restituire

Consegna un archivio arm-assessment-<commit>-<soc> contenente runner e comandi effettivi, manifest dell'ambiente, cache CMake e log di build/test, evidenza del dispatch, CSV grezzi, riepilogo, hash dei dump e dati necessari a riprodurre ogni anomalia. Includi un report Markdown di una pagina e una matrice CSV con colonne host,commit,kernel,arithmetic,backend_expected,backend_observed,oracle,cases,mismatches,status,evidence. Usa PASS, FAIL, FALLBACK, NON_APPLICABILE o NON_ESEGUITO con una motivazione. Indica quali misure consentono una decisione sul candidato int32 per quel SoC e quali ulteriori prove servono prima di proporre un cambio del default.
