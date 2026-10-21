# MadModem 0.6.4 — R13

Revisione `0.6.4-source-r13-ft-capture-qrz-cq-worklist`.

## Cosa cambia

- Periodi FT e UTC tornano in fondo a Modo. La scheda QRZ, tra Modo e Rotore,
  contiene solo le informazioni della stazione selezionata. Account nelle Impostazioni.
- «Accoda chi mi chiama» è la coda esistente delle risposte dirette al proprio
  nominativo. Selezionare più righe CQ non alimentava quella coda: il comportamento
  osservato dal tester non indicava una selezione multipla funzionante.
- Nuova «Gestisci coda CQ»: quando attiva, doppi clic successivi aggiungono i
  corrispondenti alla lista senza sostituire il QSO in corso. I duplicati sono ignorati.
  Si possono anche selezionare più CQ con Ctrl/Maiusc e premere «Accoda CQ selezionati».
- «Lista CQ» mostra una tabella numerata nell'ordine di aggiunta, con nominativo,
  locator, frequenza e stato. Aggiungere elementi non avvia la TX. «Avvia lista»
  abilita il lavoro sequenziale; «Pausa dopo QSO» lascia finire il QSO corrente;
  STOP ferma TX e lista. Avvia rimane attivo anche se la lista si svuota:
  puoi aggiungere altri corrispondenti durante l’ultimo QSO. Disabilitare la gestione CQ mette in pausa la lista.
- Prima di ogni nuovo QSO serve un CQ confermato del periodo di verifica corrente
  o successivo. Messaggi preliminari destinati solo alla visualizzazione non
  autorizzano TX. Se la stazione lavora un altro corrispondente viene saltata;
  se non arriva un CQ fresco entro un ciclo completo più 2,5 secondi viene saltata.
  L'ordine è rispettato anche quando una stazione successiva viene ricevuta prima.
- Massimo 30 corrispondenti, lista solo per la sessione. Cambio banda/modo la svuota.
  Stazioni in blacklist e proprio nominativo sono esclusi. I chiamanti diretti,
  se la relativa coda è abilitata, hanno precedenza. La lista usa lo stesso
  sequencer e la normale strategia TX; non richiede Evil Mode e non forza autolog.

## FT8/FT4: problema concreto nel percorso audio

Il log live del 20 ottobre contiene tre intervalli RX completi scartati per buchi
nella sequenza audio (2, 6 e 1 blocchi). Altri tredici scarti riguardano acquisizioni
parziali dopo avvio/TX e non sono lo stesso problema. La coda audio arriva a
1.655,1 ms. Il lavoro boundary dura in mediana 1.402 ms; il gate 478 ms; gli
aggiornamenti UI circa 60,6 ms (massimo 93,7 ms). La GUI da sola non spiega la coda.

Nel sorgente R12, il decoder tiene il mutex di configurazione durante l'intero
job. Anche la raccolta audio lo prende per leggere periodo e sigla del modo.
La coda limitata può quindi riempirsi mentre il decoder lavora. R13 pubblica
atomicamente il periodo usato dalla raccolta: queste letture non aspettano più
il mutex. La serializzazione delle impostazioni rimane; ricerca candidati,
soglie, passaggi, cancellazione interferenze, FEC e decoder CW sono invariati.

Controllo A/B: stesso binario candidato, ripristinando soltanto i quattro accessori
bloccanti R12. Il test trattiene il mutex per 750 ms e inietta 100 blocchi da
1.024 campioni a 48 kHz ogni 5 ms nel dispatcher reale da 64 blocchi.

| Modo | Accessori R12: ricevuti/persi | R13: ricevuti/persi | Chiamata raccolta peggiore R12 / R13 |
| --- | --- | --- | --- |
| FT8 | 65 / 35 | 100 / 0 | 752,5 / 0,166 ms |
| FT4 | 65 / 35 | 100 / 0 | 749,5 / 0,358 ms |

È una riproduzione controllata del blocco, non una misura del numero di stazioni
recuperate via radio. Il log non identifica la revisione binaria e non contiene
il WAV originale: non prova che questa sia l'unica causa del confronto 50 contro
37 con JTDX. R13 registra identità sorgente/build e contatori di riuso dei calcoli
incrementali per rendere verificabile la prossima prova live.

## Tempi e sequencer nel log

EU4U arriva con DT +1,3 s. Un messaggio FT8 di 12,64 s, con inizio nominale a
0,5 s e quel ritardo, termina intorno a 14,44 s: il gate a 13,5 s non può ancora
averlo completo. Le risposte vengono infatti recuperate dal boundary, dopo
l'avvio del tentativo TX già armato. La regola di un tentativo per periodo può
rinviare la risposta successiva. Il divario di due ore tra timestamp locali e
UTC è normale per il fuso del log; DT +1,3 non dimostra un errore dell'orologio locale.
R13 non sostituisce al volo una trasmissione già iniziata né altera questa politica.

## Validazione e limiti

Risultati strutturati in [R13_LIVE_RX_RESULTS.json](R13_LIVE_RX_RESULTS.json).
Build Linux Qt5 Release completata; CTest 8/8 superati. Sonda GUI CQ su FT8 e
FT4: selezione multipla, doppi clic senza duplicati/interruzioni, CQ nuovo dopo
il QSO, rispetto dell'ordine, pausa e STOP verificati in quattro esecuzioni finali.
Regressioni UI periodi/font/QRZ, risposta finale/no-response e selezione CW
Nativo R12 / Neural R13 superate. Parità numerica esatta e stesso ordine dei
candidati con/senza calcoli incrementali su sette registrazioni FT8 reali e un
caso FT4 generato; corretto fallback sull'audio modificato e dopo invalidazione.
Ripristinando i soli quattro accessori e togliendo la pubblicazione atomica,
il file decoder risulta identico byte per byte a R12.

Una prima sonda GUI, prima delle revisioni finali, è terminata con SIGSEGV in
chiusura. Non si è riprodotto nelle quattro esecuzioni finali, tre delle quali
con raccolta automatica della traccia in caso di SIGSEGV; la causa iniziale non
è stata isolata. Il controllo esteso del QSO successivo ha invece trovato e
corretto il riuso di CQ sentiti durante il collegamento precedente.

Restano da verificare le build native Windows/macOS, CAT/PTT e acquisizione
su hardware reale. La superiorità rispetto a JTDX richiede la stessa registrazione
audio, gli stessi intervalli e risultati confrontabili; non è dichiarata da
questa revisione.
