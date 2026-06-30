# MadModem 0.5.76q

This source package includes GitHub CI hardening for Linux, Windows/MSYS2 and macOS unsigned distribution builds. Version 0.5.76q continues the GitHub/macOS CI hardening by fixing the Qt6 removal of QTextStream::setCodec() in the logbook CSV export path and extending the macOS preflight to catch unguarded UTF-8 stream codec calls before the long CI build.

# MadModem 0.5.76q

This source package includes GitHub Actions hardening for Windows/MSYS2 and macOS unsigned distribution builds.

# MadModem 0.5.76q

GitHub Actions distribution hotfix over 0.5.76q. This build keeps the modem/DSP/CAT runtime unchanged, fixes macOS AppleClang/libc++ `<version>` header shadowing by renaming the root version text file to `MADMODEM_VERSION.txt`, and keeps CI packaging for Linux, Windows/MSYS2 and macOS. See `docs/platform/GITHUB_ACTIONS_DISTRIBUTION.md`.

# MadModem 0.5.76q

macOS/GitHub Actions preparation over 0.5.76e. This build does not change the modem/DSP/CAT runtime path; it adds a native macOS `.app` bundle target, macOS-safe CMake linker flags, generated `.icns` icon support, unsigned packaging scripts, and a manual GitHub Actions workflow for arm64 and Intel macOS runners. See `docs/platform/MACOS_GITHUB_ACTIONS.md`.

# MadModem 0.5.76e

FT/UI readability hotfix over 0.5.76d: FT period radio indicators remain visible even while a QSO/TX plan is armed, and decode tables now have configurable font size and row height in Settings.

# MadModem 0.5.76d

Runtime hotfix over 0.5.76b: MSK144/Q65 RX now enters the active decoder paths instead of the old “not implemented yet” guard.  The MSK144/Q65 Mode panels also get compact sequence-status boxes in the same position/style family as FT8/FT4.

MSHV RX completion pass for Q65 and MSK144: Q65 can now use the FFTW-backed `DecoderQ65` bridge; MSK144 has coherent 4/5/7-frame averaging in the classical decoder path while MIND remains only a candidate ranker.

# MadModem 0.5.75b

Audit hotfix after the MSK144 MIND domain-gate bug. Fixes domain-specific MIND Assist gating, prevents queued MSK144 Assist from reordering candidates before readiness, fixes Q65 UTC refresh, and removes developer/status chatter from Q65/MSK144 runtime status. See `docs/audit/STATIC_AUDIT_0_5_75B.md`.

# MadModem 0.5.75a

## MadModem 0.5.75a — MSK144 MIND domain-gated ranker

0.5.75a keeps the 0.5.74d UI cleanup and adds MIND Training/Assist support for MSK144 candidate ranking. Q65 TX uses the assimilated MSHV `GenQ65` generator directly; the full MSHV `DecoderQ65` RX source is staged as GPL reference while the active RX bridge remains conservative and does not emit fake decodes.


## MadModem 0.5.73b — MSK144 include-path build hotfix

0.5.73b fixes the MSHV-derived `pack_unpack_msg.h` relative include path in the MadModem port layout. No functional MSK144 DSP/UI behaviour is changed from 0.5.73a.


- Moved MSK144 decode depth out of the crowded mode panel and into a global `Decode -> MSK144 decode depth` menu, matching the MSHV-style placement.
- Kept the real MSHV mapping: `Fast = 1`, `Normal = 2`, `Deep = 3`; default remains `Normal`.
- Saved the selection as `MSK144/decodeDepth` and made the MSK144 status/log report the active depth.


One-shot experimental MSK144 integration on top of the 0.5.72e CW/fullscreen/UI baseline. CW skimmer UI fixes remain included; MSK144 adds a dedicated mode page, 15/30 s periods, standard messages, MSHV-derived TX generation, and LDPC-validated RX frame search for on-air validation.



### 0.5.71 cockpit cleanup note

MadModem now opens as a fullscreen cockpit-style console while preserving in-app window controls. The unified Settings workbench remains fullscreen. Stale standalone Settings menu actions and dead RTTY synthetic TX-scope code were removed; modem decoder/CAT/PTT logic is unchanged.

## MadModem 0.5.73 — MSK144 one-shot experimental integration

