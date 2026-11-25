# B4 · Decoder B-CNAV1 per BeiDou B1C

`NgBcnv1Decode` riceve 1800 simboli morbidi dal ramo dati, uno per 10 ms. Un simbolo positivo indica bit zero e il valore assoluto rappresenta affidabilità. `NgBcnv1Find` cerca l'inizio della trama in un blocco contiguo. Richiede il PRN atteso, la stessa polarità e l'avanzamento SOH modulo 200 attraverso due intestazioni distanti 1800 simboli. La seconda intestazione fornisce la verifica di sincronismo. Il payload pubblicato appartiene alla prima trama.

1. SF1 usa la distanza di Hamming BCH(21,6) per il PRN e BCH(51,8) per il SOH, inclusi i limiti di correzione di 3 e 11. SOH è l'indice in unità di 18 secondi. I pareggi sul SOH e i valori oltre 199 sono respinti.
2. SF2 e SF3 sono separati dall'interleaver comune 36×48. I simboli GF(64) sono composti da sei bit MSB-first. Il percorso pulito verifica direttamente il syndrome. Per errori di hard decision il decoder usa min-sum a livelli sui 64 valori, con normalizzazione 3/4 e massimo 16 passaggi. La memoria per i messaggi è allocata solo sul percorso di correzione.
3. La trama è pubblicata dopo syndrome valido, CRC-24Q separato in entrambe le sottotrame e HOW nel range. Il risultato comprende WN, HOW, SOH, IODC, IODE, PageID e i campi comuni SF3, con entrambe le sottotrame integre in byte MSB-first. La conversione di effemeridi e orologio e la gestione delle pagine per navigazione spettano agli incrementi B5–B7.

Riferimento di formato e matrici: §6.2 e allegato B dell'ICD B1C. Il valore 75 della quarta colonna non nulla nella riga SF3 con indice 17 è stato verificato col syndrome del codeword pubblicato e con la trascrizione di PocketSDR. Il valore 77 apparso nell'estrazione testuale della tabella non soddisfa il codeword dell'allegato. PocketSDR è un controllo incrociato dei dati di formato. [ICD B1C v1.0](https://www-labsticc.univ-ubs.fr/nb_ldpc/matrices/BeiDou_2017_B1C.pdf) · [Matrici PocketSDR](https://raw.githubusercontent.com/tomojitakasu/PocketSDR/master/src/sdr_ldpc.c)

## Verifica

Il banco usa un encoder di riferimento distinto dal decoder. Risolve la porzione di parità per eliminazione di Gauss in GF(64) e riproduce le due codeword di esempio dell'allegato B. Poi imposta campi noti nei payload d'esempio, genera CRC con divisione polinomiale, codifica e ricompone la trasmissione nell'ordine dell'ICD. I due vettori con CRC errato hanno comunque parità LDPC valida. Le fixture congelate sono in `tests/test_beidou_bcnv1.c`.

Il test confronta tutti i byte di SF2 e SF3, i campi estratti e gli errori corretti. Copre polarità normale e invertita, traslazione di 19 simboli, rollover SOH 199→0, intestazione seguente di polarità incongruente, dati mancanti, PRN errato, correzioni BCH e LDPC, e rifiuto separato per CRC SF2 e SF3. Un ponte sintetico genera I/Q a 2,048 Msps con replica dati BOC(1,1) in I e proiezione BOC(1,1) del pilota in Q. Il correlatore SIMD produce i simboli morbidi per decodificare la trama invertita. Il ponte usa i confini di simbolo e la fase già noti. L'aggancio e il trasferimento del timing a una catena dati B1C operativa restano da integrare e verificare su replay.

```powershell
cmake --build build/f4-qualification-release
ctest --test-dir build/f4-qualification-release -C Release -R 'beidou_bcnv1_contract|beidou_b1c_primary_contract|tracking_contract|galileo_navigation|navigation_demodulation|broadcast_pvt' --output-on-failure
```

La regressione Windows x64 Release è passata. Restano build e test nativi su Linux/macOS, replay B1C e qualifica delle prestazioni.
