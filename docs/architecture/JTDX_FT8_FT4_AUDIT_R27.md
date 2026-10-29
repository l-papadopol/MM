# Confronto del motore JTDX con MadModem — R27

## Fonte e perimetro

È stato scaricato **l'intero repository pubblico JTDX**, con cronologia Git:
[repository ufficiale](https://github.com/jtdx-project/jtdx).

- Commit analizzato: `2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb`.
- Data del commit: 1 marzo 2022, serie ufficiale 2.2.159.
- Albero scaricato: 1.141 file tracciati, 70.376.744 byte, esclusa la cronologia.
- Inventario del percorso FT8/FT4 e helper: **48 file, 15.743 righe**, con hash e
  chiamate dirette in `JTDX_R27_SOURCE_INVENTORY.json`.
- Il repository `jtdx-devel` citato nella cronologia non è pubblicamente
  accessibile. Il sorgente pubblico non viene presentato come una build futura
  o come prova della versione esatta utilizzata dal tester.

L'analisi ricostruisce il percorso completo FT8/FT4: selezione dell'intervallo,
pretrattamento, ricerca, sincronismo fine, banda base, metriche, FEC, assistenza
contestuale, memoria, cancellazione, filtri del messaggio e pubblicazione.
Non è una revisione della UI o di tutti i modem legacy di JTDX. **Non è stato
compilato/eseguito JTDX in questo ambiente:** i risultati 66 contro 43 sono il
feedback del tester, non un nuovo benchmark JTDX effettuato qui.

Il riferimento MadModem è la R26 consegnata; la R27 aggiunge diagnostica senza
cambiare intenzionalmente i criteri DSP/FEC, la profondità o i tempi limite.

## Percorso JTDX ricostruito

1. `lib/decoder.f90`, `multimode_decoder`: trasferisce i parametri della UI,
   gestisce intervalli incompleti e cambio banda, prepara hash/callsign e stato
   QSO, eventuale AGC e centratura temporale media. In FT8 suddivide la banda in
   sezioni OpenMP e chiama il decoder per ciascuna. Mantiene separati i periodi
   pari/dispari e la memoria dei segnali/decodifiche precedenti.
2. `lib/ft8_decode.f90`: esegue **3, 6 o 9 passaggi di ricerca larga** secondo i
   cicli configurati. Soglie e criteri cambiano fra passaggi; i cicli aggiuntivi
   provano anche varianti temporali del residuo. Non sono nove decoder identici.
3. `lib/sync8.f90`: costruisce la mappa tempo/frequenza, considera i Costas e
   indizi della struttura CQ, normalizza sul fondo, ordina i candidati e applica
   deduplicazione/assottigliamento. Prevede priorità QSO e candidati virtuali
   assistiti. I limiti per sezione non equivalgono al limite totale di MadModem.
4. `lib/ft8_downsample.f90`: usa una FFT lunga da 192.000 punti riutilizzata;
   ritaglia la banda intorno al candidato e torna nel tempo a **200 campioni
   complessi/s**, cioè 32 campioni per simbolo. Il residuo modificato invalida
   opportunamente la cache nei dintorni delle frequenze sottratte.
5. `lib/ft8b.f90`, `lib/sync8d.f90`: affinano DT e frequenza sulla banda base.
   Nel percorso ordinario la griglia fine in frequenza arriva a passi di 0,5 Hz;
   altri percorsi usano 0,25 Hz. Cambiano funzione di sincronismo fra passaggi e
   possono usare simboli noti/precedenti, non soltanto i 21 toni Costas standard.
6. `ft8b`: costruisce metriche coerenti su **1, 2 e 3 simboli**, una famiglia
   normalizzata per rapporto e varianti per segnali deboli. Le combinazioni
   sommano correlazioni **complesse**, preservando la fase prima del modulo.
   È diverso dal sommare le sole ampiezze di più simboli.
7. `lib/ft8v2/bpdecode174_91.f90`: BP sum-product con maschera AP, controllo CRC
   integrato e arresto anticipato; FT8 imposta 30 iterazioni massime. Se necessario
   interviene `osd174_91.f90`, con diversi livelli e pre-elaborazioni. I livelli
   4/5 ammettono ordine 2, i livelli 1/2/3 ordine 1 con differenti estensioni;
   l'ordine più alto non è applicato indiscriminatamente a tutta la banda.
8. `ft8apset`, `ft8s`, `ft8sd`, `ft8sd1`, `ft8mf1`, `ft8mfcq` e strutture in
   `ft8_mod1`: usano conoscenza del QSO, nomi/hash, messaggi e anche correlazioni
   complesse dei periodi precedenti. Esistono rami di confronto con messaggi
   plausibili e memoria dei CQ. Non è un decoder sempre cieco sul solo WAV
   corrente, nemmeno una semplice tabella dei soli due nominativi del QSO.
9. `ft8b` e `lib/ft8v2/subtractft8.f90`: ricostruiscono il GFSK, rifiniscono il
   tempo dopo la decodifica e stimano l'inviluppo complesso tramite filtraggio
   FFT. `cwfilter.f90` prepara finestre e correzioni ai bordi:4000 campioni nel
   percorso normale,3400nella variante SWL. La sottrazione alimenta nuove
   ricerche; i passaggi tardivi non sottraggono sempre.
10. `packjt77`, CRC e controlli `chkfalse8`, `chkflscall`, `chkgrid` filtrano
    protocollo, nominativi e griglie, poi arrivano callback e deduplicazione.
    Il trattamento dei falsi riconoscimenti fa parte del progetto, specialmente
    nei rami assistiti. Non basta confrontare il numero di stringhe stampate.

FT4 segue `lib/ft4_decode.f90` e gli helper `lib/ft4/`: FFT/ritaglio in banda
base a 666,67 campioni/s (32 per simbolo), sincronismo fine, metriche coerenti su
**1/2/4 simboli**, BP/AP/OSD e sottrazione. Il profilo di profondità seleziona
1 passaggio senza sottrazione, oppure 3 passaggi; il livello più profondo aggiunge
OSD. BP usa 40 iterazioni massime. La ricerca si arresta quando i passaggi
precedenti non producono materiale utile. Non è una scansione illimitata.

## Differenze concrete rispetto a MadModem

| Stadio | JTDX esaminato | MadModem R26/R27 | Implicazione da verificare |
| --- | --- | --- | --- |
| Front-end candidato FT8 | Banda base complessa 200 Hz, FFT lunga riusata | Primo percorso Goertzel su audio 12kHz; banda base 200 Hz nel recupero coerente | Costo e conservazione della fase differiscono già prima della FEC |
| Metriche FT8 | Famiglie coerenti ordinarie 1/2/3, varianti e contesto | Primo percorso log-potenza e famiglie di ampiezza; coerente nel percorso offline o nella coda limitata a 32 fallimenti | Il segnale debole può non arrivare al tentativo coerente live |
| Metriche FT4 live | Famiglie coerenti1/2/4 nel decoder normale | Il codice coerente esiste ma richiede `m_offlineAnalysisActive`; il normale live ha questo flag falso | Lacuna funzionale precisa del percorso live, non assenza dell'algoritmo nel sorgente |
| Analisi WAV FT4 | Percorso JTDX FT4 con le proprie opzioni | `analyzeAudioFile` lascia deliberatamente falso quel flag per FT4 | Anche il normale pulsante WAV FT4 non esercita il ramo coerente interno |
| BP | Sum-product, maschere AP,30 iterazioni FT8/40 FT4 | Min-sum normalizzato0,78 fino a 35 iterazioni; sum-product disponibile ma limitato al flag offline | Cambiano convergenza, fallimenti e selezione dei candidati da recuperare |
| OSD | Ordine1 con pre-estensioni; ordine 2 in rami/profili selezionati | Recupero GF(2) limitato per numero/tempo/permessi, ordine 1; ordine 2 disabilitato | Un candidato plausibile può essere respinto per budget oltre che per evidenza radio |
| Storia/assistenza | Stato pari/dispari, segnali precedenti, CQ, QSO, AP e matched filtering | Assistenza e cache più circoscritte; nessuna equivalente memoria generale della banda | Confrontare singolo WAV freddo con ricezione JTDX già avviata non isola la sola sensibilità |
| Cancellazione | Filtro FFT finito, correzioni ai bordi e rifinitura temporale dedicata | Inviluppo esponenziale avanti/indietro O(N); rifinitura temporale FT8 limitata al flag offline | Residui diversi possono nascondere il secondo segnale sovrapposto |
| Finestra temporale FT8 | Ricerca traslata daDTmedio e buffer complessi con margini azzerati | Inizio candidato non negativo e rifiuto se non entra il fotogramma con margine | Segnali anticipati o tagliati possono non essere nemmeno tentati |
| Passaggi/budget |3/6/9 ricerche, molte diramazioni condizionali per candidato | Matrice adattiva, residuale/coda e scadenze live esplicite | Non confrontare direttamente il solo contatore `passCount` fra i programmi |
| Parallelismo | Sezioni di banda OpenMP, fino a 24 thread nel sorgente esaminato | Pool di lavoro per candidati, riserve audio/UI e permessi OSD | Più thread non rende automaticamente equivalenti le ricerche |

Queste differenze **sono verificabili nel codice**. Il contributo numerico di
ciascuna ai25 messaggi ancora mancanti nel replay R26 non è stato misurato.
Sarebbe scorretto attribuire tutto il divario a un solo parametro, al clock,
alla GPU o a una specifica tecnica di decodifica.

### Due punti che richiedono particolare attenzione

**Coerenza di fase:** il codice MadModem `computeGroupedMetric` delle famiglie
iniziali raggruppa ampiezze reali. Il ramo successivo `coherentMetricRaw` somma
numeri complessi. Le due operazioni non sono intercambiabili. Accendere più
thread o aumentare il numero dei candidati non ricostruisce la fase persa.
Il downsampling60:1 di JTDX descrive il numero dei campioni FT8: **non** è una
misura di un'accelerazione cronometrata60 volte.

**Assistenza e storia:** alcuni rami JTDX confrontano il segnale con messaggi
plausibili ricostruiti, oltre ai tentativi BP/CRC autonomi. Ciò non dimostra che
le righe aggiuntive siano false. Significa che il confronto corretto deve
specificare versione, impostazioni, stato precedente e tipo di decodifica.
La sola voce Hint non descrive tutta la macchina di stato e le cache.

## Cosa possiamo concludere dal materiale del tester

Il replay R26 ha misurato38–39 messaggi con il decoder precedente e41con la
correzione dell'esclusione25 Hz. Il riferimento JTDX ne elenca66. La registrazione
è21,75 s e contiene un solo periodo completo confrontabile. Non ricostruisce la
memoria di una lunga ricezione JTDX, né sono noti versione/opzioni del tester.
Il suo log MadModem termina all'avvio RX e non documenta il clock/decoder live.

Il nuovo log R27 serve a distinguere:

- mancanza o alterazione dei campioni prima del decoder;
- candidati non ammessi dalla ricerca o dalla pre-selezione;
- fallimento del sincronismo/LLR, LDPC, CRC, unpack o messaggio;
- recupero saltato per budget, permessi o scadenza;
- successo del decoder non pubblicato perché duplicato, consegnato tardi o
  diverso dalla riga effettivamente visualizzata.

Il fatto che MadModem trovi una riga che JTDX non mostra è compatibile con
ricerche e filtri differenti. Non prova né equivalenza dei motori né falsità di
quella riga: occorre controllare segnale, CRC, contesto e ripetibilità.

## Ordine proposto per gli interventi successivi

1. **Banda base e metriche coerenti:** rendere utilizzabile il ramo FT4 nel live
   finale con scadenza; per FT8 riusare banda base/correlazioni e ampliare in modo
   misurato i fallimenti che arrivano alle metriche coerenti. Proteggere gate e
   sequencer. Confrontare sullo stesso audio, profilo e stato.
2. **BP/OSD confrontabili:** provare sum-product e OSD con gli stessi ingressi
   LLR, separando vantaggio del demodulatore e della FEC. Misurare recuperi
   reali per costo e falsi riconoscimenti; evitare di alzare ciecamente tutte le
   soglie o la profondità.
3. **Cancellazione e margini:** misurare il residuo prima/dopo ogni cancellazione
   e i segnali anticipati/tagliati. La variante Hann provata in R26 su un solo
   WAV non è una validazione generale e non è inclusa nella release.
4. **Storia e AP:** confrontare avvio freddo, sequenze consecutive e stato QSO
   con opzioni documentate. Un porting della memoria di banda richiede test
   specifici dei falsi positivi, non soltanto più righe in uscita.

Criterio di accettazione: messaggi corretti aggiunti su registrazioni diverse,
risultati pubblicati conservati, comportamento su silenzio/rumore, tempi entro
la risposta utile e prova live. Una sola registrazione non basta per validare
un nuovo motore. La R27 consegna la diagnostica per effettuare questi confronti;
non dichiara colmato il divario con JTDX.

## Riferimenti riproducibili

Tutti i percorsi si riferiscono al commit fissato, non a `master` mobile:

- [Dispatcher e memoria](https://github.com/jtdx-project/jtdx/blob/2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb/lib/decoder.f90)
- [Passaggi FT8](https://github.com/jtdx-project/jtdx/blob/2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb/lib/ft8_decode.f90)
- [Demodulatore FT8/AP](https://github.com/jtdx-project/jtdx/blob/2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb/lib/ft8b.f90)
- [Banda base FT8](https://github.com/jtdx-project/jtdx/blob/2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb/lib/ft8_downsample.f90)
- [BP](https://github.com/jtdx-project/jtdx/blob/2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb/lib/ft8v2/bpdecode174_91.f90)
- [OSD](https://github.com/jtdx-project/jtdx/blob/2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb/lib/ft8v2/osd174_91.f90)
- [Cancellazione FT8](https://github.com/jtdx-project/jtdx/blob/2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb/lib/ft8v2/subtractft8.f90)
- [FT4](https://github.com/jtdx-project/jtdx/blob/2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb/lib/ft4_decode.f90)
- [Metriche coerenti FT4](https://github.com/jtdx-project/jtdx/blob/2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb/lib/ft4/get_ft4_bitmetrics.f90)

Il sorgente JTDX resta nel suo repository GPL ufficiale; non è stato incorporato
nel motore MadModem in questa release.
