# MadModem v2.02 — WSJT-X alignment pass

This tree is based on MadModem v2.00, not on the quarantined v2.01 experiment.

## Source references actually checked

- `wsjtx-3.0.1/Decoder/decodedtext.cpp/.h`
  - `DecodedText::messageWords()`
  - `DecodedText::CQersCall()`
  - `DecodedText::call()` / `deCallAndGrid()` / `report()`
  - `stdmsg_()` validation point
- `wsjtx-3.0.1/widgets/mainwindow.cpp`
  - `auto_sequence()`
  - `processMessage()`
  - `guiUpdate()` late-start rule: transmit allowed while inside the TX period if `fTR < 0.75`
  - `m_ntx` / `m_QSOProgress` mapping
- `wsjtx-3.0.1/Modulator/Modulator.cpp`
  - `Modulator::start()`
  - FT8 useful-tone delay 500 ms
  - FT4 useful-tone delay 300 ms
  - pre-start silence with `m_silentFrames`
  - late starts by advancing `m_ic`

## What v2.02 changes

### 1. FT audio timing follows the WSJT-X modulator model

v2.00 scheduled the audio backend at `slot boundary + useful-tone delay`, then tried to compensate latency.
That is the wrong split of responsibilities.

v2.02 schedules the FT audio path at the UTC slot boundary. `Ft8Transmitter` already calculates leading silence or skip from:

```text
slotBoundaryUtcMs + audioTargetDelayMs - currentUtcMs
```

This matches the WSJT-X model: open the audio path at the TX slot, send silence until the useful-tone offset, or skip if late.

### 2. Decode-driven RX/TX frequency tracking no longer writes persistent settings

v2.00 could call `applyFt8Settings()` during decode-driven sequencer advancement. That function also calls `savePersistentSettings()` and rebuilds UI/markers.

v2.02 blocks UI signals for runtime changes caused by decodes and updates only the runtime FT state. Operator changes still go through `applyFt8Settings()`.

### 3. Auto-sequence is always active in FT4/FT8

The UI flag is hidden and forced on. The internal checkbox remains only to avoid disrupting existing code paths.

### 4. Next message can be changed while a TX is active

WSJT-X can change `m_ntx`/selected message while a current transmission is already running; the running waveform is not mutated, but the next slot must not be lost.

v2.02 stores a deferred FT TX message if a valid decode or operator double-click selects a new QSO step while the current FT worker is still active. The deferred message is armed immediately after the worker stops, before the old retry path can re-arm a stale message.

## What v2.02 does not claim to finish

- It is not a complete line-for-line import of `processMessage()`.
- It does not yet replace MadModem FT decode architecture with WSJT-X `jt9` shared-memory process model.
- It does not add UDP/PSK Reporter/ALL.TXT.
- It does not add a full ADIF/logbook redesign.

## Next architecture blocks

1. Replace `FtQsoSequencer::evaluateDecode()` with a closer direct port of the WSJT-X `processMessage()` decision table, using the existing `FtDecodedText` parser and a `m_ntx`/`m_QSOProgress` style state model.
2. Move FT generation/scheduler state to a dedicated `FtRuntimeController`, leaving `MainWindow` as UI only.
3. Add a QSO context object as the source of logbook fields: `dxCall`, `dxGrid`, sent/received reports, start/end UTC, mode, band, audio RX/TX frequencies.
