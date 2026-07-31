# MSK144 RX core split — 0.5.77.experimental

This experimental checkpoint prepares MSK144 RX for the real WSJT-X/MSHV decoder-port work.

## What changed

- Added `modems/msk144/Msk144RxCore.h` and `modems/msk144/Msk144RxCore.cpp`.
- `Msk144Decoder` remains the Qt/live-audio wrapper:
  - streaming audio ingestion;
  - 12 kHz buffer management;
  - async worker dispatch;
  - Qt signals for UI/status/MIND.
- `Msk144RxCore` now owns the pure period decode path:
  - candidate time/DF enumeration;
  - optional MIND candidate ranking and sample export;
  - coherent frame attempts;
  - LDPC/unpack validation;
  - status/telemetry result packaging.

## Why

The next MSK144 step must not keep growing `MainWindow` or the QObject decoder wrapper.  The real RX chain should be ported into a pure backend where it can be tested against WAV files and compared with WSJT-X/MSHV before UI live integration.

## Important limitation

This checkpoint does **not** claim a complete MSHV RX port.  The current native decode algorithm is preserved, just moved behind a backend boundary.  The next work item is replacing the native search/demod section inside `Msk144RxCore` with the upstream-compatible MSK144 RX chain.

## Next items

1. Add an MSK144 WAV/CLI test harness around `Msk144RxCore`.
2. Import/port the missing upstream RX steps:
   - real MSK144 sync search;
   - frequency/time search and scoring;
   - coherent demod/equalization path;
   - short-message/MSK40 receive path;
   - reference-style diagnostics.
3. Validate with known MSK144 WAV files before using MIND assisted ranking.
