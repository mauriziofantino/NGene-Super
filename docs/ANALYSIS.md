# Analisi del segnale · v0.4.0

Frontend → tap pre → conditioning condiviso (bypass/DC IIR) → ring GNSS unico → stessa finestra ai canali → tap post. Recording pre/post scelto a ricevitore fermo. PSD e histogram scelgono il proprio tap indipendentemente; nessun secondo ring GNSS.

## Raccolta e proprietà

Un worker opzionale calcola PSD e istogrammi; ciascuno ha due burst privati da massimo262144coppie, lunghezza e rate indipendenti. Il tap usa try-lock/copia limitata, saltando in caso di contesa; non esegue DSP o I/O. Il burst incompleto è scartato su discontinuità. Snapshot pubblicati per valore, deep copy prima di telemetry e nuova copia nella history browser. Nessun puntatore al ring/frontend attraversa questi confini.

Il worker elabora il ready più recente; un singolo snapshot corrente è disponibile. Il browser richiede un solo status alla volta e conserva solo l’ultimo in attesa di rendering. Sono contati salti, contese e frame invalidi. Nessuna riga grafica viene sintetizzata per un salto di sequenza. La GUI sospende l’interesse quando i pannelli non sono usati: il server combina le richieste dei browser collegati e revoca la raccolta appena l’ultimo consumatore disattiva il pannello o chiude la connessione. La scadenza di 2 s resta il recupero per client che smettono di richiedere aggiornamenti. La CLI di sola cattura non istanzia il worker.

Con spettro/waterfall disabilitati, la GUI non verifica né copia i PSD, non accumula history e non prepara bitmap, mesh, proiezioni o disegni Canvas. Anche i callback di resize e i ridisegni già accodati ricontrollano l’abilitazione. Gli istogrammi rimangono indipendenti. Sequenze PSD e istogramma distinte evitano di ridisegnare dati invariati o il pannello non aggiornato; il waterfall viene ridisegnato quando accetta una nuova riga, rispettando il proprio rate. In pausa si possono ancora ruotare e ridimensionare esplicitamente i dati conservati, senza produrre nuove FFT.

Spettro e waterfall hanno interruttori Enable separati nelle rispettive intestazioni. Il primo acceso abilita anche il produttore runtime con `spectrum_enabled=true`; l'ultimo spento lo disabilita. Ciascun contenuto spento è nascosto; basta uno dei due acceso per mantenere la richiesta PSD condivisa. Lo spettro spento non viene disegnato mentre il waterfall continua, e il waterfall spento non raccoglie history mentre lo spettro continua. Modificare lo stesso parametro da Diagnostics sincronizza gli interruttori anche in pausa. La configurazione runtime viene modificata usando i valori applicati dal server, conservando gli altri parametri e le bozze locali; la persistenza su INI richiede il salvataggio del profilo.

Il backend scarta burst incompleti e accodati quando viene revocato l’interesse; i calcoli già iniziati controllano l’annullamento tra fasi limitate e non pubblicano risultati annullati. Un’FFT già in esecuzione può terminare prima del controllo successivo. Il worker inattivo attende una notifica, senza risvegli periodici per cercare lavoro. Le risposte WebSocket omettono gli array e la verifica del checksum dei diagnostici non richiesti; metadati, contatori del ricevitore e registrazione restano disponibili. `spectrum_enabled=false` disabilita il calcolo anche se un browser continua a richiederlo.

Memoria per istanza diagnostica inferiore a6MiB (workspace, storia temporale e piano), separata dalle code del ricevitore. Non ci sono allocazioni per ogni blocco; finestratura rigenerata solo al cambio forma/dimensione. La registrazione conserva la propria politica di errore in caso di overflow; non si perdono registrazioni per mantenere fluida la GUI.

## Parametri runtime

| INI `[analysis]` | Valori / default |
|---|---|
| spectrum_enabled / histogram_enabled | true/false, entrambi true |
| spectrum_source / histogram_source | pre/post, entrambi post |
| fft_size | 256,512,1024,2048,4096,8192,16384; default2048 |
| window | rectangular,hann,hamming,blackman,blackman_harris; default hann |
| overlap_percent | percentuale intera0..99, default50; hop=floor(N*(100−overlap)/100) |
| averages | segmenti Welch1..64, default4; N+(segments−1)*hop ≤262144 |
| update_hz | PSD0,1..30 Hz, default4 |
| histogram_update_hz | istogramma0,1..30 Hz, default4 |
| histogram_pairs | 1024..262144, default65536 |
| histogram_bins | 16,32,64,128,256; default64 |
| units | dbfs_bin / dbfs_hz; default dbfs_bin |
| temporal | none / mean / ema; default none |
| temporal_frames | 1..16, default4, usati per mean |
| smoothing_alpha | peso del frame nuovo(0,1], default0,2, usato per ema |

I rate sono massimi di avvio burst; la raccolta/calcolo possono ridurre il rate effettivo. Non viene decimato il contenuto di una FFT: i campioni al suo interno sono contigui. Overlap si riferisce ai segmenti Welch nel burst, non agli aggiornamenti GUI. L’istogramma usa sempre il proprio numero di campioni, non NFFT.

GET/POST `/api/v1/analysis/config`, token e revisione richiesti per scrivere. Una nuova revisione invalida burst e risultati precedenti. `Apply analysis settings` modifica sessione/profilo in memoria; `Apply & save` a ricevitore fermo rende persistente. L’export INI comprende le modifiche runtime.

## Numerica e display

CI8 ×128 → finestra Q15 → FFT Q14 assembly /N → potenza e accumuli Welch uint64. Media dei segmenti in potenza lineare. Media temporale/EMA in potenza lineare double e solo dopo log10 per il display. Le butterfly FFT restano intere. Stato temporale resettato al cambio configurazione/epoch/discontinuità rilevata.

Guadagno coerente G=sum(w)/N; ENBW=Fs*sum(w²)/(sum(w))². dBFS/bin usa la normalizzazione del tono (raggio128 CI8 =0dBFS); dBFS/Hz divide la potenza normalizzata per ENBW. Non è dBm. Quantizzazione intera e rumore numerico restano presenti: una FFT lunga non è una garanzia di maggiore dinamica. Nessun bin0 è cancellato.

Output FFT-shift, max1024valori via maximum pooling per FFT più grandi; picco e Δf mantengono risoluzione nativa. Il pooling conserva picchi, non l’integrale del rumore. Il waterfall è una sequenza di snapshot, non una STFT continua. Scale colore globali per tutte le righe, controlli separati di history/rate/assi.

Gli istogrammi signed8 coprono[-128,127], con conteggi uint32 e momenti accumulati su64bit. Media/std/RMS/min/max/zeri/rail e DC magnitude sono calcolati da NGene. Il conteggio rail non certifica clipping RF. Per la diagnosi del bias scegliere pre; per verificare il conditioner scegliere post.

[Report implementazione, tuning, ownership, benchmark e limiti](ITERATION_04.md).
