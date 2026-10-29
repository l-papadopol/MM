# R27 — diagnostica di sessione e audit JTDX

Sorgente completo basato sulla R26. Versione pubblica 0.6.4.
Revisione `0.6.4-source-r27-session-diagnostics-jtdx-audit`.

## Come registrare una sessione

1. Aprire **Impostazioni → Diagnostica** (ultima scheda; usare le frecce delle
   schede se lo schermo è stretto).
2. Spuntare **Registra la diagnostica completa della sessione su file**.
3. Scegliere eventualmente la cartella con `…` e premere **OK**.
4. Il file si apre subito; percorso e stato sono visibili nella scheda. **Apri
   cartella dei log** apre il file manager. Annulla non attiva la registrazione.
5. Per includere costruzione della finestra e avvio dei servizi, lasciare la
   casella spuntata e riavviare MadModem. La preferenza e la cartella persistono.
6. Al termine chiudere normalmente il programma e inviare il file `.jsonl`,
   insieme alla registrazione WAV quando serve riprodurre la decodifica.

L'opzione è inizialmente disattivata. La cartella predefinita è
`QStandardPaths::AppLocalDataLocation/diagnostics`, quindi una directory utente
scrivibile anche nelle installazioni gestite. Ogni attivazione crea un nome UTC
con UUID, ad esempio `MadModem_20261028_203450_123_<uuid>.jsonl`. Non sovrascrive
file precedenti. Cambiare cartella mentre si registra chiude e drena il vecchio
file, poi ne apre uno nuovo. Il file contiene testo UTF-8, un oggetto JSON per
riga: si può leggere con un normale editor, oltre che elaborare con uno script.

## Cosa viene registrato

- Intestazione: versione, build, commit, revisione sorgente, sistema, Qt,
  architettura, CPU logiche e limite della coda. Viene scritta e scaricata prima
  di ammettere eventi concorrenti, anche attivando il log durante RX.
- Tutte le righe del log runtime prima del taglio del buffer visibile;
  messaggi Qt effettivamente emessi, inclusi warning e fatal.
- Configurazione corrente e configurazione persistita, con oscuramento dei
  campi credenziali e dei pattern password/token/Authorization riconosciuti.
- Ogni blocco audio catturato, accodato, prelevato, scartato o cancellato dai
  dispatcher e consegnato all'ingresso FT. I dispatcher RX/UI e DSP hanno
  identità distinte. Ci sono sequenza, generazione, indice, rate nominale,
  timestamp UTC/monotono, durata misurata, RMS, picco, clipping e SHA-256 dei
  campioni float. Il blocco ignorato dall'ingresso FT disabilitato è distinto
  da quello ammesso.
- Ogni tentativo di candidato FT8/FT4: frequenza/tempo/score, identificatore del
  buffer, successo o motivo del rifiuto, evidenza Costas, LLR e uso/permesso OSD.
- Inizio/fine job, intervallo, modo, profondità, range di ricerca, passaggi,
  candidati ammessi, contatori completi `PerfStats`, cache incrementale,
  SIMD/GPU, risorse, latenza e scadenze. La statistica nel decoder è distinta
  dalla consegna alla GUI.
- Decisione di pubblicazione FT e duplicati; messaggi consegnati alla GUI.
- Testo e stati RTTY/BPSK/MFSK/CW, stati Q65/MSK144/SSTV/WEFAX/Hell, progressi
  e completamenti delle immagini. Le immagini non vengono convertite in testo
  o ricopiate integralmente nel log.
- Richieste CAT/PTT/split, poll, errori, avvio/arresto audio TX. Nelle build
  Hamlib si aggiunge il callback nativo a livello `RIG_DEBUG_CACHE`, comprese
  le tracce di protocollo prodotte dalla libreria; spento il log si ripristina
  il livello NONE del programma.
- Eventi UI significativi: pressione tasto senza carattere digitato,
  rilascio mouse, rotella, chiusura e attivazione finestra. Non viene trasformato
  il campo password in un registratore di battute.
- Chiusura dei worker e footer con numero di eventi scritti, perdite e
  completezza dell'intervallo registrato.

È una diagnostica dettagliata dei percorsi strumentati, non una traccia di ogni
istruzione CPU, bin FFT, pixel o messaggio soppresso prima dell'emissione.
Non intercetta genericamente stdout/stderr di qualsiasi libreria esterna.
La cattura inizia dopo l'inizializzazione di QApplication e il caricamento del
profilo; attivarla a metà sessione non ricrea eventi precedenti. I campioni audio
restano nel registratore WAV separato: un hash non permette di ricostruirli.

## Scrittura, integrità e carico

Un thread dedicato esegue oscuramento, formattazione JSON, creazione/scrittura,
flush e chiusura. I produttori consegnano snapshot immutabili. La coda usa un
budget logico conservativo di 32 MiB; il batch in scrittura può trattenere un
altro budget. Non è una promessa sul consumo totale di memoria del processo.

