# FT MSHV performance audit — v2.27

The slow WAV/deep-decode behaviour was traced to the candidate-search implementation, not to the idea of MSHV-style multipass decoding itself.

## Problem in v2.20-v2.26

MM had adopted MSHV-like candidate parameters: 40 ms DT grid, 3.125 Hz frequency grid, Costas ABC/BC scoring and up to 600 candidates. However the implementation evaluated every candidate with repeated 1920-sample Goertzel calls for every Costas symbol and tone. That creates millions of long inner loops per FT8 slot.

## What MSHV does differently

MSHV builds spectral matrices first, then scores sync candidates by indexing tone powers from the matrix. The expensive transform is amortized over many candidate frequencies.

## v2.27 change

MM now builds a cached FFT-based Costas-symbol spectral matrix per DT hypothesis. Candidate scoring reads the 8 tone powers from this cache. The final candidate demodulator still uses the accurate per-candidate Goertzel path only for the reduced candidate set that reaches LDPC/CRC.

## Architecture preserved

No change to:

- live audio capture thread;
- decoder worker isolation;
- UTC slot scheduler;
- PTT pre-arm;
- FT TX worker;
- sequencer;
- TxPlan;
- logbook/UI flow.

Deep decode remains controlled by the checkbox. The v2.26 arbitrary hard time-budget cutoff was removed because the performance issue should be solved by using the correct spectral-matrix computational shape.
