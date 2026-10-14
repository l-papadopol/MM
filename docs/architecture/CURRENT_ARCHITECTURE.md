# MadModem 0.6.2 architecture

MadModem is a Qt Widgets desktop application with one integrated main window and
mode-specific decoders/workers. Audio capture has a dedicated thread; bounded,
coalesced relays feed decoder workers and the GUI. FT keeps its direct
AudioEngine-to-decoder queued path. RX workers reset state across audio gaps;
CAT commands, TX preparation, text assistance and logbook persistence run
separately from GUI interaction.

## Active decoder policy

- FT8/FT4 live and offline decoding is under `modems/ft8/`, with GPL-compatible
  MSHV-derived protocol helpers in `third_party/mshv_gpl/port/`.
- FT8 Deep Max uses classical Costas/LLR, LDPC, CRC, unpacking and residual/SIC
  recovery. MIND is not part of the active decoder.
- CW contains one native implementation under `modems/cw/skimmer/`: a
  full-passband carrier scanner plus independent exact-tone RX A/RX B receivers
  and a shared soft-decision Bayesian timing implementation. The optional band skimmer
  adds up to 16 independent receivers in the existing RX worker. Diagnostics are passive.
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

Managed installation paths live in `utils/InstallationPaths.h`; portable behavior is retained without `managed-install.flag`. Shared custom-window resizing lives in `utils/WindowResize.cpp`.

RTTY band processing uses independent per-pair ITA2 decoders with one shared resampler. The selected terminal remains separate. CW and RTTY share bounded value snapshots, CQ token recognition and waterfall placement. See [skimmer architecture](../skimmers/README.md). Full CTest execution is manual; package CI retains targeted deployment and installer checks.
