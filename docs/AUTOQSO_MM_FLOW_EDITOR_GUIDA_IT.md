# Guida rapida — AutoQSO / MM Flow editor

Questa scheda sta diventando il livello di programmazione visuale di MadModem. La v4.13m migliora l'editor e prepara il modello di linguaggio; l'esecuzione reale rimane volutamente protetta dai guardrail dell'app.

## Operazioni base

- **Creare un blocco**: usare i pulsanti `+ Event`, `+ Decision`, `+ Action`, `+ Timer`, `+ Variable`, `+ Compare`, `+ Loop`, `+ I/O`, `+ Math`, `+ End`.
- **Modificare un blocco**: selezionare un blocco e premere `Edit selected` o `Ctrl+E`.
- **Creare una freccia**: selezionare due blocchi e premere `Connect arrow` oppure premere `Connect arrow` e scegliere `From` / `To` nella finestra.
- **Scollegare/cancellare una freccia**: cliccare la linea della freccia e premere `Delete arrows`, oppure premere `Delete`.
- **Cancellare un blocco**: selezionare il blocco e premere `Delete selection` o `Delete`; le frecce collegate vengono rimosse insieme al blocco.
- **Zoom**: rotella del mouse.
- **Adatta vista**: `Fit`.
- **Ripristina flusso base**: `Restore default`.

## Porte delle frecce

Per decisioni, confronti e cicli usare etichette porta come:

- `yes` / `no`
- `true` / `false`
- `ok` / `retry`
- `next` / `done`
- `loop` / `exit`

Una porta vuota indica sequenza normale.

## Blocchi di programmazione

- **Variable**: set, copy, increment, clear, list/map.
- **Compare**: confronta variabili, costanti, campi di messaggi decodificati, banda, modo, regex.
- **Loop**: itera su decode, candidati, righe log o contatori limitati.
- **I/O**: input da tastiera, popup informativo, domanda popup, status/log/table output.
- **Timer**: attesa slot FT, finestra decode, delay, intervallo, watchdog, trigger UTC.
- **Math**: aritmetica e funzioni radio come distanza/bearing/locator.

## Sicurezza

Il flow può descrivere azioni astratte; il runtime reale dovrà applicare permessi e guardrail:

- niente PTT diretto fuori slot;
- niente CAT QSY durante TX;
- niente loop infinito;
- niente file/network senza consenso;
- scheduler FT sempre autorità finale su slot UTC, audio e PTT.
