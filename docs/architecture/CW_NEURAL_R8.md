# Training prolungato CW — R8

Confronto su audio sintetico nuovo, escluso da training, calibrazione e selezione dei checkpoint. CER sui caratteri, spazi inclusi.
Seed test: 72900000. Il checkpoint distribuito è quello selezionato sui dati di sviluppo allo step 7000.

| Prova | Messaggi | Caratteri | R7 | R8 |
| --- | ---: | ---: | ---: | ---: |
| Ordinari | 240 | 2131 | 3.66% | 2.11% |
| Filtri difficili | 240 | 2538 | 15.56% | 11.15% |
| Puliti / lievemente degradati | 240 | 2148 | 2.19% | 1.77% |
| Complessivo | 720 | 6817 | 7.63% | 5.37% |

## Larghezze dei filtri

| Filtro | Messaggi | R7 | R8 |
| --- | ---: | ---: | ---: |
| 100 Hz | 31 | 56.49% | 41.56% |
| 150 Hz | 24 | 18.37% | 9.80% |
| 200 Hz | 46 | 13.26% | 10.43% |
| 300 Hz | 41 | 10.94% | 7.66% |
| 500 Hz | 31 | 3.76% | 2.51% |
| 800 Hz | 30 | 6.83% | 6.83% |
| 1200 Hz | 37 | 7.26% | 4.22% |

## Messaggi lunghi e rumore

| Prova | R7 | R8 |
| --- | ---: | ---: |
| CER su 7 messaggi lunghi | 5.33% | 4.60% |
| Caratteri emessi su 6 minuti di negativi | 1 | 1 |

## Protocollo

Il modello mantiene la stessa architettura e lo stesso frontend di R7. Il training aggiunge 8.900 esempi generati nuovi: 4.000 filtrati, 1.000 non filtrati, 1.500 puliti, 1.000 ordinari, 1.000 negativi e 400 messaggi lunghi casuali. Nessun output DeepCW o WebSDR è usato come etichetta.
Durata impostata: 900 secondi del ciclo di ottimizzazione e validazione, oltre alla preparazione dei dati. A ogni controllo vengono calibrati sui dati di sviluppo due bias costanti, spazio e blank; si conserva il miglior checkpoint che rispetta i limiti di regressione sui segnali puliti/ordinari e sul rumore.
La schedulazione del learning rate dipende dal tempo trascorso: l’esatta sequenza di passi può variare con hardware e carico. Seed, pesi selezionati, cronologia e intervalli dei dati sono distribuiti.
Le statistiche includono intervalli bootstrap al 95%, ricampionando interi messaggi. Descrivono l’incertezza di questo corpus sintetico; non certificano una CER in radio.
Il confronto con il nativo e con DeepCW ufficiale resta documentato nella revisione R7. Questa tabella misura il cambiamento di MM mantenendo identici gli ingressi per R7 e R8.
I risultati reali WebSDR restano privi di trascrizioni verificate. Windows/macOS e sensibilità sulla radio reale restano da verificare.

## Fase aggiuntiva sui QSO e decisione finale

Dopo il primo confronto è stata eseguita una seconda fase di 450 secondi impostati, su 2.900 nuovi esempi, di cui 1.200 QSO con nominativi variabili. I nominativi IZ6NNH, DL1ABC e MM0ABC e le sei frasi complete richieste restano esclusi dai target di questa fase. Nome e QTH dei dati di sviluppo sono distinti da quelli di training.

La selezione ha valutato tutti i checkpoint salvati, calibrando spazio e blank sui dati di sviluppo, dando priorità alla CER QSO e mantenendo puliti/ordinari entro il 5% sullo sviluppo e un limite ai falsi sul rumore. Alcuni checkpoint hanno migliorato la sola CER QSO ma peggiorato il resto. **Nessun checkpoint ha superato il riferimento complessivo: la seconda fase è scartata.** I pesi distribuiti restano quelli della prima fase, selezionati allo step 7.000.

In totale sono stati eseguiti circa 22,5 minuti dei cicli di ottimizzazione e validazione, oltre alla preparazione e ai test. Tempi e passi esatti sono in `CW_NEURAL_R8_TRAINING_RUN.json`. Il primo ciclo ha completato 9.746 passi, il secondo 3.054. Le cronologie e i criteri sono distribuiti nei sorgenti. I checkpoint conservati della prima fase sono gli step 1.000–9.500 a intervalli di 500; il modello scelto e il suo stato grezzo sono inclusi.

Le prove complete CQ/RST/NAME/QTH e dei QSO continui sono in **CW_QSO_R8.md** e nei JSON/CSV associati. Sono il riferimento operativo prioritario: il miglioramento sui brevi messaggi casuali non certifica una prestazione equivalente su un intero QSO. Il nativo resta il motore predefinito; MM Neural CTC resta sperimentale.

## Verifiche

Build Linux Qt/Hamlib e regressioni CwDecoder/A-B/lifecycle superate. Parità Python/C++: massimo scarto dei logit 0,00000763; testo identico con blocchi di 17/997/4096 campioni. Il controllo sintetico di chunk produce `CQ CQ DE IZ6NNH TEST 73`; il controllo con inviluppo Hann dell'integrazione produce `CQ CQDE IZ6NNH TEST 73`, con uno spazio mancante ancora presente. La UI e gli altri modem restano invariati rispetto a R7. Le build native Windows/macOS e la CER su radio reale restano non verificate.
