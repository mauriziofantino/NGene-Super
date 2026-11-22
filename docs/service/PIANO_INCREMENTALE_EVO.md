# NGene EVO · Piano incrementale e registro di attuazione

**Revisione del 21 novembre 2026.** Baseline di lettura EVO `970c0dc`, ramo `chatGPT/fast-arithmetic`. Il piano è sottoposto ad approvazione per singolo incremento. La revisione documentale è conclusa. In questa revisione sono stati ispezionati sorgenti, contratti e prove storiche. Le misure prima/dopo iniziano con F1.

## 1. Esito della review e priorità

| Priorità | Riscontro sulla baseline | Conseguenza per il piano |
|---|---|---|
| Prima di confrontare le aritmetiche | `BenchmarkIntegerPcps` usa N=4096 e un codice alternato. Il ramo float legge I² di un bin, quello intero azzera tutta la mappa e accumula I²+Q² per ogni bin. Il checksum comune non verifica l'equivalenza. | **F1** rende uguale il lavoro e confronta le mappe prima di cronometrare. I tempi storici del benchmark restano attribuiti al loro esperimento. |
| Prima di forzare N in acquisizione | `Prepare` divide per il numero di campioni nella cella di compressione. A 2,048 Msps con N=16384 la prima cella Galileo è vuota. Lo stesso principio deve essere applicato anche a GPS e BeiDou, con periodi codice e fattori di compressione diversi. | **F2a** generalizzato valida rapporto, periodo utile, FFT e stream effettivo prima di allocare il workspace. È una correzione con una prova di confine propria per ciascun sistema. |
| Prima di ottimizzare le butterfly | Galileo alloca simultaneamente baseband float e intera. A N=16384 i due buffer da 56 periodi valgono 7 e 3,5 MiB per workspace. La banca di 32 spettri float vale altri 4 MiB. Con IF diversa da zero il primo risultato della compressione viene sovrascritto. | **F2b** misura tempi, memoria e cancellazione per worker. Poi confronta l'eliminazione del primo passaggio e l'allocazione per modalità come cambiamenti distinti. |
| Prima di collegare B1C al ricevitore | Lo snapshot di acquisizione indirizza `results[32]` con `prn-1`. Il tracking ha tabelle `[2][32]`. `NgAcquisitionLink` ricrea un pool per una coppia. Il PVT tratta due sistemi. | **B0** precede l'integrazione di B2. Si può sviluppare B1 su vettori indipendenti. B7 richiede un contratto esplicito su tempi e orologi. |

La sequenza del singolo satellite resta **coarse → raffinamento Doppler → raffinamento codice → conferma M-su-N**. Classic SiGe IF è il riferimento per criteri, soglie, stima del rumore e convenzioni. Nel confronto registrare separatamente gli adattamenti di IF, inversione Q, scala ADC, frequenza di campionamento e normalizzazione FFT. Lo scheduling fra costellazioni si valuta su carico e attese a parità di cattura. [Acquisizione GPS](GPS_ACQUISITION.md) · [Acquisizione Galileo](GALILEO_ACQUISITION.md) · [Misure realtime precedenti](PERFORMANCE_REALTIME_2026-11-16.md)

## 2. SIS ICD e perimetro dei segnali

I SIS ICD locali si trovano in [docs/GNSS ICD](../GNSS%20ICD/). Il materiale storico del ricevitore è in [docs/references](../references/). I parametri di segnale nella tabella provengono dai documenti indicati. Le scelte di implementazione e le verifiche mancanti sono nel piano delle fasi.

| Segnale | Replica e messaggio definiti dalla fonte | Dati per navigazione e copertura EVO |
|---|---|---|
| GPS L1 C/A, 1575,42 MHz | Codice primario da 1023 chip, 1,023 Mchip/s, 1 ms, BPSK. LNAV a 50 bit/s, preambolo TLM, parole con parità e dipendenza D29*/D30*, 300 bit per subframe da 6 s. | HOW/TOW e settimana, clock e health in SF1, effemeridi coerenti SF2–3. SF4–5 trasportano fra l'altro UTC, ionosfera e almanacco. Confrontare polarità, rollover e IOD con Classic. EVO acquisisce, insegue e decodifica. [IS-GPS-200N](../GNSS%20ICD/IS-GPS-200N.pdf) |
| Galileo E1-B/E1-C, 1575,42 MHz | Primari di 4092 chip a 1,023 Mchip/s, 4 ms. E1-B dati I/NAV, 250 simboli/s, sincronismo pagina, deinterleaving, FEC convoluzionale e CRC. E1-C pilota con secondario da 25 chip. Segnale trasmesso CBOC. | E1-B EVO usa la replica BOC(1,1) di Classic. Verificare coppia di parti pagina, tempo GST, tipi 1–4 e IODnav coerente, BGD, salute e validità dai dati di servizio. UTC, almanacco e ionosfera hanno campi e cicli propri. Il decoder estrae HKROOT/MACK grezzi. L'autenticazione richiede una verifica crittografica separata. [Galileo OS SIS ICD 2.2](../GNSS%20ICD/Galileo_OS_SIS_ICD_v2.2.pdf) · [OSNMA 1.1](../GNSS%20ICD/Galileo_OSNMA_SIS_ICD_v1.1.pdf) |
| BeiDou B1C, 1575,42 MHz | Primari Weil troncati da 10230 chip a 1,023 Mchip/s, 10 ms, 63 assegnazioni per componente. Dati BOC(1,1), pilota QMBOC(6,1,4/33) con secondario da 1800 chip, potenza dati/pilota 1:3. B-CNAV1 a 100 simboli/s, 1800 simboli per frame da 18 s. | SF1 contiene PRN/SOH, BCH(21,6) e BCH(51,8). SF2 e SF3 usano LDPC su GF(64), interleaving congiunto e CRC-24Q separati. SF2 porta settimana/ora BDT, effemeridi, clock e correzioni. SF3 è paginato e completa le informazioni di servizio, salute, integrità e sistemi temporali secondo tipo. Per il primo fix verificare tempo trasmesso, issue/età, salute, ritardo coerente con la componente e rango geometrico. Nuova catena B0–B10. [B1C SIS ICD, capp. 4–7](../GNSS%20ICD/P020180608519640359959.pdf) |
| B1I, 1561,098 MHz | Segnale e messaggi D1/D2 distinti da B1C. Il PDF B1I serve a identificare la specifica corretta. | Tenere separate sequenze, navigazione e fixture. [B1I SIS ICD](../GNSS%20ICD/P020190227702348791891.pdf) |
| GPS L2C e Galileo E5 | L2C, E5a ed E5b sono descritti dai rispettivi documenti GPS/Galileo. Bande, codici e dati differiscono da L1 C/A ed E1-B. | Fuori dagli incrementi approvati per L1/E1/B1C. I tre documenti EGNOS locali sono definizioni di servizio OS/SoL/EDAS. Per un decoder SBAS occorre stabilire la specifica di messaggio pertinente. |

