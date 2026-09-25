# R6.1 — Correzione layout Impostazioni e audit RTTY

24 settembre 2026. Revisione: `0.5.9-alpha-source-r6.1-windows-settings-layout`.

## Correzione del fallimento Windows

Il controllo segnalava pulsanti più piccoli del minimo richiesto dallo stile:
“Reset theme defaults” e i quattro selettori colore, in più lingue.
La pagina Aspetto non applicava il dimensionamento dei pulsanti delle pagine
incorporate. Il minimo fisso di 128 pixel dei selettori non garantiva lo spazio
richiesto da testo, font monospace, icona e cornice su Windows.

- Minimi dei pulsanti calcolati con `sizeHint` e `minimumSizeHint`, dopo il polish
  dello stile; applicati anche alla pagina che contiene Aspetto.
- Selettori colore ricalcolati anche quando viene aggiornato il loro contenuto.
- Pannelli Registro e Aspetto disposti verticalmente; vincoli del contenitore
  propagati all'area scorrevole. Su schermi piccoli si può scorrere la pagina.
- Nessuna esclusione o riduzione delle asserzioni della regressione UI.

File applicativo modificato: `dialogs/AppSettingsDialog.cpp`.
Test ampliato: `tests/SettingsUiRegression.h`. Il test percorre tutti gli stili
Qt disponibili, font da 9 e 14 punti, sei lingue e tutte le pagine. Gli errori
riportano stile, dimensione font e dimensioni effettive/richieste.
Le catture opzionali includono anche il fondo delle pagine scorrevoli.

## Validazione

Compilazione Release Linux con GCC 13.3, Qt 5.15.13 e Hamlib 4.5.5.
CTest: 18/18 superati. Il controllo UI copre Windows e Fusion (stili Qt su
Linux), 2 font, 6 lingue e 6 pagine: 144 combinazioni di pagina.
Log completi in `docs/r6.1-verification/`.

**Non è una prova sul runtime nativo Windows.** Il runner MSYS2/Windows deve
confermare la correzione rieseguendo il comando originale:

```sh
ctest --test-dir build-windows-msys2-legacy --output-on-failure
```

La correzione riguarda il clipping dei pulsanti segnalato. Non certifica ogni
traduzione o ogni altra geometria dell'app. Le note e i risultati R6 conservati
nel pacchetto sono storici; per questa revisione fanno fede questo documento e
`R6_1_SOURCE_SHA256.txt`.

## Analisi RTTY inclusa

`docs/RTTY_Audit_R6/RAPPORTO.md` descrive l'analisi di modem, TX/RX, AFC,
multidecoder, contest, logbook e UDP, con distinzione fra difetti riprodotti,
riscontri statici e rischi. Il banco di prova e le 51 osservazioni native sono
inclusi con le istruzioni per riprodurli.

Sono ancora aperti, fra gli altri: shift LTRS all'inizio della trasmissione,
AFC che modifica lo shift, ripresa RX dopo TX, correttezza dei campi ricevuti e
del punteggio CQ WW, completezza dei record e uscita Cabrillo. Questa hotfix
non modifica il motore RTTY e non lo dichiara pronto per il contest.

## Uso del pacchetto

Estrarre il contenuto della cartella sorgenti nella radice del repository,
includendo `.github`, e compilare con la procedura abituale. Sono inclusi i
sorgenti completi e i test, senza binari o directory di build. Non è stato
eseguito alcun push GitHub. La patch `docs/R6_to_R6_1.patch` contiene le sole
modifiche al codice/test e al marcatore revisione rispetto a R6; il pacchetto
completo comprende anche documentazione e audit.
