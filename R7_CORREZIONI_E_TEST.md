# R7 — RTTY, CQ WW e logbook

Revisione sorgenti: `0.5.9-alpha-source-r7-rtty-contest`.
Baseline: R6.1. I rapporti e log R6/R6.1 inclusi sono storici.

## Correzioni

- TX: codice LTRS esplicito a ogni nuova trasmissione; un ricevitore remoto
  rimasto in FIGS non deve interpretare TEST come 53’5.
- AFC: offset comune dei due toni, shift invariato, ricerca limitata rispetto
  alla sintonia dell'operatore. Finestra temporale accumulata indipendente dai
  blocchi audio. Nessuna modifica dei controlli usati per generare il TX.
- RX: il primo blocco dopo un riavvio volontario stabilisce un nuovo riferimento
  di continuità; i gap imprevisti continuano a invalidare il decoder.
- Macro: preservati i terminatori intenzionali; macro CQ WW delimitate. Zona
  propria TX esplicita e validazione dello scambio prima delle macro contest.
- Parser: confini del nominativo anche con slash; nessun cambio automatico di
  partner quando il form contiene già un altro call. Stato/provincia riconosciuto
  dal dominio CQ WW, dopo il report nel percorso automatico.
- Form: campi RX invalidati al cambio partner e dopo salvataggio riuscito;
  nessun avanzamento se il salvataggio fallisce. Conferma contest non modale.
- Record: frequenza, identità di stazione/operatore, UTC finale e identificatore
  del record conservati; UDP deriva dallo stesso record. In assenza di CAT la
  frequenza viene chiesta all'operatore. L'UTC resta quello del salvataggio.
- CQ WW: zona ricevuta autorevole (03 e 3 equivalenti), campi richiesti
  condizionali, validazione zona/RST/QTH/banda/frequenza, paesi contest distinti
  dal numero DXCC. I/IT9/IG9 non collidono. Record incompleti non riservano il
  dupe; i duplicati validi non raddoppiano il punteggio.
- Sessioni: separazione delle prove dal periodo contest al primo log/session
  refresh utile; esclusione dal punteggio di record fuori periodo/edizione.
- CTY: suffisso geografico riconosciuto prima del call domestico; preservati gli
  override di zona CQ/ITU dei token. /MM escluso dai moltiplicatori paese/QTH.
- UDP: scelta tra bundle compatibile, solo QSO Logged (5), solo Logged ADIF
  (12), con heartbeat. Socket UDP diretto, senza ereditare proxy HTTP.
- Cabrillo 3.0: export dal logbook, singola sessione/stazione/edizione, campi
  verificati e nessuna frequenza inventata. Categorie SINGLE-OP e CHECKLOG;
  tutte le bande selezionate restano nel file anche con categoria single-band.

## Ottimizzazioni

Il decoder selezionato viene elaborato prima dello scanner. Lo scanner usa una
finestra della stessa durata ai diversi sample rate, precalcola la pesatura una
volta per scansione e riutilizza lo spettro invece di ripetere un identico passaggio
Goertzel. Corsie vicine e callout duplicati/parziali vengono accorpati. I decoder
secondari non costruiscono scope o statistiche UI inutilizzate e seguono la
polarità scelta per il monitor. Non viene più ricalcolato l'intero punteggio a
ogni carattere digitato nei campi RX.

## Uso

1. Selezionare RTTY e il profilo CQ WW, 45,45 baud e shift 170 Hz. Verificare
   polarità e ingresso audio con la propria radio.
2. Compilare **TX Zona CQ** con la zona operativa reale. Compilare il QTH TX
   quando richiesto (USA continentali/Canada). Il dato non viene indovinato dal
   prefisso. Per il Canada il contest usa NWT, NF, LB e PEI, tra le altre sigle.
3. Nel QSO verificare call, zona e QTH ricevuti; la registrazione richiede uno
   scambio valido. Le prove fuori periodo sono conservate nel log, ma non
   producono punteggio di gara.
4. Nelle Impostazioni del logbook scegliere il formato UDP accettato dal proprio
   server. Il default mantiene il bundle precedente. Se il server registra
   entrambi i messaggi, scegliere solo tipo 5 oppure solo tipo 12.
5. Per Cabrillo usare Esporta risultato/selezionati nel logbook, scegliere il
   periodo/sessione voluto e il filtro **CQ WW RTTY Cabrillo (*.log)**. Compilare
   categoria, potenza, assistenza, nome ed email. Controllare il file prima
   dell'invio al sito del contest. L'app non effettua l'invio.

## Verifiche

Build Release Linux, Qt 5.15.13 / GCC 13.3 / Hamlib 4.5.5: riuscita.
CTest finale: **19/19 superati** (32,16 s). Il test RTTY dedicato contiene
66 controlli superati. Non sono stati eseguiti test Windows o Qt 6 in locale.

Il test CTest `madmodem_rtty_contest_regression` usa i componenti di produzione:
TX/decoder continui, encoder ITA2 indipendente, AFC con blocchi 137/1024/4096 a
44,1/48/96 kHz, ripresa/gap, scoring, CTY, Cabrillo, datagrammi UDP su loopback e
due segnali simultanei a 48/96 kHz. Nel test nativo dedicato il picco per blocco dello scanner enhanced è stato
circa 14 ms a 48 kHz e 30 ms a 96 kHz, con esattamente due callout corretti.
Sono misure singole di questo ambiente, non soglie garantite.

I log della build finale sono in
`docs/r7-verification/`. I controlli precedenti restano attivi.

## Limiti operativi e lavoro strutturale residuo

Le prove sono native Linux, non una certificazione RF né una verifica del runner
Windows/MSYS2 o del server UDP reale. Servono quelle verifiche prima dell'uso
operativo. Non è stata provata una sessione radio continua di 48 ore.

Il decoder e lo scanner restano sul thread GUI: il costo è stato ridotto, ma
non è stato introdotto un worker dedicato. L'append ADIF conserva le protezioni
atomiche e contro scritture concorrenti della R6: è ancora proporzionale alla
dimensione del log. Il punteggio si ricalcola sui record della sessione dopo il
salvataggio; non è un indice incrementale persistente. UDP resta best effort,
senza conferma del logger né outbox; la selezione di un tipo evita duplicazioni
causate dal bundle, non può garantire il comportamento di qualsiasi server.

Cabrillo integrato non gestisce MULTI-OP, overlay o stazioni mobili. Per /MM la
zona è ricevuta e il moltiplicatore paese è escluso; l'inferenza del continente
per i punti dal call domestico resta insufficiente per una nave lontana dal
paese di origine e richiede verifica manuale. La normalizzazione dei portatili
copre prefissi/suffissi geografici riconosciuti, non ogni convenzione speciale.

Non sono stati sostituiti il demodulatore o il clock con nuovi algoritmi di
fading/decisione soft; non si dichiarano soglie RF, immunità al QRM o equivalenza
con altri modem. Le 51 osservazioni dell'audit R6 restano dati della baseline,
non risultati ottenuti automaticamente sulla R7.

Riferimenti verificati: https://cqwwrtty.com/rules.htm e
https://cqwwrtty.com/cabrillo.htm (regolamento 2026).
