# MSHV 2.76.5 upstream snapshot for MadModem FT assimilation

This directory contains the exact MSHV 2.76.5 source files used as the reference for the MadModem FT8/FT4 assimilation work.
They are vendored here for traceability and future one-to-one porting, not as a UI-visible second decoder flavour.

Primary FT8 RX reference path:

- `src/HvDecoderMs/decoderft8.cpp`
  - `DecoderFt8::sync8()` candidate search: 12 kHz, NSPS=1920, NSTEP=480, NFFT1=3840, 3.125 Hz bins, ±2.5 s Costas search.
  - `DecoderFt8::ft8b()` downsample/refine/demod/LDPC/AP/report path.
  - `DecoderFt8::subtractft8()` cancellation using reconstructed i4tone, GFSK RX waveform and K_SUB=1.9962.
  - `DecoderFt8::get_spectrum_baseline()` / `baseline()` for `sbase` and SNR baseline.

Primary FT4 RX reference path:

- `src/HvDecoderMs/decoderft4.cpp`

Primary TX references:

- `src/HvMsPlayer/libsound/HvGenFt8/gen_ft8.cpp`
- `src/HvMsPlayer/libsound/HvGenFt4/gen_ft4.cpp`

MadModem v2.22 uses this source as the canonical implementation reference and removes the previous multi-flavour premise.  UI/audio/scheduler/logbook remain MadModem; FT modem algorithms are to be assimilated from this snapshot whenever direct reuse is practical.
