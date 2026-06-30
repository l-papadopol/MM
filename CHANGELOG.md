## 0.5.76j - macOS Qt6/MSHV QChar build fix

- macOS GitHub/AppleClang: fixed Qt6 strict relational comparisons between `QChar`/`QString::operator[]` and ASCII integer/char literals in active MSHV pack/unpack sources.
- CMake: disabled Qt AUTOGEN for the pure-C++ `madmodem_cwskimmer` static target to reduce noisy, irrelevant Qt diagnostics in macOS CI.
- No decoder algorithm, DSP, CAT/PTT, MIND or UI runtime behaviour changes intended.

## 0.5.76i - GitHub CI Windows/macOS hardening

- Windows GitHub/MSYS2: robust windeployqt detection, including windeployqt-qt5/windeployqt6 fallbacks and explicit qt5-tools verification.
- macOS GitHub: defensively remove stale root VERSION/version files left by copy-over repository updates; these can shadow the C++17 <version> header on case-insensitive filesystems.
- CMake: remove project-root include directory from MSK144/Q65 static targets to reduce standard-header shadowing risk.

# MadModem 0.5.76h


## 0.5.76h - macOS CI header-shadow fix

- macOS GitHub Actions fix: removed the root `VERSION` filename because AppleClang/libc++ includes the standard `<version>` header and macOS runners use a case-insensitive filesystem.
- Package scripts now read `MADMODEM_VERSION.txt`.
- Standalone macOS workflow is manual-only; the multi-platform distribution workflow remains the normal tag/release pipeline.
- No modem/DSP/CAT runtime changes.

- Added `.github/workflows/build-distribution.yml` to build Linux, Windows and macOS release artifacts on GitHub-hosted runners.
- Added Linux CI build/package scripts under `scripts/`.
- Added Windows MSYS2/MinGW64 CI build/package scripts under `scripts/`.
- Added `docs/platform/GITHUB_ACTIONS_DISTRIBUTION.md` with manual and tag-release instructions.
- Runtime code path unchanged: no DSP, decoder, CAT, UI or sequencer behavior changes.

# MadModem 0.5.76f

- Prepared the CMake project for native macOS `.app` bundle generation with `MACOSX_BUNDLE`.
- Added macOS-safe Release linker handling: Apple uses `-dead_strip` instead of GNU `--gc-sections`.
- Added generated `MadModem.icns`, `Info.plist` metadata and microphone permission text.
- Added GitHub Actions workflow for unsigned arm64 and Intel macOS artifacts.
- Added `scripts/build_macos.sh`, `scripts/package_macos.sh` and `docs/platform/MACOS_GITHUB_ACTIONS.md`.
- No FT/CW/RTTY/MSK/Q65 decoder, audio, CAT/PTT or UI-behaviour changes intended.

# 0.5.76e

- Fixed FT first/second-period indicators disappearing when period controls are disabled by an armed QSO/TX plan.
- Added Settings controls for decode table font size and row height.
- Applied decode-table readability settings to FT, MSK144 and Q65 tables.

## 0.5.76d — MSK144 async RX and mode panel cleanup

- Moved MSK144 period decode to a worker thread so period-end decode no longer freezes the UI.
- Removed transient MSK dB text labels from the waterfall; ping activity is reported only in status/log.
- Removed duplicate per-mode RX/TX/STOP/Clear/Generate buttons from MSK144/Q65 Mode panels; top RX/TX controls remain authoritative.
- Renamed MSK144/Q65 sequence status boxes so they no longer show FT8 wording.

## 0.5.76b — Q65 DecoderQ65 math/FFTW compile hotfix

- Fixed Q65 full MSHV RX bridge compile errors caused by C99/GNU complex constructs when compiling the MSHV-translated decoder as C++.
- Added `third_party/mshv_gpl/port/HvDecoderMs/mshv_complex_compat.h`.
- Cast MSHV `double complex*` buffers to FFTW `fftw_complex*` at FFTW plan creation points.
- No UI/feature changes versus 0.5.76.

