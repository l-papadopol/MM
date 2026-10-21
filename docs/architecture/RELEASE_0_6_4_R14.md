# MadModem 0.6.4 R14 — continuità audio RX

Revisione `0.6.4-source-r14-all-mode-audio-continuity`.

## Percorsi controllati e correzioni

La cattura legge tutto il PCM disponibile, conserva i byte incompleti tra callback
e produce blocchi mono da 1.024 campioni con indice, generazione e timestamp.
I decoder ricevono direttamente dalla cattura attraverso code limitate; il
waterfall usa una coda separata, dove scartare disegni arretrati è consentito.

| Famiglia | Percorso e risultato dell'ispezione |
| --- | --- |
| CW Nativo R12 / Neural R13 | RX worker indipendente dalla GUI; A/B elaborati sul worker, skimmer di banda su worker separato. Algoritmi invariati. |
| RTTY singolo / skimmer | RX worker; fino a 16 decoder indipendenti. AFC e filtri restano sullo stesso proprietario. |
| BPSK31, MFSK16/32 | RX worker; tutti i campioni ricampionati vengono elaborati. Le rimozioni di testo sono limiti di visualizzazione. |
| Feld Hell / FSK105 | RX worker; immagine e telemetria passano alla GUI come snapshot. |
| SSTV / WEFAX | RX worker; demodulazione di tutti i campioni prima di elaborare le righe. I buffer di ricerca mantengono storia limitata; eventuale backlog WEFAX viene segnalato. |
| FT8 / FT4 | Correzione R13 mantenuta: letture del profilo durante la raccolta indipendenti dal mutex dell'intero decode. R14 rende esplicito anche il contatore di overflow della coda. |
| Q65 A/B/C/D | Trovato decode sincrono nel thread di raccolta. R14 invia finestre audio/configurazioni immutabili a un worker seriale; un solo motore conserva le medie. Reset invalida lavori e risultati precedenti. |
| MSK144 | Il decode era già separato dalla raccolta. Trovata attesa lunga in chiusura durante Deep; aggiunti controlli di cancellazione solo per reset/chiusura. |
| Radio Telescope | La misura dipendeva dalla coda della GUI. R14 la esegue nel worker RX su finestre UTC fisse, con lo stesso Welch/Hann/trimmed mean. Un ritardo della GUI non modifica i campioni del dwell. Finestre mancanti/discontinue vengono marcate non misurate. |
| Registrazione WAV | Coda propria da 128 blocchi. Se il disco non tiene il passo, ferma la registrazione e segnala errore; evita di consegnare un WAV apparentemente continuo con campioni mancanti. |

La coda comune espone contatori cumulativi: inseriti, prelevati, persi per
saturazione, rimossi da reset, pendenti e massimo riempimento. Vale l'identità
`inseriti = prelevati + persi + rimossi + pendenti`. La politica di capacità
limitata è preservata. FT e Q65 ora riportano esplicitamente l'overflow;
gli altri decoder avevano già segnalazione e reset di continuità.
La registrazione conserva anche la politica preesistente di inserimento di
silenzio per alcuni salti dell’indice campioni; tale silenzio non recupera
l’audio mancante. La saturazione della sua coda viene invece intercettata
prima della scrittura e ferma il WAV con errore.

Il calcolo Q65 lavora su un thread dedicato e mantiene al massimo due periodi
completi in attesa. Mantiene lo stesso motore nativo, configurazione del periodo,
ordine e stato di averaging. Un cambio generazione elimina i risultati obsoleti.
Il segnale pubblicato usa ora sempre UTC, submodo e frequenza della configurazione
salvata con il job, anche se l'operatore cambia impostazioni mentre viene elaborato.

## Prove

I risultati finali e le quantità sono raccolti in `R14_AUDIO_CONTINUITY_RESULTS.json`.

