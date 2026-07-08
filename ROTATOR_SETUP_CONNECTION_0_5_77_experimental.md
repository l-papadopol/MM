# MadModem 0.5.77.experimental — Rotator setup connection controls

This experimental update adds per-profile connection controls to Settings -> Rotator.

## Added

- Each rotator profile tab now has a simulated status LED.
- Each rotator profile tab now has a Connect / Disconnect button.
- The button connects the selected profile immediately from Settings, without leaving the dialog.
- The status LED and text update from the live CatRotator controller.
- The connection request uses the values currently visible in the Settings dialog, including model, endpoint, baud rate, elevation axis, limits, timing model, park position and antenna beam width.
- Manual connection from Settings is profile-explicit and is not blocked by current-band profile selection logic; band assignment still controls normal runtime automatic selection.

## State colours

- Green: connected.
- Red: disconnected or failed.
- Amber: connecting / disconnecting.
- Dark grey: unknown/not yet refreshed.

The version string remains `0.5.77.experimental` for the GitHub experimental release line.
