# Radio Telescope hex map rendering fix — 0.5.77.experimental

This experimental update fixes the Radio Telescope heatmap renderer.

## Problem
The previous renderer generated an independent screen-space honeycomb and then tried to assign each drawn hexagon to the nearest scan tile. With some beam widths, especially around 20–25 degrees in Alt-Az mode, a measured scan tile could be visually hidden because the nearest screen hexagon was closer to a different, still-unmeasured planned tile.

The scan itself was not skipping tiles, but the heatmap could look as if tiles had been skipped.

## Fix
The heatmap now draws the actual scan plan:

- one visible hexagon is generated for every commanded Radio Telescope tile;
- measured tiles are drawn over unmeasured tiles;
- unmeasured planned tiles remain transparent/dark;
- optional interpolation can lightly tint neighbouring unmeasured tiles, without hiding the measured data;
- the displayed hex grid is now synchronized with the scan list and the side table.

This keeps the beam-width-driven scan grid, while preventing measured samples from disappearing on the display.
