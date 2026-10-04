# R21 — thread, code e transizioni audio

Sorgenti completi basati sulla R20, versione applicazione 0.5.9-beta.

- Codec condiviso: accesso FIFO per classe, precedenza TX, massimo quattro TX consecutive quando RX attende, cancellazione senza spin. Q65 rilascia il codec tra tentativi indipendenti; lo stato non rientrante rimane protetto.
- Preparazione TX: un lavoro attivo e una sola richiesta sostituibile in attesa. I risultati superati non vengono pubblicati; le eccezioni sono riportate al chiamante.
- Pool FT estratto in `runtime/FtDecodeWorkerPool.h`: thread creati su richiesta e riutilizzati entro il budget della piattaforma. Lavori annidati eseguiti nel thread corrente; pubblicazione atomica del batch e propagazione delle eccezioni solo dopo il completamento di tutti i task. Se il sistema non concede altri thread, vengono usati quelli disponibili.
- Audio RX: avvio/arresto richiesti senza attese sincrone della GUI, coda limitata alla richiesta più recente, annullamento e chiusura compensativa se Stop arriva durante l'apertura del dispositivo. TX testuale prosegue dopo la conferma dell'arresto RX; Stop invalida la prosecuzione. Q65/MSK144 conservano la forma d'onda attraverso la transizione asincrona e rinviano una pausa RX tardiva.
- Cambio modo: arresto comune anche per FT, attesa di entrambe le conferme RX/TX e rifiuto delle conferme superate. Reset DSP accodato. Arresto RX in chiusura incluso nel budget condiviso dei thread.
- Registrato il tipo dei marcatori waterfall per le connessioni Qt tra worker RX e GUI: gli aggiornamenti prima potevano essere scartati da Qt.

Verifica Linux Release, Qt 5.15.13: **24/24 CTest e 20/20 controlli GUI superati**. Risultati e log in `verification-r21`. Le regressioni includono priorità/equità, cancellazione con codec occupato, 5.000 richieste TX sostituite, dispositivo audio simulato lento, Stop durante l'apertura, batch FT annidati/concorrenti ed eccezioni. La prova GUI usa la finestra applicativa e non aziona radio o dispositivi fisici.

Non sono misurati DAC/RF e prestazioni su Windows o CPU ibride/ARM. Il lock di configurazione del decoder FT resta presente; le operazioni ADIF conservano la riscrittura atomica nel worker e il drenaggio delle scritture può attendere il disco. Non si certifica l'assenza assoluta di deadlock né il massimo throughput su ogni hardware.