| Verifica finale | Esito |
| --- | --- |
| 13 casi × 3 sample rate | 39/39 superati; 6.500 blocchi inseriti e prelevati, zero persi o rimossi |
| Ricomposizione PCM mono/stereo | 6/6; tutti i campioni e gli indici esatti, residui incompleti conservati |
| Raccolta FT8/FT4 con lock decode occupato | 100/100 blocchi per modo, zero buchi di sequenza |
| Q65 sotto contesa del codec | Controllo sincrono 64/100; R14 asincrono 100/100 |
| Q65 A/B/C/D, due periodi ciascuno | 8/8 risultati esatti rispetto allo stesso motore sincrono |
| CTest mirati | 10/10 superati |
| Coda CQ e disposizione Modo/QRZ | GUI FT8/FT4 superata |

Build Linux Qt5 Release completata. Il corpo delle funzioni Welch è identico
a R13. Togliendo le sole righe di cancellazione, il decoder MSK144 è identico
a R13: nessun taglio a soglie, candidati o ricerca ordinaria. Tutti gli altri
algoritmi CW, RTTY, PSK, MFSK, SSTV, WEFAX e Hell sono invariati.
Il test di flusso reale alimenta il dispatcher di produzione e il vero grafo dei
decoder mentre il thread principale non elabora eventi. Comprende DSP attivo,
A/B CW e skimmer, 16 tracce RTTY, MFSK16/32, entrambe le varianti Hell, audio
SSTV/WEFAX generato e un periodo MSK144 Deep completo a 48 kHz. Le verifiche
sono ripetute a 44.100, 48.000 e 96.000 Hz; sono prove di trasporto/continuità,
non confronti di sensibilità o validazione di tutti i sottoprotocolli SSTV.

La sonda di acquisizione introduce frammenti PCM di 1, 53, 2.047 e 5.129 byte,
anche a cavallo di campioni e blocchi: confronta ogni campione del canale scelto,
sequenza e residuo parziale, per tutti e tre i sample rate e mono/stereo.

Il controllo di contesa Q65 mantiene occupato il lock del codec per 750 ms:
il percorso sincrono sul thread di raccolta perde 36 blocchi su 100, quello
asincrono R14 ne conserva 100 su 100.

La sonda Q65 confronta i risultati asincroni con l'esecuzione sincrona dello
stesso motore su A/B/C/D, due periodi per submodo: messaggi, frequenza, DT,
averageCount e UTC. Durante il job raccoglie già il periodo successivo.
Le prove di overflow verificano i contatori e l'ordine, mentre quelle della
misura radio verificano uguaglianza esatta della potenza, finestre UTC, reset
e rifiuto dei campioni mancanti. Le regressioni native e di proprietà dei thread
controllano anche recupero RTTY, percorso TX, ricampionamento e modalità FT.

## Limiti pratici

La prima prova Deep MSK144 su rumore e portanti ha superato diversi minuti di
calcolo. La raccolta ha comunque mantenuto tutti gli 800 blocchi; la chiusura
attendeva il termine del calcolo. R14 permette di cancellare quel lavoro quando
si ferma/resetta il decoder. Non riduce i candidati e non impone nuovi tagli al
decode normale. Il costo della ricerca Deep resta un limite: se un periodo è
ancora in elaborazione, la politica MSK144 esistente segnala e salta il job
successivo. Questo è distinto dalla perdita di blocchi della raccolta audio.

Nessuna coda finita può garantire zero perdite con CPU ferma o disco bloccato
per un tempo arbitrario. Lo skimmer CW può ridurre i canali di banda e resettare
la propria storia su sovraccarico, lasciando indipendenti A/B; Q65 segnala una
coda di periodi piena. Stop, TX e cambio modo possono scartare intenzionalmente
la coda del vecchio modo. Queste condizioni non vengono confuse con un normale
intervallo RX continuo.

Queste prove usano Linux Qt5 senza hardware audio/CAT/PTT. Le perdite che avvengono
nel driver prima della consegna del PCM richiedono una prova con la scheda reale;
QAudio riporta gli errori del backend ma non offre una garanzia hardware universale.
Restano le build native Windows/macOS e la verifica on-air. La modifica non
dichiara una nuova sensibilità FT8 o parità con JTDX.
