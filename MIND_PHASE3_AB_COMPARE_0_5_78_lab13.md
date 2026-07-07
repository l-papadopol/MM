# MIND Phase 3 A/B comparison baseline - 0.5.78-lab13

This package is intentionally based on `0.5.78-lab6`, the last pre-Phase-3 MIND readiness-trigger source.

## Why this package exists

During live FT testing of `0.5.78-lab9`, the runtime log still showed valid decoder events in some slots, but the received-decode table appeared empty during subsequent tests. Instead of adding UI guard patches, this A/B source package rolls back only the Phase 3 reliability telemetry path so behavior can be compared against the known pre-Phase-3 MIND pipeline.

## Diff audit result

A direct comparison between `0.5.77` and `0.5.78-lab9` showed that `MainWindow::handleFt8DecodeReady()` is unchanged. The table insertion/update path itself was not modified by the MIND Phase 3 work.

The relevant changes after 0.5.77 are in the FT decoder/MIND path, especially `modems/ft8/Ft8RxDecoder.cpp`, `Ft8RxDecoder.h`, `DeepDspController.*`, and the MIND UI/statistics code.

## What lab13 keeps

- MIND Phase 2 readiness trigger.
- Separate FT8/FT4 profile states.
- Learning / Assist state UI.
- MIND icons from lab4.
- Settings/layout changes from lab5/lab6.

## What lab13 intentionally excludes

- Phase 3 intra-frame reliability telemetry.
- `FT MIND Reliability` log line.
- Reliability samples/weak-symbol/QSB-depth accumulation.
- Any erasure-like assist.
- Any LLR/LDPC/CRC changes.

## Test purpose

Run the same radio/audio scenario with lab13 and lab9/later. If lab13 shows decode rows normally and lab9 does not, Phase 3 reliability telemetry caused an unintended side effect or timing/budget regression. If both behave the same, the issue is not Phase 3 and we must inspect runtime conditions, filters, table lifecycle, or decode availability.
