## R20 — stato FT nella scheda Modo e controlli compatti

Il vecchio banner sopra il waterfall è ora un indicatore nella scheda Modo,
nello spazio libero sopra i periodi FT. Fondo trasparente e sottile bordo di
stato; testo breve (TX, TX tra Ns, In attesa, RX/Ultima TX) e messaggio completo
sulla seconda riga. Rimane il riferimento Tx1–Tx6, quando il messaggio coincide
con una riga. I tag interni del sequencer non compaiono. Dettagli e orario UTC
sono nel tooltip. La distinzione tra messaggio scelto e TX programmata resta.

Controlli sopra il waterfall su quattro righe allineate: periodo/comandi,
ripetizioni/limite risposte/log automatico, strategia frequenza/caller queue,
coda CQ. Periodi TX I / TX II, con spiegazione nel tooltip. Il contatore QSO e
chiamanti in attesa passa accanto allo stato nella scheda Modo. La modalità
Evil mantiene i propri controlli e il meccanismo di sblocco esistente.

Modifiche di presentazione: algoritmi modem, backend GPU, accettazione decode,
scheduler TX, CAT/PTT e gestione code conservati dalla R19. Il pacchetto contiene
le opzioni GPU della R19; il drift rimane attivo solo da144MHz e fino a±20Hz/s.

Il log ricevuto il21ottobre indica build121/sourceR17 a14.074MHz, precedente al
backend GPU R18/R19. Le righe «waterfall ... GPU» riguardano il rendering.
Non costituisce una prova del decoder GPU. La nuova casella ordinaria R19 vale
anche in HF; quella drift segue il limite RF sopra indicato.

Verifiche: build Linux; sonda UI FT8/FT4 a1280×800 e1920×1080 con cinque stati,
messaggio e riga TX, appartenenza alla scheda Modo, contatore controlli e label
CQ nascosta. Screenshot di controllo interni. Audit architettura, UI/temi,
runtime FT e release: 4/4 PASS. Il controllo temi copre 5 temi. Nessuna prova nativa
Windows/macOS o nuova misura GPU fisica in questa revisione UI.
