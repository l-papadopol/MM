# R8 — Decoder RX fuori dalla GUI e CAT asincrono

Revisione: `0.5.9-alpha-source-r8-rx-cat-workers`.
Baseline: sorgenti completi R7. I rapporti R6/R6.1/R7 inclusi sono storici.

## Ricezione

- RTTY primario e multicanale, CW, PSK, MFSK, Hell, SSTV, WEFAX e MSK144
  appartengono a `RxDecoderWorker`, con un thread dedicato. FT e Q65 mantengono
  i propri worker già esistenti.
- Il callback audio alimenta direttamente una coda limitata a 24 blocchi;
  non attraversa l'event loop della GUI per arrivare ai decoder.
- Configurazioni e comandi sono accodati allo stesso worker. La GUI legge
  copie delle immagini e della telemetria CW, senza accedere allo stato DSP.
  Gli aggiornamenti grafici SSTV/WEFAX/Hell vengono accorpati: al massimo
  una notifica pendente, con conservazione dell’immagine più recente.
- Blocchi scartati, salti negli indici, cambio generazione o sample rate
  interrompono la continuità: filtri, AFC e decoder attivo vengono resettati.
  Il normale ritorno RX dopo TX locale conserva invece lo stato previsto
  dai percorsi di ripresa rapida CW/RTTY.
- AFC RTTY e PSK/Hell operano nel worker; mantenuta la ricerca dei due toni
  di Hell FSK-105. Anche la polarità automatica RTTY viene applicata nel
  worker senza aspettare la GUI. Eliminato il vecchio AFC duplicato nella GUI.
- Waterfall e controlli grafici ricevono i risultati; una GUI impegnata non
  interrompe la consegna dell'audio ai decoder. L'analisi WAV esplicita usa
  comandi sincroni al worker per limitare la memoria e completare le immagini.

## CAT e trasmissione FT

- Rimosse le attese sincrone GUI→CAT per PTT e split. Le operazioni Hamlib/HRD
  rimangono serializzate nel thread CAT e restituiscono una conferma asincrona.
- Split, PTT e avvio audio rispettano l'ordine delle conferme. Lo slot FT
  non parte se manca la conferma entro la scadenza; viene rinviato. TUNE,
  che non ha la scadenza di un frame FT, attende la conferma.
- Timeout e STOP revocano l'autorizzazione. Una risposta tardiva viene
  compensata con PTT OFF e, solo dopo conferma del rilascio, ripristino split.
  Gestita anche la cancellazione tra esecuzione hardware e notifica alla GUI.
- Fino al termine del comando e dell'eventuale recupero, il controllo CAT
  impedisce una nuova transazione TX. Recupero fallito: ulteriori TX bloccati;
  controllare il PTT/radio e riavviare l'applicazione prima di ritentare.
- Il confine temporale FT non aspetta più l'arresto sincrono della cattura;
  il worker TX conserva il controllo della scadenza e rifiuta avvii tardivi.
- STOP e cambio modo annullano anche una preparazione ancora in attesa CAT.

## Verifiche

La suite CTest include `madmodem_runtime_workers_regression` (eseguibile con
`MadModem --runtime-regression`, piattaforma Qt offscreen):

1. Decodifica RTTY completa mentre la GUI non elabora eventi.
2. Nessuna perdita nella normale consegna dei blocchi audio.
3. Coda limitata, rilevamento sovraccarico e ripresa della decodifica.
4. Polarità automatica RTTY indipendente dai callback GUI.
   Accorpamento delle immagini anche quando la GUI non consuma notifiche.
5. CAT lento, timeout senza blocco GUI e compensazione della conferma tardiva.
6. Cancellazione in concorrenza con una conferma CAT riuscita.
7. Conferma positiva normale e blocco dei nuovi TX dopo recupero fallito.

Il controllo statico `check_rx_cat_workers.py` verifica inoltre il collegamento
reale nell'applicazione: nessuna chiamata diretta della GUI ai decoder spostati,
nessuna attesa CAT sincrona e conferma PTT obbligatoria prima dell'audio FT.
I controlli preesistenti mantengono le garanzie su split, rotore, contest e FT.
Dizionari aggiornati e nuovi messaggi tradotti in italiano, francese, tedesco,
norvegese e ceco.

Compilazione Release riuscita su Linux, GCC 13.3, Qt 5.15.13 e Hamlib 4.5.5.
**20/20 test CTest superati**, zero errori; 15 controlli nel nuovo test runtime.
I log della compilazione e della suite finale sono in `verification-r8/`.

## Ambito e limiti

Questa revisione corregge i due percorsi richiesti. Non è una riscrittura
completa di MainWindow o del logbook. La visualizzazione e alcune misure della
modalità Radio Telescope restano nella GUI; non sono decoder di comunicazioni.

Un timeout software non può interrompere forzatamente una chiamata già dentro
Hamlib o garantire un rilascio istantaneo su una radio che non risponde.
La GUI rimane disponibile, l'audio non viene autorizzato e il recupero viene
eseguito quando il backend restituisce il controllo. Le conferme usate sono
quelle del backend, non una misura indipendente del PTT fisico.

I test CAT usano un backend simulato. Restano necessari compilazione/test
Windows e verifica sulla radio reale di RX continuo, STOP, TUNE, split Rig/
Fake It e ritorno RX. Non viene dichiarata una validazione RF da contest.

## GitHub

Estrarre lo ZIP e caricare il contenuto della cartella radice nel repository,
inclusa `.github`. Non contiene directory di compilazione o binari locali.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```
