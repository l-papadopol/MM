# MadModem 0.6.4 R18 — backend GPU sperimentale per il drift FT

## Ambito e uso

R18 aggiunge un backend OpenCL per la matrice di correlazioni Costas del
recupero drift FT8/FT4. È il primo kernel DSP trasferito al dispositivo.
La ricerca ordinaria, LDPC, CRC, sottrazione, sequencer e TX restano sulla CPU.
Non si dichiara un decoder FT interamente GPU né un aumento della sensibilità.
Le correzioni delle revisioni precedenti sono conservate.

Impostazioni → FT8 / FT4 → «Prova GPU per il drift (sperimentale, ritorno
automatico alla CPU)». Il nuovo comando è disattivato inizialmente e viene
memorizzato nel profilo. Per esercitare il percorso live servono anche il
recupero drift attivo e una frequenza RF almeno 144 MHz, come nella R17.
Senza CAT vale la banda selezionata; con CAT vale la lettura della radio.
Il marcatore RX seleziona il segnale. Se il normale decoder lo trova già,
il recupero aggiuntivo può essere saltato e il kernel non viene chiamato.

## Implementazione

- Runtime OpenCL caricato dinamicamente con QLibrary: nessun SDK o runtime
  OpenCL obbligatorio per avviare MadModem o usare la CPU. Sono richiesti una
  GPU OpenCL disponibile, compilatore del dispositivo e supporto FP64 per
  provare questo backend. Si sceglie la prima GPU idonea enumerata; questa
  revisione non confronta più GPU fra loro.
- Calcolo in doppia precisione, senza fast-math. Header Khronos ufficiali
  vendorizzati, con COPYRIGHT e licenza Apache 2.0.
- Un servizio seriale dedicato possiede contesto, coda, kernel e buffer
  persistenti. Non condivide il contesto OpenGL del waterfall. Compilazione
  e riscaldamento avvengono su quel thread, fuori dalla GUI e dal normale
  budget del job RX. Anche la preparazione iniziale controlla la parità CPU.
- Un solo batch immutabile in volo; nessuna coda crescente. Le richieste
  non ammesse o non pronte lasciano eseguire la CPU. I dati di una richiesta
  scaduta restano posseduti dal worker e non vengono riutilizzati in altri slot.
- La prima matrice di ciascun modo viene calcolata anche sulla CPU. Il
  confronto richiede valori finiti, stessa forma e differenze entro
  1e-9 × max(1, valore CPU). Un riferimento CPU parziale rinvia la verifica.
- Per selezionare il dispositivo nei job successivi, il tempo totale del
  batch deve essere inferiore al 90% del tempo della matrice CPU multicore.
  Il tempo OpenCL comprende attesa dal submit, packing, trasferimenti,
  esecuzione e lettura dei risultati. Il confronto CPU usa il pool FT reale,
  non il dato CPU seriale del programma diagnostico.
- Ogni ottavo job riverifica la matrice sulla CPU. Errori, discrepanze,
  lentezza o superamento dell'attesa prevista riportano il modo sulla CPU
  per la sessione. Per riprovare, salvare la casella disattivata e poi
  riattivarla. Il tempo di attesa nel decoder è limitato a 80 ms e a una
  frazione del budget residuo; il driver continua sul proprio worker.
- Fitting del drift, soglie di accettazione, massimo tre tentativi FEC,
  validazione del messaggio e pubblicazione restano nel codice CPU comune.
  Non vengono aumentati i limiti temporali del TX.

L'opzione GPU non promette di occupare ogni risorsa del PC né di rendere più
rapido ogni carico. La selezione dipende dal confronto osservato su quel PC.

## Come leggere il log

Le righe `FT8 GPU sync:` / `FT4 GPU sync:` riportano dispositivo o motivo
fallback, tempo OpenCL totale, tempo della matrice CPU e percorso utilizzato.

