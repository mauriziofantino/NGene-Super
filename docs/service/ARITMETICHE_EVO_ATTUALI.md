# Come EVO genera le repliche, correla ed esegue le FFT

| Operazione | Metodo usato oggi nell’acquisizione EVO |
|---|---|
| Generare la portante locale | Seno e coseno all’avvio del blocco, poi una ricorrenza complessa per generare i campioni. La portante non è letta da una LUT. |
| Ottenere il rate desiderato | La portante è generata direttamente al rate richiesto. La replica di codice è campionata dai chip con un accumulatore di fase. I campioni ricevuti vengono compressi per il coarse e ripresi dalla cattura originale per il raffinamento nativo. |
| Correlare | Correlazione circolare con FFT nel coarse. Prodotto e somma I/Q nel raffinamento diretto e nella conferma. |
| Eseguire le FFT | Butterfly con coefficienti precalcolati nel piano. Tre aritmetiche disponibili per il coarse GPS e Galileo. |

## 1. Come viene generata la portante

EVO chiama `sin` e `cos` per calcolare due numeri complessi in double. Il primo è il valore della portante al primo campione del blocco. Il secondo è la rotazione tra due campioni consecutivi. Se `φ` è la fase iniziale in cicli, `f` la frequenza da rimuovere in Hz e `Fs` il rate in campioni al secondo, la generazione è questa.

```text
z[0] = cos(−2πφ)       + j sin(−2πφ)
d    = cos(−2πf/Fs)    + j sin(−2πf/Fs)
z[n+1] = z[n] × d
```

A ogni campione, EVO emette le due componenti di `z[n]` in float32. La ricorrenza resta in double e viene rinormalizzata periodicamente. Per generare la **portante quadrata** usa il segno di ogni componente della stessa ricorrenza e produce `−1` o `+1`. Il mixer float moltiplica la coppia I/Q ricevuta per quella portante con il normale prodotto complesso.

