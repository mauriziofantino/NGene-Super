# NGene EVO · Piano incrementale e registro di attuazione

## Sintesi per riprendere il lavoro · 24 novembre 2026

**Obiettivo.** Ridurre il tempo che EVO impiega a cercare i satelliti, lasciando al tracking e al calcolo di posizione il tempo di elaborare tutti i campioni. Il confronto si fa sulla stessa cattura. Per valutare l’accuratezza occorrono epoche comuni e una posizione di riferimento. Il numero di fix misura la disponibilità.

**Dove siamo.** Lavorare nel repository EVO, ramo `chatGPT/fast-arithmetic`. Il commit `29f87d4` contiene F1–F4. F6 ha misure di replay Windows GPS/Galileo e F5 ha un banco offline GPS su Windows. Il registro di §13 riporta la copertura e le verifiche aperte. Classic serve come riferimento in lettura.

| Passo | Risultato concreto | Limite della verifica |
|---|---|---|
| F1 · Confrontare lo stesso lavoro | Un banco verifica la mappa completa della ricerca rispetto a un calcolo indipendente. | Q14 e int32 superano la tolleranza in alcuni casi a Doppler errato. Questo scarto resta visibile anche se due versioni dello stesso kernel coincidono. [Report F1](../../development/profiling/F1/README.md) |
| F2a · Controllare la griglia | EVO verifica la dimensione della FFT rispetto ai campioni realmente disponibili. Galileo a 16384 punti richiede almeno 4,096 Msps. | Copertura operativa GPS/Galileo. BeiDou richiede la propria prova su 10 ms. [Report F2a](../../development/profiling/F2a/README.md) |
| F2b · Preparare una cattura una sola volta | GPS e Galileo evitano la compressione duplicata e allocano i buffer per l’aritmetica scelta. Il percorso float evita una quantizzazione superflua. | 108 ricerche in parità entro la stessa modalità. I benefici di memoria e tempo si riferiscono ai profili misurati. [Report F2b](../../development/profiling/F2b/README.md) |
| F3 · Ridurre il costo del prodotto spettrale | La riduzione di scala intera conserva il troncamento con segno. Con un worker la ricerca Q14 migliora in tutte le coppie misurate. | 54 ricerche in parità entro modalità e replay Q14 senza gap. Con quattro worker i risultati dipendono dal caso. [Report F3](../../development/profiling/F3/README.md) |
| F4 · Accelerare la FFT int32 | Su x64 le butterfly SSE2 entrano in uso da 2048 punti. Equivalenza bit per bit col riferimento scalare e riduzione del 30–53% sulla ricerca PCPS int32 nei profili misurati. | A 1024 punti resta lo scalare. Il candidato NEON è bit-esatto in emulazione. Restano le prove native e le misure su ARM. [Misure F4](../../development/profiling/F4/README.md) |

### Quanto resta e in quale ordine

1. **F4, FFT per piattaforma e dimensione.** Su x64 è verificato il percorso SSE2 int32. Per spettro, waterfall e acquisizione qualificare AVX2 su x86 e NEON su ARM, valutando AVX-512 su x86 e SVE/SVE2 su ARM sui processori che li supportano (§9.2). Il candidato NEON int32 rispetta il contratto signed Q30 in emulazione. Restano le prove native e la misura della ricerca completa su hardware ARM.
2. **F6 e F5.** Per F6, completare le misure di attesa e deadline per job e il confronto degli eventi indicizzati per campione. Il replay ha misurato la contesa del pool esistente. Per F5, estendere il banco GPS a Galileo e BeiDou e misurare decisioni e costo della catena. Il banco GPS copre CI8/CI16, portante a 16 livelli, riscalamento a byte e prodotti su repliche binarie, inclusi −128, continuità di fase e riuso della rotazione su tre tap. La replica CBOC multilivello ha un perimetro distinto.
3. **F6a, solo in presenza di perdita misurabile.** Per integrazioni coerenti estese confrontare la deriva del codice con un riferimento a fase continua. Registrare l’effetto su picco, sensibilità e falsi allarmi prima di proporre una compensazione.
4. **F7–F8, configurazione e interfaccia.** Esporre in INI, API e GUI i controlli con backend qualificato, le unità e il valore effettivo per segnale. Verificare importazione dei vecchi profili, riavvio, errori e viste canali/PVT anche in assenza di dati. Le prove U1–U6 sono in §8.
5. **Estensioni con dipendenze proprie.** Per BeiDou partire da identificatori e repliche (B0–B1), poi acquisizione e tracking, decodifica, osservabili, PVT, replay, RF e interfaccia (B2–B10). Per la richiesta iniziale su DC e navigazione, prevedere un confronto offline EVO con/senza rimozione DC sulle stesse epoche PVT. Verificare lo smoothing e le correzioni iono/tropo già presenti prima delle estensioni EGNOS e OSNMA. La qualifica sul segnale reale e l’autenticazione richiedono prove dedicate.

