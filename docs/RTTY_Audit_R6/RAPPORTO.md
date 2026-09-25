# Audit approfondito RTTY, contest e logbook — MadModem 0.5.9-alpha R6

**24 settembre 2026. Esito: la demodulazione di base funziona nelle prove pulite,
ma la catena contest non è ancora pronta per essere definita affidabile per
CQ WW senza correzioni mirate.** Sono emersi difetti riproducibili in TX e AFC,
errori di punteggio e lacune nel passaggio dal QSO al log. I 18 test della R6
non comprendevano questa batteria di prove funzionali RTTY: il loro esito
positivo non dimostrava l'assenza dei difetti descritti qui.

L'analisi riguarda RttyDecoder, RttyTransmitter, RttyMultiDecoder,
DspConditioner, trasporto RX, riavvio dopo TX, AFC, macro, campi contest,
scoring, CTY, ADIF e notifiche UDP. Le funzioni in MainWindow sono state
seguite nei collegamenti fra i componenti. Non è una certificazione RF.

Legenda: **riprodotto** = prova nativa con risultati inclusi; **statico** =
percorso e conseguenza direttamente visibili nel sorgente; **rischio** =
conseguenza dipendente da eventi/carico/dispositivo non riprodotta end-to-end.
P1: da risolvere prima del contest; P2: robustezza, operatività o prestazioni.

## 1. Risultati delle prove native

Il file JSONL contiene 51 osservazioni. Il testo principale di prova è
`CQ CQ TEST IZ6NNH IZ6NNH 599 15 TU CQ `, con shift 170 Hz e 45,45 baud.

| Prova | Risultato sulla R6 |
|---|---|
| Encoder indipendente, segnale pulito, 44,1/48/96 kHz | Testo esatto in tutti e tre i casi |
| Trasmettitore MM → decoder MM, stesse frequenze | Testo esatto nei tre casi |
| Condizionatore ordinario; filtri Mark/Space opzionali | Testo esatto nelle sei prove pulite |
| Blocchi da 137 e 4096 campioni a 48 kHz | Testo identico |
| Offset statico ±30 e ±60 Hz, AFC escluso | Testo esatto sul messaggio scelto |
| Offset ±100 Hz | Nessun testo |
| Baud effettivi 44,5 / 45 / 46 / 46,5, decoder a 45,45 | Testo esatto sul messaggio scelto |
| AWGN broadband +12/+6/0/−6 dB | Testo esatto; −12 dB: nessun testo |
| Solo rumore per 30 s | Nessun carattere spurio in questa realizzazione |
| Polaritá inversa, riconoscimento automatico | Aggancio, ma sette operazioni di edit rispetto al testo atteso; prefisso perso/alterato |
| `599`, poi una nuova trasmissione `TEST` senza reset RX | Ricevuto `599`, poi **`53'5`** |
| AFC: offset comune +15 Hz | Lo shift stimato varia fino a **155–177 Hz** nei casi eseguiti |
| Multi-decode: due stazioni sintetiche | Callout duplicati; nel caso 96 kHz enhanced al termine compare solo una delle due stazioni |
| Multi-decode enhanced + second pass | Picco di elaborazione blocco circa **106–111 ms** |
| Append ADIF su 1.000 / 10.000 record | Circa **13 / 137 ms**, prima dei refresh GUI e scoring |

I risultati AWGN non sono una misura di sensibilità comparabile con MMTTY,
2Tone o fldigi. Mancano fading selettivo, multipath, QRM adiacente forte,
impulsi e registrazioni reali con trascrizione di riferimento.

## 2. P1 — Avvio TX senza LTRS: il corrispondente può leggere cifre al posto delle lettere

**Riprodotto.** `modems/rtty/tx/RttyTransmitter.cpp`, `buildSegments()` e
`ensureShift()`. Lo stato interno viene inizializzato a lettere, quindi
`ensureShift(false)` non emette alcun codice LTRS. Il trasmettitore presume che
anche il decoder remoto sia già in lettere.

