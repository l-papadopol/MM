# Confronto decoder CW — R7

CER = (sostituzioni + cancellazioni + inserimenti) / caratteri di riferimento, spazi inclusi.
La tabella principale usa gli stessi messaggi per tutti i motori. Ogni messaggio riparte a decoder azzerato.
Native è il vero percorso CwDecoder di MadModem, con impostazioni predefinite e WPM automatici; DeepCW è il modello ufficiale eseguito esternamente.
DeepCW viene eseguito a finestre, mentre MM e Native emettono testo in streaming. DeepCW usa il frontend Python ufficiale e finestre di 5–20 s: gli audio più corti sono completati con silenzio; quelli oltre 20 s sono esclusi dal sottoinsieme comune.
Non vengono forniti al decoder testo atteso o velocità vera. Gli altri motori ricevono gli stessi campioni in blocchi di 997 campioni.

| Prova comune | Messaggi / caratteri | Native | DeepCW | R5 | R6 | R7 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Messaggi ordinari | 120 / 1029 | 16.03% | 0.00% | 14.77% | 10.20% | 4.47% |
| Filtri ricevitore difficili | 120 / 1247 | 16.28% | 0.00% | 75.06% | 26.46% | 15.72% |
| Segnali puliti / lievemente degradati | 120 / 1036 | 10.91% | 0.00% | 43.73% | 8.59% | 2.22% |

Messaggi interamente corretti, nello stesso ordine dei motori della tabella:

- Messaggi ordinari: Native 10/120, DeepCW 120/120, R5 49/120, R6 34/120, R7 87/120.
- Filtri ricevitore difficili: Native 13/120, DeepCW 120/120, R5 5/120, R6 12/120, R7 57/120.
- Segnali puliti / lievemente degradati: Native 49/120, DeepCW 120/120, R5 34/120, R6 42/120, R7 103/120.

## Tutti i record, comprese le finestre non supportate da DeepCW

| Prova | Motore | Messaggi valutati | CER |
| --- | --- | ---: | ---: |
| Messaggi ordinari | Native | 120/120 | 16.03% |
| Messaggi ordinari | DeepCW | 120/120 | 0.00% |
| Messaggi ordinari | R5 | 120/120 | 14.77% |
| Messaggi ordinari | R6 | 120/120 | 10.20% |
| Messaggi ordinari | R7 | 120/120 | 4.47% |
| Filtri ricevitore difficili | Native | 120/120 | 16.28% |
| Filtri ricevitore difficili | DeepCW | 120/120 | 0.00% |
| Filtri ricevitore difficili | R5 | 120/120 | 75.06% |
| Filtri ricevitore difficili | R6 | 120/120 | 26.46% |
| Filtri ricevitore difficili | R7 | 120/120 | 15.72% |
| Segnali puliti / lievemente degradati | Native | 120/120 | 10.91% |
| Segnali puliti / lievemente degradati | DeepCW | 120/120 | 0.00% |
| Segnali puliti / lievemente degradati | R5 | 120/120 | 43.73% |
| Segnali puliti / lievemente degradati | R6 | 120/120 | 8.59% |
| Segnali puliti / lievemente degradati | R7 | 120/120 | 2.22% |

## Rumore e portanti non manipolate

36 registrazioni da 10 s (6 minuti totali), larghezze 70–1.200 Hz e rumore a banda larga; alcune contengono una portante continua. Qui si contano i caratteri falsi, non una CER.

| Motore | Caratteri falsi | Registrazioni con testo |
| --- | ---: | ---: |
| Native | 46 | 2/36 |
| DeepCW | 6 | 5/36 |
| R5 | 102 | 11/36 |
| R6 | 1 | 1/36 |
| R7 | 1 | 1/36 |

## Messaggi lunghi

| Motore | Messaggi supportati | CER |
| --- | ---: | ---: |
| Native | 7/7 | 0.97% |
| DeepCW | 0/7 | N/D |
| R5 | 7/7 | 31.72% |
| R6 | 7/7 | 25.42% |
| R7 | 7/7 | 6.30% |

