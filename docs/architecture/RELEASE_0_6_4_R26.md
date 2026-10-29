# R26 — confronto FT8 del 28 ottobre e copia delle decodifiche

Sorgente completo basato sulla R25. Versione pubblica 0.6.4.
Revisione `0.6.4-source-r26-ft8-nearby-sic-clipboard`.

## Uso

Nella tabella RX FT8/FT4, selezionare le righe con mouse/Ctrl/Shift oppure
Ctrl+A, poi Ctrl+C. Incollare in un editor di testo, Word o Excel. La copia
contiene le colonne visibili, separate da tabulazioni, una riga per messaggio,
nell'ordine visualizzato. Non modifica il QSO, non arma TX e non cancella righe.
Nessuna selezione lascia invariati gli appunti. Il comando usa la scorciatoia
Copy della piattaforma (Cmd+C su macOS, da verificare nativamente).

## Materiale e limiti del confronto

L'Excel allegato contiene solamente l'elenco JTDX: 49 messaggi dello slot
15:38:30 e 66 dello slot 15:38:45. Non contiene le 43 righe MadModem citate nel
feedback. Si contano messaggi, non necessariamente altrettanti nominativi unici.
Il log ha 27 righe e termina all'avvio RX: conferma build126/R25, TS-890,
14.074MHz, filtro CAT3000Hz e capacità CPU, ma non contiene risultati, tempi o
stato del clock durante la ricezione. Non consente di diagnosticare l'acquisizione
Windows o di ricostruire le 43 righe osservate.

Il RAR contiene un WAV mono PCM16,16000Hz,348000 campioni (21.75s), senza
campioni saturi. La presenza dei Costas colloca il fotogramma completo nel
secondo periodo. Il replay usa la finestra di15s a partire da5.5s; non spezza il
file dall'inizio in slot arbitrari. L'allineamento è dedotto dal segnale, non
prova l'esattezza del timestamp di acquisizione. L'hash WAV è nel JSON allegato.
Il primo fotogramma è incompleto: non è un secondo confronto valido su49righe.

## Difetto corretto

Dopo una decodifica e la sottrazione del relativo segnale, `alreadyDecoded`
escludeva qualsiasi candidato entro25Hz e350ms. Questo può scartare una seconda
stazione distinta vicina a quella appena cancellata. Nell'elenco JTDX42dei66
messaggi hanno almeno un altro messaggio entro quella regione (sulle coordinate
arrotondate dell'Excel). Questo conteggio descrive l'affollamento, non42perdite
attribuibili al difetto.

La R26 riduce solamente il confronto in frequenza a3.125Hz, un passo della
ricerca FT8. Mantiene la tolleranza temporale e tutti i limiti di candidati,
LDPC/CRC, sottrazione, OSD, scadenze, sequencer e TX. Non cambia la matematica
GPU, il recupero drift, FT4 o altri modem. Nessun dato dell'Excel viene usato
come suggerimento per decodificare: il confronto conta solo messaggi prodotti
autonomamente dal decoder e validati dal percorso CRC esistente.

## Risultati misurati

Tre prove alternate R25/R26 su8registrazioni, con8worker e dati identici:

| Misura | R25 | R26 |
| --- | ---: | ---: |
| WAV tester, prova1 | 39 | 41 |
| WAV tester, prova2 | 38 | 41 |
| WAV tester, prova3 | 38 | 41 |
| Somma messaggi nelle24riproduzioni | 529 | 543 |
| Mediana tempo decoder | 698.44ms | 752.09ms |
| Massimo tempo decoder | 834.34ms | 902.82ms |

Sul WAV tester le aggiunte rispetto alla prima prova R25 sono
`OE5MON F8ADF R-03` e `CQ F4DIA JN36`. Le singole ripetizioni hanno variabilità
per l'ammissione concorrente all'OSD: su alcuni vecchi WAV cambiano o diminuiscono
le righe del singolo tentativo. Il totale cresce di14messaggi (+2.65%); non è
una garanzia di recuperare più messaggi in ogni slot. Tutti i messaggi pubblicati
in ogni job restano presenti nel risultato finale dello stesso job.

Queste sono misure LinuxQt5 su un ambiente con8CPU di quota, configurazione CAT
sviluppo senza hardware. Non equivalgono alla cattura live Windows del tester.
Resta un divario ampio rispetto ai66diJTDX: questa modifica ne corregge solo una
parte. La presenza di una riga esclusiva in uno dei programmi non dimostra da
sola che sia falsa. Non sono state misurate parità JTDX o prestazioni GPU fisiche.

`FT8_R26_JTDX_COMPARISON.csv` riporta le66righe originali e la presenza in ogni
ripetizione. `FT8_R26_RESULTS.json` contiene parametri, conteggi e verifiche.

## Esperimenti esclusi

L'affinamento del tempo di cancellazione ha peggiorato il caso di prova.
Il filtro Hann esatto per la sottrazione ha mostrato un piccolo guadagno su un
solo WAV con costo superiore, senza sufficiente validazione generale.
La selezione anticipata dei massimi locali non ha prodotto un guadagno utile.
Il recupero aggiuntivo dopo la sequenza normale, con snapshot dei residui e
limiti temporali, non ha confermato nuove decodifiche stabili: è stato rimosso,
compresi contatori e messaggi di log sperimentali. Nessuno di questi esperimenti
è presente nel codice della release.

## Riproduzione e prossimo confronto

Dopo una build Linux/GCC con Unix Makefiles:

```sh
python3 tools/run_ft_nearby_probe.py --build /percorso/build --wav /percorso/file.wav --offset 5.5 --workers 8
```

Il programma collega gli oggetti dell'applicazione e restituisce JSON; non apre
radio/audio o PTT. `--late-ms 2000` verifica che la coda coerente sia saltata
quando la risposta sarebbe fuori tempo. La preparazione incrementale simulata
non verifica driver, clock USB, dispatcher audio, GUI o trasmissione live.

Per il prossimo confronto, usare lo stesso WAV in entrambi i programmi, salvare
entrambi gli elenchi (ora Ctrl+A/Ctrl+C funziona anche in MadModem) e acquisire il
log MadModem dopo che sono apparse le decodifiche. Includere impostazioni Fast/
Deep, eventuale AP e profondità JTDX. Solo così si distinguono risultati del
motore e differenze nella catena di acquisizione o nei suggerimenti contestuali.

## Verifiche della release

Build applicazione LinuxQt5 riuscita. Superati14controlli selezionati su50
registrati: copia, coda CQ, produttore incrementale, TXpull, clock acquisizione,
sequencer, quattro guardie/audit e quattro prove SSTV (VIS/editor/correzioni/
archivio). Due eseguibili di test erano inizialmente file di build vuoti:
ricollegati, hanno superato le rispettive ripetizioni mirate.

Il probe pubblico legge direttamente il WAV originale:41messaggi,971.58ms;
il comando developer distribuito è stato eseguito anch'esso:41messaggi,
830.46ms. Queste misure singole sono separate dalla tabella delle tre prove
alternate. Silenzio e rumore con seme20261028:0messaggi. Jobritardato2000ms:
0tentativi della coda coerente, pubblicazioni conservate. Windows e radio
fisica restano da verificare dal tester.
