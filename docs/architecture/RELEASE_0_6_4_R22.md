## R22 — periodi TX/RX, ripetizioni CQ e coda chiamanti

- Le barre seguono il ruolo del periodo: TX rossa e RX verde. La selezione
  TX I/TX II scambia i colori, anche se arriva una fotografia del vecchio stato
  dal thread del scheduler. Il periodo attivo mostra avanzamento e TX/RX.
- Ripeti CQ: una nuova attivazione inizializza il totale, prima chiamata inclusa.
  Anche l’attivazione durante una chiamata CQ già in TX o già programmata
  prepara la serie. Un completamento viene contato una sola volta per slot UTC.
  Mostrato il progresso eseguite/totale. Al raggiungimento del numero configurato
  la casella viene disattivata; la prossima attivazione riparte con budget nuovo.
- Cambiare il totale durante la serie conserva le chiamate già eseguite. Se il
  totale scende al numero già completato, termina la ripetizione futura e una
  chiamata già in onda può terminare normalmente. Il limite senza risposta
  resta un budget per gli scambi QSO e non consuma il budget CQ.
- Tolto il residuo vincolo che azzerava la preferenza CQ con Evil Mode bloccato.
  Un timer di avvio CQ già accodato rispetta un successivo STOP dell’operatore.
  Come prima, una risposta valida avvia il QSO e interrompe la serie CQ.
- Nuovo pulsante Chiamanti (N) accanto ad Accoda chi mi chiama. All’attivazione
  della casella si apre una finestra non modale anche a coda vuota. Tabella
  aggiornata per ordine FIFO, nominativo, locator, SNR, frequenza AF e ultima
  ricezione UTC. Rimuovi selezionati e Svuota lista agiscono sui soli chiamanti
  in attesa. Chiudere la finestra mantiene l’accodamento attivo. Nessun clic
  nella tabella programma una TX. La Lista CQ in uscita mantiene la sua funzione.

Le foto non contengono il log degli eventi CQ: non consentono di attribuire
con certezza l’interruzione segnalata a un’unica causa. Sono corretti e coperti
dalla sonda i casi riproducibili descritti sopra, insieme alla gestione del
contatore e della preferenza. Una risposta al CQ continua ad avviare il QSO.

Verifiche: build Linux completa; sonda GUI FT8/FT4 con8combinazioni di colore,
attivazione duranteCQ,3chiamate con limiteQSO1,completamento duplicato,nuova
serie,conteggio modificato live,STOP prima del timer di avvio,preferenza senza
EvilMode,aggiunta/aggiornamento FIFO,rimozione/svuotamento senza modificare QSO.
Verificati anche6passaggi di fineTXnaturale verso il successivo CQ programmato,
senza inviare audio a dispositivi fisici. Passano le sonde risposta finale73/
timeout/QRZ e stato compatto a1280/1920.
6CTest selezionati finali PASS: un binario locale mancava del permesso eseguibile
al primo avvio; ripristinato il permesso e rieseguito il test del sequencer.
Manuale e traduzioni aggiornati in6lingue. Nessuna prova radio fisica o build
nativa Windows/macOS qui. Clock audio R21, decoder e backend GPU conservati.