Il PDF B1C era presente localmente e non tracciato da Git durante la review. SHA-256 `a297befc287fe4598735ccc99246c58e299375d79770a788538123ed4a20d137`. Per B1/B4 registrare numero di tabella o figura, PRN, periodo, convenzione chip→segno, ordine dei bit, polarità e provenienza dei vettori di confronto. Un decoder e un generatore che condividono le stesse tabelle possono condividere lo stesso errore.

## 3. Catena EVO e confronto con Classic

| Passaggio | Rappresentazione e responsabilità | Verifica prima di cambiare |
|---|---|---|
| ADC → adattatore → ring | CU8 con offset, CI8 firmato, SC16_Q11 e CI16 entrano tramite adattatore. Lo stream comune conserva coppie I/Q contigue CI8 o CI16 Q15 in base al profilo. Ring e recording mantengono campioni, ordine e metadati. | Estremi −128/+127, fondo scala Q11/Q15, endian, clipping, inversione, conversione richiesta, sidecar e indice assoluto dopo gap. |
| Condizionamento → acquisizione | Bypass operativo. Le strategie GPS/Galileo preparano griglie proprie, carrier e repliche. PCPS usa FFT, prodotto coniugato, inversa, potenze e somme non coerenti. | Distinguere copia/centratura e trattamento IF dal calcolo della soglia. Confrontare mappe complete, cella vincente anche in caso di parità, rumore wrong-Doppler e quattro esiti di conferma. |
| Tracking → demodulazione | NCO e anelli conservano stato per canale. Carrier e codice alimentano correlazioni prompt/early/late. Il Doppler pubblicato ha un filtro configurabile, 200 ms di default. | Confrontare correlatori complessi, DLL/FLL/PLL, stato lock, C/N0, indice del bit e TOW su identici intervalli. Verificare riavvio e perdita di lock. |
| Navigazione → osservabili → PVT | LNAV e I/NAV alimentano cache broadcast. Epoca comune, posizione/clock satellite, rotazione terrestre, troposfera e least squares con QR pivotato operano in doppia precisione. | Confrontare osservazioni utilizzate e scartate, residui, bias, geometria, DOP e posizione alle stesse epoche, rispetto a verità indipendente quando disponibile. |
| Stato → API/GUI · recording → replay | Snapshot e diagnostica hanno costi che dipendono da canali, mappe e richieste browser. Il file registra lo stream comune e i metadati necessari alla riproduzione. | Misurare anche a browser chiuso, con viste aperte e registrazione attiva. Testare Stop/Start, EOF, gap, metadati e validità delle ultime coordinate. |

Classic ed EVO sono processi distinti. La corrispondenza riguarda sequenze decisionali, convenzioni e dati di riferimento. Il pool condiviso, i formati comuni, lo stream a finestre e i backend costituiscono differenze architetturali EVO. Per ogni divergenza numerica osservata annotare il simbolo Classic, il simbolo EVO, i valori intermedi, l'unità e la motivazione. Consultare `LOGBOOK.md` e `OPTIMIZATION_REPORT.md` nel checkout Classic prima di intervenire sulla catena. [Migrazione](LEGACY_MIGRATION.md) · [Stream e frontend](FRONTEND_STREAM_DECISIONS.md) · [Tracking](TRACKING.md)

## 4. Operazioni critiche e contratto numerico

| Famiglia | Ingresso, intermedi, uscita | Prove e limite di utilizzo |
|---|---|---|
| FFT float | Coppie float32, twiddle precomputati, FFT di ricerca radix-2². Potenze integrate e rumore da confrontare in double. | DFT double a N piccolo e fixture analitiche a 4096/8192/16384. Separare errore relativo, errore assoluto vicino a zero, guadagno forward/inverse e fase. |
| FFT intera Q14 | Buffer int16 in/out, scaling a ogni stadio. La scala di stadio e l'arrotondamento signed appartengono al contratto Classic. | Confronto bit a bit col riferimento intero di ogni backend e confronto in unità fisiche col riferimento double. Controllare minimi negativi, clipping, impulso e toni deboli. |
| FFT intera precisa | `NgSearchFftExecuteInt32` usa int32 in/out, componenti d'ingresso entro ±16384, twiddle Q30, prodotti int64 e divisione signed verso zero. L'inversa divide per N. | Un eventuale SIMD deve preservare anche il caso negativo non divisibile per una potenza di due. Un semplice shift aritmetico cambia il risultato. |
| PCPS intera | `NgSearchIntegerPcps` quantizza il carrier, ruota la baseband, esegue FFT intere, prodotto coniugato largo, scala dal massimo e inversa. Ripristina la scala con `ldexp` prima della potenza double. | Verificare shift, scala baseband e replica, poli del segno, posizione e ampiezza di ogni bin. Distinguere core FFT nativo, costo di preparazione e adattatori float alle altre API. |
| Mixer, replica, correlatori | Ingresso CI8/CI16, prodotti stretti solo dove il bound li consente, somme allargate o prefissi double. `NgSearchMixPrefixCi8Arithmetic` usa anche coefficienti nell'intervallo −7…+7. | `mixer_bits` governa una scelta locale. Documentare scala del coefficiente, prodotto e ampiezza dell'accumulatore separatamente. Per I/Q includere nella prova entrambi i prodotti della rotazione. |
| Riduzioni sensibili | Conservare precisione double per momenti e somme sensibili della catena di riferimento, tempo/clock, geometria ECEF, QR, residui e DOP. | Vietare overflow silenzioso. Documentare l'ordine di riduzione, tie-break e errore massimo anche quando due esecuzioni arrotondano diversamente. |

