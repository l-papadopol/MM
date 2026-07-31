# Radio Telescope UI and heatmap contrast update — 0.5.77.experimental

This incremental experimental update keeps the public package version at `0.5.77.experimental` and refines the Radio Telescope operator workflow.

## UI layout

- Radio Telescope scan controls are now arranged vertically, so narrow side panels do not squeeze spin boxes and combo boxes.
- Action buttons are arranged in two-column rows instead of one crowded horizontal strip.
- The map clear action is now labelled `Reset map` and redraws the empty scan grid after clearing measured tiles.
- Frequency, dwell and settle spin boxes are explicitly keyboard-editable, have larger minimum widths, and use sensible single-step increments.

## Timing defaults

- Default dwell/integration time is now 5000 ms per tile.
- Default settling time is at least 1500 ms, or the calibrated rotator settle time if that is longer.
- Existing old experimental defaults below practical values are migrated once to the new defaults.

## Heatmap normalization

- During scanning, the heatmap keeps a stable dynamic range while measurements arrive.
- After the scan has stopped, the measured noise range is contrast-stretched on the real min/max range.
- A logarithmic response is then applied to the color mapping so small dB deltas become visible color changes.
- The legend displays `log` when this final contrast mapping is active.
