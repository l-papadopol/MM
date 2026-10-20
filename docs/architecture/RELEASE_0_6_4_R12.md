# MadModem 0.6.4 source R12

Revisione `0.6.4-source-r12-incremental-ft-rx-dx-linux-updates`.

## Modifiche per l’operatore

- **DX**, tra Mode e Rotator, contiene barre FT I/II, UTC digitale e informazioni QRZ. Il browser QRZ usa l’altezza disponibile; account e password restano nelle Impostazioni. Gli aggiornamenti automatici della stazione selezionata sono conservati.
- L’aiuto drift ora dice di posizionare il marcatore RX sul segnale e spiega che MM prova a decodificarlo quando cambia frequenza durante la ricezione. Interruttore e limite restano in **Impostazioni → FT8 / FT4**. La TX resta alla frequenza scelta dall’operatore.
- Prima di APT l’installer DEB controlla lo stato di dpkg. In presenza di un’operazione incompleta si ferma, indica `sudo dpkg --configure -a` e chiede di rilanciare l’installer dopo il completamento. Non configura automaticamente pacchetti estranei.
- L’updater portable Linux identifica il pacchetto già in uso dal nuovo marcatore e dal vecchio BUILD_INFO prima di leggere la distribuzione host. Riconosce le derivate noble, sceglie TAR.GZ o ZIP compatibili e mantiene verifica SHA-256, dati utente, backup e rollback. I messaggi distinguono pacchetto assente, piattaforma sconosciuta e checksum mancante. Una copia vecchia che non trova l’asset richiede l’installazione manuale di questa revisione una volta.

## RX durante la ricezione

Un worker separato riceve soltanto la nuova coda audio, normalmente ogni100ms.
Mentre l’audio arriva prepara le FFT4096 con Hann1920 usate dalla ricerca FT8 e
le energie Goertzel576 usate dalla sincronizzazione FT4. La ricerca successiva
riusa finestre già complete e numericamente identiche. LDPC/CRC e pubblicazione
dei messaggi restano nei job gate/boundary esistenti: questa revisione anticipa
il lavoro di analisi, senza pubblicare ipotesi non ancora validate.

Una sola coda contigua pendente, un worker, snapshot immutabili per slot e
cancellazione per generazione. Il nuovo produttore non viene atteso dalla
raccolta o dal decoder; se è in ritardo o fallisce, si usa il calcolo originale.
Le finestre successive alla fine dell’input del job non sono accessibili. Le
frequenze FT4 sono indicizzate con gli esatti bit del double. Audio modificato
per SIC/drift ricalcola le misure; gli snapshot precedenti restano disponibili
per i job boundary differiti.

Soglie, budget candidati, passate, priorità QSO, LDPC/CRC, sottrazione, recupero
coerente R11 e policy TX non sono stati ridotti. Il guard statico verifica ancora
gli hash originali dei finder dopo aver rimosso solo gli esatti wrapper di riuso.
La prova numerica confronta anche punteggi e ordine dei candidati, non solo il
numero finale di messaggi.

## Confronto misurato

Sette WAV FT8 radio reali da kgoba/ft8_lib/test/wav, tre ripetizioni alternate per
revisione, stesso eseguibile/ambiente Qt5 Linux di prova. Il pre-calcolo viene
eseguito prima del lancio misurato; il suo costo separato è nel JSON. La prova
con blocchi real-time da100ms dimostra il riuso nel percorso di raccolta effettivo.
I tempi sono del sistema di prova e non un limite garantito sul PC dell’utente.

| Misura,21slot per revisione | R11 | R12 |
|---|---:|---:|
| Primo batch pubblicato, mediana |110.1ms|62.3ms|
| Ricerca candidati totale, mediana |237.9ms|186.7ms|
| Job completo, mediana |807.9ms|742.3ms|
| Job completo, massimo |898.5ms|895.4ms|
| Margine minimo prima del limite armamento, senza GUI |441ms|444ms|
| Righe finali, incluse ripetizioni |410|409|

