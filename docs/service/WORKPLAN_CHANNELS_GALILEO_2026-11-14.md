# Channels e tracking Galileo — stato di lavoro

Aggiornato: 14 novembre 2026. Base di lavoro: repository `nav-ngene-evo`; `NGene Classic` resta il riferimento algoritmico. Nessuna verifica hardware dichiarata finché non eseguita.

## Obiettivo e operazioni

- [x] Eliminare la vista autonoma Search Diagnosis. Conservare una sola vista Channels con canali configurati, assegnazione e stato effettivo.
- [ ] Esporre in configurazione il totale canali e due modalità: quote fisse GPS/Galileo (esempio 6+4=10) e pool automatico (esempio 12), con priorità GPS o Galileo quando acquisizioni confermate competono per uno slot.
- [ ] Nella pagina del canale in tracking mostrare la mappa 2D/3D dell'acquisizione che lo ha effettivamente avviato e i correlatori correnti, con grafici leggibili, assi e unità corretti.
- [ ] Nella pagina del canale in acquisizione mostrare la ricerca corrente. Evitare costruzione, copia e richiesta delle mappe per canali non selezionati; conservare solo il lavoro DSP necessario alla decisione di acquisizione.
- [ ] Diagnosticare e correggere il tracking Galileo usando Classic come riferimento. GPS: spaziatura in chip; Galileo: spaziatura in slot BOC(1,1), pari a mezzo chip. Conservare precisione e usare i kernel SIMD esistenti o estenderli con confronto al riferimento C.
- [ ] Verificare build e test mirati, poi replay e HackRF live se il dispositivo è accessibile. Registrare separatamente risultati sintetici, replay e hardware, incluse limitazioni.
- [ ] Aggiornare TRACKING.md, documenti delle viste/configurazione e PROJECT_STATUS.md con i soli risultati dimostrati.

## Evidenza iniziale e punti di ripresa

- `src/tracking/bank.c` ha già un pool unico, `receiver.channels`, `receiver.gps_reserved`, `receiver.galileo_reserved`. Gli slot liberi seguono l'ordine di conferma; non esiste ancora una preferenza configurabile.
- `web/workspace.js` chiama Channels la vista principale, ma conserva una vista Acquisition separata con search space globale; il dettaglio canale usa il risultato corrente del PRN, che può differire dal seed storico entrato in tracking.
- `src/control/control.c` archivia le mappe di tutti i PRN dopo le ricerche. Il costo della visualizzazione va spostato dietro la selezione del canale.
- `src/tracking/tracking.c` usa `galileoSpacingChips=0.2` e offset in chip. La replica Galileo divide ogni chip in due slot; verificare conversione di unità e discriminatore DLL prima di modificarli.
- `src/dsp/tracking_replica.c` ha già percorsi SSE2/NEON per fasi e replica. La lettura iniziale non dimostra ancora la causa del mancato tracking Galileo.
- La ricerca PnP locale (`Get-PnpDevice -PresentOnly`, nome HackRF/Great Scott) non identifica questo dispositivo libusb; non usarla come criterio di assenza hardware.
- Verifica hardware effettiva: `build/tracking-hackrf/ngene.exe --list-devices` rileva libhackrf 0.9.2, un dispositivo con seriale `0000000000000000625863dc2a788e8b`. Con `build/tracking-hackrf/live-galileo-check.ini`, esecuzione live di 12.006 s, 191905408 coppie I/Q processate, zero gap e zero perdite note, 6 slot GPS e 4 Galileo. GPS 825 ricerche, Galileo 33, nessun PRN confermato tra 1 e 12; quindi il tracking Galileo non è stato esercitato. File di configurazione e output nella directory build, esclusa dai sorgenti. Guadagni 16/20 dB, RF amp e alimentazione antenna disabilitati: questa prova non stabilisce la qualità RF o la sensibilità.
- Verifica guadagni richiesta dall'utente: due prove live da 30 s a 16 Msps, zero gap/perdite note. Con LNA/VGA 32/32 dB e RF amp acceso, confermati GPS 12 e Galileo 2/3/7/8; con 40/42 dB e RF amp spento, confermati GPS 12 e Galileo 2/7/8/11. I conteggi non sono un confronto di sensibilità a parità di RF e non dimostrano il tracking; il CLI riporta solo acquisizioni.
- Calibrazione ADC integrata eseguita tramite servizio locale: dispositivo `HackRF One r9 / MAX2839`, 252 stati fisici distinti (756 osservazioni), 25.19 s. Suggerimento LNA 40 dB, VGA 42 dB, RF amp spento; RMS AC 12.422% del fondo scala, DC 2.669%, clipping vicino alle rail 0, errore di distribuzione 0.0532, squilibrio I/Q 0.605%. 173 stati scartati per segnale debole e 28 per overload (motivi sovrapponibili). Il suggerimento misura carico ADC, non C/N0, accuratezza o sensibilità GNSS. Profilo sorgente non modificato; solo INI temporaneo in `build/tracking-hackrf`.
- Prima di modificare la catena GNSS sono stati letti `NGene Classic/LOGBOOK.md` e `OPTIMIZATION_REPORT.md`; in EVO sono stati letti `AGENTS.md`, `PROJECT_STATUS.md` e `ROADMAP.md`. Restano da leggere `TRACKING.md`, i contratti di acquisizione e la migrazione Classic per i dettagli.

