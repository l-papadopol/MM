# MadModem Integration Plan

## 0.5.72 package status

This package provides the standalone library and documentation only.

## 0.5.73 recommended integration steps

1. Add `src/cwskimmer/` to the MadModem source tree.
2. Add the static library sources to the existing qmake/CMake build.
3. Instantiate one `madmodem::cwskimmer::CwSkimmerEngine` in the CW RX path.
4. Feed it the same mono audio stream used by the waterfall.
5. Do not feed it from the GUI thread.
6. Queue `CwSkimmerEvent` objects into the GUI layer.
7. Use user-selected RX A/RX B markers for the two main RX decoder lanes; keep `priorityChannels(2)` only for diagnostics/future Auto-B.
8. Use `channelStates()` for waterfall OSD labels.
9. Hide/remove old CW AFC/AGC/filter controls from UI.
10. Old selected-tone CW decoder code is removed from the active tree; rollback must use a previous release package, not hidden zombie sources.

## Proposed UI simplification

CW mode should expose only:

```text
[CW Skimmer ON]
[OSD labels ON]
[Show weak channels]
[Threshold] optional expert slider
```

Avoid reintroducing many AFC/AGC/filter controls. The assimilated engine already owns its own threshold, hysteresis, classifier, and beam search.

## Text routing

```text
All skimmer channels
    -> waterfall OSD
Best channel
    -> RX textbox primary line
Second best channel
    -> RX textbox secondary line / tagged line
```

## Rollback strategy

The first MadModem integration should keep a compile-time switch:

```cpp
#define MM_USE_CWSKIMMER_ENGINE 1
```

Not for user UI, only for developer rollback during compile/test. Once validated, remove the old path.