In Classic il metodo del report usa una tabella di blocchi e un accumulatore di fase. In EVO la tabella è prevista per i coefficienti della **FFT**. La generazione della portante locale segue la ricorrenza qui sopra, anche quando il coarse usa una FFT intera. In quel caso il mixer intero arrotonda seno e coseno float a coefficienti Q14 campione per campione, prima dei prodotti int32. [Generatore e mixer](../../src/dsp/search.c) · [Metodo Classic](WORKING_MANUAL.md#61-connection-between-the-implementation-and-the-numerical-report)

## 2. Come si ottiene il rate desiderato

Qui intervengono tre operazioni diverse.

1. **Portante al rate voluto.** Il generatore riceve direttamente `Fs` e produce un valore per campione a quel rate. Per esempio, a 8,184 Msps una portante locale di 1 kHz avanza di `1000 / 8184000` cicli per campione. Sulla griglia coarse cambia il denominatore. EVO non genera prima una portante a rate più basso da interpolare.
2. **Replica di codice al rate voluto.** Il PRN è una sequenza periodica di chip. Per ogni campione EVO avanza la posizione del codice di `rate_codice / Fs` chip e legge il chip corrispondente. Per GPS l’avanzamento usa il NCO a punto fisso. Nella replica BOC nativa si applica anche il segno della sottoportante, che può cambiare a metà chip. A circa otto campioni per chip, più campioni consecutivi ricevono il valore dello stesso chip. Questa è la ricostruzione della replica a frequenza campione.
3. **Segnale ricevuto dal rate nativo alla griglia coarse.** EVO assegna i campioni originali alle celle della griglia con confini interi. Per la cella `k` di un periodo lungo `T` secondi, il confine è `floor(k × Fs_nativo × T / N)`. Fa la media I/Q dei campioni di ciascuna cella. Per GPS a 8,184 Msps ci sono 8184 campioni nativi per millisecondo, che diventano 4096 valori della griglia coarse. Per il raffinamento di codice e la conferma EVO rilegge i campioni nativi della cattura. I 4096 valori compressi non vengono espansi per ricostruire gli 8184 originali.

In GPS il periodo vale 1 ms, in Galileo E1-B 4 ms e in BeiDou B1C 10 ms. La griglia B1C operativa usa 16384 punti e cerca il ramo dati su una banda laterale del BOC. [GPS](../../src/acquisition/gps.c) · [Galileo e BeiDou](../../src/acquisition/galileo.c) · [Generazione del codice](../../src/dsp/search.c)

## 3. Come vengono fatte le correlazioni

**Coarse, tutti i ritardi per un Doppler.** EVO toglie l’IF iniziale nella preparazione. A ogni frequenza candidata ruota i campioni della griglia, esegue la FFT del segnale e moltiplica ogni bin per il coniugato del bin corrispondente dello spettro del codice. L’inversa restituisce il vettore dei ritardi circolari. Per ogni ritardo si calcola `I² + Q²` e si sommano le potenze di otto periodi. La ricerca confronta il picco con una soglia ricavata da una riga a Doppler volutamente errato. [Coarse GPS](../../src/acquisition/gps.c) · [Coarse Galileo e B1C](../../src/acquisition/galileo.c)

**Raffinamenti e conferma, pochi ritardi o frequenze.** EVO genera la replica per il ritardo da provare, moltiplica ogni campione già traslato per il chip locale e somma I e Q in double. Il raffinamento Doppler GPS/Galileo con portante sinusoidale usa la griglia compressa. Con portante quadrata usa i campioni nativi e un oscillatore unico IF più Doppler. B1C usa il percorso Doppler nativo. Il raffinamento codice e la conferma rileggono la cattura nativa. La conferma applica la regola 3-su-4 su nuove finestre, con riferimenti a Doppler errato nelle unità della correlazione diretta. [Raffinamenti GPS](../../src/acquisition/gps.c) · [Raffinamenti Galileo e B1C](../../src/acquisition/galileo.c)

**Dove entra XOR.** Volendo calcolare `x × (−1)` con una replica binaria, il correlatore diretto può invertire il bit di segno di `x` float. Il modulo rimane lo stesso. Il selettore XOR di acquisizione agisce sul prodotto con il **codice** `±1`, dopo il mixer, e può quindi essere usato anche quando la portante è sinusoidale. Il prodotto complesso del mixer e quello in frequenza del coarse hanno un altro contratto. Per una replica CBOC con pesi diversi da `±1`, l’inversione del segno copre solo la parte binaria del prodotto. [Correlatore diretto](../../src/dsp/search.c)

## 4. Come vengono fatte le FFT

I coefficienti delle butterfly, detti *twiddle*, vengono preparati con seno e coseno alla creazione del piano. Durante l’esecuzione la FFT legge i coefficienti dalla tabella del piano. Il bit reversal e le butterfly trasformano la griglia in spettro. La modalità float accoppia stadi radix-2; i percorsi interi hanno proprie butterfly e regole di scala.

| Coarse GPS/Galileo | Dati e coefficienti | Guadagno e lavoro a valle |
|---|---|---|
| `float` | Coppie float32 e twiddle float32. | Prodotto spettrale float, inversa con normalizzazione `1/N`, accumulo della potenza in double. |
| `integer_precise` | La baseband preparata in float viene scalata e arrotondata a int16. La FFT lavora su int32 con twiddle Q30. Il codice binario entra come `±64`. | Diretta senza dimezzamento per stadio. Prodotto spettrale int64, shift comune con troncamento per rientrare in `±8192`, inversa normalizzata. Ripristino del guadagno e potenza in double. |
| `integer_scaled` | Stessa conversione della baseband. FFT int16 con twiddle Q14. Il codice binario entra come `±16384`. | Le butterfly dimezzano a ogni stadio. Prodotto spettrale int64 con shift comune, inversa ottenuta con trasformata diretta sul coniugato. Il ripristino del guadagno in double include `N²` per compensare le scale delle trasformate. |

La FFT coarse si seleziona con `[dsp] gps_fft` e `[dsp] galileo_fft`. BeiDou B1C usa il piano float operativo da 16384 punti. EGNOS usa il workspace dedicato della famiglia GPS. Il monitor RF ha la propria scelta `[dsp] spectrum_fft`, indipendente dalle due impostazioni di ricerca. [FFT float e int32](../../src/dsp/search.c) · [FFT Q14](../../src/dsp/fft/fft.c) · [Impostazioni INI](../../src/config/config.c)

### Variante Fast int8 del banco F4

Il banco F4 contiene FFT con dati intermedi int8, shift fisso o adattivo per stadio e prove con twiddle quantizzati Q7 e Q15. La precisione dei twiddle e la larghezza dei dati intermedi sono due scelte indipendenti. Un twiddle in Q7 impiega meno livelli di Q15. La butterfly può comunque allargare dati e coefficienti per eseguire i prodotti e gestire gli estremi prima di riscrivere il risultato int8. Il banco misura saturazione, esponente cumulato, errore e tempo. Il kernel adattivo ha anche una variante AVX2.

Il confronto Precise/Fast del banco è distinto dal selettore operativo. `integer_precise` attiva la FFT int32 con twiddle Q30 e `integer_scaled` attiva la FFT int16 con twiddle Q14. Il candidato int8/Q7 è disponibile nel banco F4. I selettori INI di acquisizione e monitor RF non lo espongono. [Banco e risultati F4](../../development/profiling/F4/README.md)

## Nota sul tracking

Il tracking genera la portante con il proprio NCO a ricorrenza e usa somme prefisse in double per correlare sui tratti di replica a segno costante. Il selettore XOR del tracking è letto dalla configurazione, mentre il correlatore operativo per tratti applica direttamente il segno. Con mixer quadrato o quantizzato, il C/N₀ usa anche un Prompt sinusoidale di riferimento calcolato sugli stessi campioni. EGNOS eredita la configurazione del mixer di tracking. [Tracking](../../src/tracking/tracking.c) · [EGNOS](../../src/tracking/egnos.c)

