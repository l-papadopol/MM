# MadModem 0.5.8 architecture

MadModem is a Qt Widgets desktop application with one integrated main window and
mode-specific decoders/workers. Audio capture has a dedicated thread; bounded,
coalesced relays feed the GUI/non-FT and DSP consumers, while FT keeps its direct
AudioEngine-to-decoder queued path.

## Active decoder policy

- FT8/FT4 live and offline decoding is under `modems/ft8/`, with GPL-compatible
  MSHV-derived protocol helpers in `third_party/mshv_gpl/port/`.
- FT8 Deep Max uses classical Costas/LLR, LDPC, CRC, unpacking and residual/SIC
  recovery. MIND is not part of the active decoder.
- CW contains one native implementation under `modems/cw/skimmer/`: a
  full-passband carrier scanner plus independent exact-tone RX A/RX B receivers
  and one soft-decision Bayesian timing decoder. Diagnostics are passive.
- MSK144 uses one self-contained RX/TX implementation under `modems/msk144/`,
  including full frames and MSK40 hashed short messages.
- Q65A/B/C/D use one always-built implementation under `modems/q65/`. The
  in-tree FFT/demodulator feeds the bundled QRA/CRC codec; no external FFT
  runtime or alternate receiver exists.

## Other subsystems

- runtime multilingual dictionaries in `translations/`;
- audio engines and RX WAV recorder in `audio/`;
- mode implementations in `modems/`;
- CAT/PTT through Hamlib in `rig/`;
- rotator profiles/control in `rotator/`;
- settings, logbook, map and Radio Telescope UI in their current directories;
- localized embedded HTML and optional Qt Help in `docs/help/`.

## Regression interfaces

- FT WAV regression: `--ft-regression`;
- native CW pure-C++ regression: `MADMODEM_BUILD_CW_TESTS=ON` or
  `scripts/run_cw_native_regression.sh`;
- waterfall-level regression: `scripts/run_waterfall_leveler_regression.sh`.
- generated modem round-trips: `madmodem_q65_native_regression` and
  `madmodem_msk144_native_regression` in CTest.