## Limiti

Test congelato con seed 42900000; 120 messaggi per ciascuno dei tre gruppi, campionamenti 8/16/44,1/48 kHz, velocità 12–36 WPM.
I nuovi parametri R7 sono stati scelti esclusivamente sui precedenti insiemi di sviluppo, prima di generare questo test. Si modificano due bias costanti del modello MM: spazio +5, blank +1,5. Nessun dizionario o correzione dei nominativi.
Le prove sono sintetiche e riflettono il generatore distribuito. La CER su radio reale non è misurata: le due registrazioni WebSDR non hanno trascrizioni verificate e non sono state usate come etichette.
Un risultato sotto il 5% in un gruppo non certifica quel valore per tutte le condizioni. I casi difficili, i messaggi lunghi e i falsi sul rumore restano visibili.
DeepCW resta esterno al prodotto. Il suo modello e il codice ufficiale non sono redistribuiti nel pacchetto MM né usati come insegnante per addestrarlo.
Dettagli per messaggio, hash degli eseguibili e del modello ufficiale sono nel JSON completo.

## Risultato rispetto al 5%

MM R7: 46 errori / 1.029 caratteri ordinari (**4,47%**), 23 / 1.036 puliti (**2,22%**), 196 / 1.247 filtrati difficili (**15,72%**). Su tutti i 360 messaggi: 265 / 3.312 = **8,00%**, quindi l'obiettivo complessivo non è raggiunto. I messaggi lunghi restano al **6,30%**. DeepCW ufficiale non commette errori sui 360 messaggi di questo test, ma produce 6 caratteri nei 6 minuti di negativi: il risultato sintetico non è una garanzia su segnali radio reali.

La modifica R7 è già nel motore **MM Neural CTC** selezionabile dalla UI; il nativo resta predefinito. DeepCW ufficiale è solo un confronto esterno e non diventa una terza voce nella UI. Il TCN compatto di MM non replica l'architettura di DeepCW; i risultati indicano che la sola calibrazione non basta per i casi filtrati. La prossima fase deve intervenire sul modello acustico e sul contesto temporale, convalidando separatamente audio reale trascritto.

## Verifiche della revisione

- Build Linux Qt/Hamlib dell'applicazione completata.
- Regressioni DSP native e integrazione CwDecoder superate.
- Instradamento neurale A/B, cambio motore, ripresa dopo TX, clear e discontinuità audio: superati.
- Parità Python/C++: massimo scarto dei logit 0,00000954; output identico con blocchi 17/997/4096 campioni.
- Il messaggio di controllo resta `CQ CQ DE IZ6NNH TEEST 73`: una E duplicata è ancora presente e non è dichiarata risolta.
- I 13 file originali del tracker/skimmer CW sono identici alla consegna R4. Il confronto usa quel decoder, attraverso il vero CwDecoder.
- Windows/macOS e funzionamento con radio reale non verificati in questa revisione. Il selettore UI e la sua persistenza erano già verificati in R6; la UI non viene modificata da R7.

## Riproducibilità

DeepCW esterno: repository https://github.com/e04/deepcw-engine, commit `8e264d243bbd4467bd19f3f28292219405b47e0e`. Il JSON riporta SHA-256 del modello ONNX e degli eseguibili. Nessun output DeepCW è usato come etichetta di addestramento.

## Osservazioni sulle registrazioni radio

I due WAV sono stati anche suddivisi in sei finestre non sovrapposte fino a 20 s, confrontando Native, MM R7 e DeepCW con reset identico a ogni finestra. Nel primo segmento a 14.025 kHz Native emette `W A E 7 U E E E I ET E H`; nel secondo DeepCW emette `77`. MM R7 non emette testo in queste finestre. Sulla registrazione a 14.005 kHz tutti e tre restano vuoti. Mancando una trascrizione verificata, queste osservazioni non dimostrano né decodifica corretta né miglioramento della sensibilità. Risultati, durate e hash: `CW_NEURAL_R7_RADIO_WINDOWS.json`.
