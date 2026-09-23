# MadModem 0.5.9-alpha — sorgenti R6

23 settembre 2026. Base: archivio completo R5, revisione
`0.5.9-alpha-source-r5-real-rotator-peak-tracking`.
Revisione consegnata: `0.5.9-alpha-source-r6-runtime-ui-rotator-hardening`.

## Correzioni

- **Rotore / Parcheggia:** il parcheggio usa la posizione configurata e lo stesso percorso validato di Vai. La conversione delle coordinate avviene prima di Hamlib: per geometrie a un giro, −45° diventa 315° se il backend accetta 0–360°. I target multigiro fuori dal dominio del backend vengono rifiutati, non ridotti perdendo l'informazione sul giro. Il percorso mantiene il monitoraggio del movimento.
- **Impostazioni:** rimossi i tre percorsi che forzavano il fullscreen. La finestra è ridimensionabile e inizialmente contenuta nell'area disponibile dello schermo. Utente/QTH e macro sono disposti verticalmente in una pagina scorrevole; anche i gruppi del profilo rotore sono verticali. Le pagine incorporate consentono lo scorrimento quando necessario. I pulsanti incorporati rispettano la misura richiesta dal testo; il pannello rotore è scorrevole e i pulsanti di tracking lunghi hanno una riga completa.
- **Traduzioni:** il filtro di validazione non scarta più indiscriminatamente stringhe contenenti `/`, come Audio/PTT. Restano le verifiche sui frammenti di sorgente generati erroneamente.
- **TX:** sorgente PCM finita, conteggio esatto del silenzio finale, volume atomico, fine naturale subordinata allo stato Idle del sink e all'esaurimento della sorgente. Controllo degli errori del backend all'avvio e durante la trasmissione; invalidazione dei callback di output precedenti. La trasmissione FT preparata passa dal worker dedicato con controllo della scadenza prima dell'avvio. Corretto il reset di `m_txFinishedNaturally` nel percorso di errore.
- **Ricampionamento:** FIR anti-alias prima dell'interpolazione, stato persistente fra blocchi, ritardo esposto e compensato nella timeline Q65/MSK144.
- **RX e code:** code limitate per FT, Q65 e recorder. Il Q65 riceve direttamente dalla cattura nel proprio worker. Su sovraccarico il recorder interrompe e segnala l'errore. I consumer GUI e il waterfall riconoscono cambi di generazione/frequenza di campionamento e salti nell'indice; il condizionatore conserva la generazione della cattura.
- **Waterfall:** riuso dei buffer FFT e rimozione del prefisso FIFO una volta per blocco, invece che a ogni finestra.
- **Logbook:** parser EOH rispettoso delle lunghezze, protezione da lunghezze fuori limite, conservazione di spazi e date mancanti, lock e controllo SHA-256 contro sovrascritture da copie obsolete. Indice nominativi aggiornato incrementalmente. Percorso dati utente predefinito, mantenendo il log portable già esistente o la scelta tramite `portable.flag`.
- **Build:** compatibilità dei token Hamlib verificata con 4.5.5; test FT abilitati con BUILD_TESTING; nuove regressioni native e test GUI offscreen. Workflow GitHub Linux con matrice Qt 5/6 e diagnostica CTest conservata.

## Compilazione e GitHub

Estrarre l'archivio. Copiare **il contenuto** della cartella sorgenti nella radice
del repository, includendo `.github/workflows/source-regression.yml`. Il workflow
parte su push, pull request e avvio manuale. Non è stato eseguito un push da questa
sessione. La matrice Qt 6 va confermata dall'esecuzione CI; la verifica locale è Qt 5.

Con dipendenze Qt, Hamlib, compilatore C++17, Ninja e CMake installati:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMADMODEM_REQUIRE_HAMLIB=ON -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Per acquisire le schermate delle impostazioni senza aprire dispositivi radio:

```sh
QT_QPA_PLATFORM=offscreen MADMODEM_UI_CAPTURE_DIR=ui-captures ./build/MadModem --ui-regression
```

I risultati effettivi della consegna sono in `docs/R6_TEST_RESULTS.txt`.

## Rilievi ancora aperti

Questa revisione corregge i difetti sopra elencati; non certifica che ogni bug
dell'app sia stato eliminato. L'audit iniziale resta consultabile in
`docs/R5_AUDIT_INIZIALE.md` come fotografia della base prima degli interventi.

- MainWindow conserva responsabilità e stato TX duplicato; l'estrazione di controller RX/TX/QSO non è completata.
- MSK144 e diversi decoder non FT restano orchestrati dal thread GUI. La correzione del trasporto Q65 non completa la migrazione di tutti i modi.
- Le chiamate CAT bloccanti e la sequenza di shutdown possono ancora ritardare la GUI. Il controllo della scadenza FT rifiuta un avvio già tardivo, ma non rende asincrono il CAT né garantisce il momento fisico del primo campione.
- Il logbook conserva la riscrittura atomica integrale O(N): il lock impedisce di perdere scritture concorrenti, ma non elimina il costo su log molto grandi. Il conflitto richiede di ricaricare prima di riprovare; non viene eseguito un merge automatico.
- Le copie third_party identificate nell'audit non sono state eliminate senza dimostrare che siano superflue nella build/distribuzione.
- La copertura Q65/MSK144 resta in parte sintetica; non sostituisce corpus RF, misure di sensibilità o benchmark sotto carico.

## Prove sull'hardware e desktop da completare

1. Prosistel: da 10° provare Parcheggia con il parcheggio configurato a 315°; verificare posizione finale, STOP e limiti meccanici. Provare anche Vai e target ai limiti senza cambiare arbitrariamente il cablaggio o i limiti dell'impianto.
2. Loopback audio e PTT: controllare ultimo simbolo e rilascio PTT in FT8/FT4, Q65, MSK144 e modi continui; scollegare l'uscita durante TX e verificare il rilascio. Lo stato Idle di Qt non è una misura strumentale dell'uscita RF.
3. Windows/macOS e DPI elevato: finestre impostazioni, pannello rotore, tutte le lingue e i temi. Il test offscreen Linux non riproduce le decorazioni del window manager o ogni font di sistema.
4. Sessione lunga RX/TX con CAT lento, carico CPU/disco e log grande: misurare latenza e perdita blocchi. Il FIR aggiunge costo di calcolo, non è stata dimostrata una riduzione complessiva del carico CPU.
