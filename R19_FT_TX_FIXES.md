# R19 — correzioni del ciclo TX FT8/FT4

Baseline: archivio R18 FT_LOGBOOK_INDEX fornito dall'utente il 3 ottobre 2026.
Revisione: `0.5.9-beta-source-r19-ft-tx-lifecycle`.

## Difetti riscontrati

1. Una richiesta manuale effettuata a periodo iniziato riceveva un nuovo target audio, ma lo scheduler continuava a emettere l'evento audio al vecchio confine, già scaduto. Nei log delle 07:18:01 e 07:20:05 seguono immediatamente cancellazione del tentativo e PTT ON/OFF tardivi.
2. `pendingStateChanged(false)` dello scheduler poteva togliere l'autorizzazione al piano GUI mentre la conferma CAT era in viaggio, oppure disarmare un piano sostitutivo. I messaggi non contenevano un token che permettesse di distinguerli.
3. Il callback CAT di una richiesta sostituita poteva modificare lo stato della nuova preparazione e richiedere PTT OFF. I retry non consumavano il periodo fallito, favorendo ripetizioni nello stesso slot.
4. Le righe gialle TX venivano inserite nella fase di pre-arm, prima della conferma CAT e dell'avvio audio. Non erano una prova di trasmissione.
5. `TxOutputDevice` generava campioni su richiesta ma non implementava `bytesAvailable()`: prima della prima lettura il valore era zero. Un backend che controlla la disponibilità prima di leggere non riceveva PCM. L'avvio del sink era inoltre annunciato senza controllare la progressione della riproduzione.
6. Stop dipendeva anche dai flag logici PTT e non revocava direttamente un avvio audio già accodato. Un gate CAT in errore bloccava persino un successivo comando di recupero PTT OFF.

## Correzioni

- Lo scheduler usa anche il target aggiornato per decidere quando aprire l'audio delle richieste tardive; i tempi nominali restano FT8 500 ms e FT4 300 ms.
- Il proprietario del piano è la GUI. Le notifiche senza token dello scheduler non modificano più lo stato del piano.
- Una conferma CAT può arrivare dopo l'evento di apertura audio ma prima della sua scadenza. Durante questa attesa non si avvia audio e non si inventano righe TX.
- Un tentativo scaduto viene rinviato a un periodo successivo; i callback di retry rispettano generazione, Stop e sostituzione del piano.
- I callback CAT obsoleti non toccano il piano nuovo. Il rollback del job annullato resta nel worker CAT serializzato.
- Stop richiede PTT OFF anche senza un flag keyed attivo; una richiesta OFF di recupero può attraversare il gate faulted. Le richieste ON restano bloccate in caso di errore.
- Gli avvii audio hanno un identificativo revocabile atomicamente. Gli eventi di worker appartenenti a vecchi avvii sono scartati. Il watchdog Stop non interferisce con un nuovo TX.
- La sorgente PCM dichiara la disponibilità e termina con EOF finito, senza prefetch interno QIODevice. Il backend viene monitorato: assenza di progresso oltre un secondo causa errore, arresto e richiesta di rilascio PTT.
- La riga TX viene pubblicata soltanto dopo che il backend ha richiesto campioni e dichiara tempo audio processato. Questo conferma il percorso software audio, non misura RF né il cablaggio fisico.
- Log runtime è una finestra normale non modale, riapribile dal suo pulsante.

## Verifiche

Compilazione Release Linux, Qt 5.15.13, GCC 13.3, Hamlib 4.5.5.

La suite CTest contiene 23 test. Il primo passaggio ha rilevato due problemi di verifica: cataloghi traduzioni da rigenerare e bind UDP locale vietato dalla sandbox. I cataloghi sono stati rigenerati; il test RTTY/UDP è passato con loopback consentito. Gli altri 21 test erano già passati. Le ripetizioni mirate finali sono conservate in `verification-r19`.

Sono state aggiunte verifiche alle suite esistenti per: disponibilità PCM prima della prima lettura, EOF/tail; richieste tardive FT8/FT4; cancellazione e sostituzione degli eventi di slot; recupero PTT OFF dopo fault; Stop di un avvio audio ancora in coda; scadenza prima dell'apertura del backend.

Un probe locale collegato agli oggetti reali di MainWindow ha verificato, per entrambi i modi: attesa CAT senza disarmare il piano, assenza di righe TX premature, retry non riarmato dopo Stop, impossibilità di riutilizzare lo slot fallito; inoltre la finestra log resta visibile alla disattivazione. Il probe è un supporto di verifica GCC locale, non un nuovo target CTest.

La compilazione Windows e il comportamento con scheda audio/radio reali devono essere verificati dall'utente. Il problema di RF/PTT effettivo non è dimostrabile con i soli log software.

## Prova sul PC Windows

Usare il pacchetto ricompilato completo, inclusi i file dati. Non occorre azzerare `settings.mad` né il logbook.

1. Sintonia: verificare audio in uscita e rilascio PTT quando si preme Stop.
2. FT8: selezionare un CQ con doppio clic; verificare una sola riga TX per avvio audio reale e Stop sia durante l'attesa CAT sia durante la trasmissione.
3. Ripetere in FT4. Se una scadenza viene persa, deve apparire `FT slot deferred`, senza raffiche di righe TX.
4. In caso di anomalia, copiare il log includendo `TX audio opened`, `TX audio playback confirmed` o l'errore con campioni prodotti/tempo processato, e le righe PTT circostanti.

Le modifiche LogbookIndexWorker, la politica di selezione Evil e gli algoritmi di decodifica R18 sono conservati. La correzione riguarda l'esecuzione dei piani TX e il contratto della sorgente audio condivisa.
