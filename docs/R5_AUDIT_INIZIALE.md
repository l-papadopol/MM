# MadModem 0.5.9-alpha R5 — analisi trasversale iniziale

Data: 16 settembre 2026.

## Versione e limiti della verifica

Archivio: `MadModem_0_5_9-alpha_FULL_SOURCE_R5.zip`, datato 4 settembre 2026, ultima alpha individuata fra gli archivi del progetto. Revisione interna: `0.5.9-alpha-source-r5-real-rotator-peak-tracking`. Il numero pubblico è confermato da `MadModemVersion.h`.

SHA-256 archivio: `acc76128663ad5b4d1b83e44e1ac0e2f00f6ef42f9f91824a68336b994f87d53`.

Inventario: 186 file C++/header fuori da third_party, 94.660 righe complessive, inclusi test; `mainwindow.cpp` 25.115 righe e `mainwindow.h` 2.232. I sorgenti sono rimasti invariati.

Questa è una prima analisi trasversale dei percorsi applicativi, non una certificazione di tutte le 94.660 righe. Sono stati letti i collegamenti tra acquisizione, dispatcher, decoder, scheduler, TX, CAT, logbook e UDP, e parti di DSP, rotatore e test. Non sono stati eseguiti l'applicazione, test radio, benchmark o regressioni native: nell'ambiente CMake e pkg-config non sono disponibili e non è stato predisposto un toolchain Qt. Le osservazioni sul comportamento dei backend audio richiedono conferma strumentale.

Legenda: **difetto statico** = la logica problematica è visibile nel codice; **rischio architetturale** = percorso verificato, conseguenza dipendente da carico/eventi/backend; **ottimizzazione** = lavoro evitabile identificato, costo non misurato. P1 alta, P2 media, P3 bassa priorità.

## Rilievi

### 1. P1 — Fine TX prima della conferma di riproduzione audio

**Difetto statico con impatto dipendente dal backend.** `audio/TxAudioEngine.cpp`, `TxOutputDevice::readData()` circa righe 110–158 e `handleDeviceFinished()` 375–384: la fine è accodata quando il modulatore ha generato l'ultimo buffer. La gestione successiva invoca `stopOutput()`, che ferma l'uscita; `MainWindow::handleTxStopped()` rilascia il PTT. Non viene verificato che il dispositivo abbia riprodotto i campioni accodati.

FT8/FT4, MSK144 e Q65 restituiscono zero da `trailingSilenceSamples()`. Generazione terminata e riproduzione terminata sono eventi diversi: è possibile troncare la coda del frame o togliere PTT troppo presto. Anche negli altri modi il contatore della coda sottrae `requestedSamples`, includendo i campioni utili generati nell'ultima chiamata: con 4.096 campioni tutti utili e coda richiesta di 16.000, restano 11.904 pur non avendo ancora prodotto silenzio.

Intervento: un unico ciclo TX che distingua produzione, svuotamento del buffer audio e rilascio PTT. Contare solo il silenzio effettivamente prodotto; verificare la fine sul backend, senza affidarsi a un ritardo fisso. Prova necessaria: registrare in loopback gli ultimi simboli di tutti i modi, variando buffer e backend.

### 2. P1 — Errori del backend TX non osservati

**Difetto statico.** `audio/TxAudioEngine.cpp:312–323`: dopo `QAudioSink/QAudioOutput::start()` il codice assegna `m_running = true`, emette `started()` e restituisce successo. Non connette `stateChanged` e non controlla l'errore del backend. Le verifiche preventive del formato e di `TxOutputDevice::open()` non equivalgono all'avvio riuscito della scheda audio.

Un errore all'avvio o la rimozione della scheda può lasciare l'applicazione nello stato TX senza audio, senza attivare il normale percorso di errore. Verificare anche il rilascio PTT su errore effettivo del sink, indipendentemente dai watchdog specifici del modo.

### 3. P1 — Aliasing nel ricampionamento MSK144/Q65

**Difetto statico, conseguenza numerica verificata.** `dsp/text/LinearResampler.cpp:25–72` interpola ma non filtra prima della decimazione. `Msk144Decoder.cpp:290` e `Q65Decoder.cpp:142` lo usano direttamente sull'audio grezzo; `mainwindow.cpp:13571–13588` inoltra questi modi prima di `conditionAudioForActiveMode()`.