- `OpenCL warming up; CPU retained`: preparazione ancora in corso.
- `No OpenCL platform` / `No eligible OpenCL GPU with FP64`: CPU utilizzata.
- `parity OK ... GPU selected for subsequent jobs`: confronto iniziale passato
  e dispositivo più rapido nel batch osservato. Il primo job usa ancora la CPU.
- `verified GPU matrix used ... used GPU`: quel job usa la matrice del dispositivo.
- `CPU retained: device not faster`, errore o attesa superata: CPU conservata.

Quando il job usa la GPU, il campo `CPU grid` può essere zero perché quella
matrice CPU non è stata ricalcolata in quel job. Il valore di confronto si
legge nelle righe iniziali o di riverifica. I tempi sono della fase drift,
non dell'intera risposta RX/UI/sequencer: il profilo normale resta necessario
per giudicare la scadenza di risposta.

## Risultati ottenuti qui

Non è esposta una GPU fisica. Il test OpenCL usa **PoCL su CPU**, esclusivamente
nel programma diagnostico con `--opencl-cpu`; l'applicazione seleziona solo
veri dispositivi di tipo GPU. Questo verifica compilazione del kernel, API,
trasferimenti e risultati, ma non stabilisce le prestazioni di una GPU.

| Verifica | Esito |
|---|---|
| Matrici FT8/FT4, segnale, rumore, silenzio, finestre fuori acquisizione, buffer riusati | 12/12 batch OpenCL corretti |
| Punteggi confrontati con CPU | 493816 |
| Errore assoluto massimo osservato | 5.05e-14 |
| Decoder FT8/FT4, ±2/±10/±20 Hz/s, rumore, gate RF, cambio DX e predizione; GPU disattivata | 40/40 |
| Stessi casi con GPU richiesta e runtime non disponibile | 40/40, fallback CPU corretto |
| UI opt-in, limite 20, salvataggio/ricaricamento e bande manuali | superato |
| Build Linux Qt5 Release completa | superata |
| CTest selezionati, inclusi nuovo fallback e controlli MeteoFax/Hell/audio/FT | 15/15 |

Sono test selezionati, non tutti i 44 CTest registrati. I dati numerici e i
tempi diagnostici sono in R18_GPU_RESULTS.json. I tempi PoCL sono etichettati
come CPU OpenCL e non vengono usati per rivendicare un guadagno GPU.

Restano da verificare compilazione/precisione e prestazioni su GPU fisica,
il ramo di effettivo utilizzo GPU nel decoder, selezione/rifiuto in base ai
tempi, riverifica periodica, driver Windows/macOS e percorso completo RX→TX.
Il test qui dimostra l'esecuzione del kernel su OpenCL software e i fallback;
non dimostra ancora che la GPU del PC dell'operatore sia più veloce.

## Ripetizione del confronto

Per il confronto offline della matrice dopo una normale configurazione CMake
con BUILD_TESTING=ON:

```sh
cmake --build build --target madmodem_gpu_sync_regression
./build/madmodem_gpu_sync_regression
```

Il programma riporta il nome del dispositivo e `gpu=1` quando trova una GPU.
Il test di default continua a verificare il fallback se la GPU manca; quel
PASS da solo non è una misura GPU. `--opencl-cpu` è riservato alla verifica
software con un runtime CPU installato. La colonna `cpu_serial_ms` del probe
è un riferimento seriale e non equivale alla CPU multicore del decoder.

Per la prova live: installare la nuova build, attivare le due caselle, usare
il recupero su un segnale in deriva da 144 MHz e confrontare registrazioni
identiche con opzione GPU disattivata/attivata. Conservare le righe GPU sync,
il profiler FT, le decodifiche e il tempo di risposta del sequencer. Il WAV
comune permette di distinguere il backend dalla propagazione variabile.

## Documentazione primaria

- Khronos OpenCL API: https://registry.khronos.org/OpenCL/specs/unified/html/OpenCL_API.html
- Khronos cl_khr_fp64: https://registry.khronos.org/OpenCL/specs/unified/refpages/man/html/cl_khr_fp64.html
- Header Khronos: https://github.com/KhronosGroup/OpenCL-Headers
