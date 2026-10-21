# MadModem 0.6.4 R16 — ricezione Hellschreiber

## Modifiche

Feld Hell usa un filtro I/Q Butterworth a quattro poli. Il taglio segue la
banda RX configurata (245 Hz al valore predefinito). Il filtro mantiene lo
stato fra blocchi e lo azzera su reset o cambio sample rate.
Un primo candidato a 0.75 volte la banda è stato scartato: sfumava troppo
alcuni bordi dei segnali puliti. La variante finale conserva meglio quei pixel.

Inviluppo, attacco/decadimento del picco e stima del rumore usano tempi
riferiti a 48 kHz e riscalati per la frequenza di campionamento. I tempi in
secondi restano quindi uniformi a 44.1/48/96 kHz.

FSK-105 usa un discriminatore della fase fra campioni I/Q successivi al posto
del rapporto fra due inviluppi. Il tono inferiore stampa inchiostro, quello
superiore carta. Il filtro segue la velocità del raster (taglio limitato a
80–400 Hz), con levigatura della frequenza da 0.6 ms. Shift, frequenza centrale,
velocità del raster, orientamento e controlli operatore restano quelli esistenti.

È corretto anche un problema della stima di rumore FSK: entrambi i simboli
contengono una portante, quindi mancano pause di ampiezza da cui stimare il
rumore. L'incremento continuo della vecchia stima poteva infine chiudere il
gate e sbiancare il nastro durante una trasmissione valida. In FSK la stima è
ora limitata al 45% del picco. Questo limite evita l'auto-silenziamento;
non costituisce una nuova misura del rumore né uno squelch infallibile.

## Confronto con R15

Generatore OOK/FSK indipendente dal TX MadModem, raster binario noto,
17.5 colonne/s in entrambe le varianti, 2800 pixel per caso (circa 11.43 s).
Si confronta l'immagine restituita dall'API pubblica processAudioBlock.
L'errore RMSE è misurato sull'inchiostro normalizzato 0–1, senza rialzare il
contrasto. Si escludono i primi 280 pixel per l'assestamento e gli ultimi 20;
è ammessa una sola traslazione globale da −3 a +5 pixel per il ritardo del filtro.
La traslazione scelta è riportata per ogni caso; nessun riallineamento per colonna.

| Gruppo | Condizioni | Risultato |
|---|---|---|
| Base | Feld/FSK, 44.1/48/96 kHz, SNR 40/10/0 dB | RMSE inferiore in 18/18 casi |
| Esteso | 3 scenari, stessi rate/SNR, fading, interferente, toni 500/1000/1800 Hz, offset Feld ±25 Hz e FSK ±8 Hz | RMSE inferiore in 54/54 casi |

Esempi base a 48 kHz:

| Variante/SNR | R15 RMSE | R16 RMSE |
|---|---:|---:|
| Feld, 40 dB | 0.02526 | 0.02117 |
| Feld, 0 dB | 0.12740 | 0.10111 |
| FSK, 40 dB | 0.45217 | 0.01221 |
| FSK, 0 dB | 0.45605 | 0.21690 |

Il forte guadagno FSK include la correzione del nastro che sbiancava: non è
un guadagno di sensibilità espresso in dB. L'SNR qui è riferito al rumore
aggiunto dal generatore sull'intera banda campionata. Non misura una soglia
radio standardizzata. Le prove precedenti a 7.5 colonne/s sono diagnostiche
conservate nel checkpoint; la tabella finale usa la velocità predefinita 17.5.

## Verifiche

Build Qt5 Linux Release completa superata. CTest selezionati 14/14:
regressioni Hell base/estesa, MeteoFax, trasporto audio e contatori, Q65/MSK,
resampling/TX, guard architettura/UI/FT/release. Non è stata eseguita l'intera
suite dei 43 test registrati.

I test Hell verificano anche l'uguaglianza esatta del nastro con blocchi
17/1024 campioni, cambi rate consecutivi, reset, blocchi 513/1024, zoom 1→3→1
e silenzio digitale. Sono test sull'immagine pubblica, senza accesso privato.

RxDecoderWorker alimentato alla frequenza reale con GUI inattiva:
6 casi (due varianti per tre sample rate), 900/900 blocchi elaborati,
zero persi/azzerati e massimo un blocco in coda. Questa prova usa un segnale
audio di carico a due toni, mentre la qualità dei pixel è misurata nei test
immagine separati. Non è una garanzia contro interruzioni driver o stalli illimitati.

Restano da verificare ricezioni Hell reali, hardware audio e build native
Windows/macOS. Non è introdotto un nuovo aggancio automatico di clock o sintonia;
AFC e velocità configurata conservano il percorso precedente. TX e sorgenti
degli altri modem sono invariati rispetto a R15.

Dati: R16_HELL_RESULTS.json. Per ripetere dopo configurazione BUILD_TESTING=ON:

```sh
cmake --build build --target madmodem_hell_image_regression
ctest --test-dir build --output-on-failure -R '^madmodem_hell_image_'
```