A 48→12 kHz, il rapporto intero porta a prendere un campione ogni quattro. Un tono a 11 kHz si ripiega a 1 kHz senza attenuazione anti-alias. Una riproduzione numerica di questo caso ha dato errore massimo circa 2e-12 rispetto alla sinusoide alias: non è una misura RF, ma dimostra il problema matematico. Rumore e interferenti fuori banda possono quindi entrare nella banda del decoder.

Intervento: ricampionatore con filtro anti-alias, stato continuo tra blocchi e ritardo noto nella timeline. La stessa classe è usata da BPSK/MFSK; lì va valutata insieme al condizionamento precedente, senza presumere identico impatto.

### 4. P1 — Decoder e trasporto Q65 dipendono dalla GUI

**Rischio architetturale verificato.** `mainwindow.cpp:1533–1541`, `8983–9012`, `13552–13645`: MSK144, SSTV, WEFAX, RTTY, BPSK, MFSK, CW e Hell vengono chiamati dal thread MainWindow. CW può delegare parte del lavoro internamente, ma ingresso e orchestrazione restano sulla GUI. Q65 ha un worker, ma i suoi campioni passano comunque dalla coda GUI prima di raggiungerlo.

Operazioni pesanti di decodifica rallentano i controlli; operazioni GUI lente ritardano o fanno scartare audio. FT8/FT4 hanno invece già un ingresso diretto nel worker.

Intervento: acquisizione → coda audio limitata → worker del modo attivo. La GUI deve ricevere risultati e telemetria, non trasportare campioni necessari al decoder.

### 5. P1 — Blocchi RX scartati senza segnalare discontinuità ai decoder

**Difetto statico.** `audio/BoundedAudioDispatcher.cpp:17–20` elimina i blocchi più vecchi; `mainwindow.cpp:8988–9011` registra il numero di drop e poi inoltra i superstiti. RTTY/BPSK/MFSK/SSTV non utilizzano captureGeneration, captureSequence o timestamp per ricostruire il gap nei file esaminati. WEFAX copia firstSampleIndex ma non risolve qui il salto. Il DSP waterfall ignora a sua volta i drop (`mainwindow.cpp:1586`).

I filtri e i clock possono trattare campioni separati nel tempo come contigui. Q65/MSK144 hanno invece una gestione esplicita dei salti temporali: non vanno accusati della stessa omissione.

Intervento: contratto unico di discontinuità, con reset delle parti di stato necessarie e segnalazione del frame/immagine compromesso. Non aggiungere recovery concorrenti.

### 6. P1 — Scheduler FT separato, avvio reale ancora subordinato alla GUI e CAT

**Rischio architetturale verificato.** `mainwindow.cpp:1625–1639` consegna prearm e audio-start dal scheduler alla GUI con segnali queued. Le funzioni `invokeRigPttBlocking`, `invokeRigBeginFtSplitBlocking` e `invokeRigEndFtSplitBlocking` (11106–11151) bloccano il chiamante fino al completamento CAT. `startFtPreparedSlotTransmit()` include inoltre lo stop bloccante dell'ingresso audio.

Il backend HRD usa attese sincrone: fino a 24 attese da 250 ms per una risposta (`rig/HamlibController.cpp:1716–1722`), oltre alle altre operazioni. Nel frattempo gli eventi GUI e TX restano in attesa. Il controllo di deadline FT a 20 ms può quindi rinviare la trasmissione; la presenza di un thread scheduler non rende autonomo l'avvio.

Intervento: proprietario unico della transazione TX che gestisca preparazione CAT, conferma PTT, deadline, audio e termine con eventi asincroni. La GUI visualizza lo stato.

### 7. P2 — Il budget di chiusura di 5 secondi non copre tutte le attese

**Difetto nella garanzia temporale.** `mainwindow.cpp:1861–1907` effettua stop audio e disconnessione CAT bloccanti prima di avviare il budget condiviso da 5.000 ms, circa riga 1947. Una chiamata CAT bloccata può impedire di raggiungere il punto in cui parte il timer.

Intervento: deadline complessiva dall'inizio della chiusura e arresto hardware asincrono verificato. Il distacco finale dei thread non risolve le attese che lo precedono. Non utilizzare terminate().

### 8. P2 — Code Qt non limitate su FT RX, recorder e Q65

**Rischio architetturale.** `mainwindow.cpp:9021–9029`: ogni blocco viene accodato direttamente al decoder FT e al registratore. Il gate del decoder opera quando l'evento è già stato consegnato. `13580–13586` accoda analogamente al worker Q65 senza limite esplicito.

Se elaborazione o disco rallentano, le code possono crescere e aumentare latenza/memoria. Il dispatcher limitato per la GUI non limita questi rami.

