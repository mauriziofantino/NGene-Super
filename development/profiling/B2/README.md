# B2 · Acquisizione offline del pilota BeiDou B1C

Il banco usa il primario B1C con replica complessa QMBOC e un periodo di 10 ms. Esegue la sequenza coarse, raffinamento Doppler e codice, conferma 3-su-4 su intervalli disgiunti. Il segnale e il rumore sono sintetici e ripetibili. Il test di conferma separa casi con tre, due o zero intervalli contenenti segnale mediante un taglio in ampiezza riservato al fixture. Griglia Doppler, finestre e taglio del banco non definiscono soglie o configurazioni per il ricevitore.

## Percorsi numerici

- **Griglia 16384.** Media SIMD per celle delimitate da floor(k·N/16384), calcolata dopo il wipe-off nativo. FFT, prodotto del segnale con il coniugato dello spettro della replica e IFFT riusano le primitive di ricerca di EVO.
- **Correlazione nativa a blocchi.** Partizioni da 8192 campioni, inclusa l'ultima parziale. Zero-padding a 16384, somma dei prodotti spettrali per diagonale e sovrapposizione delle IFFT. La convoluzione lineare viene ripiegata sul periodo **N**, quindi la mappa conserva ogni lag nativo anche vicino ai confini del periodo e delle partizioni.
- **Raffinamento e conferma.** Wipe-off SIMD e due correlazioni reali SIMD con riduzione allargata per ogni prompt complesso. Le repliche di codice sono preparate una volta per PRN. Il percorso di misura richiede SSE2 o AArch64 NEON. La correlazione circolare diretta in double è l'oracolo offline.

Si usano 20480 campioni a 2,048 Msps (PRN 1 e 32) e 81838 a 8,1838 Msps (PRN 33 e 63). Per ogni PRN si provano 0, 1, N-1, floor(N/3), 8191, 8192 e 8193, con Doppler iniettato a ±30 o ±130 Hz. Il coarse sintetico valuta tre righe a -100, 0 e +100 Hz. L'oracolo confronta I e Q in sette lag per caso, tra cui il ritardo vero, il confine circolare, un offset di mezzo chip e un offset di una partizione.

## Riproduzione su Windows x64

Con la build Release esistente e l'ambiente MSVC inizializzato:

```powershell
cmake --build build/f4-qualification-release --target ngene_beidou_b1c_acquisition_lab
$rows = foreach ($run in 1..3) {
    $csv = & ./build/f4-qualification-release/ngene_beidou_b1c_acquisition_lab.exe
    if ($LASTEXITCODE) { throw "B2 lab failed at run $run" }
    $csv | ConvertFrom-Csv | Select-Object @{Name='run';Expression={$run}}, *
}
$rows | Export-Csv -NoTypeInformation build/f4-qualification-release/b2-results.csv
ctest --test-dir build/f4-qualification-release -C Release -R 'beidou_b1c_primary_contract|acquisition_pool_contract|acquisition_grid_contract|fft_integer_contract' --output-on-failure
```

## Risultati

Tre ripetizioni sullo stesso binario SSE2, con ordine dei due coarse alternato per ritardo. Mediana e intervallo min–max su 42 scansioni per frequenza di campionamento e percorso. I tempi includono wipe-off, correlazione e scelta del picco per le tre righe. Le tabelle e le repliche sono preparate prima della misura. Il PC era in uso, quindi le misure descrivono questa sessione.

| Rate (Msps) | Coarse griglia (ms) | Coarse nativo (ms) | Scarto massimo coarse griglia | Perdita massima del picco griglia |
|---:|---:|---:|---:|---:|
| 2,048 | 2,622 [1,554–6,277] | 9,170 [5,365–25,445] | 1 campione | 8,674 dB |
| 8,1838 | 3,606 [1,973–5,785] | 45,118 [27,444–71,655] | 1 campione | 2,672 dB |

La perdita è misurata rispetto alla mappa a blocchi, dopo aver normalizzato ciascuna potenza per il quadrato dell'energia della propria replica a ritardo zero. Entrambi i percorsi hanno recuperato il Doppler e il ritardo iniettati dopo raffinamento in tutti i 28 casi per ripetizione. La mappa nativa ha trovato il ritardo esatto già al coarse. Lo scarto complesso massimo dall'oracolo, diviso per N, è stato 1,44e-7. A circa mezzo chip la potenza valeva fra 0,138 e 0,148 volte quella del picco vero. I quattro CTest mirati sono passati.

## Decisione per l'acquisizione

