# F5 · Aritmetica stretta sul banco offline

Il banco GPS L1 C/A PRN 7 confronta il riferimento complesso double, i
prodotti Q15 su CI8/CI16, la portante squadrata con XOR corretto, il prodotto
binario impacchettato e due candidati aggiuntivi:

- **Portante a 16 livelli.** Ogni componente assume uno dei 16 valori
  interi dispari tra −15 e +15. Il prodotto SIMD usa ingresso CI8 e accumulo
  allargato. La normalizzazione è `1 / (128 × 15)`.
- **Rotazione Q15 con uscita CI8.** Il prodotto complesso usa intermedi
  int32, poi arrotonda al più vicino, con i pareggi lontano da zero, e satura
  nell'intervallo [−128, 127]. Il segno della replica si applica dopo il
  riscalamento e si accumula in int64.

Il contratto verifica l'identità XOR su tutti i byte, il valore −128, la
parità tra SIMD e scalare, i resti di lunghezza e il riuso bit-esatto della
rotazione su tre ritardi di codice, con continuità della portante al confine
di un blocco. I casi di qualità includono segnale debole, CW, DC, clipping e
replica spostata di mezzo chip. La sensibilità usa 1024 realizzazioni
accoppiate per IF e riporta la perdita di deflessione del prompt rispetto a
double.

## Riproduzione

Compilare il target Release nelle due varianti descritte nel
[banco DSP](../../../benchmarks/dsp-options/README.md). Salvare in
`build/f5/` le due esecuzioni SSE2 come `run1.csv` e `run2.csv`, quindi
`--verify` su entrambe le varianti come `verify-sse2.csv` e
`verify-reference.csv`.

```powershell
node benchmarks/dsp-options/check-results.cjs build/f5
```

La verifica Windows x64 ha dato righe numeriche uguali con e senza SIMD.
Nel caso di forte interferenza il riscalamento ha saturato 28 095 e
27 894 componenti su 128 000, rispettivamente a IF 0 e 4 MHz.
L'errore vettoriale del prompt in questo caso è 16,1–18,3% con riscalamento
e 1,0–1,2% con la portante a 16 livelli.

| Variante | Perdita di deflessione, IF 0 / 4 MHz |
| --- | ---: |
| CI16 / Q15 | −0,0004 / −0,0006 dB |
| CI8 / Q15 | 0,0034 / 0,0058 dB |
| CI8 / portante a 16 livelli | 0,0224 / 0,0242 dB |
| Rotazione Q15 con uscita CI8 | −0,0017 / 0,0027 dB |
| XOR corretto con portante squadrata | 1,1795 / 0,9742 dB |
| Segni a un bit con portante squadrata | 3,0767 / 2,6397 dB |

Gli scarti negativi sono fluttuazioni dell'esperimento, senza evidenza
di un guadagno di sensibilità.

Due esecuzioni SSE2 hanno prodotto le stesse righe numeriche. Su 16 000
coppie, le mediane del kernel sono 14,0–17,9 µs per CI8/Q15 e 14,3–17,0 µs
per la portante a 16 livelli. Il riscalamento a byte scalare richiede
307,6–391,2 µs. Le misure non mostrano un vantaggio di tempo ripetibile
per la portante a 16 livelli.

SHA-256 del sorgente di prova
`FAE00812D210DB885AB072CA3D1286FD310E2A528D303D16AEB50FCAEEB65A3D`,
del binario SSE2
`0B9650C19C627B9E8489554DA2BDE3E56774FC1BD66122990834A89295A3AAEE`
e del binario di riferimento
`FEE87C64CC1A1C49BB5CA25E9687D7CBB9927EA6945D75100694F5AE86323F34`.

I tempi del banco coprono il kernel di correlazione. Restano da verificare
costo end-to-end, decisioni di acquisizione, falsi allarmi, Galileo e
BeiDou, tracking/PVT e build native Linux e macOS. I candidati rimangono
nel target offline.

## Prodotto Q15 con AVX2

Configurare il banco con `-DDSP_OPTIONS_AVX2=ON` e compilare in Release su x86-64.
Il sorgente AVX2 è compilato separatamente. Il banco avvia i prodotti SSE2 per
impostazione predefinita e accetta `--avx2` quando CPU e sistema operativo
supportano l'uso dei registri YMM e di AVX2.

Eseguire `dsp_options --verify` e `dsp_options --avx2 --verify`. Su Windows x64
con MSVC 19.52 le righe numeriche coincidono. Entrambi i percorsi superano il
confronto esatto con C scalare su lunghezze da 1 a 521 ed estremi dei campioni
CI8 e CI16. I coefficienti Q15 sono limitati a [−32767, +32767].

In quattro coppie di esecuzioni `--timing` e `--avx2 --timing`, in ordine
alternato, su finestre da 16 000 coppie AVX2 ha ridotto il tempo mediano del
kernel del 32–57% per CI16 e del 9–34% per CI8. I tempi assoluti variano tra
le ripetizioni. CSV in `build/dsp-options-avx2-ninja/`. Restano aperte la
misura sulla catena completa e le verifiche native Linux e macOS.