Dopo una trasmissione terminata in FIGS, il successivo messaggio alfabetico può
restare in FIGS. La prova con i componenti reali `599` → nuova istanza TX con
`TEST` restituisce `53'5`. Una portante Mark non azzera il registro lettere/cifre.
Il caso è rilevante per la macro Exchange, che termina con la zona numerica.

Correzione: LTRS esplicito all'inizio di ogni nuova trasmissione, indipendente
dallo stato locale. Verificare messaggi consecutivi, spazi, CR/LF, numeri e
ripresa dopo interruzione. L'eventuale USOS va definito come opzione interoperabile,
non usato per nascondere la mancanza del codice di shift.

## 3. P1 — AFC RX modifica anche lo shift utilizzato dal TX

**Riprodotto negli helper, collegamento al TX statico.** In
`mainwindow.cpp`, `updateTextModeAfc()` cerca Mark e Space indipendentemente;
`retuneRttyFromAfc()` scrive entrambi nei controlli condivisi. La costruzione di
`RttyTransmitter` in `buildCurrentTxModulator()` legge gli stessi controlli.

Con due toni entrambi spostati di +15 Hz, shift reale 170 Hz, il ciclo estratto
porta a 155–170 Hz con blocchi 1024/48 kHz e 163–177 Hz con 4096/48 kHz. A 96 kHz
sono stati osservati intervalli 161–170 e 156–173 Hz. I blocchi corti contengono
spesso un solo tono dominante: due massimi indipendenti non sono una stima
robusta dello shift. Il controllo ±20 Hz viene inoltre ricentrato a ogni passo,
non rappresenta un limite assoluto rispetto alla sintonia scelta inizialmente.

Correzione: shift contest fissato a 170 Hz; AFC su un offset comune della coppia,
qualità e isteresi; parametri RX separati dai parametri TX. Il TX non deve
cambiare shift o frequenza per un inseguimento RX non confermato dall'operatore.
L'AFC non lavora su blocchi inferiori a 1024 campioni: la cattura a dimensioni
variabili deve essere aggregata a una finestra stabile.

## 4. P1 — R6 annulla la ripresa rapida RTTY dopo la propria trasmissione

**Statico con predicato riprodotto.** `startRx()` invoca
`resumeAfterLocalTransmit()` per mantenere il contesto; `AudioEngine` incrementa
`captureGeneration` al nuovo avvio. Al primo blocco, `handleRxAudioBlock()` chiama
`m_rxContinuity.accept()` e quindi `RttyDecoder::reset()` e reset del multi-decode.
Il confronto fra generazioni 1 e 2 è riprodotto nel test: scatta il reset completo.

È un'interazione introdotta dall'hardening R6, da correggere: distinguere il
riavvio atteso dopo TX da una perdita imprevista di audio. Serve una transizione
esplicita che aggiorni il riferimento di continuità senza annullare il recupero
parziale. I gap imprevisti devono continuare a invalidare il frame.

## 5. P1 — Zona CQ ricevuta ignorata nel punteggio

**Statico; comportamento CTY riprodotto.** Il profilo `cq_ww_rtty` conserva
`CQZONE` ricevuta ma imposta il moltiplicatore a `source: cq_zone`.
`refreshRttyContestScore()` prende `dx.entity.cqZone`, non il campo del QSO.
Il parser CTY scarta inoltre gli override `(zona)` nei token.

Nelle prove `K1ABC` e `K6ABC` restituiscono entrambi la zona predefinita 5:
questo dimostra che non va usato il prefisso per sostituire il dato ricevuto,
non stabilisce quale sia la posizione reale di quei nominativi. Anche la zona
inviata viene derivata automaticamente dal CTY e non ha un editor dedicato.

