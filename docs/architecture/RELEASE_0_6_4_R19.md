## R19 — GPU per la demodulazione FT8/FT4 sperimentale

In Impostazioni → FT8 / FT4, nel gruppo FT8 / FT4 GPU, la nuova casella
«Prova decodifica FT8/FT4 con GPU (sperimentale, tutte le bande)» è indipendente
dal drift e inizialmente disattivata. Funziona anche in HF senza richiedere CAT.
Offload delle energie dei toni di tutti i simboli, dopo il raffinamento del
candidato: è una parte della demodulazione ordinaria. Ricerca, raffinamento,
LDPC, CRC, validazione messaggio e sottrazione restano sulla CPU.
Il recupero drift conserva la soglia RF >=144 MHz e il limite ±20 Hz/s.

Backend OpenCL FP64 dinamico: nessun SDK/runtime GPU obbligatorio all'avvio.
Un solo candidato ordinario accede al servizio; gli altri continuano su CPU.
Nessuna coda crescente. La prima richiesta per modo restituisce le energie CPU
e confronta l'intero blocco con OpenCL. Promozione solo su GPU reale, parità
entro 1e-9 * max(1,abs(energia CPU)) e tempo totale GPU inferiore al 90% del
riferimento CPU nativo. Il totale include copia audio, validazione input,
impacchettamento, trasferimenti, kernel e lettura. I buffer sono riutilizzati.
Confronto CPU ogni 16 richieste successive; attesa massima 2 ms quando si
richiedono energie già verificate. Dispositivo occupato, assente o in
riscaldamento: calcolo CPU. Errore, ritardo, lentezza o divergenza: CPU per il
resto della sessione dell'opzione. Disattivare e riattivare ripete la valutazione.
Le richieste possiedono l'audio corrente, anche nei passaggi SIC: nessun riuso
fra candidati o residui diversi. Esito e tempi compaiono nel Log runtime.

### Dove conviene provare

Il candidato FT8 di questo prototipo ha soltanto 632 energie (79 simboli ×8),
FT4 ne ha 412 (103×4). Il codice AVX2/FMA nativo FT8 ha misurato circa 0,31 ms
per questo blocco sulla CPU del banco di prova. Il margine è piccolo: questi
numeri NON sono un confronto con GPU fisica. Copie e trasferimenti possono
costare più del kernel. L'estensione più promettente è raggruppare molti
candidati o una griglia di raffinamento in una singola richiesta; richiede
riorganizzare il decoder per fasi e verificarne le scadenze. Non implementata
qui. Anche FFT e ricerca su molti bin/finestre sono candidati; prima bisogna
considerare il lavoro già anticipato durante la ricezione dalla cache CPU.
Il singolo blocco LDPC è breve, con iterazioni variabili: il presente prototipo
non lo trasferisce alla GPU. Meteofax e Hell hanno piccoli flussi continui;
non emerge dai profili disponibili una priorità per il loro offload GPU.
La GPU calcola le stesse metriche: non aumenta da sola la sensibilità.

### Verifiche e limiti

- Build Linux, OpenCL software FP64: 28/28 blocchi, 502168 valori totali,
  inclusi i 12 blocchi drift già coperti dalla R18. I 16 blocchi ordinari
  contengono 8352 energie e coincidono con il Goertzel scalare del backend.
- Confronto aggiuntivo con il codice nativo effettivo del decoder: 8/8 scenari
  FT8/FT4 (silenzio, rumore, toni fuori griglia e banda audio alta), 4176 energie.
  Errore massimo fra SIMD nativo e OpenCL: 1,70985e-10, entro la tolleranza.
- 216 coppie di candidati con GPU disattivata/richiesta senza dispositivo,
  sulle 7 registrazioni HF e 2 fixture pulite FT8/FT4: accettazione identica,
  51 coppie di decodifiche accettate identiche anche in frequenza, DT, SNR e sync.
  RF 7,074 MHz, recupero drift disattivato.
- Replay live completo, una esecuzione per configurazione: conteggi variabili
  e non equivalenti su tutti i file. Il percorso contiene budget temporali,
  OSD concorrente e tentativi interrompibili; questi risultati NON dimostrano
  né un guadagno né parità end-to-end. Conservati in R19_GPU_RESULTS.json.
- UI: casella separata, inizialmente disattivata, raccolta e persistenza PASS;
  layout verificato. 15 casi CTest selezionati finali PASS su 44 registrati
  (14 al primo giro; audit release passato dopo rigenerazione delle traduzioni).
- Banco senza GPU fisica. OpenCL eseguito tramite PoCL su CPU, esplicitamente
  consentito soltanto dalle sonde. Produzione seleziona esclusivamente GPU.
  Promozione, uso reale delle energie GPU, controllo periodico e guadagno
  fisico, così come build native Windows/macOS e catena RX/TX hardware, restano
  da verificare su un PC dotato di GPU. Nessun aumento di decodifiche promesso.

### Valutazione su PC con GPU

Usare le stesse registrazioni, frequenza e impostazioni, alternando CPU e nuova
casella per più ripetizioni. Separare riscaldamento e misure. Cercare nel Log
runtime «GPU decode»: confronto iniziale, «promoted 1», poi «tone energies used»
oppure motivazione CPU. Confrontare messaggi CRC validi, righe perdute/aggiunte,
tempo totale dello slot e ritardo della prima pubblicazione. Ripetere con
trasmissione e sequencer reali prima di considerare conveniente l'opzione.
Se non compare promozione, il prototipo ha mantenuto la CPU: non è una misura
di accelerazione GPU. Lasciare la casella disattivata se non porta vantaggi.
