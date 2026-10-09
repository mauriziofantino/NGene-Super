# Decisioni e incrementi

NGene è la base del progetto. Ogni futuro modulo GNSS partirà dall’analisi del corrispondente codice originale, documentando riuso, adattamenti e sostituzioni nella [mappa di migrazione](LEGACY_MIGRATION.md).

## Requisiti recepiti

C11 per NGene; Windows/Linux/macOS; target Intel/AMD e ARM; frontend tramite librerie native. Prima HackRF, poi bladeRF, Pluto e RTL-SDR. Nessun SoapySDR e nessun motore in C++.

Il motore è il solo consumatore logico del ring GNSS: tutti i canali ricevono la stessa porzione di stream. Conditioning separato e disattivabile. FFT, correlazioni e altri mattoncini DSP saranno librerie interne, con backend SIMD/NEON appropriati e senza FFTW. FAST XOR può mantenere approssimazioni controllate; PRECISE userà prodotti SIMD. I dati storici di occupazione CPU non sono soglie obbligatorie.

## Implementato adesso

- API C e configurazione INI valida/stretta.
- Ciclo di vita del ricevitore e frontend nativo HackRF.
- Replay CI8 indipendente, anche in parallelo al live; vettori generati soltanto nei test.
- Bypass, assemblaggio delle finestre, ring e dispatch comune.
- Contratto e validazione degli stati del canale, senza algoritmi fittizi in produzione.
- Recorder, segmenti, metadati, gestione overflow/EOF/arresto.
- Controller, GUI dark con configurazione/osservazione separate, registrazione indipendente pre/post e trasferimento file.
- FFT intera assembly SSE2/NEON, spettro/waterfall, istogrammi e configurazione runtime.
- Diagnostica con integrità PSD end-to-end, history senza righe artificiali, istogrammi indipendenti e statistiche I/Q.
- DC remover continuo opzionale, tuning hardware/segnale distinto e contratto NCO/digital IF.
- Primitive intrinsics SSE2/NEON e riferimento C; preset Windows nativi MSVC x64/ARM64.
- Portabilità del core e test; documentazione delle verifiche effettive.

## Incrementi proposti

1. **Prova HackRF reale:** verificare selezione, guadagni, stabilità dello stream, registrazioni e tempi su Windows/Linux/macOS con l’hardware dell’utente. Scegliere dataset di replay.
2. **Ingresso da file e strumenti di controllo:** leggere le acquisizioni con metadati/gap; rendere ripetibili le prove RF. Nessun algoritmo GNSS prima di un riferimento verificabile dei campioni.
3. **Contratti DSP e kernel:** portare dall’archivio utile NCO, repliche e correlazioni; verificare l’inserimento della FFT intera già adattata nelle catene GNSS. Definire scaling/range, poi reference C e SIMD x86/ARM. Confrontare XOR storico sicuro, candidato SAD, eventuali miglioramenti e prodotti; nessun vincitore presunto.
4. **Acquisizione e tracking:** adattare gli algoritmi NGene dietro le API, con scratch privato e integrazioni esplicite. Implementare HOLDOVER e riacquisizione rapida. Rivedere il contratto del canale con i primi algoritmi reali.
5. **Navigazione e assistenza:** demodulazione durante il tracking, cache dei messaggi, aggiornamento da segnale/rete, almanacco, posizione precedente e ricerca guidata dalla visibilità.
6. **Osservabili e PVT/PNT:** epoche comuni, pseudorange, posizione dei satelliti e integrazione del solver recuperabile.
7. **Altri frontend e conditioning:** nuovi formati I/Q, filtri/ricampionamento con contratto di ritardo e griglia temporale; registrazione prima/dopo conditioning scelta esplicitamente.

Le interfacce permettono sostituzioni a compilazione senza cambiare il coordinamento. Non viene promesso un ABI di plugin dinamici già stabile, né compatibilità dei futuri algoritmi GNSS con qualunque rate senza validazione.