## 0.5.76 — MSHV MSK144/Q65 RX completion pass

- Promoted Q65 RX from shell/buffer to the FFTW-backed MSHV `DecoderQ65` bridge when FFTW3 or bundled MSHV FFTW is available.
- Wired Q65 decode output into MadModem `Q65Decode` events and AVG counters.
- Added MSK144 coherent multi-frame averaging attempts following MSHV/WSJT-X depth semantics: Fast=single-frame path, Normal adds 4-frame average, Deep adds 4/5/7-frame averages.
- Kept MIND as MSK144 candidate ranker only; validation remains classical sync/demod/LDPC/unpack.

## 0.5.75b — full static audit hotfix

- Fixed MIND Assist domain gate in `setAssistMode()`: MSK144 no longer uses FT8/FT4 readiness when Assist is requested.
- Fixed MainWindow integration so MSK144 candidate reordering is enabled only when Assist is actually active, not merely queued.
- Fixed MIND profile view so manual FT8/FT4/MSK144 views do not reuse the active profile statistics incorrectly.
- Fixed Q65 QSO UTC updater to include the Q65 form.
- Removed developer-facing Q65/MSK144 status chatter from runtime UI.
- Added static audit report.

## 0.5.75a — MSK144 MIND domain gate

- Fix MIND MSK144 cold-start UI: FT8/FT4 ranker accuracy is no longer displayed as MSK144 readiness when MSK144 samples are zero.
- Add MSK144-specific Assist gate; Assist cannot promote MSK144 candidates until real MSK144 samples and validation exist.
- Keep Training mode available for collecting MSK144 ping/chunk examples.

## 0.5.75 — MIND ranker for MSK144

- Extended MIND from FT4/FT8-only visibility to FT4/FT8 + MSK144 candidate ranking.
- Added MSK144 ping/chunk feature extraction using the same 58×8 ranker input geometry.
- MSK144 MIND Training mode now exports positive/negative candidate examples from real period decode attempts.
- MSK144 MIND Assist mode reorders candidate time/DF chunks before the classical decoder attempts them.
- Final MSK144 validation remains fully classical: sync, demodulation, LDPC/CRC/unpack/parser. MIND never validates or fabricates messages.
- MIND side panel now shows MSK144 profile, samples and data split alongside FT8/FT4.

## 0.5.74d — Mode panel cleanup and waterfall scale fix

- Removed visible explanatory/help paragraphs from CW/MSK144/Q65 Mode panels.
- Repacked Q65 controls into a compact two-column layout to avoid overlapping/truncated fields.
- Kept QSO Info and standard messages in the right Mode panel, but without bottom filler text.
- Raised/reserved the waterfall frequency-scale band so Hz labels are not clipped in fullscreen.

## 0.5.74d — CW warning cleanup and Q65 finalization guardrails

- Fixed the remaining compile-blocking MSHV relative include-path regressions inherited from the original source tree layout.
- Cleaned the visible CW skimmer `-Wall -Wextra` warnings from the latest build log: signed/unsigned comparisons, empty debug macro bodies, unused debug-only parameters, and the deprecated Eigen init call.
- Kept Q65 TX on the assimilated MSHV GenQ65 path and kept Q65 RX in a safe buffered bridge until the FFTW3-backed DecoderQ65 worker is explicitly enabled.
- Added CMake guardrails for the optional full MSHV Q65 RX bridge: it requires FFTW3 and no longer risks breaking ordinary Linux/MXE builds when FFTW is absent.

## 0.5.74a — Q65/MSK144 MSHV include-path hotfix

