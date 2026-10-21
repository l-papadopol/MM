# MadModem 0.6.4 R15 — ricezione MeteoFax

Questa revisione migliora la demodulazione FM e la conservazione dei dettagli
orizzontali del MeteoFax. Mantiene le modifiche audio e alle altre modalità della
R14. La versione pubblica rimane 0.6.4.

## Modifica

Il mixer I/Q ora usa un passa-basso Butterworth di quarto ordine, realizzato
con due biquad identici su I e Q. Il taglio nominale è 800 Hz con i toni
1500/2300 Hz; segue l'ampiezza configurata dei toni, limitato a 600–1100 Hz.
Il precedente filtro a un polo aveva reiezione insufficiente dell'immagine
prodotta dalla conversione e introduceva errore nei livelli di grigio.
Lo stato del filtro resta continuo fra blocchi; viene azzerato su reset/cambio
sample rate. Non richiede finestre future né introduce accodamento di righe.

Il filtro orizzontale dei pixel passa da alpha 0.55 a 0.75: conserva più dettagli
ora che il filtro I/Q elimina meglio i prodotti della conversione. Il solo
cambio del filtro I/Q è stato scartato perché non migliorava sistematicamente
l'immagine finale con la levigatura precedente.

La soglia di ampiezza usa il modulo quadrato e la normalizzazione
trigonometrica dell'oscillatore calcola la radice solo quando necessaria
(ogni 4096 campioni). Sono ottimizzazioni locali; non cambiano il discriminator
atan2 o la sua convenzione di fase.

APT, inseguimento automatico dei toni, fasatura, clock delle righe e UI
mantengono il comportamento R14. Non viene reintrodotta la correzione slant
rimossa nelle revisioni precedenti.

## Confronto riproducibile

Il riferimento è il decoder R14, sullo stesso audio e con gli stessi parametri.
Il generatore FM è indipendente dal trasmettitore MadModem. La prova immagine
attraversa l'API pubblica processAudioBlock e confronta i pixel finali con una
mira nota, senza normalizzare contrasto o riallineare separatamente le righe.
È ammessa una sola traslazione orizzontale globale per il ritardo dei filtri.

| Prova | Esito |
|---|---|
| Demodulazione, 44.1/48/96 kHz, gradini/rampa/dettagli, SNR 60/15/5 dB | Errore inferiore in 27/27 casi |
| Immagine a 120 LPM, 3 sample rate, SNR 60/10/0 dB, fading e interferente 3100 Hz | Errore inferiore in 9/9 casi |
| Secondo seed, 60/90/240 LPM, SNR 35/5/−5 dB, 3 sample rate | Errore inferiore in 25/27 casi |

Esempi a 48 kHz, 120 LPM, RMSE in livelli di grigio su scala 0–255:

| Segnale | R14 | R15 |
|---|---:|---:|
| SNR 60 dB, immagine completa | 37.02 | 30.51 |
| SNR 60 dB, dettagli a quattro pixel | 65.90 | 54.44 |
| SNR 10 dB, immagine completa | 37.69 | 31.61 |
| SNR 0 dB, immagine completa | 46.61 | 41.19 |

Limite osservato: a 240 LPM e SNR −5 dB l'errore aumenta del 2.8% a 44.1 kHz
e dell'1.4% a 48 kHz. Non si afferma un miglioramento universale a qualsiasi
velocità/rumore. L'RMSE non è una percentuale di caratteri o pixel errati.

Risultati numerici: R15_WEATHERFAX_RESULTS.json. Le misure temporali includono
anche la generazione dell'audio: non sono un benchmark isolato del filtro.
Il gruppo demod è precedente alla modifica alpha dei pixel, che non interviene
sul buffer di frequenze misurato da quella sonda.

## Verifiche e limiti

WeatherFaxImageRegression testa la ricostruzione e i dettagli con rumore,
fading e una portante vicina; il gruppo esteso cambia seed e LPM. Controlla
anche l'uguaglianza dei pixel con blocchi da 17/513/1024 campioni, reset e
passaggi 44.1→48→96 kHz. Non usa l'hardware audio.

Non è disponibile una registrazione MeteoFax reale con immagine di riferimento:
questi risultati sono sintetici. La validazione in radio resta necessaria,
come le build native Windows/macOS. Non è stata modificata la sensibilità
APT né dimostrata una sua acquisizione migliore.

Build Linux completa e versione coerente: superate. I 12 test selezionati
(runtime/audio, Q65/MSK, guard architettura/UI/FT/release e due test MeteoFax)
sono passati. Il comando CTest generale comprende anche target non compilati
in questo ambiente e non è una verifica completa dei 41 test registrati.

Alimentando RxDecoderWorker alla frequenza reale, con GUI inattiva, sono stati
elaborati 450/450 blocchi (150 per sample rate), zero scarti/azzeramenti e
massimo un blocco in coda. È una prova software a carico limitato, non una
garanzia contro interruzioni del driver o sovraccarico arbitrario del PC.

Per ripetere i test immagine dopo una normale configurazione CMake con
BUILD_TESTING=ON:

```sh
cmake --build build --target madmodem_weatherfax_image_regression
ctest --test-dir build --output-on-failure -R '^madmodem_weatherfax_image_'
```

WeatherFaxDemodProbe.cpp è una sonda diagnostica del buffer interno: richiede
link agli oggetti dell'app e accesso privato disabilitato (`-fno-access-control`
con GCC). I due test CTest usano invece l'API pubblica e funzionano senza quella
opzione e senza dispositivi audio.