Intervento: capacità e politiche separate per ciascun consumatore; su sovraccarico del recorder fermare/finalizzare con errore esplicito, sui decoder invalidare correttamente l'intervallo. Misurare profondità, età e drop della coda.

### 9. P2 — MainWindow concentra troppe responsabilità e mantiene stato TX duplicato

**Problema architetturale.** 25.115 righe non sono da sole un bug; qui coincidono con logica di modem, CAT, PTT, contest, log, rotatore, audio, scheduler e UI nello stesso oggetto. `mainwindow.h:2107–2123` conserva messaggio/tag, boundary, delay e flag di piano oltre a `FtTxPlan`. Esistono anche stato worker, stato motore e stato GUI.

`handleTxStopped()` e `handleTxError()` replicano reset molto simili (24808 e 24931), ma non identici: ad esempio `m_txFinishedNaturally` viene azzerato nel primo e non nel secondo. Questo è un punto da verificare con sequenze di eventi, non prova autonoma di un QSO errato.

Intervento: separare controller RX, transazione TX, QSO/logging e presentazione; usare un solo piano TX e una sola transizione terminale idempotente. Evitare un nuovo controller sovrapposto ai vecchi percorsi.

### 10. P2 — Individuazione EOH nel logbook non rispetta le lunghezze dei campi

**Difetto statico.** `logbook/AdifLogbook.cpp:94–102,267–278`: il primo `<EOH>` viene cercato come sottostringa globale. Un file senza header, con `<EOH>` dentro il valore di COMMENT, viene interpretato come se l'header finisse in quel punto: il prefisso del record viene perso.

Caso minimo: `<CALL:6>IZ6NNH <COMMENT:5><EOH> <MODE:3>FT8 <EOR>`. Il parser sceglie come body soltanto ciò che segue il COMMENT. Lo splitter EOR ha già una scansione basata sulle lunghezze; EOH deve usare la stessa disciplina.

### 11. P2 — Import/export può inventare la data di QSO incompleti

**Difetto statico.** Il parser conserva record senza data valida e dichiara di preservarli anche senza CALL. `entryToAdif()` a riga 651 e `syncCommonFieldsToAdif()` a riga 229 sostituiscono però UTC invalido con l'ora corrente. Un'importazione o riscrittura può pertanto attribuire a vecchi record la data di oggi. Inoltre `cleanAdifValue().simplified()` altera spazi e newline dei valori.

Intervento: assegnare l'ora corrente solo alla creazione esplicita di un nuovo QSO locale. Serializzazione e importazione non devono inventare informazioni. Separare valori originali e campi normalizzati destinati alla ricerca.

### 12. P2 — Ogni QSO riscrive tutto il logbook nella GUI

**Ottimizzazione e rischio di latenza.** `AdifLogbook::append():395–419` copia logicamente la raccolta, aggiunge un record, riserializza l'intero file con QSaveFile e ricostruisce l'indice dei nominativi. I chiamanti sono in MainWindow (3309,17948). Il costo cresce con la dimensione totale del log e si ripete a ogni QSO; su storage lento può interferire con le deadline GUI/TX.

QSaveFile è una protezione utile da mantenere, non da eliminare per ottenere velocità. Intervento: proprietario persistente del logbook fuori dalla GUI, indice incrementale e strategia di scrittura con durabilità definita. Una sequenza di N inserimenti ha lavoro totale tendenzialmente quadratico con la riscrittura integrale.

### 13. P2 — Due istanze possono sovrascrivere reciprocamente il log

**Rischio di perdita dati.** In AdifLogbook non c'è lock interprocesso o confronto della versione del file prima della riscrittura. Due istanze caricano lo stesso snapshot: dopo l'append di A, l'append di B può sostituire il file senza il QSO di A. La scrittura atomica evita file parziali, non aggiornamenti persi.

Intervento: lock esplicito e proprietà esclusiva del log, oppure transazione che rilegga e riconcili sotto lock. Verificare anche eventuale protezione globale di istanza prima di implementare.

### 14. P2 — I test statici possono passare senza provare i comportamenti critici

**Lacuna di verifica.** Sono passate tutte le cinque suite: architecture 5 controlli, ui 4, ft 6, release 6, waterfall 1; totale 22 script invocati. Molti controllano presenza/assenza di stringhe nel sorgente. Ad esempio `check_qso_udp_logging.py` verifica i punti testuali di append e broadcast; non apre un ricevitore UDP né simula un errore disco.

