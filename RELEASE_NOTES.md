This alpha refines the Navball celestial markers with more expressive Sun/Moon icons, makes the elevation scale much less crowded, and keeps the Moon / EME tracking selector available in the Rotator tab.

This alpha adds live Sun and Moon markers to the Navball, updated in real time from local ephemeris calculations based on UTC and the configured station coordinates.

# RELEASE NOTES — current source package

## Current public version: MadModem 0.5.0-alpha.8

This alpha adds Moon / EME tracking to the Rotator tab using an internal topocentric lunar ephemeris from UTC and the configured User/QTH locator.

Versioning note: MadModem now uses Semantic Versioning (`MAJOR.MINOR.PATCH[-pre.release]`). The old `v4.13xx` package labels are retained only as internal historical snapshot tags and should not be shown as the user-facing application version.


This package continues the semantic-versioned 0.5 alpha line with a clearer rotator visual display: the old flat arrow/compass indicator is replaced by a native Qt OpenGL-backed Navball.

### Operator-visible changes

- The Rotator side tab now uses a Navball display instead of the old flat arrow indicator.
- The central yellow reticle represents the current antenna pointing; the green TG marker shows the selected target on the projected sphere.
- Mechanical azimuth values above 360° trigger an **OVERLAP ACTIVE** cue, matching Yaesu-style `N-E-S-W-N-E` rotators.
- Rotator profiles now expose an **Azimuth geometry** preset instead of relying only on a generic overlap checkbox.
- Available geometry presets are: standard 360° stop at North, standard 360° stop at South, Yaesu 450° overlap, and Custom mechanical range.
- The Yaesu-style 450° scale covers the `N-E-S-W-N-E` case used by rotators such as G-5500/GS-232-style installations.
- A configurable **Auto-reverse if end-stop is detected** option retries the other valid mechanical path when the rotator is commanded but no appreciable movement is observed.
- No-movement timeout and threshold are configurable per profile; defaults are 3000 ms and 2°.
- Calibration results now update the Settings dialog immediately at the end of calibration, including the speed spin boxes and calibration result label.
- The previous v4.13ag fixes remain: Yaesu G-601 / GS-232A fallback entry, clean Rotator status label, FT AutoQSO TX marker follows the chosen correspondent, stale rotator target cleanup, and disconnected rotator does not block FT TX.

### Safety reminders

- My Call and My Locator must be configured before TX/PTT or rotator movement.
- FT TX may be delayed/skipped while a configured and connected rotator is still moving.
- End-stop auto-reverse performs only one automatic retry per command to avoid loops.
- Flow Studio actions must pass through the capability manager; they must not directly drive PTT/audio/CAT.

### Validation status

Static packaging checks were performed in the sandbox. Full Qt compilation was not performed here because Qt development headers are unavailable in this environment. Static source checks, translation harvesting, localization audit and package integrity checks were performed.

## GitHub publication note

README.md has been rewritten as a bilingual English/Italian project homepage with explicit upstream credits, original-code statements, inspired-by reimplementation notes, feature overview and Linux/MXE build requirements.
Documentation note: the GitHub README and notices now separate compiled/linked third-party code, bundled reference material and inspired-by reimplementations.