- Added experimental MSK144 mode with 15 s / 30 s period control.
- Added MSK144 RX table, QSO form, waterfall RX marker and standard-message table.
- Added GPL MSHV-derived MSK144 TX generator and conservative LDPC-validated RX frame search.
- TX center is fixed at the protocol-standard 1500 Hz; non-standard diagnostic fallback waveform is removed.
- Added documentation in `docs/msk144/IMPLEMENTATION_0_5_73.md`.


## MadModem 0.5.70 - Settings real fullscreen hotfix

- Settings dialog now forces a true fullscreen cockpit workbench instead of relying on window-manager maximized state.
- The dialog is promoted to a top-level frameless application-modal window before exec() and re-applies fullscreen after Qt polishing.
- No DSP, decoder, CAT/PTT, rotator, logbook or modem logic changes.

## 0.5.69 - Cockpit message boxes + fullscreen Settings
- Warning/information/question message boxes now use frameless cockpit styling instead of the native grey title bar.
- Settings opens maximized as a full cockpit workbench and stays maximized when switching to/from MM Flow Studio.
- No modem decoder, CAT/PTT, rotator or logbook backend changes.

## 0.5.68 - Cockpit window screw cleanup
- Removed corner screw overlays from top-level windows/dialog chrome because they overlapped title bars, tabs and inner borders in real layouts.
- Kept the cockpit metal bezel/border styling for main window and Settings dialog.
- Left dedicated instrument-internal screws untouched where they are part of the widget drawing.
- No decoder, CAT/PTT or modem logic changes.

## 0.5.67 - Cockpit Settings dialog breathing room

- Enlarged the unified Settings dialog by roughly 20% horizontally and vertically.
- Raised the Settings minimum size accordingly so cockpit chrome, tabs and form pages do not feel cramped.
- Kept MM Flow expansion logic aligned with the new Settings baseline size.
- No decoder, CAT, PTT, FT, CW, RTTY, Hell, MFSK, rotator or logbook backend changes.

## 0.5.66 - Cockpit main chrome density

- Reduced the custom cockpit main-window title bar height, title text and window-control buttons by about 30%.
- Kept the dark cockpit frame/chrome and menu layout.
- No decoder, CAT, PTT, modem or logbook runtime logic changes.

## 0.5.65 - Cockpit main-window chrome

- Added cockpit chrome to the actual main window, not only dialogs.
- The native grey OS title bar is replaced with a dark frameless MadModem title bar.
- Added main-window cockpit border and minimize/maximize/close controls.
- Existing menu bar is preserved inside the themed header.
- Runtime modem, decoder, CAT, PTT and logbook logic are unchanged.



## 0.5.64 - Cockpit embedded dialog chrome hotfix

- Top-level dialogs keep the cockpit title bar, instrument border and corner screws.
- Embedded settings pages no longer receive a title bar or red close/ellipsis button.
- Reused QDialog pages inside Settings are now explicitly marked as embedded widgets and stripped of stale cockpit chrome if it was installed before embedding.
- No modem DSP, CAT, PTT, rotator or logbook backend changes.
### 0.5.63 Cockpit dialog/menu cleanup

Dialogs now use a MadModem cockpit-style frameless title bar with instrument border treatment, disabled checked boxes no longer appear active, and the Settings menu-bar item opens the unified settings dialog directly instead of showing a drop-down with FT WAV tools.



## MadModem 0.5.62 - MM Flow routed arrows / QSO Info polish

- MM Flow: the Connect tool now enters interactive arrow drawing mode. Click the source block, optionally click empty canvas points to create orthogonal bends, then click the destination block.
- MM Flow: double-click an arrow to edit its label. Labels and manual bend points are saved in the flow JSON.
- MM Flow: default auto-routing now uses outside lanes for branches instead of a single shared midpoint, reducing overlaps in the default graph.
- MM Flow: canvas background now follows the dark cockpit theme instead of the old white flow grid.
- UI: renamed the QSO-log side panel/group title to "QSO Info" in every language.
- Runtime modem, CAT/PTT, decoder and DSP code unchanged.

# MadModem 0.5.61

This cockpit UI polish build keeps the short SSTV tab label in all translations and removes redundant RTTY tuning-scope help labels. Decoder, CAT and PTT logic are unchanged.


Cockpit UI cleanup: SSTV tab title is shortened, text-mode QSO entry fields move into the Mode side panel, Hell removes the redundant bottom hint, and QSO map backgrounds use dark cockpit-compatible tones. Runtime decoder/CAT/PTT logic is unchanged.


