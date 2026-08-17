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
- MSK144 uses the self-contained implementation under `modems/msk144/`.
- Q65 TX is always available. Q65 RX is exposed only when the optional
  MSHV-derived FFTW3 bridge is compiled; there is no fake buffered fallback.

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