I requisiti originari sono nei [promemoria del 17 novembre](../../DSP_RESUME_2026-11-17.md) e [del 18 novembre](../../DSP_RESUME_2026-11-18.md). Per lo stato successivo valgono i report di fase e lo [stato del ricevitore](PROJECT_STATUS.md). Il [piano delle opzioni DSP](DSP_OPTIONS_PLAN.md) documenta le prove di quantizzazione e portante.

## Piano operativo aggiornato · libreria DSP e calibrazione

Questa sequenza aggiorna le priorità F4–F7. Si riusano le fixture F1, le griglie e le catture preparate F2, le mappe F3, i riferimenti Q14/Q30 e i banchi FFT, correlazione e replay già disponibili. I risultati SSE2/AVX2 e la parità NEON in emulazione restano riferimenti specifici per i profili già provati. Il probe ISA documenta le capacità disponibili. La qualifica prestazionale di AVX-512 e SVE/SVE2 richiede misure native.

| Incremento | Lavoro e criterio di uscita |
|---|---|
| 1 · F4, formati e riferimento INT8 | Inventariare ogni rappresentazione accettata da ciascun ingresso, dal dispositivo o file al buffer DSP. Includere CI8 signed, CU8 con centratura documentata e CI16 con i bit effettivi della sorgente, incluso SC16 Q11. Per ogni ingresso registrare conversione, scala, unità, formato interno e formato d’uscita. Verificare estremi, zero e ordine I/Q. Applicare la matrice a FFT e correlatori, mantenendo disponibili i percorsi float. |
| 2 · F4, FFT intere | Primo banco scalare con ingresso CI8, trasformata diretta e inversa da 8 a 16384 punti. Confrontare ampliamento esatto verso i percorsi esistenti e nuovi profili a memoria stretta. Per ogni stadio dimensionare prodotto, somma dei due prodotti complessi e addizione della butterfly sul massimo rappresentabile. Confrontare widening, shift prefissati e riscalamento per blocco con esponente esplicito. Definire arrotondamento, saturazione e scala cumulata fino a PSD o picco PCPS. |
| 3 · F4, coefficienti e SIMD | Riusare le LUT dei twiddle del piano FFT. Provare tabelle seno/coseno quantizzate, ad esempio Q7 e Q15 accanto a Q30, con regole esplicite per ±1, coniugazione e normalizzazione inversa. Misurare creazione del piano, memoria e tempo a piano riusato. Confrontare ogni kernel SIMD col riferimento scalare dello stesso profilo bit per bit, poi confrontare i diversi profili con l’oracolo ad alta precisione. Includere SSE2, AVX2, AVX-512 con le sottoestensioni effettivamente usate, NEON e SVE/SVE2 secondo operazione e hardware. Proseguire anche la qualifica Q30 già avviata. |
| 4 · F5, correlatori | Applicare la stessa matrice a mixer, prodotto coniugato e accumuli early/prompt/late. Scegliere larghezza dei prodotti e accumulatori dalla dinamica delle repliche e dalla lunghezza di integrazione. Verificare segno, code vettoriali, clipping e parità degli accumulatori. Usare i dot product specializzati quando compatibili con segno e scala del profilo. |
| 5 · F4–F6, qualifica applicativa | Per ogni nuovo profilo misurare errore massimo e RMS, scala restituita, saturazioni, PSD/waterfall, mappa PCPS, sensibilità e falsi allarmi, tracking e accuratezza PVT su replay confrontabili. Fissare le tolleranze prima delle prove. Conservare le decisioni SiGe coarse, raffinamenti e conferma M-su-N. Registrare per operazione e dimensione sia il costo numerico sia latenza e scadenze sotto carico concorrente. |
| 6 · F6–F7, opzione auto DSP calibrate | Modalità esplicita distinta da auto a precisione fissa. Rilevare CPU, ISA utilizzabili, core disponibili al processo, worker, canali attivi selezionati, frequenza campioni, dimensioni FFT e servizi concorrenti. Misurare su replay rappresentativo il profilo qualificato di maggior fedeltà e i kernel compatibili, valutando prima distribuzione del lavoro e backend a pari precisione. Se le risorse sono insufficienti, sospendere il calcolo di spettro/waterfall e istogramma prima di ridurre la precisione del ricevitore. Sospendere anche FFT/PSD condivise quando nessun consumatore attivo le richiede. Se necessario provare combinazioni di precisione inferiore per operazione, entro i limiti di qualità qualificati. Scegliere la combinazione di maggior fedeltà che rispetta deadline, latenza p99, assenza di gap e margine misurato sotto carico sostenuto. Segnalare quando nessuna combinazione soddisfa il tempo reale. |