Correzione: zona ricevuta validata 1–40 e registrata nel QSO, moltiplicatore da
quel valore; zona propria esplicita nel profilo di stazione/contest. Il database
può suggerire o segnalare una discrepanza, senza sovrascrivere il dato copiato.

## 6. P1 — Identità paese CQ WW confusa con il numero DXCC

**Statico, lookup nativi riprodotti.** Il CTY contiene correttamente voci distinte
per Italia, Sicilia e Italia africana, ma tutte hanno DXCC `248`.
`rttyContestConditionMatches(same_country)` e il moltiplicatore `dxcc` confrontano
quel numero. Ne derivano collisioni nei moltiplicatori e nella regola dei punti.

Esempio per IZ6NNH: `IG9ABC` risulta Africa/zona 33 ma DXCC 248. La prima regola
same_country gli attribuisce un punto anziché arrivare alla regola fra
continenti. Anche `IT9ABC` collide con I nei moltiplicatori. Altre entità WAE
con DXCC condiviso richiedono la stessa distinzione.

Correzione: identità del paese del contest separata dall'identità DXCC ADIF;
regole e fixture per I/IT9/IG9, GM/Shetland, TA/TA1. Non modificare globalmente
il numero DXCC del logbook per aggiustare il conteggio del contest.

## 7. P1 — QSO salvabile con scambio incompleto o dati del precedente corrispondente

**Statico.** `addQsoToLogFromForm()` verifica il nominativo non vuoto, ma non
applica `field.required`, regex e completezza dei campi ricevuti. `required`
è letto dal parser delle regole e non consumato da una validazione di salvataggio.
La regex della zona accetta 00 e 99. Gli editor RX non vengono ripuliti alla
registrazione del QSO o al cambio nominativo; l'autofill salta i valori mancanti
invece di invalidare quelli già presenti.

Conseguenza: dopo un QSO completo, un nuovo nominativo può ereditare zona/QTH
precedenti. Lo scoring conta anche record incompleti e inserisce il dupe prima
di valutare la qualità dello scambio: una prima registrazione errata può
mascherare una registrazione successiva corretta.

Correzione: oggetto QSO in lavorazione separato dagli editor; provenienza dei
valori e invalidazione al cambio partner; validazione dei campi condizionali,
range e banda; salvataggio atomico del QSO confermato. I duplicati vanno
conservati con stato esplicito, evitando la doppia registrazione accidentale.

## 8. P1 — Stato/provincia non riconosciuti dal parser CQ WW

**Statico.** Il campo QTH del profilo è `type: text`, senza regex.
`processRttyContestRxLine()` e `fillRttyContestFieldFromClick()` non implementano
il caso text. Il campo quindi è digitabile manualmente, ma non viene compilato
né dal riconoscimento automatico né dal clic. È inoltre marcato opzionale anche
quando la condizione USA/Canada è vera.

Correzione: dominio enumerato dei QTH ammessi, obbligatorietà condizionale,
riconoscimento contestuale e distinzione fra provincia ricevuta e QTH generico.
Evitare di interpretare `DE` come Delaware senza contesto radio.

## 9. P1 — Macro, delimitazione RX e identità del corrispondente non formano una transazione

**Statico.** L'autofill viene eseguito solo a CR/LF, mentre le sei macro CQ WW
non contengono terminatori. `startTextModeTx()` usa inoltre `.trimmed()`, che
rimuoverebbe anche CR/LF aggiunti soltanto ai margini della macro.
`m_rttyContestLastRxLine` conserva fino a 256 caratteri e non viene svuotato
alla fine del proprio TX o al salvataggio del QSO.

Il parser cerca il proprio call con `contains()` e sceglie il primo altro call:
non riconosce rigorosamente destinatario/mittente e confini del token. Esempio:
una riga con `IZ6NNH/P` può soddisfare il controllo di presenza di `IZ6NNH` e
poi essere scambiata per un altro corrispondente. Call portatili con prefisso
prima dello slash possono essere estratti solo in parte dalla regex.

