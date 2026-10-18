# MadModem 0.6.3 architecture

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
- CW RX A/B select **Native R12** (default) or **Neural R13** (experimental).
  Neural R13 uses the single bundled model under `modems/cw/neural/model/`,
  causal envelope normalization and bounded acoustic CTC prefix search.
  Retired CTC/Envelope settings migrate to Neural R13.
- CW contains one native implementation under `modems/cw/skimmer/`: a
  full-passband carrier scanner plus independent exact-tone RX A/RX B receivers
  and a shared soft-decision Bayesian timing implementation. The optional band skimmer
  adds 2–16 receivers in a separate bounded worker with adaptive capacity. Skimmer overflow does not reset RX A/B. Diagnostics are passive.
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

`updates/UpdateController` performs asynchronous stable-release discovery and verified downloads. Native installers handle managed copies; detached portable helpers wait for process exit, back up the folder, check the new executable with a deadline and restore on failure. TX, pending QSO and unsaved logbook state block update application.

`callsign/QrzLookupPanel` performs selected-call lookups through the official HTTPS XML API. Debouncing, generation checks, bounded responses, deadlines and ten-minute in-memory caching prevent stale results and unbounded work. Password/session values are never persisted or placed in request URLs. Lookup results are display-only and never modify QSO/rotator state.

`CwMorseSequence` supplies the same alphabet and dot-unit plan to audio CW and `CwSerialTransmitter`. The serial worker owns its QSerialPort, monotonic deadlines, KEY/PTT release and condition-variable Stop. The GUI receives character snapshots and final completion only after QThread has finished. `runtime/CwSerialTxSession.cpp` owns the GUI-side transition, fault latch and asynchronous CAT serial-port handoff. No GUI or CAT event loop drives Morse edges. Native serial mode bypasses the audio/PTT CAT route entirely; it requires the radio in CW.

A failed OFF operation blocks TX and keeps a shared CAT port reserved until a release-only Stop retry succeeds. Deadlines that overrun their tolerance stop transmission rather than compress subsequent edges. Windows requests 1 ms timer resolution only while the worker runs, paired with release on exit; actual USB/driver timing still requires hardware validation.
