## Source publication preparation — GitHub README refresh

- Rewrote README.md as a bilingual English/Italian GitHub homepage.
- Added explicit attribution sections for copied/ported/bundled open-source code.
- Added a separate section for original reimplementations inspired by WSJT-X, MSHV, JTDX, fldigi/gmfsk, QSSTV/MMSSTV and Meeus/NOAA-style astronomy formulae.
- Added a clear statement of original MadModem project code by Papadopol Lucian-Ioan / IZ6NNH / MadExp.
- Added detailed Linux and MXE Windows cross-build requirements and workflow.
- Added .gitignore for GitHub publication.

# Changelog

## MadModem 0.5.0-alpha.6 — Moon / EME rotator tracking

- Added Moon / EME tracking mode in the Rotator side tab.
- Added local dependency-free Moon ephemeris calculation from UTC and the configured User/QTH locator.
- Moon tracking bypasses the QSO/correspondent locator and drives the rotator target to the Moon azimuth/elevation when above the horizon.
- Added clean Moon status feedback for below-horizon or missing-QTH cases.

## MadModem 0.5.0-alpha.5 — rotator navball display

- Replaced the old flat rotator arrow/compass indicator with a native Qt OpenGL-backed Navball widget.
- The Navball shows current azimuth/elevation as the fixed central reticle, target point on a projected sphere, cardinal labels, elevation/azimuth grid, and an OVERLAP ACTIVE cue for mechanical azimuth above 360°.
- The widget accepts extended mechanical azimuth values up to the configured 450° overlap domain and elevation up to 180° for satellite/crossed-elevation use.
- Kept the rotator control logic unchanged; this is a rotator panel visualization upgrade on top of the 0.5.0-alpha.1 geometry/calibration baseline.

## MadModem 0.5.0-alpha.1 — public semantic version baseline

- Switched the user-visible application version from historical internal snapshot tags to Semantic Versioning.
- Main window title, About dialog, Qt application metadata, CMake project version, Windows resource metadata and online help namespace now use `MadModem 0.5.0-alpha.1`.
- Old `v4.13xx` labels remain historical/internal package references only.

## Historical internal snapshot log

This section preserves the old internal `v4.13xx` iteration notes. They are not the public application version.

## v4.13ah — rotator geometry and calibration UI fix

- Added explicit rotator azimuth geometry presets: 360° stop at North, 360° stop at South, Yaesu 450° overlap, and Custom mechanical range.
- Added configurable no-movement/end-stop detection with automatic one-shot path reversal.
- Extended rotator profile settings with stop point, no-movement timeout and no-movement threshold.
- Improved the internal rotator target mapping so display bearings and mechanical raw azimuth positions are handled separately.
- Fixed rotator calibration feedback: when calibration completes, the Settings dialog spin boxes and calibration label update immediately while the dialog is still open.
- Kept successful calibration values persistent even if the Settings dialog is later cancelled.
- Updated localized online Help and regenerated runtime dictionaries for the new rotator geometry UI strings.

## v4.13ag — rotator model/status cleanup and AutoQSO TX-follow fix

- Added Hamlib rotator capability enumeration for the Settings → Rotator model list, with fallback entries including Yaesu G-601 / GS-232A (model 601).
- Simplified the Rotator side-tab bottom status label to operator states only: connected, disconnected, not yet configured, or disabled in settings. Diagnostic text stays in the runtime log, not in the side-tab label.
- Removed the redundant FT4/FT8 activity/QSO-control caption from the FT Mode page.
- Fixed AutoQSO frequency behavior: when AutoQSO selects a CQ correspondent, the TX marker is forced to the correspondent audio frequency.
- Fixed stale rotator target handling when manually changing FT correspondents; the rotator guard now follows the newly selected QSO target instead of an old one.
- A disconnected rotator no longer blocks the FT sequencer into RX-monitor/deferred state; the TX guard is active only when the rotator backend is actually connected.

## v4.13af — compile fix for Mode runtime-log cleanup

- Fixed a bad automatic insertion from v4.13ae that referenced `modeName` inside non-FT apply/retune functions where that variable is not in scope.
- Kept Runtime log visibility controlled only by mode switching / FT UI state, so the button remains visible only in FT4/FT8 Mode.
- No decoder, audio, CAT, scheduler or rotator logic changes.

## v4.13ae — Mode-tab cleanup for scheduler, image TX and FT runtime log

- Removed the visible “Scheduler status” group title from the Mode tab in every mode.
- Removed the visible Image TX/SSTV TX group title from the image-transmit panel.
- Moved the Runtime log button out of Status and into Mode, visible only for FT4/FT8.
- Regenerated runtime dictionaries after the UI-label cleanup.

## v4.13ad — compact Status-tab RX/TX transport

- Removed the large global RX button above the right-side tabs.
- Moved compact RX and TX transport buttons to the top of the Status tab.
- RX remains a live-receiver toggle; TX starts/stops the current mode TX when valid TX content is ready.
- FT modes keep their slot-safe TX controls in the Mode tab.
- Updated inline quick-start help for the new Status-tab transport location.

## v4.13ac — rotator/scheduler and QSO map layout cleanup

- Renamed the side tab from Rotator / Scheduler to Rotator.
- Removed the scheduler panel from the Rotator tab and moved compact scheduler status to Mode.
- Increased the Settings dialog height slightly.
- Moved QSO map toolbar buttons into the map frame to recover vertical map space.

## v4.13ab — package cleanup, documentation and language refresh

- Cleaned the source-package root: removed old per-iteration changelog/readme fragments.
- Moved retained WSJT-X alignment/architecture notes under `docs/architecture/`.
- Updated `README.md`, `RELEASE_NOTES.md`, `TRANSLATION_AUDIT.md` and localized Qt Help pages.
- Refreshed runtime UI dictionaries for recent rotator, QSO-map, FT guard and Flow Studio strings.
- Removed the hidden/obsolete standalone CatRotator-window settings widget from the settings dialog code.

## v4.13aa — static-audit safety cleanup

- Added central station-identity guard for TX/PTT/rotator actions.
- Fixed FT TX re-arm/defer logic for the rotator guard.
- Reordered locator/country/prefix lookup to prefer exact country/DXCC aliases before callsign-prefix fallback.
- Removed the obsolete standalone rotator window from active runtime/build flow.
- Improved rotator overlap/range handling and peak-search planner scaffolding.
- Consolidated Flow capability handling and translation coverage for newly harvested keys.

## v4.13z — direct rotator target and identity guard

- Added locator/country/callsign target entry in the Rotator / Scheduler side tab.
- Bearing/distance are computed from the configured home locator.
- Rotator movement and TX are blocked when My Call or My Locator is missing/invalid.

## v4.13x–v4.13y — rotator peak-search configuration and online help

- Added per-rotator band table from 40 m to 10 GHz with AZ/EL search spans and Auto peak checkbox.
- Added selectable peak-search algorithms with incompatible entries disabled.
- Updated multilingual online help for FT, rotator, scheduler, QSO map and Flow Studio.

## v4.13t–v4.13w — FT bearing, lamps, rotator inertia and cleanup

- Added Bearing column to FT4/FT8 received table.
- Added MOV/RDY rotator lamps and moved them to a visible status location.
- Added rotator inertia calibration parameters and FT TX guard while the rotator is not ready.
- Removed duplicated QSO-map toolbar controls and kept the Layers dialog as the single map-display configuration point.

## v4.13 core line

- v4.13 remains the practical FT8/FT4 baseline family. Later decoder experiments outside this line should remain quarantined unless explicitly revalidated.
