# MadModem 0.6.4 R31 — clean e completamento dell'integrazione

Questa revisione completa il recupero a due residui rimasto nel checkpoint della prima R31. La build dell'applicazione parte da una cartella nuova, senza riutilizzare oggetti della consegna precedente. Versione pubblica 0.6.4; marcatore sorgenti 0.6.4-source-r31-native-ft-clean.

Il completamento riguarda l'integrazione R31: la parità generale con JTDX rimane un obiettivo aperto, soprattutto in live.

## Cosa cambia

Il primo residuo sottrae i modelli nativi affinati; il secondo aggiunge i modelli del recupero supplementare già presente nella R30. Le due ricerche condividono il limite temporale complessivo e aggiungono risultati alle righe già pubblicate. La misura dei segnali usa esclusivamente i campioni e le decodifiche effettive, senza fornire al decoder i testi attesi del tester.

I contatori nativeRecovery sono separati da quelli del tail coerente precedente. Un errore nella preparazione FFT interrompe soltanto il passaggio facoltativo, conservando le decodifiche precedenti. Le cancellazioni e il limite UTC restano attivi in entrambi gli stadi; il secondo non riceve altro tempo. Gate anticipato, percorso GPU ordinario e recupero Doppler dedicato mantengono il comportamento precedente.

Non vengono aggiunti motori o runtime Fortran. I riferimenti sono WSJT-X, JTDX e MSHV. I sorgenti del Doppler e dei metodi FT4 di ricerca, decodifica e SIC restano identici alla prima R31 e alla baseline R30; gli altri modem e CAT/PTT/sequencer/logbook non sono stati modificati. Il calcolo min-sum equivalente della R31 rimane condiviso da FT8/FT4.

La pulizia comprende la registrazione dei nuovi header nel progetto CMake, l'esclusione della cartella build ordinaria da Git, i contatori diagnostici dedicati e un eseguibile Doppler distinto per ogni invocazione del relativo strumento di verifica. Le varianti scartate restano nel checkpoint e non entrano nella build di produzione.

## Risultato FT8

Stesso estratto del tester, otto worker, tre ripetizioni offline. Si contano separatamente le righe totali e le corrispondenze esatte con i 66 messaggi della lista JTDX.

| Revisione | Righe offline | Corrispondenze esatte JTDX | Ripetibilità osservata |
| --- | ---: | ---: | --- |
| R30 congelata | 60 | 55 | confronti precedenti |
| Prima R31 | 61–62 | 56–57 | due ripetizioni precedenti |
| R31 clean | 62 | 57 | identico nelle tre ripetizioni |

VU2XZ PD9DY JO22 e TT1GD RA6OB KN98 sono presenti nelle tre prove. Il nuovo recupero aggiunge due messaggi al risultato precedente dello stesso job, senza rimuovere le 60 righe già ottenute. Il suo costo misurato è circa 1,02–1,06 s; l'intera analisi offline richiede circa 5,60–5,92 s in questo ambiente. Le cinque ulteriori righe non confermate già presenti nella R30 restano classificate come tali. Restano nove messaggi JTDX non recuperati.

Le due prove gate + boundary danno 51 e 50 righe, entrambe con 47 corrispondenze JTDX. Il nuovo passaggio viene saltato perché il tempo disponibile è già esaurito. Non si dichiara un miglioramento live o una parità generale. Le sette altre registrazioni FT8 offline producono rispettivamente 27, 22, 25, 22, 33, 28 e 20 righe; tutti i risultati pubblicati restano nell'uscita del rispettivo job. Questi conteggi non certificano da soli la precisione su segnali sconosciuti.

## FT4 e controlli

La matrice FT4 comprende 42 condizioni: 36 con segnale debole e sei di solo rumore. Conferma 14 messaggi corretti e nessuna decodifica nei sei controlli di rumore. Rimane lo stesso falso positivo già documentato nella R30, QQ4PYT/P BT1FPH CM88, ampiezza 0.013 e seme 0. Non è stato aggiustato un filtro sul singolo esempio. Per misurare la parità FT4 serve un corpus reale affollato con risultati di riferimento.

La build completa dell'applicazione Linux Qt5 Release è stata eseguita da una cartella nuova. Passano 14 controlli mirati: primitive numeriche, produttore incrementale, recupero FT8/FT4, frame generati e rumore, conservazione delle righe pubblicate, rifiuto del recupero aggiuntivo a scadenza superata, sequencer, fallback GPU e controlli dei sorgenti. La suite completa di 58 verifiche della prima R31 resta documentata separatamente; non viene presentata come ripetuta integralmente in questo clean.

I 46 casi sintetici Doppler FT8/FT4 passano, comprendendo derive fino a ±20 Hz/s, soglia RF, disabilitazione, rumore e previsione del periodo successivo. I controlli dedicati di risposta applicativa passano: cancellazione, pubblicazione asincrona, chiusura finale, risposta QSO e impostazioni Doppler/QRZ. Le verifiche sintetiche non sostituiscono una radio reale.

La verifica ASan/UBSan sul ricevitore nativo aggiornato e sul test del front-end completo è stata superata. Le librerie esterne non sono automaticamente strumentate e LeakSanitizer rimane disattivato nel runtime corrente; il runner dispone dell'opzione esplicita --leaks per sistemi compatibili.

## Consegna e limiti

Il pacchetto contiene l'intero progetto CMake, i workflow GitHub esistenti, licenze, documentazione e gli output del giro di verifiche concluso. Non contiene gli oggetti della build né gli eseguibili di prova. L'inventario SHA256 nella radice permette di verificare tutti i file consegnati.

È caricabile su GitHub come revisione di sviluppo. Restano da eseguire le build native Windows/macOS e Qt6 di questa esatta revisione tramite i workflow e le prove su radio/PTT/GPU fisici. La priorità successiva per la sensibilità è recuperare i nove messaggi mancanti e rendere il vantaggio disponibile entro il tempo live: aumentare il budget offline non risolve questo punto.

I dati completi sono in FT_R31_CLEAN_RESULTS.json e r31-clean-validation/. FT_NATIVE_REIMPLEMENTATION_R31.md descrive gli algoritmi; WSJTX_R31_SOURCE_INVENTORY.json identifica i riferimenti analizzati.
