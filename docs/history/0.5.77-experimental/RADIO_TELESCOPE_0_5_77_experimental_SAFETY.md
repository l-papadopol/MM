# MadModem 0.5.77.experimental — Radio Telescope rotator safety update

This experimental build keeps the public version string `0.5.77.experimental` and adds a safer scan lifecycle for Radio Telescope mode.

## Rotator scan lifecycle

Radio Telescope scans now follow a controlled step-and-stare sequence:

1. Validate that the selected rotator profile is enabled and connected, unless explicit bench mode is enabled.
2. Save the current pre-scan rotator position.
3. Move to the configured park/start position from the active rotator profile.
4. Generate the scan tile list from the antenna beam width.
5. Filter generated tiles against configured azimuth/elevation endpoints.
6. Use a serpentine order for Alt-Az rows and a park-nearest start for azimuth-ring scans to reduce unnecessary backtracking.
7. Command every tile through `CatRotatorController::setAzEl()`, so existing overlap/450-degree geometry, endpoint handling, auto-reverse-on-stall and calibrated speed/ETA logic stay centralized in the rotator controller.
8. Wait for the calibrated movement ETA plus settle time before sampling the selected audio slice.
9. Stop automatically at the end, stop RX audio capture and command the rotator back to the pre-scan position.

Manual Stop still stops the scan without forcing an automatic return, so the operator stays in control during abnormal or emergency conditions.

## Notes

- The build remains experimental and receive-only for Radio Telescope mode.
- The package name/version intentionally stays `0.5.77.experimental` for GitHub experimental releases.
