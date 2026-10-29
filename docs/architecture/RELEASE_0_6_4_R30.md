# MadModem 0.6.4 — sorgenti R30

La R30 completa il percorso nativo FT8/FT4 recuperato dalla chat precedente: il port MSHV 2.76.7 opera accanto al ricevitore MadModem, con risultati incrementali, lavoro limitato dalle deadline e conservazione delle decodifiche anticipate. Restano disponibili la correzione Doppler, le opzioni GPU, le modalità precedenti, CAT/PTT, sequencer, coda QSO e logbook. La versione pubblica rimane 0.6.4; il marcatore sorgenti è `0.6.4-source-r30-reference-ft-recovery`.

## Confronto misurato sul WAV del tester

Misure eseguite con la build Linux Qt5 Release. WAV mono PCM16 a 16 kHz, estratto con il ricampionatore dell'applicazione a partire da 5,5 s. Tre esecuzioni per scenario, stessa topologia di otto worker. La baseline disattiva esclusivamente i due agganci di recupero R30 e mantiene il resto del ricevitore, comprese le correzioni R29.

| Scenario | Baseline | R30 | Corrispondenze con i 66 messaggi JTDX | Risultati ulteriori senza conferma |
| --- | ---: | ---: | ---: | ---: |
| Solo boundary | 41 | 41 | 41 | 0 |
| Gate anticipato + boundary finale | 41 | 52 | 48 | 4 |
| Analisi offline | 36 | 60 | 55 | 5 |

I conteggi sono identici nelle tre ripetizioni di ciascuno scenario; nessun messaggio della baseline è andato perso. Il confronto usa il testo esatto, eliminando soltanto le annotazioni di paese della lista fornita dal tester. Gli hash irrisolti non vengono trattati come nominativi risolti.

Il tempo finale del recupero viene contenuto dalla deadline UTC. Nel campione, il gate usa circa 1,45 s prima del boundary e il lavoro finale circa 1,1 s; sono misure di questa macchina, con controlli cooperativi e normali variazioni dello scheduler. Il solo boundary spesso esaurisce il tempo nel percorso esistente: il guadagno richiede il gate anticipato. Le prime decodifiche continuano a essere pubblicate durante il lavoro. I risultati offline richiedono circa 3,7–4,0 s nelle tre prove e non vanno presentati come risultati live.

**La parità con JTDX non è dimostrata.** Offline restano 11 messaggi della lista del tester non recuperati, mentre cinque righe ulteriori non hanno conferma indipendente. JTDX, MSHV e WSJT-Z non sono stati eseguiti tutti come applicazioni complete sullo stesso corpus e con le stesse impostazioni. L'audit confronta i sorgenti identificati dagli hash, non una supposta versione del tester.

## Compilazione e regressioni

- Linux Qt5 Release: applicazione completa compilata, 53/53 CTest superati. La suite comprende FT, sequencer, CAT/worker, logbook, CW, RTTY, MSK144, Q65, SSTV, Hell e meteo-fax.
- Linux Qt6 Release: build pulita dell’esatto albero di consegna, 53/53 CTest superati (117,04 s). La configurazione applica lo stesso requisito di inizializzazione automatica degli scratch del port. Restano warning di deprecazione Qt6 in codice legacy/importato.
- Nuovo motore nativo (comprende una prova TX concorrente sul codec condiviso): FT8/FT4 su segnale noto, silenzio e rumore; cache del medesimo input, scadenza del boundary, cambio generazione, cancellazione attiva, FFT reale/complessa e istanze FT4 concorrenti.
- ASan/UBSan: nuovi sorgenti di recupero strumentati, prove native superate anche sulla build Qt6 finale e con TX concorrente. Il controllo perdite è separato: LeakSanitizer non riesce a ispezionare i thread `/proc` in questo ambiente; il runner offre `--leaks` per ambienti compatibili.
- Doppler: 46/46 casi sintetici FT8/FT4 superati, inclusi ±20 Hz/s, soglia RF, disabilitazione, rumore e tracciamento del periodo successivo. Il metodo `recoverDriftingSignal` è byte per byte identico a quello nello ZIP R30 allegato.
- Corpus aggiuntivo: sette registrazioni FT8 e tre campioni sintetici FT4, totale 124 -> 164 messaggi, nessuna perdita di un risultato baseline. Le aggiunte nelle registrazioni non sono una misura autonoma dei falsi positivi.
- Risposta applicativa: pubblicazione asincrona, sei messaggi unici, singola chiusura finale, aggiornamenti QSO durante il lavoro, stato ultimo tentativo/73, timeout e impostazioni Doppler/QRZ superati. La prova sintetica di risposta usa il percorso precedente per le scadenze storiche e non certifica il PTT radio reale.

Il controllo Q65 è stato corretto per verificare il collegamento del target Q65, mantenendo la sua indipendenza da FFTW. Il test dell'editor SSTV include esplicitamente `QIODevice` per Qt6. La configurazione e la build GitHub includono FFTW3 su Ubuntu, Fedora, MSYS2 UCRT64 e Homebrew.

## Riproduzione e file

- `FT_DECODER_COMPARISON_R30.md`: confronto dei tre progetti e adattamenti.
- `FT_R30_SOURCE_INVENTORY.json`: riferimenti e hash dei sorgenti analizzati.
- `FT_R30_RESULTS.json`: risultati, messaggi mancanti, risultati ulteriori e limiti.
- `FT_R30_MESSAGE_COMPARISON.csv`: confronto messaggio per messaggio, scenario e ripetizione.
- `r30-validation/`: log build/test, risultati Doppler, corpus e JSONL completi.
- `../platform/BUILD_R30.md`: dipendenze e comandi di verifica.

## Verifiche sul campo ancora necessarie

Compilazione nativa Windows e macOS attraverso i workflow, pacchetti su sistemi reali, GPU fisica, radio/CAT/PTT e commutazione RX/TX effettiva. Per FT4 serve inoltre un corpus reale affollato con risultati di riferimento: le prove sintetiche dimostrano l'integrazione e specifiche regressioni, non la sensibilità assoluta o la parità generale. Il ramo aggiuntivo non applica automaticamente il Doppler al proprio segnale: il recupero Doppler dedicato MadModem continua a operare nel ricevitore esistente.
