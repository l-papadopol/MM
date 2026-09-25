> Audit storico R6. Per lo stato dopo le correzioni leggere `../../R7_CORREZIONI_E_TEST.md`.

# Audit RTTY / CQ WW — MadModem R6

Analisi completata il 24 settembre 2026 sui sorgenti R6 consegnati. I sorgenti
RTTY non sono stati modificati durante questa analisi. Leggere `RAPPORTO.md`.
`results/native.jsonl` contiene 51 osservazioni, non 51 test tutti superati:
include casi volutamente sfavorevoli, riproduzioni di difetti e misure di tempo.

## Riprodurre le prove

Servono CMake, Ninja, un compilatore C++17, Python 3 e Qt 5 Core/Gui/Network.
Impostare MM_SOURCE al percorso assoluto della cartella sorgenti R6 (anche R6.1,
che per RTTY mantiene lo stesso codice).

```sh
python3 native/extract_afc.py "$MM_SOURCE"
cmake -S native -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMM_SOURCE="$MM_SOURCE"
cmake --build build --parallel 4
./build/rtty_audit > results-rerun.jsonl
```

L'encoder di riferimento sintetizza ITA2 separatamente dal trasmettitore MM.
Le prove usano anche RttyTransmitter, RttyDecoder, RttyMultiDecoder,
DspConditioner, AudioContinuity, AdifLogbook e CtyCountryFile reali.
Gli helper AFC sono estratti testualmente da mainwindow.cpp; il ciclo che li
chiama riproduce intervallo, blocchi e limiti del ramo RTTY. Non è un test GUI
end-to-end dell'AFC. La GUI di logging/contest è stata analizzata staticamente.

I tempi sono misure singole su questo ambiente Linux, non benchmark
comparativi fra prodotti o garanzie per Windows. Non sono presenti prove RF,
loopback PTT, fading multipath, simulazione completa di pileup, oppure una
sessione operativa di 48 ore. Il rumore AWGN è riferito alla potenza complessiva
per campione a 48 kHz, non a una banda convenzionale di 2500 Hz; il decoder
limita inoltre i campioni a ±1. Non ricavare soglie di sensibilità RF da questi
numeri. L'auto-polarità ha un transitorio iniziale: la perdita iniziale misurata
va distinta da un errore permanente.
