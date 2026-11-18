# Ripresa esatta EVO — 18 novembre 2026

## Vincolo operativo

Lavorare SOLO in `C:\Users\MaurizioFantino\Downloads\Projects\nav-ngene-evo`, branch `chatGPT/fast-arithmetic`. Classic è solo riferimento in LETTURA; non modificarlo. Non rifare analisi generiche: continuare dal punto qui sotto. Prima di editare: `git status --short`, verificare branch. Preservare tutte le modifiche non committate, incluse quelle precedenti in `include/ngene/dsp/search.h`, `include/ngene/tracking.h`, `src/config/config.c`, `src/dsp/search.c`, `src/tracking/tracking.c`, `DSP_RESUME_2026-11-17.md`. L'utente vuole lavoro completato, non proposte o report di possibili passi.

## Fatto in questa ripresa (non committato)

- Config `[dsp]` operativa, serializzata e con alias di lettura per i vecchi key aritmetici di `[acquisition]`, `[galileo_acquisition]`, `[tracking]`. GUI tab `DSP Arithmetic`, spostate scelte aritmetiche; UI normalizza vecchi INI. Parametri algoritmici restano nei rispettivi tab.
- Tre scelte FFT esposte per GPS/Galileo: `float` radix-2² SIMD EVO; Q14 intera SSE2/NEON derivata da Classic; int32 senza shift di stadio. **CORREZIONE OBBLIGATORIA DELL'UTENTE: una FFT intera deve ricevere e restituire buffer interi e svolgere l'intera trasformata con operazioni intere/SIMD, senza conversioni float nelle SIMD.** Il kernel Q14 `NgFftExecute` già soddisfa questo contratto isolatamente. È stata aggiunta `NgSearchFftExecuteInt32` con buffer int32/interi, ma è scalare e va resa SIMD. L'integrazione coarse attuale `NgSearchFftExecuteMode` passa da buffer float convertendo a interi e poi viceversa: NON soddisfa ancora il percorso operativo richiesto e NON va dichiarata completata. Modificare GPS/Galileo coarse per tenere ingresso, mixer/correlazione spettrale e uscita FFT in interi, preservando scale e decisioni. Solo allora esporre/qualificare le scelte intere.
- Tre FFT selezionabili anche per spettro con `dsp.spectrum_fft`; analisi runtime espone `fft_mode`. Implementazione in `src/analysis/analysis.c` e `src/web/server.c`. Build/test analysis passati prima dell'ultima modifica al mixer; controllare nuova build.
- Mixer quantizzato CI8 usa prodotti SSE2/NEON a 8 campioni e carrier per ricorrenza, non più trigonometria per campione. Ultimo edit appena fatto: modalità `mixerBits` 4/8/16 più float 0, con controllo CI8 in tracking. **NON ANCORA COMPILATO/TESTATO dopo quest'ultimo edit**. Il test numerico è stato esteso in `tests/test_tracking.c`; eseguirlo subito e correggere eventuali errori.
- Smoothing PVT: storia candidata viene committata solo con soluzione valida; riferimento temporale corretto quando cambia `nominal`. Test `broadcast_pvt` passato; serve ancora replay PVT/accuratezza prima di dichiarare stabilità.

## Verifiche già effettivamente svolte

- Build MSVC x64 multisdr Release completa passata prima dell'ultimo edit mixer. Eseguire build con CMake assoluto `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`, `--build --preset windows-msvc-x64-multisdr --parallel 4`; sandbox MSBuild FileTracker può richiedere esecuzione escalated. Evitare riconfigurazioni inutili; per test mirati `--build build/msvc-x64-multisdr --config Release --target ngene_tracking_tests --parallel 4`.
- `gps_acquisition_contract`, `analysis_worker_contract`, `core_contracts`, `broadcast_pvt`, `tracking_contract` passati prima dell'ultimo edit. Prima delle nuove FFT tutti i 28 CTest passati (due test PowerShell richiedono `Set-ExecutionPolicy -Scope Process Bypass`).
- Benchmark 16384 punti x64 del *wrapper* con conversioni float/interi: inizialmente float 108.9 µs, int32 917.1 µs, Q14 241.0 µs; dopo ottimizzazione conversioni SSE2 in un'altra esecuzione float 220.2, int32 1276.3, Q14 161.2 µs. **Confronto non idoneo a giudicare la FFT intera nativa; rumore fra run elevato.** Benchmark nativo `ngene_fft_tests --benchmark`: Q14 SSE2 assembly 39.2 µs a 16384, contro riferimento scalare 461 µs. Non asserire speedup del percorso completo o prestazioni Raspberry.
- Browser Playwright non installato; test browser non eseguito.