Correzione: conservare i separatori intenzionali delle macro, parser incrementale
con confini chiari e destinatario validato, conferma operatore per cambiare
partner, isolamento del buffer fra QSO. Nessun riempimento silenzioso da righe
appartenenti a uno scambio precedente.

## 10. P1 — Frequenza e identità di stazione assenti dal QSO testuale salvato

**Statico.** `LogbookEntry` dispone di `freq`, `operatorCall`,
`stationCallsign`, `utcEnd`, ma `addQsoToLogFromForm()` non li popola.
Registra la banda e l'UTC del clic. Il pacchetto UDP tipo 5 usa invece la
frequenza CAT corrente e il nominativo di stazione dal contesto.

Quindi ADIF persistente e notifica UDP non rappresentano necessariamente lo
stesso insieme di dati. Manca uno snapshot della frequenza operativa quando
si effettua il QSO; un successivo cambio CAT può alterare ciò che viene notificato.

Correzione: un solo record completo e immutabile, con frequenza verificata,
identità e tempi; ADIF, scoring e UDP devono derivare da quel record confermato.
Campi standard CQZ/STATE vanno mappati oltre ai tag applicativi quando appropriato.

## 11. P1 — Esportazione Cabrillo non implementata nel percorso esaminato

**Statico.** Il sorgente usa `cabrillo_id` per CONTEST_ID e riconoscimento del
profilo. Non contiene un writer Cabrillo: il dialogo logbook esporta ADIF,
CSV e documenti/stampa. La presenza di CQ-WW-RTTY nel JSON non equivale a
un export valido per la consegna del contest.

Correzione: writer Cabrillo 3.0 specifico, header categoria/operatore/stazione,
righe QSO con frequenza, UTC e scambi, selezione della sessione, nessuna
invenzione di campi mancanti. Confronto con fixture della struttura ufficiale.
Un convertitore esterno resta una possibilità, ma i dati vanno prima conservati
correttamente nell'ADIF; la conversione non ricrea dati mai registrati.

## 12. P2 — Multi-decode produce più corsie per una sola stazione

**Riprodotto.** Due segnali puliti hanno prodotto cinque o sei callout, con
ripetizioni di IZ6NNH e DL1ABC. `scanCandidates()` non seleziona prima i massimi
locali della coppia; il raggruppamento usa una distanza fissa di 18 Hz.
`rebuildCallouts()` non deduplica per sorgente/nominativo. In enhanced a 96 kHz,
alla fine della sequenza di prova appare soltanto IZ6NNH.

Le finestre sono espresse in campioni, quindi la risoluzione e il tratto di
messaggio osservato cambiano con la frequenza di campionamento. Il second pass
riesegue Goertzel sull'intero spettro e scarta poi le bande forti: non sottrae
i segnali forti né esegue una vera cancellazione delle interferenze.

Correzione: canalizzazione a frequenza interna unica, rilevamento dei massimi
locali, associazione stabile delle corsie, deduplicazione spaziale e storico
con scadenza. Gli auto-reverse request dei decoder secondari non sono collegati
a un gestore: la doppia ipotesi viene calcolata senza applicare la richiesta.

## 13. P2 — Carico e UI: lo scanner precede il decoder selezionato sul thread GUI

**Ordine statico, costi misurati.** `handleRxAudioBlock()` esegue prima
`RttyMultiDecoder::processAudioBlock()` e poi il decoder selezionato, entrambi
nel thread GUI. In enhanced/second pass il blocco più lento ha richiesto
106–111 ms. Il limite CPU interviene sopra 140 ms e non limita il costo degli
altri decoder già attivi.

`goertzelPower()` ricalcola il coseno della finestra per ogni campione e per ogni
frequenza analizzata. I decoder secondari costruiscono anche tracce dello scope
e notifiche non usate. La R6 protegge la dimensione della coda, ma non impedisce
che il lavoro GUI provochi drop e successivo reset.

