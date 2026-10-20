# MadModem 0.6.4 source R11 — budget completo RX/UI/sequencer/TX

Revisione `0.6.4-source-r11-ft-reply-deadline-coherent-tail`.
Questa revisione rivaluta la decisione R10: più tempo di CPU è accettabile se i
messaggi aggiuntivi arrivano entro la scadenza operativa. Una mediana più alta,
da sola, non giustifica il rifiuto di un recupero.

## UI conservata e verificata

Le due barre I/II sostituiscono il quadrante. UTC digitale conservato; gruppo
clock 84px invece di 168px, barre 188×48px nella sidebar 426px. Font 14→8 applicato
alle celle esistenti e ai nuovi CQ/TX/storico, conservando grassetto e barrato.
Prove FT8/FT4 PASS anche dopo questa modifica. CW Native R12/Neural R13 invariati.

## Tempi effettivi letti dal codice

Tutti i tempi della tabella sono riferiti all'inizio dello slot RX.

| Evento | FT8 | FT4 |
|---|---:|---:|
| Durata slot |15.000s|7.500s|
| Avvio decoder rapido/gate |13.500s|6.100s|
| PTT pre-arm nominale per la risposta |14.350s|6.850s|
| Boundary e avvio recupero completo |15.000s|7.500s|
| Primo tono nominale della risposta |15.500s|7.800s|
| Ultimo armamento automatico ammesso da MM con frame intero |16.340s|8.940s|
| Obiettivo fine recupero aggiuntivo, con 190ms riservati |16.150s|8.750s|

Per raggiungere il pre-arm nominale, gate→UI→sequencer dispone di 850ms FT8 e
750ms FT4. Il generatore prepara la waveform in un worker separato; il PTT
ha 650ms di anticipo nominale. La risposta può essere armata prima della fine del
job: il QSO attivo consuma le pubblicazioni progressive già CRC-valide.

Il limite tardivo di MM deriva dalla stessa formula usata dal TX:
`slotMs - signalMs - 120ms allowance - 700ms preparazione - 200ms guardia`.
Risultato: 1340ms FT8/1440ms FT4 dopo l'inizio dello slot TX. La durata del frame
usata dai profili è 12640ms FT8 e 5040ms FT4. Questi limiti mantengono il frame
intero nel periodo: l'avvio nominale resta l'obiettivo, e una partenza tardiva
produce un DT maggiore che dipende anche dalla finestra del ricevitore remoto.
Non confondere il limite di ammissione di MM con un nuovo timing del protocollo.

Il budget consigliato per il solo recupero post-boundary è quindi circa 1150ms
FT8 e 1250ms FT4, riservando 150ms a coda GUI/decisione e 40ms di guardia. FT4 usa
la policy condivisa per il TX ma NON riceve un nuovo recupero coerente live.

## Modifica DSP inclusa

Solo FT8: dopo tutte le passate normali, la sottrazione e l'eventuale recupero
drift, ritentare fino a 32 candidati LDPC/CRC falliti nella prima passata, in
ordine di ranking originale. Il retry usa metriche coerenti sull'audio originale,
senza nuova scansione, senza nuovi GF2 OSD, senza sostituire risultati acquisiti.

Prima di ogni candidato viene controllato il cutoff UTC derivato dallo slot RX,
oltre a cancellazione e budget esistente. Un job differito non riparte con un
nuovo budget. Il cutoff è preventivo fra candidati: un tentativo LDPC in corso
può terminare oltre l'obiettivo; la guardia e i controlli finali TX rimangono attivi.
Il percorso rapido/gate e le sue priorità QSO restano invariati.

Il log riporta candidati ritentati, nuovi messaggi effettivi, tempo del tail e
margine residuo al cutoff di armamento quando la GUI riceve i risultati finali.
Il contatore dei nuovi messaggi esclude i recuperi coerenti duplicati.

## Confronto nelle stesse esecuzioni

