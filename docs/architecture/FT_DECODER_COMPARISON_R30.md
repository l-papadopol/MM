# FT8/FT4: confronto JTDX, MSHV e WSJT-Z — R30

## Sorgenti identificabili

| Progetto | Riferimento analizzato | Ruolo nella R30 |
| --- | --- | --- |
| JTDX pubblico | `2a0e2bea8c66c9ca94d2ea8034cf83a68cfa40eb` | Confronto metriche coerenti, memoria/AP, BP/OSD, SIC e suddivisione in banda; audit R27 conservato |
| WSJT-Z | `b3a06e89ae0174c51c8595d99e40ca179738eb0a` | Confronto FT8/FT4 con rami BP/OSD e cancellazione WSJT; file Fortran letti, applicazione non integrata |
| MSHV | 2.76.7; archivio SHA256 `2cdac0941c5e734e62983eb3782297f10cdd8373888fd09608a3c465987791e8` | Port C++ nativo del percorso FT8/FT4, isolato e collegato al ricevitore MadModem |

L'inventario completo con gli hash è in `FT_R30_SOURCE_INVENTORY.json`; i 166 file JTDX/WSJT-Z recuperati sono stati confrontati byte per byte con gli hash del checkpoint. Questi riferimenti sono riproducibili; non identificano necessariamente il programma JTDX esatto del tester e non attestano automaticamente quale sia oggi il commit più recente di ciascun progetto.

## Risultati del confronto del codice

| Stadio | JTDX pubblico | WSJT-Z analizzato | Integrazione R30 |
| --- | --- | --- | --- |
| Ricerca | Più cicli e sezioni di banda, candidati assistiti, memoria dei periodi | Ricerca Costas, affinamento in banda base e passaggi sul residuo | Conserva la ricerca MadModem e aggiunge fino a quattro intervalli sovrapposti nel port MSHV |
| Metriche FT8 | Correlazioni complesse su 1/2/3 simboli e varianti normalizzate | Famiglie coerenti e varianti di metrica in `ft8b` | Conserva le correzioni R29; aggiunge il percorso MSHV con le sue metriche e memorie |
| Metriche FT4 | Famiglie su gruppi di simboli | `get_ft4_bitmetrics` e più famiglie di LLR | Percorso R29 più recupero nativo FT4 MSHV |
| FEC | BP e OSD, maschere AP e controllo CRC | `decode174_91`, OSD dipendente dalla profondità/AP | Recupero MSHV con protocollo/CRC; cancellazione anche nei cicli OSD; codicec condiviso con priorità TX |
| AP e memoria | QSO, CQ, nominativi e segnali precedenti | Maschere AP dipendenti dal QSO | Port con memoria per istanza; il coordinatore passa myCall/dxCall e stato AP generale, QSOProgress attualmente 0 |
| SIC | Ricostruzione GFSK e filtro FFT sul residuo | Sottrazione FT8/FT4 fra passaggi | SIC MadModem esistente conservata; il port lavora su una propria copia e sul proprio residuo |
| Tempi | Dipendono da configurazione, passaggi e scheduling | Dipendono da profondità e scheduler | Deadline UTC del recupero: gate fine periodo −50 ms, boundary +1100 ms; limite offline 6500 ms; controlli cooperativi |
| Doppler | Non assunto equivalente al ricevitore MadModem | Non assunto equivalente al ricevitore MadModem | Corpo del recupero Doppler MadModem conservato e ricontrollato con segnali sintetici fino a ±20 Hz/s |

Non viene costruito un “algoritmo migliore” scegliendo soltanto il maggior numero di risultati. La scelta verificabile è aggiungere un percorso maturo, mantenere la ricezione esistente e confrontare i messaggi completi. JTDX continua ad avere differenze di assistenza contestuale, memoria e scheduling; nella R30 non viene dichiarata la parità generale.

## Adattamenti necessari

Il port sostituisce segnali QObject con callback, porta configurazione e memoria mutabile nelle istanze, mantiene FFTW plan/buffer per istanza e serializza il planner. Le chiamate al codec messaggi usano il mutex condiviso con priorità TX. La memoria dei risultati è indicizzata per periodo e generazione, con invalidazione del contesto QSO/banda; piccoli passi CAT/Doppler conservano la storia. I callback di pubblicazione sono serializzati e i messaggi già presenti non vengono duplicati.

È stato corretto l'indice della qualità di sincronismo FT8: i tre blocchi di sette simboli devono riempire esattamente 21 posizioni. Gli indici del secondo e terzo blocco sono `k-29` e `k-58`; gli indici originali raggiungevano l'elemento 21 fuori dall'array. I risultati dei prototipi precedenti a questa correzione non sono usati come prova della release.

Il port rimane C++/Qt e usa FFTW3 double. Gli header dei componenti originali e la GPL sono conservati. La modifica del controllo Q65 limita il divieto di FFTW al target Q65, che mantiene il proprio motore nativo.

## Verifica e limiti

`FT_R30_RESULTS.json` e `FT_R30_MESSAGE_COMPARISON.csv` conservano aggiunte, perdite e confronto esatto con la lista del tester. Le annotazioni di paese vengono rimosse prima del confronto; i nominativi/hash irrisolti non sono equiparati a nominativi risolti. I 60 messaggi offline comprendono 55 corrispondenze con i 66 JTDX e cinque risultati aggiuntivi senza verifica indipendente. Il numero 60, quindi, non prova la parità.

La prova gate + finale raggiunge 52 messaggi (48 corrispondenze JTDX); il solo boundary raggiunge 41. L'utilità dell'integrazione dipende anche dal lavoro anticipato disponibile. La validazione FT4 è sintetica: manca un corpus reale affollato confrontato con i tre programmi nelle stesse condizioni.
