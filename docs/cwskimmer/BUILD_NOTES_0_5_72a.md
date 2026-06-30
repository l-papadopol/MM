# MadModem 0.5.72a CW Skimmer Manual A/B UI

## Purpose

0.5.72a keeps the 0.5.71 fullscreen/cockpit UI baseline and corrects the first CW skimmer UI integration.

## Changes

- The skimmer's internal FFT channels are no longer exposed as fixed waterfall markers.
- The waterfall shows only operator markers:
  - green `A` marker: left click;
  - optional blue `B` marker: right click;
  - no `CW1`, `CW2`, `CW3`, ... marker grid.
- The main CW RX textbox is driven only by the user-selected A/B marker channels.
- Other skimmer channels can produce waterfall text overlays only when they have actual decoded text.
- The old visible CW Bandwidth/AFC controls are hidden from the CW mode panel; the skimmer owns its adaptive threshold/noise/hysteresis path internally.
- The old selected-tone CW decoder/ggmorse source was removed from the active tree to avoid zombie rollback ambiguity.
- `m_cwSecondaryDecoder` is no longer instantiated; RX B is a marker inside the single skimmer engine.

## User model

Left click on the waterfall selects RX A. Right click selects or enables RX B. The skimmer still monitors the passband internally, but automatic channel ranking does not move A/B and does not spam the RX textbox.