- Added Q65A/Q65B/Q65C/Q65D mode entries.
- Added Q65 activity page, QSO form and Tx1..Tx7 message table.
- Added Q65 settings panel: period, decode depth, RX/TX frequency, DF tolerance, averaging, AP decode, max drift and EME delay.
- Integrated active MSHV Q65 TX generation (`GenQ65`) with no non-standard fallback waveform.
- Staged full MSHV `DecoderQ65`/`decoderpom` RX source as GPL reference for the next bridge step; default RX buffers periods and does not emit fake decodes.
- Updated version to 0.5.74.


## 0.5.73b - MSK144 MSHV include path build hotfix

- Fixed the ported MSHV `pack_unpack_msg.h` include path after moving `HvPackUnpackMsg` under `third_party/mshv_gpl/port/`.
- The original upstream relative include `../../../HvTxW/hvqthloc.h` was correct in the MSHV tree, but wrong in the MadModem port layout; it is now `../../HvTxW/hvqthloc.h`.
- No functional MSK144 DSP/UI changes in this hotfix.



## MadModem 0.5.73a — MSK144 decode-depth UI correction

- Moved MSK144 decode depth out of the crowded mode panel and into a global `Decode -> MSK144 decode depth` menu, matching the MSHV-style placement.
- Kept the real MSHV mapping: `Fast = 1`, `Normal = 2`, `Deep = 3`; default remains `Normal`.
- Saved the selection as `MSK144/decodeDepth` and made the MSK144 status/log report the active depth.

## MadModem 0.5.73 — MSK144 one-shot experimental integration

- Added experimental MSK144 mode with 15 s / 30 s period control.
- Added MSK144 RX table, QSO form, waterfall RX marker and standard-message table.
- Added GPL MSHV-derived MSK144 TX generator and conservative LDPC-validated RX frame search.
- TX center is fixed at the protocol-standard 1500 Hz; non-standard diagnostic fallback waveform is removed.
- Added documentation in `docs/msk144/IMPLEMENTATION_0_5_73.md`.


## 0.5.73
- Removed the visible Clear terminal / Load text / Clear input utility row from generic text terminal pages to recover vertical RX/TX workspace.
- Added MSK144 assimilation survey documentation under docs/msk144; no half-implemented MSK144 mode is exposed yet.

## 0.5.73

- Fixed the cockpit fullscreen title-bar controls: minimize, maximize/restore and close now use real Qt standard title-bar icons instead of text placeholders that could be elided as `...` on some desktop themes.
- The maximize button now switches automatically to the restore icon when the main window is fullscreen/maximized.
- Enlarged the custom title-bar buttons slightly to avoid glyph/icon clipping on high-DPI themes.

## 0.5.73c
- Fixed cockpit terminal readability after callsign highlighting: normal RX text remains amber instead of black-on-black.
- Clicking a highlighted callsign in CW/BPSK/RTTY/MFSK RX terminals now auto-fills the current QSO Call field.
- Hardened Mode/Language pop-up menus for frameless true-fullscreen operation on Linux window managers where QMenu could fall behind the fullscreen window.
- Improved CW vertical OSD contrast with bright glyphs and a small shadow/halo.


## 0.5.73b

- CW waterfall OSD: decoded skimmer text now appears as persistent vertical glyph trails beside the signal instead of flashing horizontal labels.
- Waterfall scale labels: reserved scale bands prevent bottom/side frequency labels from being cut or crossed by grid/marker lines in all modes.


## 0.5.73a - CW skimmer manual A/B UI correction

- CW waterfall now shows only operator A/B markers; internal FFT skimmer channels are hidden.
- CW RX textbox is controlled by user-selected RX A/RX B, not by automatic top-two channel ranking.
- Skimmer OSD labels appear only for actual decoded text, not for raw channel/SNR activity.
- Old ggmorse/selected-tone CW decoder source removed from the active tree to avoid zombie code.

## MadModem 0.5.71 - Fullscreen cockpit console + zombie cleanup