Sette WAV radio reali da kgoba/ft8_lib/test/wav, tre ripetizioni. Hash, messaggi e
tempi completi in `R11_FT_TIMING_RESULTS.json`. La baseline è il risultato delle
passate ordinarie nello STESSO job, prima del recupero aggiuntivo: evita di
attribuire al tail le variazioni fra due esecuzioni con scheduling OSD diverso.

| Misura, 21 slot | Risultato |
|---|---:|
| Righe delle passate normali |403|
| Nuove righe del recupero |+6|
| Totale finale |409|
| Tempo totale mediano |770.4ms|
| Tempo totale massimo |832.8ms|
| Costo massimo del solo tail |80.5ms|
| Minimo margine fino all'armamento, prima della GUI |507ms|

Sono righe di decodifica, NON stazioni uniche e NON la risoluzione del confronto
JTDX 50/MM 37. I sei recuperi provengono dalla ripetizione di un WAV: corpus piccolo,
non una stima generale di sensibilità. Nel controllo aggiuntivo, tutti i messaggi
pubblicati dalle passate precedenti restano nel risultato finale. Con RX boundary
già vecchio di 2000ms il tail tenta 0 candidati: nessun budget viene rinnovato.

## Misura UI e risposta

Test reali della MainWindow e del sequencer, con radio e uscita audio disabilitate;
il generatore TX asincrono è reale. Il numero include coda evento, aggiornamento
tabelle, decisione del sequencer e waveform pronta, non l'ack di una radio fisica.
Su due esecuzioni, con 51 righe il massimo è circa 100ms, con 101 circa 136ms:
150ms è la riserva minima adottata per queste fasi in questa build.

Esecuzione R11 con sei segnali sintetici:

| Percorso | Decoder totale | Risposta armata | Waveform pronta |
|---|---:|---:|---:|
| FT8 wsjtx-gate |282.6ms|249.9ms|263.9ms|
| FT8 boundary |578.2ms|396.3ms|415.2ms|
| FT4 wsjtx-gate |171.8ms|nessuna risposta,5/6|—|
| FT4 boundary |242.1ms|161.3ms|171.0ms|

Il segnale diretto FT4 non viene recuperato dal gate in questo fixture; viene
recuperato dal boundary. Questo rende necessario valutare anche il percorso
post-boundary e non assumere che il gate trovi sempre il corrispondente.
La waveform pronta prima del termine del decoder conferma la pubblicazione
progressiva per il QSO attivo. Nessun test autorizza l'avvio RF fuori scadenza.

## Verifiche e limiti

- Build Linux Qt5 Release PASS; suite FT 6 e release 8 PASS.
- Policy C++: soglie esatte 1340/1440, riserva 190ms, deadline ancorata allo slot.
- FT8/FT4: barre/font, risposta finale 73, timeout obsoleto, mancata risposta,
  QRZ, preview senza TX e deduplica 95 righe PASS.
- Pubblicazione sei segnali FT8/FT4 passiva e QSO attivo: 6/6 PASS.
- Replay con deadline scaduta e conservazione messaggi pubblicati PASS.
- CAT/PTT, apertura backend audio e radio fisica non misurati qui; la riserva
  di preparazione 700ms è una regola esistente, non una misura del tuo driver.
- Build native Windows/macOS e audio originale del tester restano da verificare.
  FT4 coerente rimane offline finché manca un confronto reale adeguato.

Sonde: `FtEndToEndTimingProbe.cpp`, `FtRecordedReplayProbe.cpp` (diagnostiche,
collegate agli oggetti app con `-fno-access-control`, eseguire in cartella di
prova isolata); `FtTimingPolicyRegression.cpp` è autonomo e usa static_assert.
I replay mappano l'inizio del boundary all'UTC corrente, senza simulare nuove
scadenze per audio vecchio. WAV e build non sono inclusi nello ZIP sorgenti.
