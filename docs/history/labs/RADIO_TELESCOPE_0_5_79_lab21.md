# MadModem 0.5.79-lab21 — Radio Telescope UI/scan refinement

This lab continues from lab20 and keeps the rotator-required policy for real sky scans.

Changes:
- Stops RX audio capture automatically when a Radio Telescope scan completes.
- Moves the scan summary and measured-tile table from the central heatmap area to the Mode side panel.
- Adds a CSV export button for measured sky/RFI noise samples.
- Enables table autoscroll while tiles are sampled.
- Changes the default audio slice from 1000-1100 Hz to 1000-2000 Hz; legacy 1000-1100 settings are migrated to the new default.
- Draws the projected hex grid before/during/after scans; measured hexes are filled with semi-transparent heatmap colour so the degree grid remains visible.
- Keeps rotator movement driven by CatRotatorController::setAzEl() and uses the calibrated rotator timing model via estimatePointingTimeMs(), with the additional settle delay from the Mode panel.
