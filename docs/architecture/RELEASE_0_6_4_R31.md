> Historical first R31 handoff. The current state is [R31 clean](RELEASE_0_6_4_R31_CLEAN.md).

# MadModem 0.6.4 — sorgenti R31

Consegna di sviluppo del 30 ottobre 2026. Il pacchetto contiene l'ultima variante compilata, basata sulla R30 recuperata. Versione pubblica 0.6.4; revisione sorgenti 0.6.4-source-r31-native-ft-front-end. È pronto per essere caricato come sorgente su GitHub e proseguire lo sviluppo; la parità generale con JTDX non è ancora raggiunta.

## Modifiche effettive

- Recupero aggiuntivo nello stesso decoder C++ FT8: conversione spettrale a 200 Hz, affinamento coerente Costas, rifinitura dei segnali noti e nuovo residuo SIC con inviluppo cos². Opera dopo i risultati del percorso esistente e aggiunge nuove righe.
- Min-sum LDPC equivalente con costo lineare per riga; planner FFTW sincronizzato fra il nuovo codice e il recupero già presente.
- Scadenza live UTC e budget offline distinti; cancellazione controllata anche durante la ricerca aggiuntiva.
- Nessun ulteriore motore o runtime Fortran introdotto. Il recupero MSHV presente nella R30 è conservato. I riferimenti studiati sono WSJT-X, JTDX e MSHV.
- Correzione Doppler mantenuta. Il corpo del relativo metodo e i metodi FT4 di ricerca, decodifica e sottrazione sono identici alla R30. Gli altri modem e le funzioni CAT/PTT/sequencer/logbook non sono stati modificati.

## Risultati disponibili

Stessa estrazione del tester e otto worker; confronto con il binario della R30 congelata. Due ripetizioni per scenario, in ordine alternato. Si confrontano i testi esatti con i 66 messaggi JTDX forniti dal tester.

| Scenario sul tester | R30 | R31 | Corrispondenze JTDX R30 → R31 |
| --- | ---: | ---: | ---: |
| Offline, ripetizione 1 | 60 | 61 | 55 → 56 |
| Offline, ripetizione 2 | 60 | 62 | 55 → 57 |
| Gate + boundary, ripetizione 1 | 52 | 51 | 48 → 47 |
| Gate + boundary, ripetizione 2 | 51 | 49 | 47 → 46 |

Offline le 60 righe R30 sul tester sono conservate. VU2XZ PD9DY JO22 è recuperato nelle due ripetizioni; TT1GD RA6OB KN98 in una delle due. Le cinque righe ulteriori non confermate della R30 rimangono tali e non sono conteggiate come corrispondenze JTDX. Restano 9–10 messaggi attesi non recuperati.

Sulle otto registrazioni offline rimangono aggiunte e variazioni fra esecuzioni, riportate integralmente in FT_R31_RESULTS.json e FT_R31_MESSAGE_COMPARISON.csv. Le prove live non dimostrano un miglioramento: il nuovo recupero spesso non viene ammesso perché il percorso precedente ha già consumato il tempo disponibile. Il confronto conserva anche le righe perse fra esecuzioni; non si dichiara assenza generale di regressioni live.

FT4 conserva gli algoritmi precedenti. La variante FIR FT4 studiata non dava vantaggi misurabili nella matrice disponibile e non è stata adottata. La R31 non certifica parità FT4 né sensibilità assoluta su segnali reali affollati.

## Verifiche eseguite e stato

- Applicazione completa Linux Qt5 Release compilata.
- 58 regressioni CTest superate complessivamente: 57 nella prima esecuzione; il controllo statico del finder è stato adattato alla sola nuova cancellazione e ripetuto con successo, insieme a spettro e front-end. Gli hash della matematica della ricerca precedente restano invariati.
- Prove numeriche: 120 convoluzioni FIR indipendenti, 18 sincronismi complessi, otto traslazioni spettrali con buffer concorrenti, 60000 righe min-sum identiche alla formula precedente.
- Front-end: nove frame generati con offset di frequenza/tempo, dodici controlli di rumore, un percorso completo a due segnali e conservazione dei risultati pubblicati nello stesso job.
- ASan/UBSan delle quattro primitive matematiche superati nelle prove di sviluppo. Il ricevitore completo aggiornato non è stato ancora rieseguito sotto sanitizzatori.

Su richiesta dell'utente le ulteriori prove sono state fermate per consegnare i sorgenti. Non sono state completate sulla R31 finale la matrice FT4 completa, la ripetizione dei 46 casi Doppler, le sonde dedicate di risposta UI/runtime, la build Qt6 e le build native Windows/macOS. I risultati storici R30 restano documentati separatamente e non vengono attribuiti alla R31.

## Prossimo passo concreto

La priorità è rendere ripetibile il guadagno nativo e recuperare il divario live, lavorando sul residuo e sul costo dei passaggi precedenti. È stata abbozzata una variante con due residui consecutivi: prima i soli modelli nativi, poi quelli supplementari. Non è stata ancora compilata ed è conservata nel checkpoint, separata dai sorgenti di produzione di questo ZIP. Serve inoltre un corpus FT4 reale con risultati di riferimento per misurare la parità.

## File utili

- FT_NATIVE_REIMPLEMENTATION_R31.md: algoritmi, limiti temporali e scelte adottate.
- WSJTX_R31_SOURCE_INVENTORY.json: sorgenti WSJT-X identificati da commit e SHA256.
- FT_R31_RESULTS.json e FT_R31_MESSAGE_COMPARISON.csv: risultati e singoli messaggi.
- r31-validation/: log e output grezzi dei confronti conclusi.
- ../platform/BUILD_R31.md: compilazione e comandi disponibili.
- SOURCE_MANIFEST_SHA256.txt nella radice: inventario dei file consegnati.
