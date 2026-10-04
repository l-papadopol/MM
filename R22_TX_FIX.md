# R22 — FT8/FT4 e RTTY TX

- Il rilascio PTT e il ripristino split non vengono annullati quando si arma il periodo FT successivo. Un recupero CAT riuscito non viene più convertito in un falso blocco permanente della TX.
- Timeout e reali errori di rilascio mantengono il blocco di sicurezza; Stop può riprovare il recupero anche a TX ferma.
- Le macro testuali accettano correttamente la preparazione asincrona. Il testo resta associato alla richiesta mentre RX viene chiusa; le risposte rapide RTTY funzionano anche con l’editor principale vuoto.
- Le righe TX e l’evidenziazione compaiono alla conferma della riproduzione audio. Stop scarta le risposte rapide accodate.
- Orologio FT: distanza tra quadrante e display aumentata, altezza minima del pannello protetta; eliminata la riga colorata sotto il display.
- Conservate le correzioni qint64/Qt6/macOS della R21.1.

Verifica locale: build Linux Qt 5.15, suite CTest e probe GUI con backend CAT simulato. Nessuna prova RF o compilazione Windows/macOS eseguita in questo ambiente.