Correzione: worker RTTY dedicato, priorità al canale selezionato, scanner
separato con budget, FFT/finestra precomputata, telemetria e scope a frequenza
limitata. Le prove non dimostrano un drop effettivo a questi tempi su ogni PC.

## 14. P2 — Log e scoring bloccano ancora la stessa GUI

**Misurato in parte.** Append R6: circa 13 ms con 1.000 record e 137 ms con
10.000 record piccoli. Il codice legge, verifica hash e riscrive l'intero ADIF;
la copia del vettore e la serializzazione restano proporzionali al log.
Dopo il salvataggio esegue highlight, mappe, scoring e una finestra modale di
conferma. Lo scoring rilegge tutti i record, ordina la sessione e ricalcola tutto.
Ogni modifica di un campo RX richiama lo scoring.

Correzione: coda di persistenza con conferma, indici incrementali per sessione,
banda/call e moltiplicatori; messaggio di successo non modale. Preservare le
protezioni R6 contro perdita di dati e scritture concorrenti. Un errore di disco
non deve incrementare seriale/punteggio né inviare il QSO al logger esterno.
Questo ultimo ordine è già corretto nel percorso attuale.

## 15. P2 — Sessione contest persistente, ma non legata automaticamente all'edizione

**Statico.** `ensureRttyContestSession()` riusa UUID e data per profilo finché
l'operatore non richiede una nuova sessione. Il profilo CQ WW non ha periodi;
lo scoring filtra per UUID, non per l'intervallo dell'edizione. Può quindi
includere prove precedenti e QSO fuori contest se restano nella stessa sessione.
Le bande dichiarate nel profilo non vengono applicate nel salvataggio/scoring.

Correzione: edizione e intervallo UTC espliciti, separazione del log di prova,
validazione delle bande e gestione chiara dei record fuori periodo. Il dupe
per nominativo+banda nella stessa sessione è una buona base, non va sostituito
con il semplice “mai lavorato prima” del log generale.

## 16. P2 — UDP invia due notifiche di logging per lo stesso QSO

**Rischio di interoperabilità, non duplicazione dimostrata su un logger reale.**
`sendQsoLoggedBundle()` invia heartbeat, QSO Logged tipo 5 e Logged ADIF tipo 12.
Un ricevitore che registra entrambi può duplicare il QSO. L'invio riuscito
indica accettazione del datagramma dallo stack locale, non conferma del logger.
Non c'è outbox persistente o riconciliazione. Sono accettati IP letterali e
localhost, non nomi DNS generici.

Correzione: profilo di compatibilità del logger e scelta esplicita del messaggio,
identificatore locale del QSO, registro degli invii e reinvio controllato.
Provare il server UDP che l'operatore usa davvero, verificando **un solo QSO**
con tutti i campi anche dopo STOP, errore di rete e ripetizione del clic.

## 17. P2 — CTY e nominativi portatili richiedono un resolver dedicato al contest

**Riprodotto nel lookup.** `IZ6NNH/EA8` viene risolto come Italia; il lookup
prova l'intero call e può accettare il prefisso iniziale prima di valutare il
suffisso geografico. `IZ6NNH/MM` viene ugualmente classificato come Italia e il
calcolo dei moltiplicatori non ha un'esclusione specifica per maritime mobile.

Correzione: riconoscimento strutturato di prefisso/suffisso geografico e stato
MM; separazione fra suggerimento CTY, identità contest e scambio ricevuto.
Testare anche call speciali e override del database, senza dedurre
automaticamente la zona operativa da una sola cifra del nominativo.

## 18. P2 — Algoritmi e impostazioni: evitare promesse non sostenute dai test

Il decoder è un demodulatore a due mixer I/Q e filtri esponenziali, gate con
isteresi e UART asincrona a decisione centrale. Ha una valutazione statistica
di due polarità. Non è un motore vuoto: le prove pulite lo dimostrano.

