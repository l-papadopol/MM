# R9 — Avvio Windows e comandi rapidi del rotore

Revisione: `0.5.9-alpha-source-r9-windows-rotator-ui`.
Baseline: R8 completa, incluse le correzioni ai worker RX e al CAT asincrono.

## Menu all'avvio

Il codice precedente forzava il fullscreen tre volte: all'apertura, al primo
ciclo eventi e dopo 250 ms, richiamando anche attivazione e sollevamento della
finestra. Questa sequenza può interferire con la gestione dei popup. È coerente
con il feedback Windows: Impostazioni è un'azione diretta che apre un dialogo,
mentre Modo e gli altri titoli aprono menu popup.

La R9 apre una normale finestra massimizzata e rimuove i richiami ritardati
che forzavano fullscreen e focus. Anche il pulsante di ingrandimento e il
doppio clic sulla barra del titolo alternano ripristino e massimizzazione.
I popup Windows mantengono i flag nativi Qt. La cornice grafica resta
trasparente agli eventi del mouse.

Il navball del rotore, disegnato interamente con QPainter, ora usa QWidget
invece di una superficie QOpenGLWidget non necessaria. Disegno e coordinate
restano gli stessi; il pannello non richiede più un contesto OpenGL aggiuntivo.
Il waterfall conserva il proprio rendering OpenGL.

La causa del comportamento sul desktop Windows va confermata dal test utente:
questa revisione elimina il percorso fullscreen/attivazione coinvolto, ma i
test automatici locali non riproducono il compositore grafico di Windows.

## Rotore

- Cursori per azimut ed elevazione, sincronizzati con i campi numerici.
- Il pannello tiene conto delle etichette su più righe: quando è stretto
  aumenta l’altezza del contenuto e usa lo scorrimento, evitando sovrapposizioni.
- I cursori seguono i limiti del profilo, compreso l'overlap fino al limite
  configurato; l'elevazione è disabilitata sui rotori solo azimutali.
- Quattro memorie nominabili per ciascun profilo rotore, persistenti nel file
  `settings.mad`. Sono posizioni az/el scelte dall'operatore, non zone
  geografiche con un azimut universale.
- Per salvare: impostare gli angoli con i cursori e cliccare una memoria
  vuota, quindi darle un nome (per esempio Giappone o America).
- Per aggiornare una memoria: impostare gli angoli e fare clic destro sul
  pulsante, confermando il nome.
- Per richiamare: cliccare il pulsante della memoria, poi **Vai**. Cursori e
  richiamo modificano soltanto i valori: non avviano il motore.
- Una memoria fuori dai nuovi limiti viene rifiutata con un messaggio; non
  viene trasformata silenziosamente in una destinazione diversa.
- I nomi lunghi vengono abbreviati sul pulsante e rimangono completi nel
  suggerimento, insieme agli angoli salvati.

La logica del controller, incluso il parcheggio già confermato funzionante,
non è stata modificata. Il puntamento geografico già presente tramite campo
Target rimane disponibile. Non è stato aggiunto il trascinamento sul navball.

## Verifiche

Il nuovo test `MadModem --window-rotator-regression` controlla:

- massimizzazione iniziale senza fullscreen;
- arrivo dei clic alla barra menu attraverso la cornice;
- apertura popup e selezione delle azioni, prima e dopo ripristino/
  massimizzazione, con i temi Avionica, Qt e Classic Dark;
- disponibilità del popup delle combo;
- sincronizzazione bidirezionale dei cursori, overlap e disattivazione El;
- salvataggio/richiamo delle memorie, separazione per profilo e rifiuto
  delle posizioni fuori limite;
- assenza di comandi al motore durante modifica o richiamo dei valori.

Compilazione Linux Release riuscita (Qt 5.15.13, GCC 13.3, Hamlib 4.5.5).
**21/21 test CTest superati**, incluso il controllo della sovrapposizione
tra navball e campo azimut. I test UI locali usano Qt offscreen.
Risultati della compilazione e della suite finale in `verification-r9/`.
I nuovi testi sono presenti nei sei dizionari dell'app.

## CW e prova Windows

Questa revisione non cambia gli algoritmi di decodifica CW. I test sintetici
non equivalgono a una verifica su segnali radio reali. Per diagnosticare il
feedback CW serve un WAV RX non ricompresso (30–60 secondi), con indicazione
di velocità approssimativa, frequenza audio, impostazioni e comportamento
osservato: assenza di decodifica, caratteri errati, ritardo o altro.

Su Windows verificare, senza ridimensionare dopo l'avvio: File, Modo, Lingua
(se disponibile), Aiuto, scelta CW/RTTY, apertura Impostazioni e ritorno ai menu.
Ripetere dopo ripristino e massimizzazione. Verificare poi i cursori e una
memoria del rotore, usando Vai per avviare il movimento.

## GitHub

Caricare il contenuto della cartella nello ZIP, inclusa `.github`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```