### 0.5.59 cockpit UI note
The cockpit theme now uses green checked boxes consistently and removes the stray amber/brown lower border around the waterfall instrument.


### 0.5.58 Cockpit waterfall layout repair

The cockpit theme now treats the waterfall as the instrument surface instead of wrapping it in nested chrome. This removes the misleading lower margin seen in 0.5.57 while preserving the dark/amber avionics style. Runtime modem, CAT and PTT logic are unchanged.

### 0.5.57 cockpit layout repair

MadModem 0.5.57 keeps the cockpit/avionics theme but fixes the first real-layout issues found in 0.5.56: less wasted waterfall chrome, visible combo arrows, compact SSTV controls, wider tabs and an unobtrusive splitter handle. Decoder, CAT, PTT and modem runtime code are unchanged.

### 0.5.56 cockpit UI readability pass

MadModem 0.5.56 keeps the dark cockpit/avionics theme while reducing label weight, tab padding and local oversized RTTY controls. The UI should take less space and avoid tab label clipping while retaining the black/amber instrument look.

# MadModem 0.5.53 cockpit avionics UI completion

### 0.5.55 cockpit density cleanup

The cockpit/avionics theme remains enabled by default, but the first heavy pass has been refined: screw/bezel overlays are no longer applied automatically to every nested container, padding and borders are lighter, and tab labels are allowed to use more of the available width. This keeps the aircraft-panel look without wasting space or truncating Settings tabs unnecessarily.


This build completes the broad cockpit/avionics visual pass started in 0.5.52. It keeps the runtime behaviour from the previous line and focuses on a coherent dark high-contrast UI: amber labels, black instrument panels, metallic bezels, corner-screw overlays, cockpit-styled controls, and integrated MIND/VU/waterfall framing.

Important: decoder, CAT/PTT, TS-890, FT8/FT4 core, CW runtime, RTTY, Feld Hell, MFSK, scheduler, logbook, maps and rotator logic are not changed by this UI pass.

Packaging fix: all shell scripts, including build_all.sh and third_party/hamlib_lgpl/*.sh, are explicitly executable in the source package.

---

# MadModem 0.5.52

MadModem 0.5.52 introduces the first global **Cockpit / Avionics** user-interface theme.

This release keeps the 0.5.51 CW 3 kHz retune and Auto-WPM hotfix, the 0.5.50 CW baseband preconditioner, the 0.5.49 source-tree TX layout cleanup, and all recent RTTY/Feld Hell/MIND UI work.

## UI cockpit theme

The application now applies a dark, high-contrast cockpit-style theme at startup:

- matte black / charcoal window background;
- amber/nixie-like text as the default technical UI colour;
- dark instrument-style panels and group boxes;
- metallic borders and stronger bevels;
- cockpit-style tabs, buttons, combo boxes, spin boxes and input fields;
- green/red semantic colours preserved for RX/TX and active/alarm states;
- MIND Nixie gain gauge integrated with the same dark instrument style.

The theme is implemented in `utils/CockpitTheme.*` and applied from `main.cpp` so it works in Linux and static MXE Windows builds without requiring an external QSS file.

## Source layout

The source layout introduced in 0.5.49 is preserved:

- mode-specific transmitters live under `modems/<mode>/tx/`;
- shared TX interfaces live under `core/tx/`;
- there is no ambiguous top-level `tx/` directory.

## What was not changed

This release is UI-focused. It does not alter:

- PTT/CAT/Hamlib or TS-890 handling;
- FT8/FT4 decoder core;
- RTTY decoder core;
- Feld Hell DSP/timing;
- MFSK;
- scheduler logic;
- logbook, maps or rotator logic.


### Build script permissions

The full-source zip stores Unix executable bits for all `*.sh` scripts. If your archive manager strips them anyway, run:

```bash
bash fix_permissions.sh
./build_all.sh
```

`build_all.sh` also calls nested helper scripts through `bash`, so Hamlib helpers do not require their executable bit after a damaged extraction.
### GitHub CI note for 0.5.76q

0.5.76q is still a CI-portability/package line, not a DSP retune: the changes are limited to macOS/Qt6 build compatibility, GitHub packaging scripts, and MSHV support-code API modernization required by Qt6.

The macOS GitHub build now runs `scripts/check_macos_portability.sh` before CMake. This intentionally fails early if a stale root `VERSION/version` file, active Qt5-only `QRegExp`, missing MSHV Boost Clang shim, or unguarded `QTextCodec` would make AppleClang/Qt6 fail later.
