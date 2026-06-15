# WSJT-X session-state alignment notes for MadModem v2.05

WSJT-X keeps FT operation structured around decoded messages, QSO progress, selected TX message and the modulator/scheduler. MadModem had accumulated many independent `MainWindow` fields for the same conceptual state, which made it easy for UI, sequencer, retry and logbook paths to disagree.

## v2.05 architectural correction

MadModem now introduces `FtQsoSession` as the single mutable FT QSO/session record. `MainWindow` still owns the visible widgets and high-level commands, but it no longer directly owns the scattered QSO fields.

`FtQsoSession` owns:

- current sequencer state;
- CQ-repeat status and timeout;
- active-QSO status;
- DX call and grid;
- sent/received reports;
- QSO start time and auto-log guard;
- sticky retry state;
- current/last TX selection state;
- last decoded SNR used by standard-message generation;
- observed call->grid fallback cache.

## Runtime flow

```text
Ft8RxDecoder::Decode
  -> MainWindow lightweight routing/UI append
  -> FtQsoSession::makeContext()
  -> FtQsoSequencer::evaluateDecode()
  -> FtQsoSession::applyDecision()
  -> FtTxPlan via FtQsoSession::makePlan()
  -> FtSlotScheduler / FtTxWorker
```

## Why this matters

The previous architecture allowed bugs where:

- the table showed one message but retry used another;
- logbook used incomplete DX/grid context;
- STOP cleared only part of the old QSO state;
- a new double-click changed UI fields but not the actual scheduled TX plan.

`FtQsoSession` is not a complete byte-for-byte port of WSJT-X `processMessage()`, but it gives MadModem the missing state-ownership boundary needed before further WSJT-X logic is ported.