### Doppler del codice e integrazione coerente

La tesi di Maurizio Fantino ricava il legame fra Doppler della portante e
tasso di chip (`R_rx = R_nom × (1 + f_D/f_RF)`, eq. 4.14). Nel modello
Galileo a bassa dinamica, uno scarto di circa 2,64 chip/s accumula circa
0,011 chip in 4 ms, 0,026 in 10 ms e mezzo chip in circa 189 ms. La tesi
adotta l'approssimazione al primo ordine per l'acquisizione breve e
richiama la correzione per l'allineamento fine in tracking. Per EVO la
durata da verificare è l'intervallo coerente effettivo, compresi eventuali
periodi primari successivi. Il mezzo chip illustra la deriva del modello
e la perdita ammissibile va misurata per replica, griglia e componente.
[Study of Architectures and Algorithms for Software Galileo Receivers, §§4.4–4.5, pp. 38–40](../references/PhD_Fantino070206.pdf#page=66)

- **Baseline EVO.** Il coarse PCPS GPS/Galileo usa repliche a tasso
  nominale e somma le potenze di otto blocchi coerenti da 1 ms per GPS
  e 4 ms per Galileo. I raffinamenti e la conferma usano già il tasso di
  chip corretto per l'ipotesi Doppler. Il tracking inizializza il tasso
  dal Doppler di acquisizione e applica poi la correzione DLL.
  [GPS](../../src/acquisition/gps.c) · [Galileo](../../src/acquisition/galileo.c) · [Tracking](../../src/tracking/tracking.c)
- **Compensazione a ogni periodo PRN.** Per le durate candidate usare
  una finestra coerente di `M` periodi primari interi, con `M >= 1`.
  Provare un aggiornamento del tasso una volta per periodo e mantenere
  fisso il passo dell'NCO di codice all'interno di quel periodo. Come
  seconda variante, mantenere il tasso nominale nel periodo e
  riallineare la fase di codice solo al suo confine usando la deriva
  cumulata. Conservare il residuo frazionario fra periodi e verificare
  l'errore residuo all'interno di ciascun periodo rispetto al riferimento
  double a fase continua. Entrambe le varianti mantengono l'indice
  assoluto dei campioni e la continuità della fase portante. In
  acquisizione, quando si riusano correlazioni per periodo, riallineare
  le uscite complesse prima della somma coerente su `M` periodi.
  In tracking, distinguere la cadenza della compensazione al confine
  PRN dalla cadenza di aggiornamento degli anelli e di pubblicazione
  delle misure. Questa strategia per dwell coerenti multiperiodo è nuova
  per EVO rispetto a Classic in acquisizione e tracking. Classic
  corregge il tasso al passaggio dal coarse agli stadi successivi e
  aggiorna il tasso in DLL durante il tracking ordinario.
- **Acquisizione con integrazione estesa.** A parità di campioni e
  intervallo osservato, confrontare replica nominale, replica a tasso
  compensato per l'ipotesi `f_D` e riferimento double con fase continua.
  In un blocco coerente correggere la deriva durante la somma complessa.
  Per i profili ibridi, dopo il calcolo della potenza di ciascun blocco
  riportare le mappe alla stessa epoca di fase codice prima della somma
  non coerente. Misurare distintamente perdita in ciascun blocco e
  spostamento del picco fra blocchi. Includere residuo Doppler della
  portante, attraversamenti di simboli dati e codice secondario, e prova
  al confine del periodo primario. Usare segni conosciuti, ipotesi
  esplicite sui segni o blocchi compatibili con le transizioni previste.
  Qualificare a parte una variazione delle durate, delle somme o del
  modello di rumore prima di collegarla a soglie operative.
- **Tracking con integrazione estesa.** Confrontare il tracking
  corrente con un candidato in cui la stima Doppler aiuta l'NCO di
  codice e la DLL corregge il residuo. Usare il Doppler riferito alla
  portante RF (ricevuta meno nominale), separando IF digitale e contributi
  di oscillatore e clock di campionamento. Contare una sola volta il
  Doppler già incluso nel seed. Conservare la fase attraverso chunk e
  confini coerenti. Valutare offset e rampa Doppler con entrambi i segni,
  aggiornamento di DLL/PLL/FLL e risposta a transizioni, gap e riaggancio.
  Misurare errore di codice in chip, E/P/L, C/N0, lock, simboli e TOW.

## 5. Opportunità SIMD e loop periodici

SSE2 e NEON sono già presenti nei kernel di ricerca e tracking. La FFT Q14 ha inoltre backend specifici per target. Verificare dalla configurazione CMake, dagli oggetti collegati e dal backend selezionato quale implementazione esegue ogni prova. Per AVX/AVX2 serve un candidato misurato con verifica ISA/OS e fallback C/SSE2. Il numero di lane è un limite di parallelismo aritmetico. Con registri da 128 bit sono possibili 16 byte, 8 halfword, 4 float o 2 double per istruzione, prima di widening, shuffle e riduzioni. [Kernel di ricerca](../../src/dsp/search.c) · [Opzioni DSP e misure storiche](DSP_OPTIONS_PLAN.md)

| Ordine | Loop e dipendenze | Esperimento e criterio |
|---|---|---|
| 1 | Conversione frontend, preparazione griglia, potenze e somme PCPS. Celle contigue, possibile dipendenza di somma fra periodi. | Eliminare passaggi duplicati, misurare byte letti/scritti e tempo per cattura. Conservare ordine delle somme, polling di cancellazione e coda scalare per blocchi corti. |
| 2 | Prodotti spettrali, ricerca del massimo, riuso carrier/replica per Doppler/PRN. Il massimo decide lo shift intero. | Confrontare due passate con prodotto temporaneo e riduzione vettoriale. Contabilizzare scratch e cache per worker. Confrontare AoS/SoA includendo trasposizione e riallineamento. |
| 3 | FFT per stadi, NCO/rotazione e correlazioni su chip-run. Butterfly indipendenti nello stesso stadio, ricorrenza di fase tra campioni. | Confrontare batching di bin/PRN e fusione mixer-correlatore su blocchi indipendenti. Verificare fase ai bordi, code di lunghezza dispari, early/late/prompt e rumore wrong-Doppler. |
| 4 | Replica periodica, C/N0 e filtri temporali, PLL/FLL/DLL, smoothing Doppler. Ogni aggiornamento dipende dal precedente. | Vettorizzare tra canali indipendenti solo se si recupera il costo di raccolta dei dati. Tenere scalari feedback e decisioni. Misurare latenza, attese, lock e simboli. |
| 5 | Effemeridi, posizione satellite, rotazione terrestre e troposfera sono separabili per satellite. Householder QR, pivoting, covarianza/DOP e stato di filtro dipendono dai passi precedenti. | Profilare prima. A pochi satelliti mantenere la variante scalare double. Nessun nuovo Kalman in questo incremento. Confrontare residui, rango, clock e DOP. |
| 6 | Refresh GUI e decimazione mappe/RF. I dati visualizzati possono avere risoluzione ridotta. | Misurare lavoro e traffico con vista attiva e inattiva. La riduzione per il display rimane a valle delle decisioni GNSS. |

## 6. BeiDou B1C · incrementi approvabili

| ID | Dipende da | Modifica e criterio di accettazione |
|---|---|---|
| B0 | F1 per la regressione integrata | Generalizzare identificatore sistema/PRN, snapshot, cache e proprietà del pool prima della terza strategia. Test PRN 1/32/33/63, rifiuto 0/64 per B1C, stesso PRN su tre sistemi, capacità slot distinta dal numero di PRN, fairness, cancellazione, distruzione parziale e compatibilità API GPS/Galileo. |
| B1 | ICD e vettori di riferimento | Repliche separate dati/pilota, assegnazione PRN, fase e secondario. Confrontare chip e confini di periodo con vettori indipendenti, verificare potenza e polarità. Rendere esplicita la componente usata al primo esperimento. |
| B2 | B0, B1, F1, F2a | Coarse, Doppler, codice, M-su-N su singolo satellite. A 2,048 Msps un periodo nativo contiene 20480 campioni, a 8,1838 Msps ne contiene 81838. Il creatore attuale accetta potenze di due fino a 16384. A entrambe le frequenze, 16384 soddisfa il vincolo delle celle di F2a. Confrontare la griglia ricampionata e un candidato a blocchi con un oracolo circolare da 10 ms, inclusi ritardi al confine. Una FFT singola oltre 16384 richiede un'estensione del backend. Valutare aliasing, costo e ambiguità BOC. Richiedere approvazione esplicita per le soglie specifiche B1C. |
| B3 | B2 | Tracking dati/pilota, aggancio secondario, Doppler, mezzi chip e transizioni simbolo. Verificare lock, errore di codice/fase, C/N0 e riacquisizione su iniezione nota. Per l'integrazione estesa, confrontare il contributo Doppler all'NCO di codice e l'allineamento del secondario secondo la sezione 4. |
| B4 | B1 per il decoder isolato, B3 per integrarlo | Allineamento SF1 per PRN/SOH e polarità, BCH, deinterleaving SF2/3, LDPC GF(64), CRC-24Q e pagine. Vettori buoni, invertiti, traslati e corrotti. Confrontare bit campo per campo prima di pubblicare dati. |
| B5 | B4 | Cache effemeridi, clock e salute per PRN/issue. Test età, cambi di issue, aggiornamenti parziali, campi orbitali specifici tra cui variazione di A e del moto medio, ISC/TGD della componente. Verificare posizione e clock a epoche note. |
| B6 | B3, B5 | Pubblicare pseudorange e Doppler all'epoca comune. Dichiarare BDT, origine settimana, trasformazione verso il riferimento ricevitore, rollover e scala delle unità. Verificare tempi e segni con osservazioni indipendenti. |
| B7 | B6 e modello clock approvato | Estendere `NgPvtCompute` e matrici oggi dimensionate per due sistemi. Nel modello a bias indipendenti servono 3 coordinate più un bias per sistema attivo, quindi 4/5/6 incognite, geometria di rango pieno e un modello di drift verificato. Testare BDS solo e tutte le combinazioni miste, riferimento terrestre BDCS, offset temporali, residui e DOP. |
| B8 | B2–B7 per la catena completa | Replay B1C con hash, metadati, traiettoria/posizione o riferimento esterno. Ripetere a freddo, con gap e con tre costellazioni. Pubblicare TTFF in tempo campione, continuità ed errore alle epoche comuni. |
| B9 | B8 e dispositivo disponibile | RF fisica con antenna, frontend, guadagni, banda, Fs, sito e durata registrati. Salvare cattura e verificare che il replay riproduca il risultato. Documentare la copertura effettiva del pilota. |
| B10 | B0 e contratto API, B8 per indicare disponibilità qualificata | Aggiungere scelta B1C, canali, pagine e PVT misto alle viste esistenti. Test browser su stato parziale, PRN 63 e profili storici. Mostrare distintamente acquisito, in tracking, decodificato e usato dal PVT. |

Un aumento globale del massimo FFT coinvolgerebbe anche buffer diagnostici e storici. Qualsiasi proposta deve stimare separatamente RAM per piano, worker e PRN. Il semplice zero-padding a una potenza di due non garantisce la correlazione circolare alla periodicità del codice. In B2 usare il periodo primario da 10 ms come caso di confine e separare l'effetto della griglia dalla deriva di codice accumulata nel periodo e fra i blocchi. Ripetere per le durate coerenti candidate e per la componente dati o pilota scelta.

## 7. Parametri e persistenza

Il catalogo operativo parte da parser, validatore, serializzatore, API e controlli effettivamente usati. Per ogni chiave F7 registra unità, default dal costruttore, estremi inclusivi, passo, dipendenze, valore effettivo, costo CPU/RAM, modifica a caldo o al riavvio e comportamento con profili precedenti. [Configurazione](CONFIGURATION.md) · [Parser e serializzazione](../../src/config/config.c) · [Metadati dei moduli](../../web/module-parameters.json)

| Categoria | Esempi e decisione GUI |
|---|---|
| Utente e hardware | Formato comune auto/CI8/CI16, frequenze in Hz nel profilo, MHz/GHz o Msps nella vista. Range e passi dipendono dal frontend. Mostrare il valore fissato dal SiGe e avvisare della perdita di precisione se si restringe l'ADC. |
| Algoritmo avanzato | Dimensione FFT separata per sistema e opzione `auto`: GPS 1024 come default, Galileo 4096/8192/16384 subordinato a Fs e BeiDou 16384/32768 oppure N validato dalla strategia a blocchi. PRN GPS/Galileo 1–32 e BeiDou 1–63 dopo B0. `tracking.doppler_smoothing_ms` 0–10000 ms, default 200, zero disattiva. Rendere visibili profilo DSP e modalità applicata. |
| Risorse | Slot, riserve per sistema, worker, budget acquisizione, finestre e code. Mostrare unità e allocazione stimata. Verificare capacità aggregata, carico massimo e precedenza tracking/PVT. |
| Derivati, alias e compatibilità | IF digitale da frequenza segnale e sintonia. Ricondurre vecchie chiavi aritmetiche alla forma canonica `[dsp]`. `pvt.carrier_smoothing_length` viene accettata dal parser storico senza impostare un filtro. Trattarla come compatibilità di lettura. |
| Solo INI, mascherati o hardcoded | Incrociare chiavi lette, scritte e realmente consumate. Segnalare controlli mancanti e duplicati nel risultato F7. Parametri di test, seed, ripetizioni e opzioni di profiling restano negli strumenti di sviluppo. |

### Dimensione FFT configurabile per costellazione

N non deve essere una costante globale né dipendere dall'ultimo sistema attivato.
F7 deve prevedere una scelta separata per ciascun segnale e un valore derivato
automaticamente quando è selezionato `auto`:

| Sistema | Valori iniziali candidati | Regola principale |
|---|---|---|
| GPS L1 C/A | `auto`, `1024`, `2048`, `4096` | Il periodo primario è 1 ms. L'auto operativo usa già 2048/4096 in base a Fs. F2a verifica la geometria del candidato 1024 e lascia a F7 la scelta manuale, dopo il confronto sulla ricerca completa. |
| Galileo E1-B | `auto`, `4096`, `8192`, `16384` | Il periodo primario è 4 ms. F2a verifica `N ≤ floor(Fs × 4 / 1000)` sullo stream aperto. Per 16384 serve almeno 4,096 Msps. |
| BeiDou B1C | `auto`, `16384`, `32768` e valori successivi solo dopo estensione del creatore | Il periodo è 10 ms. A 2,048 e 8,1838 Msps la griglia a 16384 può coprire 10 ms senza celle vuote. B2 deve confrontare aliasing, picchi e ritardi al confine rispetto all'oracolo nativo, includendo la scelta della componente. N=32768 richiede un altro backend o una strategia a blocchi. |

I nomi INI definitivi vengono decisi in F7 dopo l'inventario delle chiavi, ma devono
essere separati nel modello dati e nell'API. Esempio di contratto proposto:

```ini
[acquisition]
gps_fft_size=1024
galileo_fft_size=auto
beidou_b1c_fft_size=auto
```

La GUI deve mostrare separatamente il valore selezionato e quello effettivo,
incluso il fallback applicato al riavvio o al cambio frontend. Un valore manuale
non compatibile con Fs o con il backend selezionato viene rifiutato dal validator;
non viene convertito silenziosamente in un'altra dimensione. La selezione manuale
resta disponibile per benchmark, replay e diagnostica, mentre il valore principale
può restare `auto` nella configurazione normale.

## 8. Interfaccia · incrementi verificabili

| ID | Cambiamento circoscritto | Prova browser e risultato atteso |
|---|---|---|
| U1 | Gerarchia e stato globale in `web/index.html` e `web/app.js`. | Avvio, attesa, EOF, Stop, errore e riconnessione. L'operatore vede stato corrente e azione disponibile anche da tastiera. |
| U2 | Parametri e DSP in `receiver-config.js` e `module-parameters.js` insieme a F7. | Importazione di INI vecchio, range dipendente da frontend/Fs, errore di salvataggio e due schede con revisioni concorrenti. Rileggere il valore salvato dopo riavvio. |
| U3 | Canali e acquisizione in `acquisition-table.js` e `acquisition-search-space.js`. | Riga stabile al refresh, zero canali, campi invalidi, scroll su mobile, navigazione da tastiera e PRN elevato dopo B0. Nessuna mappa residua attribuita al satellite successivo. |
| U4 | PVT, mappa e skyplot in `pvt-map.js` e viste collegate. | Primo fix, perdita fix, coordinate obsolete o mancanti e geometria incompleta. Indicazione testuale, unità leggibili e nessun punto spurio a (0,0). |
| U5 | Diagnostica RF, recording e replay nelle viste esistenti. | Stato senza campioni, gap, file esaurito, ripartenza, pause dei grafici. Il controllo del grafico non deve alterare il flusso o la registrazione. |
| U6 | Densità, adattamento e accessibilità in `style.css` e `workspace.js`. | Playwright a 390 e 1440 px, zoom 200%, focus visibile, etichette e tabelle fruibili. Confronto latenza e richieste per vista con dataset fisso. |

## 9. Percorsi aritmetici candidati

- **A · FFT float veloce.** Confrontare lo stato attuale con un solo cambiamento per volta fra twiddle contigui, radix-4, split-radix, Stockham, in-place/out-of-place e batching. Provare separatamente N=1024 per GPS, N=8192/16384 per Galileo e N=16384 per BeiDou dopo il confronto circolare di B2. Un candidato FFT singola N=32768 richiede prima l'estensione del backend. Misurare costo delle permutazioni, cache e scratch per ogni combinazione. Confronto DFT/reference prima di ogni misura.
- **B · FFT intera stretta.** Un vincolo di prodotto effettivo entro 16 bit impone un bound sugli operandi, per esempio 8 × 8 con widening esplicito. Q14 Classic e int32/Q30 restano riferimenti distinti. Fissare scala Q, normalizzazione per stadio, arrotondamento signed, saturazione, accumulo a 16/32 bit provato e gestione di CI8/CU8/SC16/CI16. Un ingresso CI16 richiede una politica esplicita di quantizzazione.
- **C · Vettori puri a 8 bit.** Provare separatamente prodotto saturato, carrier quantizzato e replica bipolare. Una somma di correlazione su tutto il periodo richiede headroom dimostrato. XOR esatto per un cambio di segno richiede correzione del complemento e una replica compatibile. Fissare i regimi a priori nelle prove offline. Per segnali deboli o non caratterizzati mantenere il percorso preciso. Qualsiasi passaggio automatico basato su C/N0 comporta una nuova decisione da approvare.
- **D · Mixer/correlatore/tracking.** Oracolo double, candidato float, intero a 16 bit e candidato a 8 bit, ciascuno con scala e ampiezza degli accumulatori esplicite. Includere costo di conversione, saturazioni, armoniche del carrier, rumore, C/N0, errori di fase e lock. Una regressione di sensibilità o di decisione mantiene il candidato nel banco sperimentale.

## 10. Ordine di implementazione, test e misure

| ID | Prerequisiti e intervento | Confronto prima/dopo e accettazione |
|---|---|---|
| F1 | Congelare baseline, fixture e oracoli. Correggere il confronto PCPS nel target di test, riusare i banchi FFT/tracking. | Stessi campioni, replica GNSS, carrier, inizializzazione, periodi e mappa I²+Q². Misurare separatamente precomputazione, `Prepare`, kernel e ricerca. Risultati ripetibili e discrepanze numeriche registrate per modalità. |
| F2a | Verifica di correttezza della griglia per sistema. Può procedere con test mirati indipendenti dal benchmark prestazionale. | GPS con N=1024/2048/4096, Galileo con N=4096/8192/16384 e BeiDou con il periodo completo e i candidati previsti. Provare Fs vicini ai confini, IF nulla/non nulla, formati stream, `auto`, valori manuali e replay con sidecar. Rifiuto esplicito delle combinazioni impossibili prima dell'allocazione, comportamento precedente sui casi validi. |
| F2b | F1, F2a. Il doppio passaggio con IF non nulla e la quantizzazione intera in modalità float sono presenti sia in GPS sia in Galileo. Misurare le due strategie e valutare separatamente passaggio IF, quantizzazione e allocazione per modalità in workspace e piano FFT. | Parità di mappe, rumore wrong-Doppler e decisioni all'interno della stessa modalità. Misurare direttamente `Prepare` con IF nulla e non nulla e verificare latenza di cancellazione, RAM allocata, memoria residente, creazione e costo per cattura. Preservare chiamanti e distruzione su errore parziale. |
| F3 | F1, F2b. Profilare il PCPS con i workspace per modalità e il costo di `Prepare` già isolato. Ottimizzare un prodotto spettrale o una riduzione per volta in `src/dsp/search.c`. | Conservare le durate e la compensazione operative correnti. Misurare tempi per bin/periodo e ricerca, scala ripristinata, mappe complete, rumore wrong-Doppler, picco e M-su-N. Contabilizzare scratch, banda e cancellazione concorrente. Conservare gli scarti F1. |
| F4 | F1 e profilo risultante da F3. Separare butterfly, permutazione e normalizzazione nei percorsi float e int32, dove la FFT domina la riga. Valutare un kernel per volta e tenere distinta la prova Q14. | T1–T4, fallback C, SSE2/NEON. Provare il parametro N di ciascun sistema, includendo GPS=1024 nel banco e le dimensioni ammesse per Galileo/BeiDou quando supportate. Per int32 confrontare ogni uscita nativa, divisioni signed Q30 e scala inversa col riferimento scalare. Valutare velocità del kernel e della ricerca prima di integrare un backend qualificato. |
| F5 | F1, bound documentati. Famiglie strette e mixer/correlatori della sezione 9 nel banco separato. | T1–T5 con segnali deboli, dinamica piena, tono interferente e repliche ammesse. La matrice deve includere N selezionabile per GPS, Galileo e BeiDou, ma il primo banco può restare offline. Contabilizzare clipping, sensibilità e costo end-to-end. Prima dell'uso operativo serve una decisione sul singolo profilo. |
| F6 | F1, misure dei colli residui. Tracking, batching e scheduling multicanale a precisione invariata. | Confrontare carico, coda, deadline, gap e attese per costellazione. Verificare gli stessi eventi sullo stesso intervallo di campioni e misurare separatamente latenza e disponibilità. |
| F6a | Studio futuro della compensazione Doppler di codice quando la deriva nell'intervallo coerente provoca una perdita misurabile, anche con somme non coerenti successive. Eseguire acquisizione e tracking come confronti indipendenti. | Usare la sezione 4, T3 e T5. A parità di intervallo, separare durata coerente, numero di blocchi, riallineamento fra blocchi e correzione a ogni periodo PRN. Confrontare il tasso nominale, l'aggiornamento del tasso per periodo e la correzione di sola fase al confine con un oracolo double a fase continua. Includere misura CPU/RAM e qualificazione di sensibilità e falsi allarmi in acquisizione. Valutare ogni modifica operativa come incremento distinto. |
| F7 | Inventario della sezione 7 e contratto backend stabile per ogni controllo. Implementare U2 e verificare GUI/API/INI. | T7–T9, valori limite, migrazione andata/ritorno e persistenza dopo restart. Escludere dai controlli attivi le chiavi ignorate. |
| F8 | U1 e U3–U6 su viste esistenti, B10 solo dopo i propri prerequisiti. | Accettazione autonoma per vista con Playwright desktop/mobile, fixture di errore e misura con diagnostica aperta. |

**Rollback di fase.** Mantenere separabili modifiche al banco, validazione, scheduling e kernel. Per un candidato DSP conservare la precedente implementazione qualificata, registrare perché la variante è sospesa e ripetere il confronto con il medesimo manifest. Per B1C conservare la possibilità di avviare e validare GPS/Galileo da profili esistenti. Per la UI verificare il round-trip di un profilo precedente dopo il ripristino. Usare un flag solo se occorre selezionare esplicitamente due comportamenti supportati.

| Test | Fixture, metrica e criterio |
|---|---|
| T1 · Unità numeriche | Impulso, tono on/off-bin, DC, zero, estremi signed, residui negativi, lunghezze dispari dove supportate. Test deterministico per backend. Uscita bit-esatta per contratti interi equivalenti, errore massimo/RMS e normalizzazione per gli altri. |
| T2 · Formati e stream | Stessa forma d'onda codificata CI8, CU8, SC16_Q11 e CI16, entrambe le polarità Q, IF positiva/negativa, clipping e gap. Controllare ordine I/Q, campioni emessi, indice e scala; tolleranza in LSB della conversione dichiarata. |
| T3 · Acquisizione | Cattura fissata per PRN e griglia, periodi uguali per ramo, più otto periodi per il caso Galileo. Codice corretto, wrong-Doppler, solo rumore, bordo picco/pareggio, segnali deboli. Identici PRN e decisioni coarse/refinement/M-su-N sui casi di equivalenza. Per integrazione estesa, provare Doppler nullo e di entrambi i segni, deriva nota, residuo portante, picco principale e secondario BOC e stabilità del ritardo dopo la somma di blocchi riallineati. |
| T4 · Performance | Build Release, stesso host e toolchain, hash input, seed fisso proposto 20261121 più elenco seed per repliche. Proposta iniziale di 5 warm-up e 24 coppie A/B in ordine bilanciato. Annotare affinità/core, turbo, thread, carico, RAM, dimensioni, configurazione e tempi grezzi. Mediana accoppiata e intervallo di confidenza. p95/p99 con numerosità delle singole chiamate sufficiente, separando variabilità fra run. |
| T5 · Tracking e PVT | Replay deterministico con inizio e fine in campioni, GPS, Galileo e misto; B1C quando disponibile. Confrontare E/P/L, TOW, C/N0, osservabili, posizione 3D, velocità, clock, DOP, TTFF e continuità. Riportare numero di epoche comuni e perse; accuratezza assoluta solo con verità nota. Per integrazione estesa, usare la traiettoria di fase codice iniettata come oracolo durante offset, rampa Doppler e ripartenza. |
| T6 · Replay e RF | Replay con/senza gap, restart, recording e sidecar. Su hardware registrare modello, firmware, antenna, sito, guadagno, durata e riferimento. Separare risultati di emulazione, replay e banco fisico. |
| T7 · API e configurazione | INI storici e canonici, valori limite/invalidi, scritture concorrenti, payload senza fix e senza dati. Confrontare schema precedente, errore restituito e valori effettivi dopo rilettura. |
| T8 · Browser | Playwright su fixture stabili, desktop/mobile, tastiera e dati mancanti. Verificare contenuto e interazioni, non soltanto screenshot. |
| T9 · Distribuzione | Build normale Release, test CTest pertinenti, configurazioni web on/off e backend disponibili. Installazione/packaging, avvio da pacchetto e replay breve. Annotare piattaforme effettivamente eseguite. |

Eseguire T1–T3 e i contratti toccati in CI per modifica. Eseguire i confronti temporali su host controllato e RF quando disponibile. Prima di ogni fase fissare input/hash, seed e numero di trial, dimensioni, tolleranze numeriche e criterio di decisione nel registro. Distinguere falsi allarmi per cella e per ricerca. Con 0 eventi in 100 prove indipendenti il limite superiore unilaterale al 95% è circa 0,0295. Una tale campagna non verifica una Pfa di 10⁻⁹.

## 11. Ipotesi e decisioni aperte

Prima della prima modifica al codice concordare il perimetro di F1, gli input distribuibili, l'host di misura, il percorso Classic di riferimento e le tolleranze per famiglia. Una baseline deterministica sintetica è utilizzabile anche se manca un replay RF con verità nota. In tal caso la qualifica dell'accuratezza di posizione rimane aperta. F2a ha una prova autonoma sul denominatore.

Per B2 rimangono da scegliere componente iniziale, griglia e soglie B1C. Per B4 occorrono vettori indipendenti con polarità e sincronismo dichiarati. Per B7 occorre concordare riferimento temporale/terrestre e modello dei bias. Per i backend ARM e AVX occorre associare ogni affermazione di prestazione a hardware reale. Eventuali obiettivi percentuali di velocità, centimetri di differenza o scarti C/N0 sono criteri sperimentali da fissare prima della prova. Le soglie operative di acquisizione e PVT hanno un procedimento di approvazione separato.

## 12. Primo incremento implementabile

**F1** interessa [tests/test_acquisition.c](../../tests/test_acquisition.c) per il confronto PCPS e, se serve un driver separato, i banchi in [benchmarks](../../benchmarks/). Riutilizzare [tests/test_fft.c](../../tests/test_fft.c) e [tests/test_tracking.c](../../tests/test_tracking.c) per le rispettive baseline. Scrivere il manifest di misura e i risultati nel percorso di profiling dell'incremento. Il main operativo resta privo di strumentazione sperimentale. Per F2a i punti di intervento successivi sono [workspace Galileo](../../src/acquisition/galileo.c), validazione sullo stream effettivo e relativa vista di configurazione.

## 13. Criteri di chiusura e registro degli incrementi

**Stati.** Proposto → approvato → in corso → implementato → verificato → accettato. Usare *sospeso* con motivo per una regressione o una prova mancante. *Implementato* richiede una revisione identificabile. *Verificato* richiede prove riproducibili. L'accettazione di un'ottimizzazione richiede un miglioramento ripetibile sulla metrica concordata, equivalenza numerica, assenza di nuovi gap e memoria giustificata. Per una correzione o funzionalità vale il relativo criterio funzionale con controllo delle regressioni.

**Come aggiornare il registro.** A ogni transizione aggiungere una riga datata con un solo ID e riferimenti relativi agli artefatti. La riga più recente per ID ne indica lo stato corrente. Riportare hash prima/dopo, fixture, esito e motivo delle prove mancanti. Per le misure creare il report in `development/profiling/<ID>/`, ripristinare la configurazione normale dopo il profiling e registrarne la verifica. Aggiornare il documento tecnico EVO pertinente e `PROJECT_STATUS.md` quando cambia una capacità consegnata. Classic resta consultabile come riferimento storico.

| Data | ID | Stato | Revisione prima → dopo | Test, misure prima → dopo e report | Decisione o limite aperto |
|---|---|---|---|---|---|
| 2026-11-21 | REV-01 | Review conclusa | EVO `970c0dc` · solo documento | Ispezione sorgenti e contratti, riscontri nelle sezioni 1–4 | Piano disponibile per approvazione per fase. |
| 2026-11-21 | F1–F8, incluse F2a/F2b | Proposto | Baseline `970c0dc` | Da eseguire per incremento | Primo incremento proposto F1. |
| 2026-11-21 | B0–B10 | Proposto | Baseline `970c0dc` | Da eseguire per incremento | B1 isolabile. B0 precede l'integrazione di B2. |
| 2026-11-21 | U1–U6 | Proposto | Baseline `970c0dc` | Da eseguire per vista | U2 coordinato con F7. |
| 2026-11-21 | F1 | Approvato | Baseline `970c0dc` | Riprodotto il difetto del banco PCPS a HEAD | Approvazione limitata alla fase F1. |
| 2026-11-21 | F1 | Verificato | `970c0dc` → stesso HEAD con patch F1 locale, SHA-256 del driver e del binario nel report | Oracolo indipendente a 8192/16384, 144 ricerche cronometrate, 29/29 CTest Release con ripetizione del contratto di build, build normale verificata. [Report e dati grezzi](../../development/profiling/F1/README.md) | Q14 e int32 fuori tolleranza sulle mappe wrong-Doppler. F2a attende approvazione. |
| 2026-11-22 | F2a | Approvato | `970c0dc` con patch F1 locale | La baseline accetta N=16384 a 2,048 Msps. | F2a approvato come incremento di correttezza. |
| 2026-11-22 | F2a | Verificato | `970c0dc` con F1 → stesso HEAD con F1 e F2a locali | 30/30 CTest Release, replay CLI manuale/auto, notifica in Edge, output delle regressioni preesistenti identici. [Report](../../development/profiling/F2a/README.md) | F2b misura GPS e Galileo, separando compressione duplicata, quantizzazione e allocazione. B2 conserva la prova circolare a 10 ms. |
| 2026-11-22 | F2b | Approvato | `970c0dc` con F1 e F2a locali | Banco isolato con varianti incrementali e misure per uno e quattro worker. | Confronto numerico per singola modalità. |
| 2026-11-22 | F2b | Verificato | `970c0dc` con F1/F2a → stesso HEAD con F2b locale | 384 processi di misura, 108 ricerche in parità entro modalità, replay CLI a zero gap, 30/30 CTest Release. [Report e dati](../../development/profiling/F2b/README.md) | F3 parte dai workspace per modalità. Misurare separatamente PCPS e code di annullamento sotto concorrenza. |
| 2026-11-22 | F6a | Proposto | Piano aggiornato dopo F2b | Modello e caso di riferimento dalle [§§4.4–4.5 della tesi Fantino](../references/PhD_Fantino070206.pdf#page=66). | Lo studio e il banco a integrazione estesa restano futuri. La soglia di interesse è una perdita misurabile dovuta alla deriva nell'intervallo coerente. |
| 2026-11-22 | F3 | Approvato | `970c0dc` con F1/F2a/F2b locali | Profiling PCPS alle durate correnti e confronto entro ciascuna modalità. | Il banco a integrazione estesa passa allo studio futuro F6a. |
| 2026-11-22 | F3 | Verificato | `970c0dc` con F1/F2a/F2b → stesso HEAD con F3 locale | Packing spettrale intero con troncamento conservato. 54 ricerche in parità entro modalità, 96 processi di timing, replay Q14 a zero gap, 30/30 CTest Release. [Report e dati](../../development/profiling/F3/README.md) | In F4 partire dalle FFT float/int32 e separare il profilo Q14. La compensazione per integrazione coerente estesa resta in F6a. |

Le capacità già presenti nella baseline, compresi PCPS intero, backend SIMD e GUI DSP, sono riferimenti per il confronto. I risultati storici mantengono la data, la revisione e la copertura riportate nei documenti originali. [Stato progetto](PROJECT_STATUS.md) · [Validazione](VALIDATION.md) · [Benchmark FFT](../../benchmarks/fft/README.md)

## 14. Review Astra suggerita

Le decisioni che beneficiano di una seconda review sono la dimostrazione dei bound e del rounding per SIMD int32 e prodotti stretti (F4/F5), la correlazione circolare B1C e l'ambiguità del pilota (B1/B2), la conformità LDPC/CRC con vettori indipendenti (B4) e la coerenza BDT/GPST/GST con il rango del PVT misto (B6/B7). Per ciascuna presentare la decisione concreta, lo spezzone di codice o modello, un controesempio e la prova che lo copre.