In F6 verificare su replay un aumento dei canali attivi fino al sovraccarico e il successivo recupero. Contare le elaborazioni diagnostiche effettivamente eseguite e misurare gap e deadline di acquisizione e tracking. In F7 distinguere servizi richiesti e servizi effettivi, mostrare il motivo della sospensione e riattivare solo quelli richiesti quando il margine torna stabile. Registrare le transizioni nel rapporto di calibrazione.

In F7 esporre calibrazione e scelta manuale via INI/API/GUI. Salvare macchina, build, configurazione, carico, profili ammessi, kernel scelti per operazione e margine misurato. Rivalutare al cambio dei core disponibili, dei canali o degli altri parametri di carico. Applicare il profilo a un confine coerente di elaborazione, con scala e stato compatibili. Nel registro §13 conservare anche i candidati scartati, le motivazioni e i limiti delle prove.

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
| Algoritmo avanzato | Dimensione FFT separata per sistema e opzione `auto`: GPS 1024 come candidato manuale da qualificare, Galileo 4096/8192/16384 subordinato a Fs e BeiDou 16384/32768 oppure N validato dalla strategia a blocchi. PRN GPS/Galileo 1–32 e BeiDou 1–63 dopo B0. `tracking.doppler_smoothing_ms` 0–10000 ms, default 200, zero disattiva. Rendere visibili profilo DSP e modalità applicata. |
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

### 9.1 Librerie DSP selezionabili su x86 e ARM (F5 e F7)

In F5 organizzare le implementazioni dietro l’interfaccia comune di `ngene_dsp`, riusando i kernel esistenti e compilando ciascuna specializzazione con le opzioni ISA locali alla libreria. Prevedere una libreria C di riferimento, SSE2 e AVX2 su x86, NEON e NEON con Dot Product su ARM. Valutare AVX-VNNI e SVE2 sui kernel per cui esiste un candidato misurabile. Documentare per ogni libreria i kernel coperti e i formati supportati.

In F7 rendere la scelta disponibile da INI, API e GUI con lo stesso comportamento su x86 e ARM. Proporre `auto` come default anche per i profili precedenti. In questa modalità scegliere per kernel l’implementazione qualificata dalle misure tra quelle compilate e supportate da CPU e sistema operativo. Consentire all’utente di scegliere esplicitamente una libreria compatibile e di vedere la preferenza salvata e il backend effettivo per kernel, compresi i kernel comuni usati dalla libreria. Validare una scelta esplicita incompatibile con un errore che indichi le alternative disponibili. Il profilo aritmetico si configura indipendentemente dalla libreria.

Prime prove di qualifica:

1. **AVX2 sul prodotto Q15 esistente.** Sul banco x86 confrontare il kernel SSE2 con una variante a 256 bit per CI8 e CI16, usando gli stessi campioni, coefficienti e accumulatori finali. Verificare la parità bit per bit con il riferimento scalare anche agli estremi signed e per lunghezze che lasciano un vettore parziale.
2. **Prodotto fra byte per la portante a 16 livelli.** I coefficienti F5 da −15 a +15 entrano esattamente in un byte. Confrontare il candidato attuale con `SDOT` su ARM e con AVX-VNNI su x86. Per `VPDPBUSD`, se il campione signed viene traslato di +128, sottrarre dall'accumulo 128 volte la somma dei coefficienti corrispondenti. Verificare i limiti degli accumuli intermedi e la parità col riferimento a 16 livelli.
3. **Rotazione Q15 con uscita CI8.** Vettorizzare il candidato scalare con AVX2 e NEON. Confrontare i byte in uscita, il conteggio delle saturazioni e i tre correlatori early/prompt/late con gli stessi criteri di arrotondamento, anche al confine tra blocchi.

