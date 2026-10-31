# R31 — correzione sequencer e avvio FT8/FT4, TXFIX1

Questa correzione parte dai sorgenti R31 CLEAN WINFIX1. Il log della sessione Windows build 129, commit add5054def19f1b8c0ac1168d3522f6100efd44d, documenta errori prima del primo campione PCM e rinvii durante l'attivazione CAT/PTT. Sono corretti il percorso comune al sequencer normale e quello usato dall'Auto QSO di Evil Mode.

## Problemi individuati

L'orario desiderato del primo tono era usato anche come scadenza definitiva del backend. Una scheda audio o una conferma CAT in ritardo di poche centinaia di millisecondi faceva quindi annullare un frame ancora interamente trasmissibile nel periodo. Nel log, per esempio, il 73 delle 21:08:15 viene rinviato per un ritardo di 192 ms sul target, pur essendoci spazio per il frame completo.

Inoltre handleTxError cancellava il piano TX ma lasciava attivo il QSO, senza programmare il messaggio successivo. Un QSO nel log rimane così attivo dalle 20:58:30 circa alle 21:03:59. I tempi citati sono quelli del log testuale dell'utente. Un ulteriore rischio era che la successiva notifica stopped del vecchio worker cancellasse il piano sostitutivo appena armato.

## Correzioni

- L'orario desiderato del tono e l'ultimo avvio PCM ammissibile sono distinti. Il limite resta vincolato alla fine del periodo, alla durata del frame e ai margini già previsti: FT8 ammette il primo PCM fino a +2040 ms; FT4, con il profilo da 5040 ms, fino a +2140 ms. Il limite di ammissione iniziale del piano e le scadenze RX restano invariati.
- Al primo pull il trasmettitore controlla che l'intero frame entri ancora nella finestra. I frame automatici non vengono tagliati e non saltano campioni iniziali. Il taglio al bordo del periodo resta consentito soltanto per il burst parziale richiesto esplicitamente dall'operatore.
- Un errore audio riprogramma il piano nel successivo periodo selezionato. Il contatore di mancate risposte non viene consumato da un frame non trasmesso. Se nel frattempo è arrivato un messaggio più avanzato, ad esempio il 73, il recupero conserva quel piano e il relativo stato.
- Il request ID del worker fallito viene invalidato: le sue notifiche tardive non possono cancellare il nuovo piano. STOP e i controlli di generazione restano prioritari.
- Tre fallimenti consecutivi dell'avvio CAT/audio fermano l'automazione e lasciano disponibile RX. Un frame concluso correttamente azzera questo conteggio. Il guasto del backend non viene attribuito a una mancata risposta del corrispondente.
- Un messaggio duplicato con la stessa frequenza conserva il piano già valido anche dopo il confine UTC, finché il frame completo entra nel periodo. Il watchdog non rilancia un avvio già richiesto e in attesa di completamento.

## Verifiche

La build completa Linux Qt5 è riuscita. Passano quattro controlli CTest mirati: politica temporale, architettura asincrona con finestre di playback, sequencer e controlli del percorso FT.

La nuova prova UI riproduce, per FT8 e FT4, sia il funzionamento normale sia Evil Mode: errore prima del primo PCM, notifica tardiva del worker precedente, tentativi QSO già parzialmente consumati, arresto dopo errori ripetuti, piano duplicato nel periodo corrente, conservazione del 73 differito e STOP prima del riarmo. Tutte e quattro le combinazioni passano. Il test simula una cattura RX già attiva e non apre dispositivi fisici.

Nel corso della verifica passano anche i controlli esistenti su pubblicazione asincrona, ultimo tentativo, RR73/73, timeout obsoleto e aggiornamento del corrispondente. Il workflow manuale di regressione esegue ora anche il runner della risposta/sequencer; il runner accetta --probe per isolare il controllo interessato.

Non sono stati cambiati decoder RX, metriche FT8/FT4, recupero Doppler, selezione delle stazioni in Evil Mode o correzione TLS WINFIX1. Non è stato ripetuto il benchmark di sensibilità della R31. Le prove con radio, CAT e scheda USB dell'utente e la build Windows di questo esatto patch restano da confermare sul campo/CI.
