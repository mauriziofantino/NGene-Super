# Formato delle registrazioni

Per `prefix=prova` vengono creati:

- `prova_000000.cs8`, `prova_000001.cs8`, …: coppie I/Q signed8 interlacciate, senza header; due byte per coppia, nessun problema di endianness multibyte.
- `prova_000000.cs8.meta.ini`, `prova_000001.cs8.meta.ini`, …: sidecar schema 2 associato a ogni segmento, con parametri per il replay automatico e conteggio delle coppie.
- `prova.index.csv`: associa ogni porzione scritta a indice temporale e posizione nel segmento.
- `prova.meta.ini`: configurazione, sorgente, tempo host di apertura e risultato finale.

Un prefisso esistente non viene sovrascritto. Se l’apertura fallisce a metà, gli eventuali file già creati vengono lasciati visibili e incompleti; scegliere un nuovo prefisso o rimuoverli consapevolmente. La destinazione è una directory locale già esistente.

## Indice

```text
epoch,first_sample,count,segment,segment_offset_samples,flags,missing_known,missing_before
```

Tutte le quantità di campioni sono coppie I/Q. L’offset in byte del file è `2 * segment_offset_samples`. Una finestra può occupare più righe se attraversa un confine di segmento.

`flags & 1` indica un gap prima di questa porzione; `flags & 2` una finestra parziale. `missing_before` è interpretabile come quantità esatta soltanto se `missing_known=1`. Non inseriamo zeri per colmare perdite. Non concatenare ciecamente i file ignorando indice ed epoche quando sono presenti gap.

Il tempo relativo dentro un’epoca continua deriva da indice/fs. L’orario host Unix nei metadati **non è** un timestamp RF o GNSS. Il backend HackRF non dichiara una misura dell’errore del clock.

## Integrità del salvataggio

Il writer opera su una coda limitata con copia dedicata. Le finestre accettate sono scritte nello stesso ordine del motore. L’overflow della coda o un errore di scrittura arrestano il run; il programma restituisce un codice non zero. Non si fa affidamento su perdita silenziosa o memoria illimitata per mantenere il ritmo.

Il footer `[result]` indica `complete=true` soltanto quando il run e il writer si sono chiusi normalmente. Questo significa chiusura software riuscita, **non assenza di gap**: consultare indice e contatori. Il footer comprende campioni scritti, blocchi, segmenti, campioni ricevuti dalla sorgente, perdite note e campioni rimasti nella coda del frontend al termine.

L’assenza del footer, `complete=false`, errori di esecuzione o dimensioni non coerenti indicano una registrazione da verificare. Un errore di I/O può lasciare un ultimo blocco incompleto, anche un byte isolato; non interpretarlo come una coppia valida. `fclose`/flush sono controllati, ma non viene promessa transazionalità fra i file o durabilità contro interruzioni di alimentazione. Un futuro formato contenitore/checksum può aggiungerle senza cambiare il motore.

La segmentazione avviene esattamente al numero di coppie configurato. Con segmentazione disabilitata i file possono superare 4 GiB se il filesystem lo consente: gli indici e i conteggi sono a 64 bit e la scrittura è sequenziale.


## Registrazione controllata dalla GUI

La directory scelta è sul computer di NGene. Il browser può essere locale o remoto attraverso il tunnel documentato nel README. Il nome base è scelto dall’utente; tempo host e identificativo casuale distinguono le sessioni, mentre `_000000`, `_000001`, ecc. identificano i segmenti. In modalità CLI si mantiene il prefisso esatto del file INI senza suffisso di sessione automatico.

**Transfer** scarica un singolo file chiuso tramite HTTP verso il browser, conservando l’originale. Per ricostruire la registrazione trasferire i segmenti `.cs8`, l’indice `.index.csv` e i metadati `.meta.ini`. Nessuna conversione dei byte o memorizzazione dell’intera registrazione nella RAM del browser è richiesta dal codice GUI: il download è nativo del browser. La scelta della cartella locale di download dipende dalle impostazioni del browser. Il trasferimento resta per singolo file; non è implementata una sincronizzazione automatica. **Delete** consente di eliminare un file chiuso dopo conferma e controllo delle sessioni in uso.

`Closed` nella GUI significa che il file non è più aperto dal recorder corrente. Anche una registrazione interrotta con errore può essere chiusa: verificare `complete` e i gap nei metadati. La lista mostra fino a 200 file della cartella selezionata. I file attivi e i collegamenti simbolici non vengono offerti come download.


## Tap pre/post (v0.3)

Il parametro `recording.point` seleziona pre o post conditioning. Il file metadata contiene `recording_point=pre_conditioning` oppure `post_conditioning`. La vista pre preserva i blocchi originali; la post segue le finestre assemblate condivise. Il conditioner operativo può essere bypass oppure DC IIR continuo. In bypass i valori pre/post coincidono; con DC IIR il punto di salvataggio determina i valori preservati. Il test con conditioner trasformante verifica il comportamento futuro senza introdurre filtri fittizi nel prodotto.

Ogni sidecar di segmento è scritto/finalizzato dal writer su disco, senza I/O nella callback RF. I parametri sono copiati all’apertura del recorder. Il file di sessione schema 1 resta disponibile per compatibilità e riepilogo; il conteggio di un sidecar schema 2 riguarda soltanto il suo raw. [Importazione e controlli](REPLAY.md).