- Main window now starts as a true fullscreen cockpit console with the custom in-app minimize/maximize/close controls still visible.
- File menu keeps a safe Exit action (Ctrl+Q) that closes through MainWindow::close().
- Fixed menubar ordering after Settings became a direct action: File, Mode, Settings, Language, Help.
- Settings tab labels no longer change width due to selected-tab bold text; tabs use normal weight and no eliding.
- Removed stale standalone Settings actions/methods and dead RTTY synthetic TX scope code found in the 0.5.70 zombie survey.
- Removed the obsolete 0.5.27 TX/PTT preflight README from the source root.
- No decoder, DSP, CAT/PTT, rotator, logbook backend or modem protocol changes.


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
## 0.5.63 - Cockpit dialog chrome and Settings menu cleanup
- Added custom cockpit chrome for application dialogs: frameless dark title bar, instrument-style border and opt-in screw overlay.
- Disabled checked checkboxes no longer look active/green; only enabled checked boxes use the bright green fill.
- Converted the top-level Settings menubar item into a direct action that opens the unified Settings dialog. Removed the old Settings drop-down entries for FT WAV analysis/auto-test from that menu.
- Runtime decoder/CAT/PTT logic unchanged.



## MadModem 0.5.62 - MM Flow routed arrows / QSO Info polish

- MM Flow: the Connect tool now enters interactive arrow drawing mode. Click the source block, optionally click empty canvas points to create orthogonal bends, then click the destination block.
- MM Flow: double-click an arrow to edit its label. Labels and manual bend points are saved in the flow JSON.
- MM Flow: default auto-routing now uses outside lanes for branches instead of a single shared midpoint, reducing overlaps in the default graph.
- MM Flow: canvas background now follows the dark cockpit theme instead of the old white flow grid.
- UI: renamed the QSO-log side panel/group title to "QSO Info" in every language.
- Runtime modem, CAT/PTT, decoder and DSP code unchanged.

## 0.5.61 - Cockpit SSTV/RTTY polish
- Forced the SSTV image tab title to the short “SSTV” string in every UI language so it no longer truncates.
- Removed the redundant RTTY tuning-scope explanatory labels under the scope.
- No modem DSP, CAT, PTT or decoder changes.


## 0.5.60 - Cockpit side-panel QSO cleanup
- Shortened the SSTV image tab label to “SSTV”.
- Moved text-mode QSO log fields for RTTY, PSK, MFSK, CW and Hell from the central activity page into the Mode side panel.
- Removed the redundant Hell explanatory label from the bottom of its Mode page.
- Switched QSO map page/background rendering to dark cockpit-compatible tones.
- No decoder, CAT, PTT, FT8/FT4, RTTY, CW or Hell DSP logic changes.


## 0.5.59 - Cockpit waterfall edge + green checkbox hotfix
- Fixed the cockpit waterfall lower edge: the outer groupbox no longer paints a separate amber/brown bottom line over the waterfall frame.
- The actual waterfall frame now owns the single visible instrument border.
- Checked checkboxes now use semantic green fill consistently across the UI instead of amber/orange.
- Runtime modem, decoder, CAT/PTT and rotator logic unchanged.


## 0.5.58 - Cockpit waterfall lower-margin repair

- Repaired cockpit waterfall chrome after screenshot review.
- Removed nested frame padding below/around the OpenGL waterfall surface.
- Made `grpWaterfall` a flat instrument holder and `frameWaterfall` the actual instrument surface.
- Kept the cockpit theme, combo/spin arrows, wider tabs and compact typography from 0.5.57.
- No modem, CAT, PTT, FT8/FT4, CW, RTTY or Hell runtime logic changes.

## 0.5.57 - Cockpit layout repair after screenshot test

- Reduced empty chrome above and around the waterfall by compacting the untitled waterfall frame and its layout margins.
- Restored visible drop-down/spin arrows for cockpit combo boxes and spin boxes.
- Compacted the SSTV side panel: fixed-height action buttons, lower spacing, hidden divider lines and maximum-height settings group.
- Made side tabs wider and disabled tab eliding/scroll buttons for Mode/Rotator/MIND labels.
- Removed the visible heavy splitter bar between main content and side panel.
- No decoder, CAT, PTT or modem runtime changes.

