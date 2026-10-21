# MadModem 0.6.4 R17 — drift RX da 144 MHz, fino a ±20 Hz/s

## Funzionamento

La casella esistente in Impostazioni → FT8 / FT4 abilita il recupero vicino
al marcatore RX. Il recupero ora richiede anche una frequenza RF di almeno
144000000 Hz. Frequenze inferiori e contesto RF sconosciuto non avviano il DSP
aggiuntivo, non eseguono tentativi e non consumano il budget drift.
La casella resta memorizzata passando fra bande: tornando in VHF/UHF il recupero
può operare automaticamente, senza dover riselezionare la preferenza.

Con CAT connesso prevale la frequenza letta dalla radio, anche fuori dalle
frequenze FT standard. Senza CAT si usa la frequenza standard della banda
selezionata dall'operatore. In quest'ultimo caso la radio deve essere realmente
sintonizzata sulla banda scelta. Per eventuali transverter, il valore CAT deve
rappresentare la frequenza RF operativa: questa revisione non introduce un
nuovo offset transverter.

Il contesto RF è atomico: la GUI lo aggiorna senza attendere il mutex del job.
L'idoneità RF è ricontrollata anche fra ricerca e tentativi FEC. La predizione
della deriva non passa da una stazione/modo a un altro e viene esclusa dopo
un QSY RF di almeno 1 kHz rispetto al riferimento verificato.

## Range e algoritmo

Il limite impostabile passa da 5 a 20 Hz/s in GUI, caricamento/salvataggio e
decoder. Il valore iniziale rimane 2 Hz/s e la casella rimane disattivata per
impostazione iniziale. I profili esistenti conservano il valore selezionato.

20 Hz/s indica una variazione di 20 Hz per ogni secondo. Su un frame FT8 da
12.64 s la variazione totale è circa 252.8 Hz; su FT4 da 4.944 s circa 98.88 Hz.
Non è un massimo teorico universale: serve un segnale dentro la banda passante
e una deriva sufficientemente lineare durante il messaggio. Segnali tagliati
dal filtro radio e derive fortemente non lineari non vengono ripristinati da
questa sola compensazione.

Non è stato soltanto alzato il limite numerico:

- Baseband complessa da 500 a 1000 campioni/s, filtro 330 Hz per mantenere
  l'intero segnale nel nuovo intervallo di deriva.
- Evidenza Costas calcolata per ogni simbolo di sincronismo. Ogni ipotesi di
  deriva segue la frequenza dei singoli simboli, invece di supporla costante
  per un intero blocco Costas.
- Ricerca dei tempi distribuita nel pool CPU persistente e dimensionato dalla
  topologia, con buffer locali e unione protetta delle migliori ipotesi.
- Compensazione della fase quadratica sull'audio analitico, quindi normali
  controlli LDPC/CRC. Al massimo tre tentativi FEC; risultati già acquisiti
  conservati. Restano i budget drift di 280 ms FT8 e 220 ms FT4, con controllo
  fra le fasi e cancellazione cooperativa: non sono limiti di preemption del
  sistema operativo né garantiscono l'intero percorso RX/UI/TX su ogni PC.

Ricerca normale FT8/FT4, soglie, FEC, sottrazione e limiti TX sono invariati
rispetto a R16. Non è stata modificata la pianificazione del job boundary:
le misure qui sotto sono del recupero drift isolato, non del tempo totale
dal confine del periodo alla risposta automatica. Il recupero segue ancora
le passate normali. In HF la nuova condizione elimina il costo drift visto
nei log R13 forniti dall'utente.

## CPU, FPU, SIMD e GPU

Il nuovo lavoro sfrutta più core tramite il pool FT esistente: sette worker
nelle prove di questo ambiente; il numero dipende dal PC e dalle riserve per
acquisizione e interfaccia. I calcoli in virgola mobile usano le unità FP della
CPU. Il motore toni conserva i percorsi SIMD selezionati a runtime; questa
revisione non aggiunge un nuovo kernel SIMD al correlatore drift.

La GPU continua a gestire il waterfall tramite il percorso OpenGL esistente.
Non è stato aggiunto un backend GPU per il decoder/drift. Qui non è esposto
alcun dispositivo /dev/dri o NVIDIA con cui verificarlo. Non si dichiara quindi
un'accelerazione GPU del DSP né un benchmark GPU. Un backend compute resta
un lavoro distinto, da misurare includendo trasferimenti e sincronizzazione.

## Verifiche

Generatore FT reale MadModem; deriva aggiunta indipendentemente con
scipy.signal.hilbert e fase quadratica, più rumore gaussiano deterministico.
Frequenza audio nominale 1033 Hz; frequenza RF del ricevitore 144000000 Hz.
Questi segnali sintetici non stabiliscono la sensibilità su segnali satellitari.

| Prova | Esito |
|---|---|
| FT8 e FT4, derive −20/−10/−2/+2/+10/+20 Hz/s, rumore 0.01 e 0.04 | 24/24 messaggi recuperati e CRC validi |
| Frequenza sconosciuta, 14.074 MHz, 50.313 MHz, 143999999 Hz, entrambi i modi | 8/8: zero tentativi e zero tempo drift contabilizzato |
| Opzione disattivata e solo rumore con opzione attiva | 4/4, nessun messaggio accettato |
| Predizione fra periodi a +2 e +20 Hz/s, FT8/FT4; cambio DX | 4/4 |
| UI limite 20, salvataggio/riavvio, banda manuale 40m/2m | superato |

Tempi del recupero nelle 24 prove, sette worker:

- FT8: 81.3–96.1 ms.
- FT4: 31.8–53.2 ms.

Il test di cambio DX verifica il centro effettivo della ricerca: il vecchio
centro predetto non viene ereditato. Una stazione ancora dentro la normale
finestra locale può comunque essere decodificata. La prima versione del test
richiedeva erroneamente di sopprimere anche quel messaggio CRC valido; il
controllo è stato corretto sulla proprietà effettivamente richiesta.

Build Linux Qt5 Release completa e 14 CTest selezionati superati, inclusi
MeteoFax/Hell e guard runtime/UI/FT/release. Non è la suite completa di 43 test.
Restano da verificare Windows/macOS nativi, hardware radio e GPU, e registrazioni
reali con derive elevate. Rapporto numerico: R17_DRIFT_RESULTS.json.

Per ripetere i test drift opzionali dopo una build Release con compile_commands:

```sh
python3 tools/run_ft_drift_probes.py build
```

Servono NumPy/SciPy. Il programma di prova si collega agli oggetti dell'app e
usa accesso diagnostico privato; non è richiesto alcun dispositivo audio.
