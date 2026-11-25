# BeiDou B1C · B5–B7, dal messaggio al PVT

## B5 · Effemeridi, clock e salute

La cache per PRN riceve una trama B-CNAV1 già validata dal decoder. SF2 fornisce in unità SI la coppia effemeridi/orologio con IODE, IODC, toe, toc, ΔA, Ȧ, Δn₀ e Δṅ₀. Una coppia nuova è utilizzabile quando IODE coincide con gli otto bit bassi di IODC. Durante un aggiornamento non ancora coerente si conserva la coppia precedente entro il suo intervallo di validità. Sequenza di campioni o tempo BDT retrogradi vengono rifiutati. Un nuovo epoch di continuità ricomincia la cache. Il gate preliminare usa età massima di 4 h sia per toe sia per toc e richiede una pagina di salute riconosciuta. HS, DIF, SIF o AIF non nulli escludono l'osservazione. La soglia di età riprende il limite orbitale di EVO e costituisce un criterio conservativo di questa integrazione.

Il propagatore usa i riferimenti MEO e IGSO dell'ICD. Per il pilota applica TGD B1Cp, per i dati aggiunge ISC B1Cd alla correzione di gruppo. TGD B2ap è conservato nei dati per uso successivo. Il PVT calcola posizione, velocità, correzione relativistica e drift in double, includendo l'evoluzione di Aₖ e la derivata del moto medio. I tipi riservati e GEO sono esclusi dalla cache B1C. [ICD B1C, §§7.4–7.7, 7.14–7.15](https://www-labsticc.univ-ubs.fr/nb_ldpc/matrices/BeiDou_2017_B1C.pdf)

## B6 · Epoca e osservabili

L'inizio del primo chip di SF1 ancora WN/HOW/SOH allo stream. Il chiamante fornisce l'integrale di codice tracciato in chip fino al campione richiesto, quindi il tempo ricostruito usa direttamente l'avanzamento frazionario e gestisce il rollover. La libreria rifiuta un cambio di continuità o un'ancora più vecchia di due trame (36 s, in tempo di codice). Fino a quel punto toe, toc e il tempo di trasmissione restano in BDT. Per un'osservazione comune, EVO può prolungare la misura fino a 10 ms dal termine dell'integrazione, usando la frequenza di codice misurata e mantenendo il Doppler più recente.

L'epoca comune nominale usa GPST, con BDT +14 s e origine di settimana GPS 1356 per la conversione assoluta. Il contatore BDT a 13 bit è trattato modulo 8192 per il rollover della cache. L'API di conversione assoluta riceve una settimana BDT con era già risolta dal chiamante. La soluzione allinea i TOW tramite differenze modulo settimana prima di formare le pseudorange. La velocità radiale è −c·Doppler/1575,42 MHz, positiva in allontanamento. La pseudorange fornita dall'API usa il TOW nominale del ricevitore e conserva i bias prima delle correzioni broadcast. [ICD B1C, §7.3](https://www-labsticc.univ-ubs.fr/nb_ldpc/matrices/BeiDou_2017_B1C.pdf) · [RINEX 3.05, §4.1.4](https://docserver.gesdisc.eosdis.nasa.gov/public/project/S6A_RO/rinex305.pdf)

## B7 · Geometria e bias

Il QR con pivot in double usa tre coordinate e una colonna di bias di codice per ogni costellazione **ammessa**, per un totale di 4, 5 o 6 incognite. Il contributo BeiDou usa l'orbita BDCS con il proprio valore di rotazione terrestre. Il PVT misto assume ECEF BDCS e WGS84/ITRF nominalmente allineati. Le differenze fra realizzazioni e gli offset fini BGTO/GGTO restano da verificare su dati reali. Si mantiene il drift Doppler comune del modello EVO, coerente con una singola sorgente che usa lo stesso oscillatore L1/E1/B1C. I bias di codice sono indipendenti e includono gli offset costanti residui. La qualifica del drift con registrazioni e dell'accuratezza assoluta resta aperta. [Riferimento ICAO BDCS e ITRF](https://www.icao.int/sites/default/files/APAC/Meetings/2021/2021%20CNS%20SG%2025/4-Information%20Papers/IP07_ICAO-AI.5.4-PFA-OF-ANNEX-10-VOLUME-I-ON-DFMC-AND-GBAS.pdf)

## Verifica riproducibile

Windows x64 Release con MSVC e Ninja. Suite completa 28/28 in 166,31 s, build senza profiling.

- Una trama B-CNAV1 codificata in modo indipendente attraversa BCH, LDPC e CRC prima di alimentare la cache. Coincide con campi IGSO noti per la prova B5.
- Per tale orbita si controllano coordinate entro 2 mm, velocità entro 0,1 mm/s e clock entro 1 ps rispetto a un riferimento con Newton e differenze centrate. Si verificano scadenza, salto di epoch, salute, issue transitorio e i due ritardi di gruppo.
- Il test di tempo passa dal rollover BDT e dal TOW GPST comune, quindi verifica la pseudorange e il segno della velocità radiale su valori deterministici.
- Il PVT sintetico copre tutte le sette combinazioni non vuote di GPS, Galileo e BeiDou sia a metà settimana sia al rollover GPS. Include PRN omonimi, bias iniettati, residuo inferiore a 2 cm, verifica DOP con inversione indipendente della matrice geometrica e rifiuto di una geometria degenere. I limiti numerici descrivono le fixture e non l'accuratezza RF.

```powershell
cmake --build build/f4-qualification-release
ctest --test-dir build/f4-qualification-release -C Release --output-on-failure
```

Le API B5–B7 lavorano con snapshot forniti dai chiamanti. La catena DSP B1C operativa deve ancora alimentarle dai dati e dal tracker e va qualificata su replay. Le verifiche native Linux e macOS restano aperte. WSL su questa macchina risponde E_ACCESSDENIED.