Per ciascun candidato misurare prima il kernel su hardware nativo. Per l'adozione nel ricevitore richiedere un vantaggio ripetibile sulla catena completa, con la stessa cattura, lo stesso numero di worker e gli stessi indici campione. Confrontare decisioni di acquisizione e risultati di tracking e PVT entro il medesimo profilo aritmetico. Per il dispatch a runtime verificare il supporto di CPU e sistema operativo, registrare il backend effettivo e provare anche il fallback SSE2, NEON di base o C sulla piattaforma pertinente.

### 9.2 FFT SIMD su x86 e ARM per spettro, waterfall e acquisizione (F4 e F7)

Qualificare in F4 le FFT dietro le interfacce comuni di `ngene_dsp`. Su x86 confrontare AVX2 con SSE2. Su ARM partire dal confronto nativo tra NEON e C scalare, iniziando dal candidato int32/Q30 già verificato in emulazione, poi misurare Q14 e float. Per ogni modalità provare tutte le potenze di due del kernel, da 8 a 16384, misurando separatamente creazione del piano ed esecuzioni con piano riusato. Registrare il backend più veloce per modalità, dimensione e CPU, compresi i casi in cui conviene il kernel esistente.

**Estensione delle prove a CPU con SIMD più ampio (decisione del 2026-11-24).** Il confronto AVX2/SSE2 su Windows giustifica un prototipo AVX-512 a partire dalla FFT int32/Q30. A partire da 2048 punti il tempo del kernel è sceso del 46–56% e la ricerca a un worker ha beneficiato sia su GPS sia su Galileo. Per Q14 il guadagno del kernel è più contenuto e alcune dimensioni PSD rallentano. Dare quindi precedenza a Q30 e misurare Q14 e float separatamente. Su ARM completare prima la misura nativa NEON Q30, poi confrontare SVE/SVE2 con NEON sullo stesso hardware. [Misure Q30](../../development/profiling/F4/README.md#incremento-avx2-int32q30) · [Misure Q14](../../development/profiling/F4/README.md#incremento-avx2-q14)

In F4 confrontare AVX-512 con AVX2 sulla stessa CPU usando la matrice FFT, PSD/waterfall e acquisizione di questa sezione. In F5 estendere il confronto ai correlatori con prodotti allargati, contro AVX2 su x86 e NEON su ARM. Compilare i candidati in unità dedicate. In F7 abilitare ogni kernel soltanto dopo verifica delle estensioni richieste dalle istruzioni compilate (AVX-512F ed eventuali sottoestensioni come BW per operazioni a 16 bit), del supporto del sistema operativo allo stato vettoriale e, su ARM, delle capacità SVE/SVE2 esposte al processo. Provare anche la richiesta esplicita su CPU incompatibile e verificare il fallback documentato. Per ammettere un candidato in auto richiedere correttezza e un vantaggio ripetibile nel carico applicativo per CPU, formato e dimensione. Conservare nel registro anche regressioni e casi senza beneficio, indicando backend effettivo, CPU, compilatore, worker e dispersione delle misure.

Su AArch64 valutare SVE per le butterfly float e SVE2 per le butterfly intere Q14 e int32/Q30, verificando prodotti allargati, scala e arrotondamento. Confrontare ogni candidato con NEON sulla stessa CPU. Scrivere i loop in funzione del numero di lane attive e registrare la lunghezza vettoriale effettiva del thread di misura. [Guida Arm a SVE](https://learn.arm.com/insights/sve/) · [Istruzioni DSP di SVE2](https://documentation-service.arm.com/static/6579cb8c159ca73387223bea)

| Utilizzo | Dimensioni da provare | Accettazione sul percorso completo |
| --- | --- | --- |
| Spettro e waterfall 2D/3D | Tutte le potenze di due da 256 a 16384 per la PSD | Usare il worker PSD comune alle tre viste. Con gli stessi campioni, finestra, overlap e medie, confrontare potenza per bin, scala, posizione dei picchi e sequenza delle righe pubblicate. Misurare CPU e latenza per PSD con spettro e waterfall attivi insieme, riusando la stessa pubblicazione. |
| Acquisizione PCPS GPS e Galileo | GPS operativo 2048/4096, più 1024 al banco. Galileo 4096/8192/16384 con Fs compatibile secondo F2a. | Esercitare FFT diretta e inversa. Sulla stessa cattura confrontare mappa completa, rumore a Doppler errato, picco, raffinamenti e conferma M-su-N. Misurare tempo di ricerca e CPU a parità di worker. Estendere a BeiDou quando saranno disponibili le fixture B1/B2. |

Per Q14 e int32 richiedere parità bit per bit con il riferimento della modalità, inclusi scaling, arrotondamento, saturazione e normalizzazione. Per float fissare prima della misura le tolleranze massime e RMS rispetto al riferimento, verificandole su impulsi, toni centrati e fuori bin e rumore. In F7 applicare il selettore di §9.1 anche alla FFT e riportare il backend effettivo per modalità e dimensione. Verificare scelta esplicita e `auto` in base alle estensioni disponibili su CPU e sistema operativo. Su ARM includere il caso NEON di base e, dove disponibili, SVE e SVE2. Usare le misure native di PSD e acquisizione per decidere quali varianti adottare.

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
| F7 | Inventario della sezione 7 e contratto backend stabile per ogni controllo. Implementare U2 e il selettore delle librerie DSP di §9.1 in GUI/API/INI su x86 e ARM. | T7–T9, valori limite, migrazione andata/ritorno e persistenza dopo restart. Per entrambe le architetture provare auto, selezione esplicita, libreria indisponibile e lettura del backend effettivo. Escludere dai controlli attivi le chiavi ignorate. |
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

Per ogni nuovo confronto registrare gli input distribuibili, l'host di misura, il riferimento numerico e le tolleranze per famiglia. Una baseline deterministica sintetica è utilizzabile anche se manca un replay RF con verità nota. In tal caso la qualifica dell'accuratezza di posizione rimane aperta. F2a ha una prova autonoma sul denominatore.

Per B2 rimangono da scegliere componente iniziale, griglia e soglie B1C. Per B4 occorrono vettori indipendenti con polarità e sincronismo dichiarati. Per B7 occorre concordare riferimento temporale/terrestre e modello dei bias. Per i backend ARM e AVX occorre associare ogni affermazione di prestazione a hardware reale. Eventuali obiettivi percentuali di velocità, centimetri di differenza o scarti C/N0 sono criteri sperimentali da fissare prima della prova. Le soglie operative di acquisizione e PVT hanno un procedimento di approvazione separato.

## 12. Scheda di implementazione F4

**Baseline e perimetro.** Partire da `5de362f` e conservare i dati locali F4. Il primo candidato riguarda le butterfly di `NgSearchFftExecuteInt32` in [search.c](../../src/dsp/search.c). La funzione scalare è il riferimento per l’equivalenza. La specifica numerica è comune ai backend C, SSE2 e NEON. La validazione prestazionale è per architettura.

| Ordine | Intervento | Evidenza richiesta per passare oltre |
|---|---|---|
| 1 | Aggiungere al banco F4 una chiamata diretta alla FFT a N=1024, poi coprire 2048, 4096, 8192 e 16384. Congelare vettori, seed, direzione, versione del compilatore e hash della baseline. | Prove riproducibili con zero, impulso, tono, estremi ammessi e residui negativi. N=1024 viene provato direttamente anche quando il profilo GPS operativo seleziona un’altra N. |
| 2 | Implementare un solo candidato SIMD int32 per volta, iniziando da x64 dove esiste il profilo. Documentare i limiti di prodotti e somme intermedi e conservare la divisione signed Q30 con troncamento verso zero e la scala della trasformata inversa. | Confrontare **ogni componente I e Q** con il C scalare, in entrambe le direzioni. Accettare zero differenze per questo contratto, inclusi i casi negativi al confine dell’arrotondamento. Ripetere la stessa prova sul backend NEON quando disponibile. |
| 3 | Verificare i chiamanti. In acquisizione esercitare le ricerche GPS/Galileo native. Nello spettro controllare il ramo int32 in [analysis.c](../../src/analysis/analysis.c), che passa per `NgSearchFftExecuteMode`, e rendere espliciti formati e conversioni ai confini. | Dal buffer di ingresso FFT all’uscita del kernel intero il percorso deve restare intero. Confrontare scala e bin dello spettro, mappe complete, rumore a Doppler errato, picco, raffinamenti e conferma M-su-N. Conservare i riferimenti F1 per il confronto fra aritmetiche. |
| 4 | Cronometrare in Release senza timer interni, con gli stessi input, ordine A/B bilanciato e uno e quattro worker. Confrontare prima/dopo int32 e usare float e Q14 come confronti distinti. | Tempi grezzi e mediane per FFT e ricerca completa, memoria per worker, latenza di cancellazione e gap del replay. Registrare eventuali regressioni e attribuire i risultati all’host effettivo. |
| 5 | Integrare il candidato dove equivalenza e vantaggio sulla ricerca sono ripetibili. Se fallisce un criterio, conservare il backend precedente e registrare il caso riproducibile. | Report in `development/profiling/F4/`, revisione e hash, configurazione normale ripristinata, CTest e replay con esiti. Registrare separatamente piattaforme misurate, emulate e ancora da provare. |

Durante F4 conservare ricerca coarse, raffinamento Doppler, raffinamento codice e conferma M-su-N, inclusa la stima delle soglie. I timer appartengono al banco di sviluppo. Le misure dei worker restano compatibili con lo scheduling fra costellazioni.

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
| 2026-11-24 | F4 | In corso | `5de362f` | Profilo iniziale float/int32 Windows x64, N=4096/16384. [Misure e riproduzione](../../development/profiling/F4/README.md) | Butterfly int32 da valutare per prima. Restano banco diretto N=1024 e qualifica del kernel. |
| 2026-11-24 | F4 x64 | Verificato | `5de362f` → patch locale SSE2 | 80 FFT e 1.015.808 componenti I/Q identiche, 54 ricerche PCPS in parità, replay da 13.087.200 coppie a zero gap, 30/30 CTest Release. Riduzione della ricerca int32 del 30–53%. [Report](../../development/profiling/F4/README.md) | SSE2 da N=2048. NEON resta da qualificare. |
| 2026-11-24 | F4 ARM64 | In corso | patch locale NEON | 60 FFT, 761.856 componenti I/Q identiche in Unicorn. Compilazione incrociata del sorgente per Windows, Linux e macOS ARM64. [Report](../../development/profiling/F4/README.md) | Candidato opt-in. Restano build native e misure su hardware ARM. |
| 2026-11-24 | F4 AVX2 int32 | Banco Windows verificato | Candidato locale | Parità da 8 a 16384 punti nelle due direzioni e confronto GPS/Galileo a uno e quattro worker. [Report](../../development/profiling/F4/README.md#incremento-avx2-int32q30) | Candidato opt-in. Misura nativa NEON predisposta nel job macOS ARM64. |
| 2026-11-24 | F4 AVX2 Q14 | Banco Windows verificato | Candidato locale | Parità FFT da 8 a 16384 punti, sette dimensioni PSD e nove fixture PCPS. [Report e CSV](../../development/profiling/F4/README.md#incremento-avx2-q14) | Candidato opt-in. Beneficio sulla catena dipendente da dimensione e profilo. |
| 2026-11-24 | F6 | Replay Windows eseguito | `29f87d4` | Matrice tracking/acquisizione su CI8 misto, quattro replay da 144 milioni di coppie con zero gap e stessi quattro campioni PVT osservati. [Report](../../development/profiling/F6/README.md) | Eventi di acquisizione osservati diversi. Rimangono aperte parità degli eventi e deadline per job. |
| 2026-11-24 | F6 | Prima traccia per job | Candidato locale | Replay int32 SSE2/AVX2 con attese e tempi di decisione per costellazione. Le 46 decisioni con chiavi campione comuni coincidono. [Report](../../development/profiling/F6/README.md#traccia-per-ricerca-e-confronto-f4-int32) | Una coppia di replay. Restano da misurare ripetibilità e deadline operative. |
| 2026-11-24 | F5 | Banco GPS Windows verificato | `29f87d4` + banco locale | Portante a 16 livelli e rotazione riscalata a byte, con riferimento double e contratti su −128, clipping e continuità. [Report](../../development/profiling/F5/README.md) | Restano Galileo/BeiDou e le verifiche su acquisizione, tracking/PVT e costo della catena. |

Le capacità già presenti nella baseline, compresi PCPS intero, backend SIMD e GUI DSP, sono riferimenti per il confronto. I risultati storici mantengono la data, la revisione e la copertura riportate nei documenti originali. [Stato progetto](PROJECT_STATUS.md) · [Validazione](VALIDATION.md) · [Benchmark FFT](../../benchmarks/fft/README.md)

## 14. Review Astra suggerita

Le decisioni che beneficiano di una seconda review sono la dimostrazione dei bound e del rounding per SIMD int32 e prodotti stretti (F4/F5), la correlazione circolare B1C e l'ambiguità del pilota (B1/B2), la conformità LDPC/CRC con vettori indipendenti (B4) e la coerenza BDT/GPST/GST con il rango del PVT misto (B6/B7). Per ciascuna presentare la decisione concreta, lo spezzone di codice o modello, un controesempio e la prova che lo copre.
