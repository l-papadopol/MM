# MSK144 Assimilation Survey for MadModem

Status: analysis / porting plan.  The MSK144 engine is **not** enabled in the UI by this cleanup package; it must be ported as a controlled decoder core before being exposed as an operator mode.

## Protocol facts to preserve

MSK144 is a meteor-scatter mode.  The protocol uses 144-bit long frames at 2000 baud, with audio tones centered around the standard WSJT-X audio carrier.  A long frame carries a packed user message, CRC and LDPC parity, plus sync words.  The protocol also has a 20 ms short-message format for very short pings after the callsigns are known.

Operator-facing consequences for MM:

- T/R period selector: 15 s and 30 s.
- Decode depth: Fast / Normal / Deep.
- Frequency tolerance / RX offset control near 1500 Hz.
- Optional short messages.
- Optional SWL short-message tracking.
- Contest mode: grid exchange instead of reports.
- Phase equalization should be designed as a later expert setting, not placed in the hot UI initially.

## WSJT-X source survey

User supplied `wsjtx-3.0.2-src.tar.gz`.  Relevant decoder files are in `lib/`:

- `decode_msk144.f90` — block decoder entry point used by WSJT-X batch decode.
- `mskrtd.f90` — real-time decoder, called on 7168-sample blocks at 12000 Hz with 50% overlap.
- `msk144spd.f90` — short-ping decode path.
- `msk144sync.f90` / `msk144_freq_search.f90` — synchronization and frequency search.
- `msk144decodeframe.f90` — LDPC decode + unpack.
- `msk40*.f90` — short-message decoder.
- `msk144signalquality.f90` — eye/quality and optional phase-training support.
- `genmsk_128_90.f90` / `genmsk40.f90` — TX waveform generation support.

Important dependency notes:

- The WSJT-X implementation is Fortran-heavy.
- It depends on `packjt77`, `packjt`, LDPC helpers, CRC/hash helpers and `four2a`.
- `four2a` uses FFTW3 via `fftw3mod.f90`.
- Directly linking the Fortran files into MM would add a Fortran compiler/runtime and FFTW dependency to the build.  This must be handled explicitly for Linux and MXE/Windows before enabling the mode.

## Recommended MadModem architecture

Do **not** paste MSK144 code into `MainWindow`.

Recommended target:

```text
modems/msk144/
  Msk144Mode.h/.cpp
  Msk144RxDecoder.h/.cpp
  Msk144TxGenerator.h/.cpp
  Msk144Types.h

third_party/wsjtx_gpl/msk144/
  fortran_core/ or ported_cpp_core/
  NOTICE.md
```

Runtime pipeline:

```text
live audio blocks
  -> resample/normalize to 12000 Hz
  -> 7168-sample overlapping ring buffer
  -> WSJT-X-derived MSK144 core
  -> decoded line: UTC, SNR, DT, Freq, Message
  -> MM FT-style decode table + QSO sequencer
```

## UI target

Initial MSK144 page should be close to WSJT-X/MSHV practice, but fitted to MM:

- Monitor / Stop Monitor.
- Clear messages / Reset QSO.
- T/R period: 15 s / 30 s.
- RX frequency around 1500 Hz.
- DF tolerance.
- Decode depth: Fast / Normal / Deep.
- Short messages checkbox.
- SWL checkbox.
- Contest checkbox.
- TX report selector.
- Generated message buttons Tx1..Tx7.

The main decode view should reuse the FT decode table style rather than a free text terminal.

## Porting order

1. Add MSK144 mode metadata and UI only after the decoder core compiles cleanly.
2. Build a standalone `msk144_test` CLI first, using WAV input and the WSJT-X core.
3. Only after CLI validation, connect live RX in MM.
4. Then add TX waveform generation.
5. Then add auto-sequencer details and contest/short-message extras.

## Current cleanup package

0.5.72e only removes central text-mode utility buttons that waste vertical space.  It deliberately does not expose a half-implemented MSK144 mode.
