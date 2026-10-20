# MadModem 0.6.4 source R10 — risultati e limiti

Revisione `0.6.4-source-r10-ft-dt-period-bars-fonts-coherent-cache`.

## Modifiche incluse

- Il quadrante FT8/FT4 è sostituito da due barre orizzontali I/II, con colore,
  indicazione del periodo attivo e tempo RX/TX. UTC digitale conservato.
  Nella prova con barra laterale di 426px il gruppo scende da 168 a 84px;
  barre 188×48px. Nessuna modifica ai tempi dello scheduler.
- Il font scelto per le tabelle si applica anche alle celle esistenti, ai nuovi
  CQ, alle righe TX e allo storico. Grassetto e barrato restano conservati.
- Il DT FT8 riportato usa il riferimento nominale 0.5s: un inizio a 0.5s
  produce DT 0.0s. I Candidate mantengono il tempo assoluto per SIC.
  Anche il tie-break interno conserva il vecchio riferimento assoluto:
  la correzione del valore mostrato non cambia intenzionalmente la selezione.
- La correlazione coerente riusa una tabella di fasori 8×32/4×32, conservando
  espressione trigonometrica e ordine delle somme. Verifica: 40140 correlazioni
  bit-identiche incluse estremità dei buffer. Microbenchmark 200000 chiamate:
  121.738→15.126ms. Questo è il costo del kernel, NON del decoder intero.
- Le passate live, i limiti candidati/OSD, la ricerca e la sottrazione R9 restano
  attivi come prima. Il coerente mantiene l'abilitazione offline già esistente.
  I tentativi di recupero coerente live sono stati provati e NON promossi.

## Confronto su audio radio reale

Sette WAV da kgoba/ft8_lib, 15s mono 12kHz; tre ripetizioni con ordine alternato,
Linux x86_64, Qt5 Release, sette worker. Nessuna build durante il confronto.
Il JSON allegato contiene hash corpus, messaggi, frequenze, SNR, DT e tempi.

| Variante | Righe decode,21 slot | Messaggi distinti per slot, sommati | Mediana | Massimo |
|---|---:|---:|---:|---:|
| R9 originale | 409 | 361 | 723.1ms | 795.1ms |
| Cache + DT isolato, senza tail | 409 | 361 | 740.6ms | 889.9ms |
| Tail coerente 32 candidati, scartato | 418 | 370 | 776.3ms | 1133.4ms |

Le righe possono contenere lo stesso testo a tempi/frequenze diversi: non sono
conteggi di stazioni uniche. Anche i messaggi distinti vengono contati nuovamente
in ogni ripetizione. Il corpus è di sviluppo e non prova sensibilità generale.

La variante tail aggiunge messaggi senza rimuovere i risultati già acquisiti
nello stesso job. Tuttavia nei confronti fra esecuzioni rimangono variazioni
legate alle risorse/OSD: in due coppie manca un messaggio presente in R9. Il costo
mediano è superiore. Non viene quindi presentata come miglioramento affidabile
né inclusa nel decoder live distribuito. La precedente variante coerente in ogni
passata aveva 398→432 righe e 753→803ms, ma anch'essa una mancata intermittente;
non confrontare direttamente numeri raccolti in sessioni diverse.

Il benchmark cache offline su due WAV, un worker, ha restituito gli stessi
messaggi/frequenze/SNR e DT normalizzati: 23 e 13 righe. Tempi R9/cache:
4879/4894ms e4452/4926ms. Non è dimostrato un miglioramento della latenza globale.

## Feedback JTDX 50 / MM 37

I 37 messaggi comuni nel foglio del tester hanno uno scarto DT di +0.4/+0.5s,
media +0.459s; frequenze entro 1Hz. Lo scarto SNR medio è -6.24dB, mediano -5dB;
non è stato applicato un offset cosmetico allo SNR. Fra i 13 mancanti ci sono
anche segnali forti e sovrapposti: il solo Excel non individua la causa.

Manca il WAV originale dello slot 15:04:15. Queste modifiche NON dimostrano il
recupero dei 13 segnali mancanti, la parità con JTDX o un problema di clock PC.
Per chiudere quel confronto servono lo stesso audio e le impostazioni di entrambi.

## Verifiche

- Build Linux Qt5 Release; guard release 8 e FT 6.
- GUI FT8/FT4: barre compatte, font 14→8 sulle celle vecchie/nuove,
  preservazione grassetto/barrato, righe TX e storico.
- GUI sequencer: ultima risposta, 73 finale, timeout obsoleto, mancata risposta,
  QRZ che segue il corrispondente; tutte passate.
- Pubblicazione: sei segnali sintetici in FT8 e FT4, controllo di messaggi,
  duplicati e completamento canonico: PASS con QSO attivo e inattivo.
- Nessuna modifica ai file CW Native R12/Neural R13, CAT, audio, updater,
  workflow di distribuzione o sequencer. Build native Windows/macOS e
  comportamento su radio reale non eseguiti in questo ambiente.

## Riproduzione

`tests/FtCoherentCorrelationRegression.cpp` è autonomo:
`c++ -O2 -std=c++17 tests/FtCoherentCorrelationRegression.cpp -o correlation-check`.
Eseguire `./correlation-check`.

`tests/FtRecordedReplayProbe.cpp` si collega agli oggetti dell'app al posto di
main.cpp, con le stesse librerie/define/include e `-fno-access-control` per la
sonda diagnostica. Legge file raw float64 mono 12kHz; la conversione WAV PCM16 è
sample/32768.0, senza normalizzazione di livello. Senza variabili usa boundary;
`PROBE_OFFLINE=1` seleziona offline e `PROBE_WORKERS=N` fissa i worker.
`FtPublicationProbe.cpp`, `FtAppearanceUiProbe.cpp` e `FtResponseUiProbe.cpp`
usano lo stesso collegamento. Non sono componenti dell'app distribuita.

## Continuità

Sorgenti, sonde, WAV, varianti scartate e risultati sono conservati in checkpoint
versionati separati dalla release. Salvare prima di build/prove lunghe e dopo
ogni risultato importante; non usare una directory temporanea come unica copia.

## Ultima esecuzione sul binario di consegna

Build completa riuscita. Sette WAV: 137 righe, mediana 744.7ms; nessuna
richiesta di recupero coerente live. Pubblicazione passiva: FT8 6/6 in 809.9ms,
primo risultato visibile 159.7ms; FT4 6/6 in 276.8ms, primo risultato 78.0ms.
Con contesto QSO attivo: FT8 6/6 in 629.9ms, FT4 6/6 in 299.7ms.
Sono tempi di questa macchina e di questo set, non limiti garantiti.
