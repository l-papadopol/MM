# MadModem changelog

## 0.5.78 — native CW receiver and clean source tree — 2026-07-23

- Replaced the previous CW receive subsystem with one clean-room C++
  implementation. No legacy CW decoder, fallback, adapter or copied external CW
  core remains in the source tree or CMake target.
- Added independent exact-tone RX A/RX B receivers with complex baseband,
  bounded AFC, robust MARK/noise models and one-millisecond soft observations.
- Added a native Bayesian timing decoder that jointly evaluates dots, dashes and
  element/character/word gaps. Raw measured intervals are never resized or
  merged; SNR changes confidence only; WPM is derived rather than imposed.
- Added full-passband carrier discovery with persistent sub-bin lanes. The
  waterfall shows carrier labels only and no isolated decoded glyphs.
- Kept direct RX WAV recording, passive CW diagnostics, stable passband-aware
  waterfall levelling and the zoom/pan bar below the frequency labels.
- Removed obsolete CW tests, documentation, source directories and packaging
  references. The pure-C++ native test is now the only CW regression target.
- Native CW regression passes clean 20/30 WPM messages, wrong-WPM acquisition,
  short-QSB input, noise-only suppression and two 25 Hz-separated carrier lanes.
- Added progressive posterior-prefix commit: completed characters are published
  individually with sub-second post-character latency instead of waiting for a
  phrase-level flush.
- Unified the public per-lane timing clock and bounded Auto-WPM movement to avoid
  abrupt 27→50 WPM jumps while still acquiring a wrong initial hint.
- Made the waterfall passband mask persistent and the noise-floor reference
  history-based, so broad keyed energy or one ambiguous FFT row cannot pump the
  entire display.
- Preserved the FT capture timeline, FT8/FT4 two-stage scheduling, global audio
  engine, other modem paths, CAT/PTT, rotator, logbook and multilingual UI.