Non risultano una decisione soft multi-ipotesi del carattere, correzione del
fading selettivo o un clock adattivo con metrica di qualità del frame.
Il “matched filter” del condizionatore è realizzato con coppie di biquad sui
toni e miscelazione del segnale: la denominazione non dimostra da sola una
prestazione ottima. Le costanti del gate e del rumore sono in parte per-campione,
quindi i tempi cambiano con il sample rate. La UI ricostruisce e resetta il
modem in `applyRttySettings()` anche per cambi di opzioni non strettamente DSP.

Serve una batteria con testi e WAV di riferimento, tassi di errore dei caratteri,
falsi nominativi e acquisizione dopo TX, prima di introdurre altri algoritmi.
Gli script RTTY attuali verificano soprattutto la presenza di stringhe nel
sorgente e la struttura delle regole; non proteggono questi comportamenti.

## 19. Requisiti CQ WW verificati e conseguenze pratiche

Fonte ufficiale consultata: [regolamento 2026](https://cqwwrtty.com/rules.htm).
Il contest è il **26–27 settembre 2026**, dalle 00:00 UTC di sabato alle
23:59:59 UTC di domenica; termine ordinario del log: 29 settembre, 23:59 UTC.
Parametri previsti: ITA2, 45,45 baud, shift 170 Hz; bande 80/40/20/15/10 m.
Scambio: RST e zona CQ, con QTH aggiuntivo per USA continentali/Canada.
Il regolamento distingue moltiplicatori zona, paese contest e QTH per banda.
Il multidecoder wideband è assistenza: va considerato nella categoria scelta.

Per evitare dipendenze indesiderate dall'automazione, chiamata del partner,
cambi di frequenza e logging devono restare iniziati dall'operatore, secondo
il regolamento. Non propongo autolog autonomo del QSO.

Per il formato di uscita consultati anche:
[istruzioni Cabrillo](https://cqwwrtty.com/cabrillo.htm) e
[invio log / conversione ADIF](https://cqwwrtty.com/logs.htm).
Il file finale deve poter essere verificato prima dell'invio; il pacchetto di
questo audit non invia log né dati a servizi esterni.

## 20. Ordine degli interventi per arrivare a una build da contest

| Priorità | Intervento | Criterio di accettazione |
|---|---|---|
| 1 | LTRS iniziale e frame TX | Sequenze numeri→lettere corrette anche con decoder remoto già in FIGS |
| 1 | AFC a shift fisso, RX/TX separati | 170 Hz TX invarianti con offset, rumore e cambio dimensione blocchi |
| 1 | Ripresa RX R6 | Riavvio atteso conserva il contesto; gap inatteso invalida il frame |
| 1 | QSO completo e campi CQ WW | Nessuna zona/QTH ereditata; dato ricevuto validato e usato nello scoring |
| 1 | Identità contest e Cabrillo | Fixture I/IT9/IG9, zone USA, QTH, portatili e file di uscita verificato |
| 2 | Log/UDP come singola transazione | Una registrazione locale e una esterna, nessun avanzamento dopo errore |
| 2 | Worker e indici incrementali | RX non dipende da repaint, scansione larga o riscrittura ADIF |
| 2 | Robustezza modem | Corpus indipendente con QRM/fading, falsi call, deriva e transizioni RX/TX |

Prima del contest concentrerei il rilascio sui difetti riproducibili e sulla
correttezza del log. Cambiare simultaneamente tutto il demodulatore renderebbe
più difficile attribuire eventuali regressioni. La migrazione del worker e i
miglioramenti di sensibilità vanno misurati con lo stesso corpus.

**Stato della consegna:** audit e banco di prova completati. Le correzioni RTTY
elencate sono ancora da implementare e verificare. La R6.1 allegata corregge il
layout Windows segnalato, non include silenziosamente una riscrittura del modem.