Non si campionano selettivamente i tentativi di decodifica per accorciare il
file. Se la coda si satura, il log contiene `diagnostic.loss`, il footer indica
`complete:false`, e la UI segnala il numero di eventi persi. Un errore di file
interrompe l'ammissione e viene mostrato nella scheda e nel runtime/status bar.
Non viene presentato come una registrazione completa.

Ogni batch viene scaricato, con risveglio almeno ogni 250 ms anche in inattività.
La chiusura normale smette di ammettere eventi e drena quelli già accettati.
Un messaggio Qt fatal richiede un flush con attesa limitata prima dell'abort.
Non è un gestore di SIGSEGV né una garanzia contro mancanza di alimentazione:
in caso di crash può mancare il footer o l'ultima parte del file.

Il log completo ha un costo CPU e può influire sui tentativi al limite del
budget. Il test dopo lo spostamento della formattazione nel writer ha prodotto:

| Prova alternata | Log spento | Log acceso | Differenza | Messaggi spento/acceso |
| --- | ---: | ---: | ---: | ---: |
| 1 | 904,58 ms | 1027,67 ms | +123,10 ms | 41/41 |
| 2 | 898,86 ms | 942,16 ms | +43,30 ms | 41/41 |
| 3 | 808,11 ms | 895,63 ms | +87,52 ms | 39/41 |

Mediana delle differenze per coppia: **+87,52 ms**. Tre piccoli esperimenti non
sono una garanzia prestazionale. Il conteggio cambia anche per l'ammissione
concorrente OSD; le 41 righe col log acceso non costituiscono un miglioramento
dell'algoritmo. Le prove usano il decoder e il WAV del tester, 8 worker, Linux
Qt5 e una quota CPU di 8 core. Non includono acquisizione hardware continua,
CAT reale o GPU fisica. Ogni replay registrato ha prodotto circa 358 kB e 814
righe, footer completo e zero eventi persi; una sessione live registra anche
molti eventi audio/UI e può crescere sensibilmente di più.

## Verifiche

- Unità: 3000 eventi concorrenti, ordine/sequenza, flush, drain, Unicode/spazi,
  nomi unici, errore cartella/ripresa, oscuramento ricorsivo e overflow marcato.
- Processo figlio Qt fatal: record fatal presente dopo SIGABRT, senza footer
  normale, come previsto.
- Workflow nativo: dialogo reale, Annulla/OK, attivazione e disattivazione live,
  persistenza, sei lingue/geometria, warning Qt, eventi UI senza testo password,
  blocco audio e dispatcher, WAV FT8 e sintesi/decoder FT4, successi e rifiuti,
  statistiche del produttore e della GUI. Controllo della chiusura registrata.
- Ramo Hamlib: verifica di compilazione con gli header Hamlib; il trasporto
  CAT fisico e Windows/macOS nativi richiedono prova sul sistema del tester.

Il risultato dei controlli finali è in `SESSION_DIAGNOSTICS_R27_RESULTS.json`.

## Studio JTDX

`JTDX_FT8_FT4_AUDIT_R27.md` ricostruisce il percorso del repository ufficiale al
commit fissato. `JTDX_R27_SOURCE_INVENTORY.json` ne elenca i file e gli hash.
Sono documentati front-end, metriche coerenti, BP/OSD, AP/memoria, sottrazione,
finestra temporale e parallelismo, distinguendo fatti di codice e contributi
ancora da misurare. Non viene dichiarata parità JTDX e non sono incorporati
algoritmi JTDX non validati nella release della diagnostica.

## Esito finale dei controlli

Build Linux Qt5 riuscita. Backend verificato anche come libreria autonoma in
Debug. Superati 17 controlli selezionati su 51 registrati: diagnostica, audio
flow accounting, filtro ricevitore, copia, clock, TX pull, coda CQ, produttore
incrementale, sequencer, quattro guardie/audit e quattro regressioni SSTV.
Due guardie iniziali hanno richiesto di lasciare identico il finder congelato
(spostando il solo log nel chiamante) e riallineare l'ordine del dizionario;
entrambe le ripetizioni mirate sono passate, senza allentare le guardie.

L'ultimo workflow nativo ha verificato 1577 tentativi FT8/FT4 e anche gli
agganci degli altri modi tramite segnali sintetici, oltre alla chiusura con
registrazione attiva. Il ramo Hamlib è stato compilato con gli header della
copia inclusa nel progetto. Le prove non sostituiscono un test su radio Windows.

Nei record, il flag `ft.job.begin.offline` è il profilo interno del core;
`ft.performance.offline` descrive la provenienza dell'analisi. FT4 mantiene
attualmente i due significati separati, come spiegato nell'audit del decoder.
