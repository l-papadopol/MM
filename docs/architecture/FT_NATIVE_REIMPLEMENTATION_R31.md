# Reimplementazione nativa FT8/FT4 — R31

La R31 sviluppa il decoder C++ di MadModem usando come riferimenti WSJT-X, JTDX e MSHV. Non introduce un altro motore, un processo esterno o un runtime Fortran. Il recupero MSHV già presente nella R30 resta attivo: il nativo non ha ancora dimostrato di sostituirne tutte le decodifiche.

Il riferimento corretto è WSJT-X. La release studiata è v3.0.2, commit ccdfaf3c1c109010d15399674ce278167cfde848; WSJTX_R31_SOURCE_INVENTORY.json identifica undici file Fortran mediante SHA256. JTDX e MSHV mantengono i riferimenti degli inventari R27/R30. WSJT-Z compare soltanto nei documenti storici.

## Percorso adottato

I passaggi ordinari R30 mantengono ricerca, demodulazione, SIC e recupero Doppler. Dopo la pubblicazione dei risultati esistenti, Deep Max può eseguire un recupero aggiuntivo nello stesso ricevitore nativo FT8:

1. Distingue i modelli dei passaggi nativi da quelli del recupero supplementare già presente.
2. Affina tempo e frequenza dei modelli nativi sul campione originale; accetta il modello affinato soltanto quando FEC/CRC e testo coincidono con la decodifica nota. Li sottrae con il FIR e cerca nel primo residuo.
3. Affina e sottrae i modelli supplementari, quindi cerca nel secondo residuo. Entrambe le ricerche condividono la stessa scadenza; il secondo stadio non acquista un nuovo budget.
4. Ogni ricerca ammette al massimo 384 candidati ed esclude le posizioni già note. Aggiunge soltanto messaggi nuovi che superano tutti i controlli; oltre 36 bit discordanti fra decisioni dure e parola decodificata, rifiuta il nuovo candidato.
5. Un errore nella preparazione dello spettro salta il passaggio facoltativo. I risultati precedenti e quelli già aggiunti restano disponibili.

Il recupero non parte nel gate anticipato, con DSP Plus disattivato, con GPU ordinaria attiva, dopo cancellazione o senza tempo residuo. Il limite live è UTC del periodo + 15 s + 1050 ms, con margini di ammissione; il budget per candidato resta 24 ms. La modalità offline concede al solo nuovo recupero fino a 3000 ms e 96 ms per candidato, evitando che il limite live condizioni il confronto di sensibilità. I controlli sono cooperativi, fra operazioni finite: non costituiscono una garanzia di schedulazione del sistema operativo.

L'evento diagnostico ft.native.recovery riporta candidati, aggiunte, worker, durata e raggiungimento del limite. I contatori nativeRecoveryCandidates, nativeRecoveryDecodes, nativeRecoveryPasses, nativeRecoveryMs e nativeRecoveryCutoff sono separati dal tail coerente precedente.

## Algoritmi C++

- **Conversione spettrale FT8.** Una FFT reale da 192000 punti viene calcolata una volta per buffer immutabile. Ogni candidato ritaglia la banda da −1,5 a +8,5 spaziature rispetto alla base, con taper coseno, e torna nel tempo con 3200 campioni complessi a 200 Hz. I buffer più corti sono completati con zeri. Ogni residuo riceve un nuovo spettro; il puntatore del buffer non è una chiave di cache.
- **Sincronismo coerente.** La correlazione Costas affina il tempo e la frequenza, con ricerca ±2,5 Hz a passi di 0,25 Hz. La rotazione complessa conserva la fase fra simboli; DT e frequenza della metrica riuscita aggiornano il modello SIC.
- **Inviluppo FIR.** La convoluzione centrata cos² di 4000 campioni usa somme mobili DC/coseno/seno, con costo lineare. Normalizza la massa ai bordi validi e azzera il resto. Opera soltanto sul nuovo residuo FT8. Il supporto matematico per 1400 campioni FT4 è verificato, ma non è stato adottato nel SIC FT4: la matrice disponibile non ne dimostrava un vantaggio.
- **Min-sum LDPC.** Due minimi e il prodotto dei segni sostituiscono la scansione quadratica dei vicini. Il calcolo è condiviso da FT8/FT4 e conserva scala 0,78, clipping, iterazioni, parità e CRC. Il confronto indipendente su 60000 righe comprende pareggi e zeri con segno e verifica l'identità bit per bit.
- **Concorrenza FFT.** Lo spettro è immutabile e condiviso; buffer e piano inverso sono propri di ogni thread. Un mutex comune serializza il planner FFTW anche rispetto al recupero R30. Puntatore al contesto e scadenza vengono ripristinati a fine scope.

Il corpo del metodo Doppler resta identico alla R30. FT4 conserva i suoi algoritmi di ricerca, metriche coerenti e SIC; la R31 non presenta il nuovo front-end FT8 come un miglioramento di sensibilità FT4 già dimostrato.

## Esperimenti rifiutati

La sostituzione diretta del front-end dei passaggi principali guadagnava sul tester ma perdeva alcune righe su altre registrazioni, soprattutto nelle prove live. È stata sostituita dal recupero additivo. Riutilizzare la stessa banda stretta per tutte le metriche successive faceva perdere TT1GD RA6OB KN98: rimane il filtraggio distinto del recupero coerente, sempre dentro il decoder nativo.

La differenza delle ampiezze come prima metrica produceva due righe non confermate senza nuove corrispondenze JTDX ed è stata scartata. Una prima normalizzazione FIR fuori dall'intervallo valido era instabile per masse quasi nulle: l'output ora è limitato all'intervallo valido e confrontato con convoluzione diretta.

Sonde, varianti e dati grezzi sono conservati nel checkpoint. I messaggi attesi del tester sono utilizzati soltanto per valutare gli output: non entrano nella ricerca, nei bit a priori o nelle priorità di produzione.

## Riproduzione

CTest aggiunge FIR contro convoluzione diretta, sincronismo complesso con offset noti, conversione spettrale concorrente, equivalenza min-sum e front-end su frame GFSK/rumore. La prova del front-end comprende anche l'intero percorso con due segnali e la conservazione dei risultati pubblicati nello stesso job.

Strumentazione del ricevitore nativo e della prova:

    python3 tools/run_ft_coherent_regression.py --build build --probe FtNativeFrontendRegression.cpp --sanitize

L'opzione --leaks abilita LeakSanitizer su sistemi in cui l'ispezione dei thread è disponibile. Le librerie esterne non vengono strumentate automaticamente.

Isolamento del core nativo per il confronto:

    python3 tools/run_ft_replay_probe.py --build build --native-only --compile-only

Un confronto con R30 richiede il suo albero o binario congelato. Disabilitare il recupero supplementare nella R31 misura il nativo R31. La sonda di replay espone separatamente gate, boundary, tempi e righe; non sostituisce una prova RX/TX con radio. I risultati misurati e i limiti sono nel rapporto di verifica R31.
