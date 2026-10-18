# Prove CW con messaggi completi e QSO

Le prove includono `CQ CQ CQ DE IZ6NNH IZ6NNH PSE K`, `UR 599 599 BK`, risposte con nominativi ripetuti, NAME/QTH, rapporti 559/5NN e chiusure TNX QSO 73 SK.
72 messaggi: sei sequenze a 12, 20, 28 e 36 WPM in tre condizioni. Altri tre test mantengono il decoder attivo durante un QSO intero, con quattro passaggi a velocità diverse e pause di tre secondi.
Nessuna velocità o testo di riferimento viene fornito ai decoder. Il nativo usa l’acquisizione automatica standard. CER sui caratteri, spazi inclusi.

| Prova | Numero | Nativo | MM R7 | MM R8 |
| --- | ---: | ---: | ---: | ---: |
| Puliti | 24 | 1.23% | 11.33% | 11.70% |
| Filtro 300 Hz | 24 | 2.59% | 13.79% | 7.64% |
| Filtro 100 Hz | 24 | 3.08% | 64.41% | 48.89% |
| QSO continui, pause di 3 s | 3 | 3.98% | 23.48% | 17.40% |

## Messaggi, nominativi e rapporti esatti

| Prova | Motore | Messaggi esatti | Nominativi esatti | Rapporti esatti |
| --- | --- | ---: | ---: | ---: |
| Puliti | Native | 19/24 | 32/36 | 32/32 |
| Puliti | R7 | 3/24 | 17/36 | 27/32 |
| Puliti | R8 | 3/24 | 11/36 | 24/32 |
| Filtro 300 Hz | Native | 6/24 | 26/36 | 32/32 |
| Filtro 300 Hz | R7 | 2/24 | 18/36 | 25/32 |
| Filtro 300 Hz | R8 | 6/24 | 20/36 | 26/32 |
| Filtro 100 Hz | Native | 2/24 | 25/36 | 32/32 |
| Filtro 100 Hz | R7 | 0/24 | 0/36 | 0/32 |
| Filtro 100 Hz | R8 | 0/24 | 0/36 | 6/32 |
| QSO continui, pause di 3 s | Native | 1/3 | 16/21 | 12/12 |
| QSO continui, pause di 3 s | R7 | 0/3 | 12/21 | 8/12 |
| QSO continui, pause di 3 s | R8 | 0/3 | 8/21 | 8/12 |

Un nominativo o rapporto si conta una volta per ciascuna occorrenza attesa. La valutazione allinea le parole conservandone l’ordine: parole unite e ripetizioni perse contano come errori, anche quando una parte dei caratteri è corretta.

## DeepCW ufficiale: sottoinsieme comune

L’esempio ufficiale accetta finestre di 5–20 s. Sono supportati 45 dei 72 messaggi; le sequenze più lente/lunghe e i tre QSO continui restano fuori dal suo limite. Per questo confronto tutti i motori usano esattamente gli stessi 45 messaggi.

| Motore | Messaggi | CER |
| --- | ---: | ---: |
| Native | 45 | 2.47% |
| R7 | 45 | 26.14% |
| R8 | 45 | 17.83% |
| DeepCW | 45 | 0.00% |

## Esempi a 20 WPM, audio pulito

Atteso: `CQ CQ CQ DE IZ6NNH IZ6NNH PSE K`

- Native: `CQ CQ CQ DE IZ6NNH IZ6NNH PSE K`
- R7: `CQ CQ CQ DE IZ6NNH IZ6NNH PSEK`
- R8: `CQ CQ CQ DE IZ6NNH IZ6NNH PSEK`
- DeepCW: `CQ CQ CQ DE IZ6NNH IZ6NNH PSE K`

Atteso: `UR 599 599 BK`

- Native: `UR 599 599 BK`
- R7: `UR 599 599 BK`
- R8: `UR 599 599 BK`
- DeepCW: `UR 599 599 BK`

## QSO nuovi con semi disgiunti e nome/QTH distinti

I messaggi fissi sopra sono prove funzionali richieste dall’utente e hanno evidenziato il limite della prima fase. Per verificare la generalizzazione si aggiunge un test nuovo con semi disgiunti e un vocabolario NAME/QTH distinto da training e sviluppo. Questo test non sceglie i pesi.

| Motore | Messaggi | CER | Messaggi esatti | Nominativi esatti | Rapporti esatti |
| --- | ---: | ---: | ---: | ---: | ---: |
| R7 | 120 | 13.91% | 28/120 | 84/136 | 56/74 |
| R8_A | 120 | 10.80% | 29/120 | 81/136 | 59/74 |
| R8 | 120 | 10.80% | 29/120 | 81/136 | 59/74 |

R8_A indica il candidato dopo i primi 15 minuti; R8 indica i pesi effettivamente consegnati. Se la seconda fase non supera la selezione, i due modelli coincidono.
Tutte le prove sono audio generato con riferimento noto. Le registrazioni WebSDR restano prive di trascrizione verificata: non esiste ancora una CER misurata sulla radio reale. Risultati completi e testi decodificati sono nei JSON allegati.

Audit dei nominativi generati: 115 distinti nel test; 1 compare anche nel corpus della fase QSO scartata. Quella fase non contribuisce ai pesi consegnati. I messaggi completi e i semi restano distinti; non si dichiara una separazione assoluta di tutte le identità dei nominativi.