## Prossima azione esatta

1. **Priorità immediata:** rimuovere l'adattatore float dal percorso FFT intero di acquisizione/spettro. Usare `NgFftExecute` con buffer int16 I/Q interi per Q14, e `NgSearchFftExecuteInt32` con buffer int32 I/Q per la variante precisa; ottimizzare quest'ultima con SIMD. Mantenere i risultati interi fino a potenza/correlazione con scala esplicita. Prima di editare rileggere questa correzione dell'utente, non ripartire da una proposta.
2. Verificare GUI e testo backend (ancora dice `INTEGER FFT` anche con float scelta). `[dsp]` roundtrip e valori invalidi aggiunti in `tests/test_core.c`, `core_contracts` passato.
3. Valutare scala/accuratezza FFT su fixture/replay e PVT smoothing su epoche comuni e metrica, NON solo numero di fix. Benchmarkare il percorso nativo intero senza conversioni float nel loop; non promettere prestazioni Raspberry senza misura su Pi.
4. OSNMA resta NON IMPLEMENTATO. Classic contiene codice vecchio 2014 ma non trasferire alla cieca. Riferimenti ufficiali letti: Galileo OSNMA SIS ICD 1.1 `https://www.gsc-europa.eu/sites/default/files/sites/all/files/Galileo_OSNMA_SIS_ICD_v1.1.pdf`, Receiver Guidelines 1.3 `https://www.gsc-europa.eu/sites/default/files/sites/all/files/Galileo_OSNMA_Receiver_Guidelines_v1.3.pdf`. OSNMA occupa 40 bit della odd page I/NAV (8 HKROOT +32 MACK), 15 pagine/30s, richiede catena ECDSA/TESLA/MAC e tempo GST affidabile (<30s). EVO `src/navigation/galileo.c` decodifica CRC ma scarta quei 40 bit. Non dichiarare autenticazione dalla sola estrazione. Cercare eventuale ICD allegato dell'utente prima di scegliere specifica. Nessuna chiave inventata.
5. Segnalazione OpenAI richiesta dall'utente: nessun tool di invio feedback/supporto disponibile rilevato. NON dichiarare invio avvenuto.

## Correzione esplicita dell'utente al termine del turno

L'utente ribadisce: la FFT intera Classic **funziona già** con input interi, output interi, butterfly interi SIMD. La nuova variante deve risolvere la perdita di precisione restando anch'essa intera e SIMD. La FFT float è soltanto la terza opzione EVO. Non usare cast float nelle SIMD della FFT intera; non presentare il wrapper con conversioni come implementazione richiesta. Non interrompere il lavoro per fare resoconti o chiedere istruzioni già date. In questo turno è stata aggiunta `NgSearchFftExecuteInt32` nativa intera ma ancora scalare; `gps_acquisition_contract` e `core_contracts` passano. Il collegamento operativo intero resta da completare. Il motivo della precedente interruzione è stato un errore dell'assistente nella gestione del limite token, non una richiesta dell'utente.

## Rischi immediati

- Il mixer 4/8/16 con `-128`/inversione Q passa `tracking_contract` dopo l'ultimo edit. `gps_acquisition_contract` e `core_contracts` passano dopo la nuova API int32. Non ancora ricompilato/testato tutto dopo le ultime modifiche.
- `src/analysis/analysis.c` piano FFT alternativo ricreato quando cambia lunghezza; verificare test runtime. La modalità Q14 usa il percorso precedente.
- La FFT integer scaled ha errore numerico maggiore; nessuna prova ancora su accuratezza PVT o RF live. Non cambiare soglie o logica coarse/refinement/M-su-N.