## 0.5.56 - Cockpit typography and tab density cleanup

- Removed remaining heavy bold styling from the cockpit theme.
- Reduced tab/button/input typography weight and padding so side tabs such as Rotator/MIND fit without clipping.
- Removed RTTY local 11pt overrides that made the Mode sidebar too bulky.
- Kept the black/amber avionics style while reducing UI compression.
- No decoder, CAT, PTT, FT8/FT4, RTTY or Hell runtime logic changes.

# MadModem 0.5.53 cockpit avionics UI completion

## 0.5.55 - Cockpit density and screw cleanup

- Reduced the cockpit theme visual weight after real UI testing.
- Bezel/screw overlays are now opt-in only instead of being installed on every nested frame/group/tab widget.
- Reduced global border thickness, padding, tab minimum widths and control margins to avoid compressing mode panels and Settings tabs.
- Kept the dark avionics palette, amber text, green RX and red TX semantic colours.
- Preserved the 0.5.54 build-script permission self-heal.


This build completes the broad cockpit/avionics visual pass started in 0.5.52. It keeps the runtime behaviour from the previous line and focuses on a coherent dark high-contrast UI: amber labels, black instrument panels, metallic bezels, corner-screw overlays, cockpit-styled controls, and integrated MIND/VU/waterfall framing.

Important: decoder, CAT/PTT, TS-890, FT8/FT4 core, CW runtime, RTTY, Feld Hell, MFSK, scheduler, logbook, maps and rotator logic are not changed by this UI pass.

Packaging fix: all shell scripts, including build_all.sh and third_party/hamlib_lgpl/*.sh, are explicitly executable in the source package.

---

# Changelog

## 0.5.52 - Cockpit / avionics UI theme

- Added global dark cockpit UI theme via `utils/CockpitTheme.*`.
- Applied amber/nixie-like default text, black instrument panels, metallic borders and high-contrast input widgets.
- Preserved semantic RX/TX colours and existing MIND Nixie gain gauge.
- No intentional changes to PTT/CAT, decoder cores, scheduler, logbook, maps or rotator logic.

## 0.5.51 - CW 3 kHz retune + Auto WPM tracking hotfix

- Expanded CW receive tone clamp from 2000 Hz to 3000 Hz.
- Expanded CW tone spinbox/settings clamp from 2500 Hz to 3000 Hz.
- Relaxed Auto WPM tracking acceptance for real carriers using preconditioner SNR plus ggmorse cost, while keeping heavy smoothing.
- No decoder core, CAT/PTT, FT, RTTY or Hell behavior changes beyond CW receive tuning/tracking.

## 0.5.50

- CW: added a dedicated baseband preconditioner before ggmorse.
- CW: raw live audio is mixed around the selected CW tone, narrow-filtered, soft-squelched and then re-modulated for ggmorse.
- CW: ggmorse remains the only text decoder; no native/fuzzy CW decoder was restored.
- CW: status now reports preconditioner SNR (`preSNR`) along with ggmorse cost/threshold.
- Source layout from 0.5.49 is preserved: mode-specific TX code remains under `modems/<mode>/tx/`, common TX API under `core/tx/`.

No changes to PTT/CAT/TS-890, FT8/FT4 core, RTTY core, Feld Hell, MFSK, scheduler, logbook, maps or rotator.


## 0.5.54 - Build script permission hardening

- Hardened `build_all.sh` against archive managers that strip executable bits from nested scripts.
- `build_all.sh` now invokes bundled helper scripts through `bash`, so Hamlib build helpers work even if their executable bit is lost.
- Added `fix_permissions.sh` for one-command permission repair after extraction.
- Updated the Makefile to invoke bundled Hamlib helper scripts through `bash` as well.
- No decoder, CAT/PTT, UI runtime, radio, or modem logic changes.
