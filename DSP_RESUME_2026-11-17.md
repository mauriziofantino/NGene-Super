# Ripresa lavoro DSP — 17 novembre 2026

## Destinazione obbligatoria
Lavorare esclusivamente in C:\Users\MaurizioFantino\Downloads\Projects\nav-ngene-evo, branch chatGPT/fast-arithmetic. Prima di ogni modifica verificare percorso, repository, branch e git status. Preservare le modifiche dell'utente al firmware SiGe e alla UX. Non modificare NGene Classic.

## Stato effettivo
In EVO sono state introdotte modifiche parziali, non validate, a:
- include/ngene/dsp/search.h: API NgSearchMixPrefixCi8Quantized.
- include/ngene/tracking.h: campo mixerBits.
- src/config/config.c: parsing e salvataggio tracking.mixer_bits.
- src/dsp/search.c: primo mixer quantizzato SCALARE, con trigonometria per campione; non soddisfa il requisito di velocita/SIMD e va sostituito o tenuto soltanto come riferimento.
- src/tracking/tracking.c: selezione del nuovo mixer CI8 e validazione 0/8.
Le tre FFT NON sono integrate in EVO. Nessun risultato Classic valida EVO.
La build normale ha incontrato accesso negato MSBuild FileTracker; rilanciata con autorizzazione tramite scripts/build-windows.ps1 -Action Build -MultiSdr -Parallel 4. Controllare processo eventualmente terminato e build/dsp-resume-build.log prima di dichiarare un risultato. Non dichiarare build/test/replay riusciti senza verificarli.

## Requisiti autorizzati, ancora da completare
1. Tre FFT selezionabili: aritmetica intera Classic con perdite storiche, intera migliorata, float. API comune e scale coerenti per spettro e acquisizione. Benchmark: la variante intera destinata alla velocita deve essere realmente piu veloce della float. Non importare alla cieca i guadagni Classic nel contratto EVO.
2. Opzioni DSP indipendenti: formato campioni, quantizzazione portante, ampiezza risultati, correlazione XOR/prodotto. Configurazione persistente ed esposizione coerente all'utente. Rifiutare combinazioni non implementate, senza fallback silenziosi.
3. Ring buffer nel formato nativo CI8 o CI16; preferire gli 8 bit quando nativi/supportati. Non restringere implicitamente frontend a 16 bit.
4. Mixer e correlatori su interi SIMD, mantenendo percorso preciso e aggiungendo percorso realmente a byte. Portante sinusoidale a 16 livelli, operandi byte, prodotto intermedio sufficientemente largo, risultato riscalato a 8 bit e correlazione direttamente sui byte. L'utente vuole velocita reale, non conversioni che ricadono nel percorso 16 bit. Valutare assembly secondo lo stile delle librerie EVO/Classic; non chiamare veloce un kernel scalare.
5. Codice binario +/-1: confrontare XOR/maschera e prodotto SIMD senza presumere un vincitore. Dove conviene, applicare il segno PRN prima della portante; verificare DC, rounding, -128 e scala. Nel tracking valutare il riuso della stessa rotazione fra early/prompt/late e gli altri tap. CBOC multilivello ESCLUSA: sara un altro prompt.
6. Generazione SIMD di codice e portante sovracampionati, conservando fase fra blocchi, NCO e convenzioni delle conversioni intere Classic. Soglie e filtri possono restare float/double.
7. DC: test con e senza rimozione in EVO, giudicato rispetto al PVT (epoche comuni, coordinate, dispersione, errore su GT applicabile). Non basta il numero di fix. Solo se priva di impatto potra essere trattata nella sola visualizzazione degli istogrammi. Non trasferire conclusioni Classic a EVO.
8. Conservare acquisizione del singolo satellite: coarse, raffinamento Doppler, raffinamento codice, conferma M-su-N. Separare adattamenti numerici da decisioni e stima soglie; non ritoccare queste ultime per mascherare perdite numeriche.
9. Test numerici indipendenti, benchmark dei kernel e replay completi. Distinguere fix da accuratezza e replay da hardware live. Main senza strumentazione sperimentale. Aggiornare documentazione/logbook SOLO DOPO codice e verifiche.
10. In coda completare quanto manca in EVO di carrier smoothing, iono/tropo, EGNOS e OSNMA partendo da Classic e ICD fornito (verificare se allegato effettivamente disponibile). Predisporre caricamento chiavi se necessario; nessuna credenziale inventata.

## Materiale gia esistente in EVO da riusare
Leggere AGENTS.md e i contratti correnti, senza ripetere ricerche inutili:
- docs/service/PROJECT_STATUS.md, ROADMAP.md, DSP_OPTIONS_PLAN.md
- docs/service/GPS_ACQUISITION.md, GALILEO_ACQUISITION.md, TRACKING.md
- docs/service/FFT_REVIEW.md, LEGACY_MIGRATION.md, CODING_STANDARD.md
- src/dsp/fft/fft.c e backends x86/ARM: FFT Q14 /N esistente.
- src/dsp/search.c: FFT float radix-2^2, mixer/correlatori float SIMD e prefix CI8 fusi.
- src/dsp/tracking_replica.c: correlazione per somme di run dei chip.
- src/acquisition/gps.c e galileo.c: coarse FFT float, raffinamenti e conferma.
- src/tracking/tracking.c: percorso fused CI8 e chip-run correlation.
- benchmarks/dsp-options/: studio gia eseguito, riusare riferimenti e harness.
La roadmap dice DC streaming ritirata; non confondere questa scelta con una sottrazione di media di finestra da sperimentare.

## Recupero del lavoro fatto erroneamente in Classic
Non lavorare nuovamente su Classic. Il commit 040a4c4 (WIP: add selectable fast arithmetic paths) conserva il prototipo precedente. Il revert e5a962e e sul branch chatGPT/fast-arithmetic di Classic; main era pulito all'ultimo controllo. Patch non committata salvata in NGene Classic/build/fast-arithmetic-uncommitted.patch. Questi sono riferimenti da leggere/adattare, non da applicare a EVO come patch: API, scale e architettura sono diverse. I prototipi byte Classic erano piu lenti, non una soluzione prestazionale valida.

## Preferenze operative dell'utente
Aggiornamenti brevi, niente scuse ripetute o analisi rifatte. Procedere nel repository giusto, preservare lavoro esterno, non dichiarare completato cio che resta da fare. L'utente spegne il PC e riprende domani da questo file.