Le righe non sono stazioni uniche. Il decoder R11 ha già variazioni fra
esecuzioni dovute allo scheduling e ai budget FEC/OSD. Anche R12 mostra piccole
variazioni: questo confronto **non dimostra un guadagno di sensibilità né parità
esatta dell’intero decoder multithread**. Nei tre replay compare anche un CQ
JR5MJS nella R11 che non compare nelle tre R12; è conservato nel confronto
messaggio per messaggio, non scartato dalla statistica. Nel controllo diagnostico
a un solo worker, tutti i messaggi R11 sono conservati su sette WAV e R12 ne
aggiunge uno. Il singolo worker non è la configurazione distribuita.

La prova indipendente dei candidati è esatta su sette WAV FT8 e un campione FT4
con sei segnali generati. Verifica anche il finder QSO FT4, input modificato,
slot errato, invalidazione e snapshot differito. Non sostituisce un corpus FT4
radio reale, ancora assente.

## UI, sequencer e waveform

Test con sei segnali generati, vera MainWindow/sequencer e generatore TX,
con collegamenti RF e uscita audio hardware scollegati. Millisecondi dal lancio
RX, con analisi R12 già pronta come accade durante la ricezione.

| Percorso | Decoder R11/R12 | Waveform pronta R11/R12 |
|---|---:|---:|
| FT8 gate |302.6/281.7ms|280.9/261.2ms|
| FT8 boundary |708.9/681.9ms|506.3/490.3ms|
| FT4 gate |179.6/135.7ms|nessuna risposta, entrambi5/6|
| FT4 boundary |314.2/257.8ms|226.8/180.0ms|

Il gate FT4 non recupera il messaggio diretto in questo fixture in entrambe le
revisioni; il boundary recupera6/6. I budget restano quelli R11: gate a13.5s FT8
e6.1s FT4;850/750ms fino al pre-arm nominale; recupero aggiuntivo entro
boundary+1150/1250ms con riserva UI/decisione e guardia190ms. Il controllo finale
TX conserva il frame intero. La waveform può essere pronta prima della fine del
job grazie alle pubblicazioni progressive. Il test non misura l’ack della radio.

## Verifiche e limiti

- Build completa Qt5 Release Linux; test CTest del produttore incrementale e updater.
- Produttore bloccato: submit/snapshot/invalidate proseguono; nessuna pubblicazione della generazione cancellata; eccezione worker con fallback.
- Tab DX in ordine corretto, barre188×48 e gruppo84px; FT8/FT4 a1024×768,1280×900 e1600×1000. Font CQ/TX/storico e CW NativeR12/NeuralR13 conservati.
- Installer reale con strumenti sistema simulati: dpkg incompleto, audit fallito, journal pendente e stato pulito; nessuna modifica al sistema di prova.
- Updater: identità bundle/BUILD_INFO prima dell’host, derivate Ubuntu, piattaforme sconosciute rifiutate, TAR preferito e ZIP alternativo. TAR/ZIP preservano stato e rollback.
- Suite FT e release, guard delle versioni, dizionari e documentazione. La guida ha anche la sorgente del generatore aggiornata, per conservare il testo nelle rigenerazioni future.
- Il controllo UI RTTY richiedeva una chiamata per canale assente anche nella R11: ora verifica il percorso di reset completo effettivamente presente. I sorgenti del decoder RTTY coincidono con la baseline.
- Il nuovo worker non attende i calcoli sul percorso capture; il vecchio mutex di configurazione del decoder può comunque rallentare una callback. Nel replay real-time il massimo osservato è circa115ms FT8 e3ms FT4: non promettiamo callback sempre immediate.
- Nessuna prova nativa Windows/macOS, radio/CAT/PTT fisico o audio originale del beta tester. Nessuna dichiarazione di recupero delle13stazioni del confronto JTDX. FT4 coerente resta offline.

Dati completi, hash WAV, messaggi e tempi: `R12_INCREMENTAL_RESULTS.json`.
Sonde diagnostiche: `FtIncrementalRegression.cpp`, `FtCaptureIncrementalProbe.cpp`,
`FtIncrementalRecordedReplayProbe.cpp`, `FtIncrementalTimingProbe.cpp`, da collegare
agli oggetti app con `-fno-access-control` in una cartella di prova isolata.
`FtIncrementalProducerRegression.cpp` è invece registrato direttamente in CTest.
WAV, eseguibili, build e impostazioni delle prove non sono nello ZIP sorgenti.
