# MadModem v2.03 — WSJT-X alignment correction

This pass addresses the unresolved v2.02 notes without reviving the quarantined v2.01 changes.

## WSJT-X source areas used as reference

- `Decoder/decodedtext.cpp/.h`: parsed text, message words, call/grid/report extraction.
- `widgets/mainwindow.cpp::auto_sequence()` and `processMessage()`: QSO progress, Tx row transitions, stop-tolerance anti-QRM, final 73 handling.
- `Modulator/Modulator.cpp::start()`: FT pre-start silence / late-start belongs in the modulator/audio path, not in a speculative scheduler hack.
- WSJT-X `jt9` process/shared-memory architecture: the decoder is separated from the GUI process. MadModem v2.03 mirrors this principle with a dedicated Qt decoder thread rather than fully importing jt9 IPC.

## Implemented in v2.03

### Decoder separation

`Ft8RxDecoder` is no longer a `MainWindow` child and is moved to `m_ft8RxThread`. Audio blocks are delivered through queued invocations. Decode results, status, and performance stats return through queued signals.

### Sequencer final handling

The final acknowledgement branch now distinguishes:

- sent `RR73` earlier: not the same as sent final `73`;
- received `RR73/RRR/73` while waiting for final: arm Tx6 `73` unless final `73` was already sent.

This follows the practical WSJT-X progression where `RR73/RRR/73` advances QSO progress instead of leaving MadModem stuck repeating R-reports.

### QSO context / locator logging

The logbook takes locator data from the active QSO context first. A small observed-grid map is maintained from real decoded CQ/grid messages only as fallback. No locator is fabricated; if no valid grid was actually decoded, the ADIF grid field remains empty.

## Still not claimed

- This is not a literal import of the entire `MainWindow::processMessage()` body.
- This is not a full jt9 shared-memory process port.
- Special operating activities, contest exchanges, Fox/Hound, and VHF contest variants remain outside the current MM FT standard-QSO target.