`Q65NativeRegression.cpp` prova un round-trip sintetico a 12 kHz e periodo 15 s per A/B/C/D; 30/60/120 s hanno controlli di geometria, non equivalenti prove complete di ricezione. `Msk144NativeRegression.cpp` usa brevi sequenze sintetiche prodotte dal TX interno. Utili, ma non dimostrano interoperabilità indipendente, sensibilità né comportamento a 44,1/48/96 kHz.

Inoltre il target `MadModemFtSequencerSelfTest` è opzionale (`MADMODEM_BUILD_FT_TESTS=OFF`) e non è registrato con `add_test`: abilitare BUILD_TESTING non basta a eseguirlo in CTest.

Priorità test: sink audio guasto/fine buffer, timestamp/drop, CAT lento, log incompleto e concorrente, WAV esterni con esiti attesi. I test nativi presenti non sono stati eseguiti qui.

### 15. P3 — Allocazioni e spostamenti evitabili nel waterfall

**Ottimizzazione non misurata.** `dsp/DspEngine.cpp:28–45` costruisce una nuova finestra e rimuove il prefisso del FIFO a ogni hop. `analyzeWindow():72–98` rialloca real, imag, magnitudes e output per ogni FFT. SSTV e WEFAX contengono ulteriori rimozioni in testa ai buffer.

Intervento: ring buffer con indici e workspace FFT riutilizzabile. Misurare allocazioni/secondo e latenza prima/dopo, conservando identici hop, finestratura e geometria temporale. Non confondere questo con un'esigenza già dimostrata di GPU.

### 16. P3 — Duplicati fisici presenti, ma non prova di doppia esecuzione

La scansione hash di file .c/.cpp/.h ha trovato 12 gruppi di duplicati esatti, tutti sotto third_party, soprattutto fra port/reference/upstream MSHV e file accessori Hamlib. Nessun gruppo esatto è stato trovato nel C++ applicativo fuori third_party.

Le copie di riferimento non equivalgono a due decoder attivi. L'eventuale alleggerimento va fatto sulla distribuzione e sull'elenco delle sorgenti compilate, preservando provenienza e licenze. La duplicazione applicativa più rilevante è quella delle responsabilità e delle transizioni TX descritta sopra, non questi header identici.

### 17. P2 — Percorso predefinito del log dipende dalla scrivibilità della directory eseguibile

**Rischio operativo.** `AdifLogbook::defaultPath():347–350` restituisce `applicationDirPath()/logbook.adi`. Una distribuzione in una directory protetta può quindi impedire il salvataggio con utente normale. La configurazione può scegliere un altro percorso e l'errore viene riportato; non è una perdita silenziosa dimostrata.

Intervento: distinguere esplicitamente modalità portable e installata, con posizione dati scrivibile e migrazione controllata. Il problema riguarda anche installazioni Linux in directory di sistema.

## Parti già sensate da conservare

- Input FT diretto nel worker, separato dalla GUI.
- Dispatcher limitati già presenti per alcuni rami.
- Sessione FT e sequencer estratti almeno in parte, con un core di decisione separato.
- Scrittura logbook atomica e broadcast UDP dopo append riuscito nei due punti esaminati.
- Scelta dei dispositivi configurati senza fallback silenzioso.
- Lock condiviso dei codec weak-signal, da non rimuovere senza rendere reentranti le primitive.
- Gestione esplicita della timeline nei wrapper Q65/MSK144.
- Algoritmi stateful reali nel rotatore: la loro correttezza sotto movimento/rumore non è stata verificata dinamicamente in questa analisi.

## Ordine di intervento proposto

1. Correggere ciclo di vita TX, controllo errori sink e rilascio PTT; costruire prove riproducibili della coda audio.
2. Correggere il ricampionamento e rendere esplicite le discontinuità RX.
3. Togliere trasporto audio e attese CAT dal thread GUI, con un proprietario unico delle transazioni TX.
4. Proteggere semantica e concorrenza del logbook, poi spostarne il lavoro I/O fuori dalla GUI.
5. Consolidare lo stato duplicato; ottimizzare FIFO/FFT dopo misure; ampliare test indipendenti.

Restano da approfondire con esecuzione: sensibilità/interoperabilità di ogni modem, sequenze complete di cambio modo durante TX, race sotto stress, split su radio reali, rotatore e radiotelescopio, rendering UI, persistenza di tutte le impostazioni, codec e librerie terze, build Windows/macOS/Linux. Non è stata attribuita a questi ambiti una falsa conclusione di assenza bug.
