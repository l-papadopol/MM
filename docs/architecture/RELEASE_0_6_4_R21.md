## R21 — sincronizzazione del clock audio FT8/FT4

Il log della build123/R20 mostrava un flusso medio di circa47933,81frame/s
rispetto ai48000nominali e una crescita del vecchio indicatore di latenza
40→965ms durante circa11minuti di RX continua. La R21 stima il rapporto fra
conteggio dei frame ricevuti e tempo monotono e usa questa informazione per
allineare la ricezione FT8/FT4 alla griglia UTC. Non richiede un nuovo comando.

### Implementazione

- `CaptureClockTracker`: un punto di inviluppo inferiore per secondo; finestra
  di60s; mediana delle pendenze fra punti distanti almeno10s. Primo modello
  dopo almeno15s e12punti. Il tempo UTC non entra nella stima di frequenza.
- Rapporto del periodo ammesso0,995…1,005. Una retroazione di fase di20s
  corregge l'offset accumulato; contributo limitato a±2000ppm e variazione
  del rapporto limitata a200ppm/s. La mappatura avanza continuamente per ogni
  blocco PCM. Nessuna eliminazione di frame per inseguire l'orologio.
- Il PCM conserva campioni e ordine del backend. Il campo `sampleRate` mantiene
  il contratto esistente, compresa la calibrazione manuale.
  `AudioBlock` trasporta anche la durata misurata `captureSpanNs`, conservata
  dal condizionatore. FT8/FT4 usano le durate per interpolare su una griglia
  temporale continua ed esatta di12000campioni/s. Il prefiltro FT esistente
  resta attivo; i filtri non vengono azzerati a ogni variazione della stima.
- In presenza di un vero salto fra blocchi, il nuovo interpolatore riparte
  dal blocco disponibile: non crea audio sintetico che attraversa il vuoto.
  I controlli esistenti di slot parziali, sequenza e generazione rimangono.
- Avvio/riavvio RX azzera la stima. Un salto UTC superiore a250ms o un cambio
  esplicito della calibrazione genera una nuova epoca di cattura, con normale
  scarto del primo slot incompleto. Una calibrazione manuale diversa dazero
  mantiene il controllo manuale e disattiva la stima automatica della frequenza.
- La latenza di consegna al decoder usa un timestamp monotono posto prima
  dell'invio del blocco, indipendente dalla calibrazione del clock audio.
  Il vecchio valore basato sull'età UTC mescolava clock e attese di consegna.
- Ogni30s compare `Audio capture clock`: nominale, frequenza misurata,
  frequenza della mappatura, residuo del modello, `locked` e calibrazione
  manuale. `locked 1` indica che è disponibile un modello, mentre la fase può
  ancora assestarsi. Durante i primi secondi il percorso usa il nominale.

Il percorso dei produttori storici/offline senza durata misurata è conservato.
L'adattamento del ricampionamento descritto qui riguarda FT8/FT4; gli altri
modem mantengono i propri ricampionatori. GPU, algoritmi FEC/CRC, ricerca,
sottrazione e scheduler TX non sono modificati da questa revisione.

### Verifiche

- Build Linux completa PASS.
- 15simulazioni di900s a44100/48000/96000Hz e scarti−3000/−1378,94/0/
  +1378,94/+3000ppm. Callback irregolari, consegna iniziale ritardata180ms,
  ritardi occasionali e recuperi a raffica. Misura del regime dopo180s:
  errore massimo8,74ms; errore finale della frequenza inferiore a1,42ppm.
  Sono risultati simulati e non una misura della scheda audio dell'utente.
- Due prove di oltre12minuti del ricampionatore con scarti±1378,94ppm:
  uscita12kHz continua, durata coerente e tono1000Hz recuperato.
- Calibrazione manuale, reset, saltoUTC di1s e vuoto reale di input PASS.
- 10prove native con PCM16 e messaggio MSHV noto: AudioEngine dopo la lettura,
  DspConditioner, ricampionamento FT e CRC. Entrambi i modi e cinque scarti:
  indici e timestamp continui, slot completi, messaggio esatto in tutti i casi.
  Le callback/backend fisici sono simulati: non è una prova hardware Windows.
- 12CTest selezionati su45registrati PASS, inclusi clock, audio, ricampionamento,
  Q65/MSK144 nativi, runtime FT, architettura, temi/UI, release e fallback GPU.
- Build native Windows/macOS e ricezione prolungata sulla USB reale restano
  da eseguire. Il caso FT4 iniziale della sonda usava un candidato troppo
  anticipato e falliva anche sulla fixture originale; la ricerca ordinaria
  trovava0,336s. Corretto soltanto il seme della sonda, senza cambiare FT4/FEC.

### Verifica sulla radio

Lasciare RX continua sui20m per10–15minuti e raccogliere il Log runtime.
Con calibrazione manuale a zero, verificare le righe `Audio capture clock`:
la frequenza può assestarsi attorno a47934Hz se si ripete lo scarto del log.
Osservare separatamente il residuo del modello e la latenza di consegna:
non devono più essere interpretati come lo stesso fenomeno. Verificare anche
DT, messaggi decodificati, buchi/sovrapposizioni e comportamento dopo RX/TX.
La stima dipende dall'arrivo dei dati del backend: il nuovo log serve a
validarne il comportamento con il driver e con l'orologio effettivi del PC.
