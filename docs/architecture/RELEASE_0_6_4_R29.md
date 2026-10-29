# MadModem 0.6.4 R29 — correzioni FT8/FT4

La R29 applica le quattro correzioni individuate nell'indagine R28. La versione pubblica resta 0.6.4; il marcatore completo è `0.6.4-source-r29-ft-coherent-crc-osd`. R28 era un rapporto, senza modifica del software.

## Correzioni

- FT8: eliminate le famiglie che sommavano ampiezze indipendenti, ridondanti e con 6/12 metriche finali azzerate. I recuperi ammessi usano il ramo complesso 1/2/3 simboli, che copre tutti i bit e include il sincronismo nei gruppi finali. La prima metrica economica resta disponibile; il gate rapido mantiene la politica precedente.
- FT4: le metriche complesse 1/2/4 sono utilizzabili nel normale profilo profondo dopo il fallimento della prima metrica. Anche l'analisi WAV, che usa il core live FT4, le esercita.
- FT4: aggiunto il recupero OSD GF(2) di ordine 1, profondità 91; ordine 2 disabilitato. Si applica solo dopo gli altri tentativi, con almeno 10 simboli di sincronismo corretti, profilo DSP+ e permesso condiviso. CRC e unpack FT4 restano obbligatori. Il permesso viene restituito; tentativi, successi e costi sono esposti nella diagnostica.
- FEC condivisa FT8/FT4: sia min-sum sia sum-product accettano un vettore solo con sindrome e CRC validi. Un vettore di sola parità non interrompe più anticipatamente l'iterazione come successo. Il chiamante conserva ulteriori controlli CRC/protocollo.

I recuperi aggiuntivi hanno un'ammissione di massimo 24 ms dall'inizio del candidato, cancellazione e termine UTC di 1100 ms dopo la fine del periodo. Il limite controlla l'ingresso ai tentativi, non interrompe una singola operazione aritmetica già iniziata. Il gate iniziale non esegue il recupero coerente/OSD FT4. La prima metrica e le ricerche mantengono le proprie politiche; non si promette un limite rigido di 24 ms per tutto il decoder.

La coda FT8 evita di ripetere il coerente sui candidati che lo hanno già eseguito. Le soglie generali di ricerca, i finder congelati, le matrici FEC, il TX e gli altri modem sono conservati.

## Verifica e riproducibilità

`tests/FtCoherentRecoveryRegression.cpp` viene collegato agli oggetti effettivi dell'applicazione tramite `tools/run_ft_coherent_regression.py --build BUILD`. Richiede un build Unix Makefiles già completato; viene registrato in CTest su quella configurazione. Usa accesso ai metodi interni solo nella sonda, senza cambiare l'API di produzione.

La prova costruisce una parola LDPC di parità valida e CRC errato e verifica che entrambe le FEC la respingano; verifica anche un codice valido. Confronta i casi sintetici FT4 dell'indagine precedente, aggiunge rumore senza segnale e silenzio, e controlla gate e periodo scaduto. Il riferimento delle 36 condizioni normali passa da 7 messaggi R27 a 10 R29: due recuperi coerenti e uno OSD. Il candidato è noto dal segnale pulito: non è una misura di sensibilità cieca o in dB.

Il workflow GitHub manuale `source-regression.yml` usa Unix Makefiles per rendere disponibile la sonda con Qt5 e Qt6; il workflow di distribuzione mantiene il packaging esistente. Non è stato eseguito GitHub Actions da questo ambiente; nessun push è stato effettuato.

I risultati numerici dei WAV, i confronti alternati, i controlli e i limiti sono nel JSON allegato. I conteggi possono variare con l'ammissione concorrente OSD: non equivalgono a una garanzia di conservare ogni singola riga fra processi distinti. I messaggi già pubblicati nel medesimo job devono invece restare nel risultato finale.

## Perimetro

Non è un porting completo di JTDX: memoria generale di banda, AP contestuale, filtro di cancellazione e margini temporali restano differenze da valutare separatamente. Non viene dichiarata parità con JTDX. Il nominativo ricevuto solo da MadModem nel feedback originario resta non identificato nel materiale disponibile; non viene certificato come reale senza la riga/audio corrispondente.

Validazione locale Linux Qt5, senza CAT fisico o GPU reale. Qt6, Windows, macOS e ricezione RF richiedono i rispettivi build e test. Il registro diagnostico R27 rimane disponibile in Impostazioni → Diagnostica e va accompagnato dalla registrazione WAV per le prove sul campo.

## Risultati misurati

Tre confronti alternati su otto registrazioni: **543 righe R27 → 563 R29**. Sul WAV del tester: 41 righe in ogni prova R29; riferimento JTDX disponibile 66, senza nuova esecuzione JTDX. Mediana dei 24 job: 748.2 ms R27, 718.3 ms R29; margine minimo nuovo alla scadenza di risposta 455 ms. Le eventuali righe perse fra processi distinti sono elencate nel JSON, insieme alle aggiunte; nessun risultato già pubblicato viene perso nel proprio job.

17 controlli CTest selezionati superati in aggregato, 52 registrati. I guard sono aggiornati alla nuova politica coerente live, mantenendo gli hash congelati dei finder. Controlli aggiuntivi su silenzio e rumore: zero decodifiche.
