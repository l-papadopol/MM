# MadModem 0.5.79-lab23 — Radio Telescope rotator peak-search integration

This checkpoint connects Radio Telescope peak refinement to the existing Settings -> Rotator auto-peak search configuration instead of using a separate ad-hoc best-tile command.

## Changes

- The Radio Telescope **Auto peak** action now uses `rotator/RotatorPeakSearch` and the active rotator profile selected algorithm.
- Supported algorithms are the same as the rotator setup page: bounded adaptive, golden-section, Nelder-Mead, SPSA, and extremum seeking, subject to axis compatibility.
- The refinement limits come from the active rotator profile band rows: azimuth/elevation search span and auto-peak settings.
- If RX is still running at scan completion, Auto peak can add up to 8 local probe tiles and measure them before holding the best point.
- If RX is already stopped, Auto peak does not create unmeasured probe tiles; it points the best measured tile.
- The antenna beam width remains stored in the active rotator profile and is used by Radio Telescope tiling.
