# R31 TXFIX2 — controllo operatore, TX tardiva e coda Auto QSO

Questa correzione parte dall'archivio R31 TXFIX1. L'utente conferma che ricezione, fluidità, recupero dei decode e risposte ordinarie funzionano bene. La modifica riguarda la gestione TX e la coda; non modifica decoder RX, recupero Doppler, budget di decodifica o parametri GPU.

## Difetti corretti

1. Con Gestisci coda CQ attivo, il doppio clic veniva consumato dall'accodamento: non cambiava immediatamente interlocutore. Ora il doppio clic prende il controllo della selezione e della TX. L'accodamento multiplo resta disponibile attraverso Accoda CQ selezionati.
2. Se c'era un TX attivo, una selezione manuale veniva trattata come messaggio per un periodo successivo; inoltre perdeva il permesso di partenza tardiva. Ora il vecchio audio viene fermato prima di armare il nuovo messaggio, mantenendo l'intento manuale e lo stato del nuovo QSO. Il periodo già tentato blocca i tentativi automatici duplicati ma consente una sostituzione esplicita dell'operatore.
3. La scelta del periodo FT4 usava il testo HHmmss della tabella. Ora usa prioritariamente il timestamp completo della decodifica, conservando il mezzo secondo dei periodi FT4.
4. Auto QSO accettava i CQ soltanto quando libero e svuotava il buffer quando occupato o dopo una selezione. Ora raccoglie e aggiorna una coda anche durante il QSO e dopo il completamento asincrono della consultazione del logbook; selezionare una stazione non elimina le altre.
5. Le notifiche del vecchio worker TX non devono cancellare la sostituzione manuale. L'identità della richiesta terminata viene invalidata prima del riarmo; i messaggi obsoleti non modificano il nuovo piano.

Le correzioni TXFIX1 sull'errore prima del PCM, il riarmo e la conservazione del 73 sono incluse.

## Confronto con WSJT-X ufficiale

Sono stati studiati i sorgenti ufficiali https://github.com/WSJTX/wsjtx, commit `967c85a6119110788b0df3782cf796485b2cee14`, datato 23 settembre 2026. Non è WSJT-Z.

| Percorso WSJT-X | Riscontro | Applicazione a MadModem |
| --- | --- | --- |
| widgets/mainwindow.cpp, doubleClickOnCall/processMessage | Selezione esplicita, scelta messaggio, m_restart per TX già in corso | Il doppio clic può sostituire il TX senza essere assorbito dalla coda. MadModem attende lo stop del vecchio audio prima del nuovo avvio. |
| widgets/mainwindow.cpp, auto_sequence | Controlli su stato dello scambio, corrispondente, messaggio indirizzato e frequenza | Conservati il sequencer e le risposte ordinarie attuali; la coda non interrompe un QSO attivo. |
| Modulator/Modulator.cpp, start, righe 76–103 | Ritardo nominale FT8 500 ms / FT4 300 ms; una partenza tardiva avanza l'indice dei campioni al tempo già trascorso | Per la richiesta esplicita dell'operatore il primo pull PCM calcola la posizione UTC nel waveform già preparato. Non ricomincia il prefisso del messaggio in ritardo. |
| widgets/mainwindow.cpp, stopTx/on_stopTxButton_clicked | Stop modulatore, rilascio PTT e disattivazione dell'automazione sono passaggi distinti | Conservati il gate CAT asincrono, STOP prioritario, annullamento e verifica della richiesta corrente. |

La nuova implementazione è indipendente: nessun motore o eseguibile esterno viene integrato. Il comportamento di avvio delle risposte automatiche rimane quello attuale a frame completo; la ricerca del campione UTC riguarda le partenze manuali esplicite. Un avvio manuale è ammesso solo se resta una porzione utile del messaggio e termina comunque entro il periodo; se il messaggio è ormai finito viene scelto il periodo successivo.

## Coda di lavorazione

La lista CQ mostra anche i candidati di Evil Auto QSO e la loro priorità. Sono conservate le politiche esistenti: nuovo paese, nuovo locator, nuova banda, nuovo modo, stazione non ancora lavorata, distanza, SNR e frequenza. I chiamanti diretti in attesa mantengono la precedenza; una lista scelta dall'operatore mantiene il suo ordine e precede i CQ automatici.

I candidati automatici sono al massimo 64 e vengono rimossi dopo cinque minuti senza aggiornamenti. Una stazione vecchia rimane in attesa di un CQ recente e non viene chiamata solo perché era stata vista prima. Un messaggio diretto successivo o nello stesso periodo impedisce che un vecchio CQ la faccia chiamare mentre lavora altri. Il logbook, la blacklist, i duplicati e il cooldown per mancata risposta vengono verificati prima della scelta. Un CQ più debole ma nuovo aggiorna la freschezza del candidato. A coda piena si scartano prima i candidati ormai vecchi o impegnati.

L'esaurimento dei tentativi del QSO lascia disponibili gli altri candidati. STOP e la disabilitazione di Auto QSO impediscono nuovi avvii automatici. La lista offre rimozione e svuotamento per le voci mostrate. Le spiegazioni e l'aiuto sono aggiornati in tutte e sei le lingue.

## Verifica e limiti

La prima verifica completa della correzione di coda/controllo operatore ha superato la build Linux Qt5, cinque CTest e le prove UI di risposta, recupero TX e coda/operatore per FT8 e FT4. Dopo il confronto WSJT-X è stata aggiunta la verifica esatta dei campioni del waveform tardivo; anche la build finale, i cinque CTest e tutte e tre le prove UI passano. I risultati sono registrati nei log allegati e in FT_R31_TXFIX2_RESULTS.json.

La nuova prova UI usa MainWindow reale e simula RX/TX senza aprire schede audio o radio. Controlla CQ durante un QSO, risposta asincrona del logbook, priorità nuovo locator rispetto a SNR, conservazione delle voci, traffico diretto, CQ aggiornato, STOP, doppio clic con coda attiva, scelta FT4 e notifiche tardive del vecchio worker. La prova PCM confronta campione per campione il suffisso trasmesso con il waveform di riferimento.

Non è stato ripetuto il benchmark di sensibilità perché RX non è cambiato. La compilazione Windows e il comportamento con radio/CAT/audio fisici richiedono conferma su CI e sulla stazione dell'utente; il fix Windows WINFIX1 resta incluso.