La griglia 16384 è la candidata veloce, ma il ritardo può ridurne sensibilmente il picco. La mappa a blocchi fornisce un controllo nativo per provare allineamenti alternativi delle celle e segnali vicini alla soglia. Entrambi i rate campionano sotto Nyquist rispetto alla fondamentale BOC(6,1) di 6,138 MHz. La simulazione campiona la forma d'onda ideale senza filtro analogico, perciò la risposta dei front-end e il comportamento vicino alla soglia restano da verificare con modelli filtrati o replay. La correlazione operativa e le soglie specifiche B1C restano da qualificare prima dell'integrazione. Eseguire anche il banco e i quattro CTest su Linux e macOS nativi, incluso NEON su ARM64.


## Griglia sfalsata e somme coerenti per tranche · 25 novembre 2026

I 10230 chip del primario durano 10 ms. Il periodo contiene 80000 campioni complessi a 8 Msps e 81838 a 8,1838 Msps. Griglie da 8192 o 16384 punti possono coprire lo stesso periodo mediante riduzione. A 8192 punti si ottengono 0,801 celle per chip, a 16384 se ne ottengono 1,602. La riduzione comprime anche la struttura BOC(1,1). Per questa prova si usa 16384 e si mantiene come controllo la mappa coerente nativa a blocchi.

La variante SIMD prova due origini per le celle. Il secondo passaggio ruota ciclicamente il segnale già compensato in Doppler di 1 campione a 2,048 Msps, di 2 agli altri due rate. Si corregge il lag di ciascuna mappa prima di scegliere il candidato più forte. Ogni passaggio usa tutti i 10 ms. Il mixer Doppler e lo spettro del codice sono condivisi fra i due allineamenti. I picchi dei due passaggi sono ipotesi alternative sul ritardo.

Per rate e livello sono stati provati 24 ritardi attorno al confine circolare e a un terzo del periodo, con ±30 Hz e semi di rumore diversi. Il pilota campionato è filtrato con un FIR periodico simmetrico da 17 tap, cutoff min(0,39·Fs, 2,5 MHz). La replica è adattata al medesimo filtro. Dopo la sintesi si aggiunge rumore uniforme I/Q di ampiezza 0,10. Le due ampiezze di segnale arbitrarie sono 0,004 e 0,012. Il FIR digitale applicato dopo il campionamento serve a questa fixture e non modella l'anti-alias analogico. Un caso è nel bacino di raffinamento se il massimo appartiene alla riga Doppler nominale e cade entro ceil(Fs/1,023 MHz) campioni dal ritardo vero.

| Rate (Msps) | Debole, griglia singola | Debole, griglia sfalsata | Debole, blocchi nativi | Mediane coarse sul debole (ms), stesso ordine |
|---:|---:|---:|---:|---|
| 2,048 | 6/24 | 9/24 | 18/24 | 2,662 / 4,926 / 9,574 |
| 8,000 | 18/24 | 24/24 | 24/24 | 3,892 / 6,890 / 49,984 |
| 8,1838 | 19/24 | 24/24 | 24/24 | 3,569 / 6,056 / 53,244 |

Al livello 0,012 tutti i percorsi sono nel bacino in 24/24 prove a ciascun rate. Rispetto al picco nativo, normalizzato per l'energia della propria replica, la perdita massima a 8,000 Msps è scesa da 6,412 a 1,737 dB e a 8,1838 da 8,427 a 2,835 dB. A 2,048 è scesa da 6,026 a 4,787 dB. I tempi sono wall time di un'unica sessione Windows x64 Release SSE2, con ordine dei tre metodi ruotato per caso. I conteggi misurano l'ingresso nella finestra su segnali sintetici e non fissano una probabilità di acquisizione o di falso allarme.

Sugli stessi 144 casi il banco separa il prompt in dieci tranche, con estremi floor(k·N/10). Somma I e Q prima di calcolare la potenza e conserva l'origine di fase del codice e della portante. Il massimo scarto complesso rispetto all'oracolo circolare, diviso per N, è 3,84e-10. Questo verifica la scomposizione coerente per il tracking B3. Il passo successivo dovrà verificare gli NCO attraverso chiamate e finestre distinte, i correlatori anticipato/puntuale/ritardato e il comportamento quando la fase attraversa il confine del periodo.

```powershell
cmake --build build/f4-qualification-release --target ngene_beidou_b1c_acquisition_lab
& ./build/f4-qualification-release/ngene_beidou_b1c_acquisition_lab.exe |
    Set-Content build/f4-qualification-release/b2-regression.csv
& ./build/f4-qualification-release/ngene_beidou_b1c_acquisition_lab.exe --phase-sweep |
    Set-Content build/f4-qualification-release/b2-phase-sweep.csv
```

Per il prossimo confronto di acquisizione, verificare un'altra proiezione coerente su front-end filtrato o replay, con più PRN e segnali attorno alla soglia. Il doppio allineamento è un candidato vicino a 8 Msps. A 2,048 Msps i casi persi mostrano che servono ulteriori prove prima dell'uso operativo. Verifica nativa Linux e macOS da eseguire.
