# MadModem 0.5.76c — MSK144/Q65 runtime RX hotfix

## Problem

0.5.76b compiled, but pressing RX in MSK144 or Q65 still printed:

- `MSK144 is not implemented yet.`
- `Q65 is not implemented yet.`

The compiled decoder objects existed, and `handleRxAudioBlock()` already routed live audio to them, but `MainWindow::startRx()` still rejected both modes before the audio engine was started.

## Fix

`MainWindow::startRx()` now treats both modes as implemented:

- MSK144 calls `applyMsk144Settings()`.
- Q65 calls `applyQ65Settings()`.
- RX start now reaches audio capture for both modes.
- RX reset clears/flushes the proper MSK144/Q65 decoder state and decode tables.

## UI/sequencer cleanup

The Mode side panel now contains compact `Sequence status` groups for both modes, positioned with the same intent as the FT8/FT4 sequence status area. Incoming decodes select the next plausible standard-message row:

- directed call/grid -> report row;
- report -> R-report row;
- R-report -> RR73 row;
- final acknowledgement -> 73/final row.

This is intentionally conservative: it selects/prepares the next row, but does not auto-transmit unattended.