## Stato

Ricognizione iniziale e accesso hardware completati; letti anche `TRACKING.md` e `LEGACY_MIGRATION.md`. Implementazione e validazione del tracking Galileo ancora da svolgere. Per riprodurre il difetto serve una sessione con acquisizioni Galileo confermate, idealmente conservando una breve registrazione I/Q per confronti ripetibili. Aggiornare questa sezione a ogni passaggio sostanziale.

## Checkpoint implementazione 14 novembre

- Rimossa la vista Search diagnostics e il relativo rendering globale. Le vecchie URL #acquisition aprono Channels.
- Aggiunta receiver.auto_priority: 0 ordine di conferma, 1 preferenza GPS, 2 preferenza Galileo tra risultati confermati disponibili per slot condivisi. Le quote fisse si ottengono con totale uguale alla somma delle riserve; il pool auto con riserve zero. La preferenza non espelle canali gia assegnati.
- Configurazione Galileo salvata/esposta in tracking.galileo_spacing_slots; 1 slot = 0.5 chip. Import del vecchio galileo_spacing_chips conservato. Geometria DSP interna invariata, SSE2/NEON conservati.
- Grafici correlatori: ritardi Galileo in slot, scala orizzontale centrata sul ritardo corrente, linea tra tap e griglia ampiezza.
- Build Release tracking-hackrf completata. Quattro CTest mirati passati; test slot esteso e passato anche con preferenza opposta alla strategia piu veloce; test geometria JS passato; test browser workspace aggiornato a otto viste passato (il messaggio finale del test riportava ancora nine).
- Live servizio 30 s a 16 Msps, LNA/VGA 40/42 e RF amp spento: snapshot in build/tracking-hackrf/live-status-12.json e live-status-final.json. Galileo PRN7: PLL attivo, Doppler circa 1087 Hz contro acquisizione circa 1217 Hz, main_peak_verified falso; al termine 1078 Hz. PRN2/8 perdono anche coerenza del picco; PRN3 mantiene CN0 circa 29.5 dB-Hz. Un flag PLL non dimostra lock valido.
- Prossimo lavoro: portare e validare il supervisore Classic, in particolare CorrectPllFalseLock in tracking/tracking.c (decisione basata su media codeRate, ipotesi distanti 125 Hz a 4 ms, voto M/N). Non introdurre una correzione fissa di 125 Hz dal solo confronto col seed. Verificare anche BOC peak e comportamento CN0.
- Ancora DA IMPLEMENTARE: mappe correnti calcolate/copia solo per canale selezionato; snapshot immutabile della mappa che ha originato ogni tracking; miglioramento completo dei grafici 2D/3D. Gli archivi backend attuali continuano a copiare le mappe per tutti i PRN.

## Channels a elenco e demodulazione GPS

- Channels usa ora una tabella a elenco, con link alla pagina del canale. Colonne: canale, costellazione, PRN, stato, loop, CN0, Doppler, allocazione, bit e TOW.
- Aggiunta libreria NGene::Navigation indipendente da driver/GUI. Stato privato per tracking, alimentato una volta per ogni integrazione coerente GPS completata, mai dai campioni della telemetria.
- Portata la sincronizzazione GPS di Classic: cinque intervalli validi tra transizioni a multipli di 20 ms, soglia bassa PLL 0.4; accumulo di venti prompt per bit. Il periodo dei loop resta 1 ms.
- Portate le sei maschere di parita GPS e inversione D30 da gpsMessageParser.c. Navigation (TOW) richiede dieci parole consecutive valide, preambolo TLM e HOW con subframe 1..5 e TOW in settimana. TOW scade se non arriva un nuovo subframe entro un intervallo.
- Live fisico HackRF 30 s: GPS12 sincronizza bit e valida 4 subframe; ultima ID4, TOW578544 s; 1390 bit e un errore di parita iniziale. Artefatti build/tracking-hackrf/navigation-live-15.json e navigation-live-final.json. Non e una prova di effemeridi, osservabili o PVT.
- NUOVO LAVORO RICHIESTO, ANCORA PENDENTE: parsing completo effemeridi/clock GPS, Galileo I/NAV con deinterleaving/Viterbi/CRC e gestione issue-of-data; tempo ricevitore comune e osservabili; posizioni/clock satellitari; integrazione solver PVT Classic e controlli qualita. Non far diventare un TOW valido un fix o una qualifica completa di tracking.
- Fonte Classic letta: gpsTracking.c, gpsMessageParser.c/.h, navigationMessage.c/.h, navigationMessageParser.c, galileoMessageParser.c e pvt.c. Classic PVT dipende da Eigen tramite pntNumerics.cpp; migrare rispettando contratto C11 EVO richiede definire il confine del solver, non ricopiare i globali del main Classic.
