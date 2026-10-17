# NGene è la base del progetto

Il progetto è l'evoluzione incrementale di NGene. L'archivio originale costituisce la base funzionale e algoritmica: prima di implementare una funzione GNSS si esamina il corrispondente modulo esistente, si identifica ciò che è riutilizzabile e si motiva ciò che richiede adattamento.

Il primo incremento introduce l'infrastruttura necessaria al percorso I/Q e alla portabilità. Il suo codice operativo è nuovo; gli algoritmi GNSS originali non sono ancora collegati. Questo stato non declassifica il codice originale a semplice esempio.

## Riferimento verificato

I due archivi allegati, ngene.tar e ngene(2).tar, risultano identici byte per byte, con dimensione 28.467.200 byte e SHA-256:

    c15c72e9e715e6a77ee6ccf694adb3e2d939a355b1f6ad3ff4e82121de8e1acb

L'archivio originale resta separato dal pacchetto del primo incremento ed è conservato senza modifiche. Il manuale di lavoro contiene l'inventario e l'analisi precedente. I percorsi sotto riportati sono relativi alla radice ngene/ dell'archivio.

## Mappa iniziale di migrazione

| Base NGene | Destinazione/responsabilità | Stato e lavoro necessario |
|---|---|---|
| receiver/ngene.c, receiver/channelManager.c | src/core/, API canale | Primo motore I/Q implementato. Recuperare progressivamente assegnazione, scheduling e logiche di controllo utili; adattare gli offset al nuovo avanzamento comune. |
| frontEnd/circularBuffer.c e relativo header | src/stream/ | Nuovo ring complesso implementato. Conserva il principio dello stream condiviso; lease e gap sono espliciti. |
| frontEnd/ | src/frontend/ | HackRF nativo implementato. I driver dismessi non sono inclusi nel nuovo target. Seguiranno altri adattatori nativi. |
| asmFolder/arrays.c, asmFolder/fft.c e altri kernel inclusi testualmente | src/dsp/ e include/ngene/dsp/ | FFT interna già adattata e verificata, con reference e kernel SSE2/NEON. Libreria matematica separata per trasformate, statistica e potenza. Restano da migrare mixer, repliche e correlazioni con contratti numerici espliciti. |
| acquisition/ | Futuro modulo di acquisizione | Partire da ricerca grossolana, raffinamenti, conferma e somme non coerenti esistenti. Separare stato, workspace e accesso ai campioni. |
| tracking/ | Futuro modulo di tracking | Migrare GPS/Galileo, discriminatori, loop e accumulatori. Adattare mixer e correlatori a I/Q; aggiungere holdover e riacquisizione concordati. |
| utilityGnss/ | Futuri servizi di segnale, tempo e navigazione | Recuperare selettivamente codici, parsing e decodifica dei messaggi. Separare dati condivisi e stato per canale; verificare la provenienza dei singoli file. |
| warmStart/ | Futuro servizio di assistenza | Recuperare la logica utile di visibilità e avvio assistito; aggiungere cache, posizione precedente e fonti di aggiornamento definite. |
| pvt/ | Futuri osservabili e soluzione PVT/PNT | Recuperare tempo ricevitore, misure e solver utili dopo aver fissato le epoche comuni e validato la catena I/Q. |
| osnma/, chimera/ | Futuri moduli trasversali | Conservare nell'inventario: fuori dal primo incremento non significa funzionalità da eliminare. Decidere integrazione e dipendenze durante la migrazione. |
| guiSupport/, utility/ | Futuri servizi di controllo, diagnostica e uscite | Separare il calcolo GNSS dall'interfaccia e dal sistema operativo; selezionare il codice riutilizzabile per responsabilità. |
| libraries/, componenti esterni incorporati | Dipendenze tracciate | Valutare l'utilizzo; preservare licenze e attribuzioni esterne quando i sorgenti vengono trasferiti. |

Le destinazioni future descrivono responsabilità, non moduli già implementati o ABI definitive.

## Regola per ogni migrazione

1. Identificare file, funzioni, assunzioni numeriche e dipendenze nella base NGene.
2. Definire cosa mantenere, adattare o sostituire e registrare il motivo. Sostituire un'implementazione non comporta cambiare automaticamente l'algoritmo.
3. Separare algoritmo e stato per canale da ring, driver, sistema operativo e backend CPU.
4. Verificare con casi di riferimento e replay RF, quando disponibile. Il passaggio IF→I/Q cambia le operazioni: non richiedere un confronto byte per byte fra quantità matematicamente diverse.
5. Integrare nel nuovo motore e aggiornare questa mappa con prove e limiti.

FAST/PRECISE, FFT intera, acquisizione, tracking e PVT non vengono riscritti alla cieca né considerati già portati. Per i kernel SIMD si parte dalle operazioni e dalle ottimizzazioni NGene, verificando quali assunzioni restino valide per gli ingressi multibit. Le prestazioni storiche restano riferimenti, non soglie obbligatorie.


## Migrazione FFT — v0.3.0

`asmFolder/fft.c`, `fft.h` e macro SSE2 di `simd.h` sono stati analizzati per il nuovo kernel `src/dsp/fft/fft.c` con butterfly SSE2/NEON separate. Restano radix-2, bit reversal, signed16 complesso e twiddle Q14; cambiano gestione registri, ABI, headroom e punto di scaling. Non è un wrapper del vecchio sorgente né una sostituzione già verificata in acquisizione/tracking. Vedere [analisi dettagliata](FFT_REVIEW.md) e test numerici.
