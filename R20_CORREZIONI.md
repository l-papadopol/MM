# R20 — correzioni architetturali

Sorgenti completi, basati sulla R19; versione applicazione 0.5.9-beta.

- Preparazione FT8, FT4, Q65 e MSK144 nel worker `WeakSignalTxPreparer`, con cancellazione anche durante l’attesa del mutex del codec. Nessuna generazione di messaggi weak-signal nella GUI.
- Piano FT unico per messaggio, tag, slot, Tune e ritardi. Il piano dell’ultima TX si aggiorna solo alla conferma di avanzamento audio. Handoff dei modulatori con proprietà mantenuta anche se una chiamata accodata viene annullata.
- Silenzio iniziale FT calcolato al primo prelievo PCM del backend, con rifiuto delle partenze scadute.
- `AsyncLogbook` serializza caricamento, QSO, importazione e cancellazione. Conferme e UDP seguono il commit; i duplicati FT vengono verificati nello stesso worker della scrittura. Chiusura con drenaggio e conservazione in memoria dei QSO non salvati per il successivo tentativo.
- Indici ricostruiti da snapshot del registro confermato; conferme tardive non modificano un altro QSO/sessione contest. Seriali contest riservati prima di accettare lo scambio successivo.
- Incluse le correzioni R20 ad AFC RTTY, cache temporali FT, Stop/Tune, associazione exchange/corrispondente e selezione Cabrillo.

Verifica: compilazione Release Linux Qt 5.15.13; 24/24 CTest superati e 9/9 controlli della finestra reale. Log in `verification-r20`. I test includono mutex occupato, cancellazione, dedupe concorrente, errore di scrittura, retry e drenaggio.

La scrittura ADIF conserva la sostituzione atomica completa, ora fuori dalla GUI. Non è un append incrementale. Restano necessarie le prove Windows/audio/PTT reali; i test automatici non misurano latenza DAC/RF. Il burst manuale tardivo ereditato dalla R18 resta invariato. I rapporti delle revisioni precedenti sono storici.
