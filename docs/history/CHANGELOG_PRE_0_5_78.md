## 0.5.77.experimental - Production cleanup

- Fixed Windows QComboBox dropdowns (including MeteoFax Station): keep the item view inside Qt's private popup container and raise the container safely instead of promoting the view to a top-level popup.
- Removed FT8/FT4/MSK144 bundled auto-test UI/actions, report popups and package audio fixtures.
- Removed `tests/wav` from source/package/install scripts; manual WAV analysis remains available through the existing file-open diagnostic path.
- Removed MM Flow Studio, visual flow editor, flow runtime, shadow logging and related Settings/Help pages.
- Updated README, release notes, help resources, translations and packaging scripts to reflect the removal.



## 0.5.77.experimental — MIND removed from runtime

### UI cleanup follow-up
- Removed visible MIND/DDSP remnants from Help/UI-facing documentation.
- Removed obsolete DDSP/MIND shadow-learning lab document from the package.
- Updated platform docs so GitHub builds no longer mention MADMODEM_MIND_OPENMP.

- Removed MIND/DeepDsp neural trainer from build and application runtime.
- Removed MIND side tab, icons, translations, trainer thread and OpenMP build option.
- Removed FT/MSK callback wiring so decoders run only classical DSP/LDPC paths.
- Verified RTTY remains a classical matched-filter/Baudot path and has no neural integration.


## 0.5.77.experimental — Radio Telescope hexmap rendering fix

- Fixed Radio Telescope heatmap drawing so measured tiles are no longer visually skipped when the screen honeycomb does not align with the commanded scan grid.
- The map now draws one hexagon for every planned scan tile and overlays measured tiles above unmeasured ones.
- Added `RADIO_TELESCOPE_0_5_77_experimental_HEXMAP_FIX.md`.


## 0.5.77.experimental — Rotator setup connection controls

- Added per-rotator-profile Connect / Disconnect button in Settings -> Rotator.
- Added simulated LED state indicator for each rotator profile.
- Settings connection now uses the live dialog values for that profile and updates status from CatRotator.

# MadModem 0.5.77.experimental — GitHub tag/version guard

- Added CI preflight `scripts/ci_release_version_guard.sh`.
- GitHub release builds now fail if the selected tag does not match `MADMODEM_VERSION.txt`.
- Distribution workflow checkout is explicit on `${{ github.ref }}`.
- Release title now uses the full source/package version, including `.experimental`.


## 0.5.77.experimental — Radio Telescope rotator safety update

- Radio Telescope scan now saves the pre-scan rotator position, moves to the configured park/start position before scanning, filters tiles against configured endpoints, uses a safer serpentine/park-nearest scan order, and returns to the pre-scan position on automatic completion.
- Manual stop remains operator-controlled and does not force an automatic return.
- Version string remains `0.5.77.experimental` for the GitHub experimental release line.

## 0.5.77.experimental - Experimental Radio Telescope release

Experimental release line for building and publishing the Radio Telescope prototype through GitHub Actions without promoting the normal lab branch.

- Adds the receive-only Radio Telescope mode for sky/RFI noise heatmaps.
- Requires a connected rotator for real scans; bench/no-rotator use is explicit.
- Uses antenna beam width from the rotator/antenna profile to derive scan tiles.
- Adds honeycomb heatmap rendering, CSV export, side-panel sample table and RX guard handling.
- Keeps FT8/FT4 lab16/lab17 fixes from the current source lineage.

## 0.5.78-lab17

- Added decode-table readability presets: Custom, Compact, Normal, Large and Low vision.
- Added an optional decode-table-only font family selector; leaving it on global UI font preserves the current application font.
- Localized the decode readability labels and tooltips in EN/IT/FR/DE/NO/CS.
- Forced decode/message table vertical headers to fixed section mode so newly inserted RX rows inherit the configured row height while RX is active.

## 0.5.78-lab16

- Confirmed lab15 FT4 SNR fix path and kept the SIMD/tone-bank FT4 live speed work.
- Made FT live decode rolling averages mode-local so switching FT8 -> FT4 no longer carries FT8 latency history into FT4 telemetry.
- Added the emitting FT mode name to `PerfStats` and log labels (`FT8 decode profiler`, `FT4 decode profiler`).
- Filled FT4 profiler diagnostic counters for attempted candidates, LDPC-tried, LDPC-fail, CRC-fail and message-reject paths. This fixes misleading `LDPC tried 0` while FT4 candidates were actually scored/decoded.

## 0.5.78-lab15

- Fixed FT4 displayed SNR/report: FT4 no longer collapses every CRC-valid decode to -21 dB when the candidate sync ratio is unavailable.
- Added FT4 four-tone Goertzel bank with AVX2/FMA runtime dispatch, SSE2 fallback and scalar fallback, mirroring the FT8 optimized tone-bank shape.
- Made FT4 live residual pass adaptive: live Deep/DSP+ only starts the second residual scan while there is still a safe FT4 time budget; offline/WAV analysis keeps the wider multi-pass path.
- FT4 profiler now accumulates search/decode time across residual passes so the numbers explain the total slot time instead of hiding the second-pass decode cost.


## 0.5.78-lab14

- MIND Phase 3 reintroduced as passive post-decode reliability telemetry only.
- Reliability now samples only emitted/CRC-valid FT8/FT4 decodes, outside the candidate LDPC hot path.
- Added passive runtime line: `FT MIND Reliability passive`.
- Live FT4 DSP+ residual passes capped to two passes; offline/WAV analysis keeps full four-pass FT4.
- Based on 0.5.78-lab13 A/B baseline; lab7/lab8/lab9 Phase 3 path remains quarantined.

## 0.5.78-lab13 - MIND Phase 3 A/B baseline

- Diagnostic A/B source package based on 0.5.78-lab6.
- Keeps MIND Phase 2 readiness trigger, FT8/FT4 separate profiles, Learning/Assist UI icons, settings layout and coordinate support.
- Does not include Phase 3 intra-frame reliability telemetry from lab7/lab8/lab9.
- Purpose: compare decode/display behavior before and after Phase 3 without adding UI guard patches.

## 0.5.78-lab5 - Settings layout and station coordinates hotfix

## 0.5.78-lab6 — MIND readiness trigger

- Added per-profile MIND readiness gate for FT8 and FT4.
- MIND now tracks active profile positive/negative samples and validation state separately.
- New profile lifecycle states: Cold, Learning, Validating, Assist Ready, Assist Active.
- FT4 can learn and score immediately, but pruning/extra recovery remains gated until the FT4 profile is mature.
- MIND panel now reports active-profile Pos/Neg and readiness state instead of global mixed counters.


- Reduced global and Settings tab horizontal padding so the MIND tab is no longer clipped on compact/fullscreen layouts.
- Moved User / QTH / Macros to the first Settings tab.
- Removed the separate Appearance / UI top-level tab and embedded its controls beside the Logbook file block.
- Reworked Audio / PTT so Audio devices and PTT control are shown side by side in the unified Settings page.
- Added optional precise station latitude/longitude fields in User / QTH / Macros, stored separately from the Maidenhead locator and used by the rotator home-position setup when enabled.
- Kept MIND Phase 2, lab4 neon icons, decoder validation and packaging logic unchanged.

## 0.5.78-lab3 — MIND readiness UI icons

## 0.5.78-lab4 - MIND icon and gauge layout hotfix

- Replaced the procedural MIND Learning/Assist drawings with clean neon-style PNG resources.
- Learning now uses a recognizable stylized brain icon.
- Assist-ready now uses a clean graduation-cap icon without the previous green alien-looking tassel.
- Increased MIND gain gauge vertical space and separated the state label from the nixie digits to avoid overlap.
- Kept existing MIND readiness, FT8/FT4 Phase 2 scoring, i18n strings and decoder validation logic unchanged.


- Added a 64x64 MIND state icon directly on the MIND gain gauge.
- Brain icon = Learning; graduation-cap icon = Assist active; grey icon = disabled/unavailable.
- Added localized tooltip text for profile, assist readiness, samples, validation count and readiness reason.
- No decoder, LDPC, CRC, QSB combining or erasure-like weighting changes in this lab.

## 0.5.78-lab2 - MIND Phase 2 build fix


## 0.5.78-lab1 - MIND Phase 2 FT ranker instrumentation

- Added per-mode MIND scoring callback for FT8/FT4 instead of a generic FT-only callback.
- Added FT4 native MIND candidate sample export using a deterministic 58 x 8 feature view derived from FT4 tone energies.
- Added FT4 MIND candidate scoring telemetry before LDPC while keeping final validation strictly LDPC + CRC + unpack + parser.
- Changed MIND FT assist readiness to be per active profile, so FT4 readiness is not borrowed from FT8 statistics.
- Extended MIND JSON stats with FT8/FT4 validation counters and per-profile ranker accuracy fields.
- No symbol-level weighting, erasure-like LDPC modification or multi-period combining yet; those remain later lab phases.

## 0.5.77 - Windows package cleanup and UI appearance controls

- Windows standalone packages keep `MadModem.exe`, Qt plugins, DLLs, OpenSSL runtime, help, tests and runtime data under `bin/`.
- The Windows package root exposes a `MadModem.lnk` launcher with the application icon, plus README/RUN notes and license files.
- Added Settings -> Appearance / UI with selectable themes: Avionica, Qt Default, Hacker Green, Classic Dark and High Contrast.
- Added global UI font controls with theme-default font mode and manual family/size override. Hacker Green uses a system monospace font by default.
- New appearance strings are localized through the runtime i18n files for IT/EN/FR/DE/NO/CZ.
- FT QSO activity/history table rows can be double-clicked to select or arm valid FT transmissions, matching the workflow from the left decode table where possible.
- Keeps the 0.5.76 shutdown, Windows OpenSSL and rotator popup safety fixes.

## 0.5.76 - Rotator, MIND and Settings UI hotfix

### 0.5.76 rotor safety correction
- Restored the rotator band/profile safety gate for manual Connect.
- When the selected rotator cannot be used on the current band, the Rotator panel now shows a warning popup instead of looking dead.
- Kept the Windows OpenSSL runtime packaging fix, the new bilingual README and the runtime/UI fixes.


- CatRotator Connect is no longer disabled by the current band/profile guard; explicit manual connection remains available while automatic QSO tracking is still band-gated.
- The unified Settings OK/Cancel buttons are larger and easier to hit.
- Audio/PTT now greys out the PTT serial-port selector when CAT/Hamlib PTT or audio-only PTT is selected, avoiding the false impression that a rotator serial port is being reserved for PTT.
- MIND is fixed in Assist-requested mode and the selector is removed from the side panel. Training remains autonomous and continuous at low priority while assisted ranking is still readiness-gated.
- MIND profile view now follows the active FT8/FT4/MSK144 runtime profile automatically.
- The MIND gain counter was reduced to consume less side-panel space.

### 0.5.76 - GitHub homepage README and screenshot scaffold

- Replaced the old technical README with the new bilingual GitHub project homepage text.
- Added `docs/images/README.md` as a placeholder for the planned screenshot gallery.

## 0.5.76
### Windows QSO map HTTPS runtime hotfix
- Windows/MSYS2 standalone packages now explicitly bundle Qt/OpenSSL runtime DLLs (`libssl*.dll` and `libcrypto*.dll`).
- This fixes `QSslSocket::supportsSsl() == false` in portable Windows packages, which forced the QSO map to use the offline fallback even when online OSM HTTPS tiles should work.
- The packaging script now fails early if the matching MSYS2 OpenSSL runtime is missing during Windows packaging.
 - Final release

- Final source label for the validated 0.5.76 release/tag.
- Keeps the validated Linux/macOS/Windows packaging workflow.
- Keeps the Windows standalone runtime-DLL closure fix.
- Keeps the shutdown lifecycle fix that stops the MIND trainer thread before QObject destruction, preventing the close-time segmentation fault observed on Linux/macOS/Windows.
- No modem/DSP/CAT/decoder feature changes.

## 0.5.76ab - Shutdown lifecycle diagnostics / safe close

- Base: 0.5.76aa packaging-success line.
- Adds ordered MainWindow shutdown through closeEvent before QApplication exits.
- Stops UI timers, FT scheduler/TX/RX workers, CAT/Hamlib, audio engines, NTP and DSP threads in a single idempotent shutdownRuntime() path.
- Adds explicit [shutdown] diagnostics to stderr and the runtime log to identify any remaining close-time crash point.
- No workflow/package/script changes; no decoder/DSP/CAT feature changes.

## 0.5.76aa - Windows standalone DLL closure hotfix

- Based on the last user-validated 0.5.76w package line.
- Leaves Linux and macOS packaging untouched from 0.5.76w.
- Fixes only Windows MSYS2 standalone packaging: the recursive runtime DLL closure is now executed before ZIP creation.
- Treats additional Windows system DLLs such as CFGMGR32.dll and EVR.dll as OS-provided instead of trying to bundle them.
- No modem/DSP/CAT/audio/MIND/UI runtime code changes.


## 0.5.76w - macOS framework packaging hotfix

- macOS packaging no longer runs dylibbundler after macdeployqt/manual Qt framework deployment, because that could leave Contents/Frameworks empty on arm64 CI.
- macOS package prunes the optional Qt PDF imageformat plugin if QtPdf is not bundled, avoiding a false dependency failure.
- Windows remains the GitHub-built MSYS2/MinGW64 standalone package with recursive DLL closure, not MXE/static.
- No runtime DSP/CAT/audio/UI/decoder changes.


### 0.5.76v - GitHub standalone packaging correction

- Windows release job returns to MSYS2/MinGW64 dynamic packages, but packages are now standalone: after windeployqt a recursive objdump dependency closure copies all non-system DLL dependencies and fails if anything remains unresolved.
- Windows artifacts are named legacy-standalone and avx2-standalone; they are no longer advertised as MXE static.
- macOS packaging no longer passes -qmldir to macdeployqt; it bundles only the minimal QML modules required by the QSO map to avoid optional QtStateMachine/Qt3D/VirtualKeyboard deployment failures.
- macOS framework validation still requires QtSerialPort.framework inside MadModem.app.


## 0.5.76u - GitHub release artifact hardening

- Windows release artifacts are GitHub-built MSYS2/MinGW64 standalone dynamic packages, split into legacy CPU baseline and AVX2 variants.
- Release publication now rejects stale 0.5.76p/q/r/s/t or MSYS2/dynamic Windows assets before upload.
- Release publication validates Windows static ZIPs and fails if DLLs are present.
- macOS packaging force-bundles required Qt frameworks, including QtSerialPort.framework, and fails if Contents/Frameworks is empty.
- macOS ZIP content is checked before upload so an empty Frameworks directory cannot reach a release.
- Linux remains a single x86_64 tar.gz artifact.
- No modem, DSP, CAT/PTT, audio, Q65/MSK144/FT8 or MIND runtime code changes.

## 0.5.76u - GitHub release asset cleanup and package identity hardening

- Release job deletes old assets on an existing tag before uploading fresh artifacts, preventing stale 0.5.76p/MSYS2 packages from staying visible in GitHub Releases.
- Release job validates that every uploaded package filename contains the current MADMODEM_VERSION.txt value and rejects old assets.
- Windows MXE packages include a BUILD_INFO.txt marker and continue to reject non-system Qt/MinGW DLL imports.
- macOS app bundle Info.plist now uses MADMODEM_VERSION.txt for CFBundle version strings and package_macos.sh explicitly bundles/verifies QtSerialPort.framework.
- No modem/DSP/CAT/PTT/audio/MIND/runtime retune.

## 0.5.76u - GitHub MXE static Windows release packaging

- Replaces the official Windows GitHub release job with Linux-hosted MXE static Qt5/MinGW builds.
- Produces two Windows release ZIPs: legacy static baseline and AVX2 static.
- macOS packaging now verifies bundled Qt framework closure so missing QtSerialPort.framework is caught before release.
- Linux packaging now emits only one tar.gz artifact, not both zip and tar.gz.
- No FT8/FT4, DSP, CAT/PTT, audio, MIND or UI runtime changes.

## 0.5.76p - macOS Qt6 QTextStream UTF-8 encoding fix

- Fixed the Qt6/macOS build failure in `dialogs/LogbookDialog.cpp`: `QTextStream::setCodec()` was removed in Qt6, so CSV export now uses `QTextStream::setEncoding(QStringConverter::Utf8)` on Qt6 and keeps `setCodec("UTF-8")` on Qt5.
- Extended `scripts/check_macos_portability.sh` to reject unguarded `QTextStream::setCodec("UTF-8")` before the long macOS CI build.
- No FT8/FT4 DSP, CAT/PTT, audio, MIND, or UI runtime behavior changes are intended.

## 0.5.76o - macOS Qt6 QChar control-code conversion fix

- Fixed Qt6/AppleClang build failure in `modems/mfsk/tx/MfskTransmitter.cpp` where STX/EOT control-code integers were passed to a `QChar` parameter.
- Added macOS preflight coverage for bare integer literals passed to known QChar-parameter helper functions.
- No DSP, CAT/PTT, FT8/FT4, or audio runtime retune intended; this is CI/macOS Qt6 compatibility hardening.

## 0.5.76n - macOS Qt6 qint64 min/max overload fix

- Fixed Qt6/AppleClang ambiguous `qBound<qint64>` calls in `mainwindow.cpp` by making all bounds and progress constants explicit `qint64`.
- Normalized other qint64 Qt min/max/bound call sites with explicit `qint64{...}` or `static_cast<qint64>(...)` to avoid repeating the same macOS failure later in FT/CW/SSTV/WEFAX/MSK/rotator code.
- Extended `scripts/check_macos_portability.sh` to catch bare integer literals in `qBound/qMax/qMin<qint64>` calls before the long CI build.
- No intended DSP, CAT/PTT, audio, FT8/FT4 or UI runtime behavior changes.

## 0.5.76m - macOS Qt6 QString::midRef removal

- Replaced remaining active MSHV `QString::midRef()` calls with `QString::mid()` in Q65/MSK144 support sources for Qt6/macOS builds.
- Extended `scripts/check_macos_portability.sh` to fail early on active `midRef`/`QStringRef` usage.
- No runtime DSP/CAT/audio retune intended; this is a CI/macOS portability hardening update.

## 0.5.76l - GitHub CI macOS broad portability hardening
- Added a vendored Boost/AppleClang compatibility shim for the MSHV MSK144 generator (`boost/config/compiler/clang.hpp`).
- Added `scripts/check_macos_portability.sh` to fail early on known macOS/Qt6 traps: stale root `VERSION`, active `QRegExp`, missing Boost Clang shim, and unguarded Qt5-only APIs.
- Hardened MSYS2 PATH handling and Windows deploy-tool diagnostics.
- Added MSYS2 `qt5-translations` and `angleproject` to Windows CI dependencies to reduce windeployqt/runtime deployment failures.

## 0.5.76k - macOS Qt6 QRegExp removal fix

- Replaced active MSHV `QRegExp` usage with `QRegularExpression` in `HvTxW/hvqthloc.cpp`.
- Hardened `HvQthLoc` ASCII callsign/locator comparisons for Qt6/AppleClang by using explicit Latin-1 values.
- Also updated the optional full-Q65 `HvDecoderMs/decoderpom.cpp` regexp path for Qt6 compatibility.
- No DSP, decoder, CAT/PTT, UI runtime, FT8/FT4, MSK144 or MIND logic changes beyond Qt API compatibility.

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
- No modem decoder, CAT/PTT, rotator or logbook backend changes.

## 0.5.68 - Cockpit window screw cleanup
- Removed corner screw overlays from top-level windows/dialog chrome because they overlapped title bars, tabs and inner borders in real layouts.
- Kept the cockpit metal bezel/border styling for main window and Settings dialog.
- Left dedicated instrument-internal screws untouched where they are part of the widget drawing.
- No decoder, CAT/PTT or modem logic changes.

## 0.5.67 - Cockpit Settings dialog breathing room

- Enlarged the unified Settings dialog by roughly 20% horizontally and vertically.
- Raised the Settings minimum size accordingly so cockpit chrome, tabs and form pages do not feel cramped.
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
- Converted the top-level Settings menubar item into a direct action that opens the unified Settings dialog. Removed the old Settings drop-down entries for FT WAV analysis from that menu.
- Runtime decoder/CAT/PTT logic unchanged.




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


## 0.5.77.experimental — Radio Telescope UI and heatmap contrast

- Reworked Radio Telescope side-panel layout with vertical controls and two-column action buttons.
- Renamed the map clear action to Reset map.
- Made audio frequency spin boxes explicitly keyboard-editable.
- Updated default Radio Telescope timing to 5000 ms dwell and at least 1500 ms settling.
- Added final logarithmic heatmap contrast normalization after completed scans.
### Radio Telescope Welch PSD noise estimator
- Replaced the first Goertzel-bank Radio Telescope band-power prototype with a robust Welch PSD estimator.
- The selected audio slice is now integrated from Hann-windowed overlapped FFT segments with trimmed-mean rejection of narrow carriers/spurs.
- Tile values remain dBFS band power and are still averaged over the configured dwell interval.

## 0.5.77.experimental - FT4 display diagnostics

- Added FT4/FT8 UI-path diagnostics showing decoder decodes, decodeReady signals, rows inserted, duplicate/update rows, blacklisted rows and visible table row counts.
- Added FT4 row insert/update trace lines to separate decoder success from table/rendering issues.
- Added FT4 live-decode warning threshold at 800 ms.
- Decoder core, MIND, LDPC/CRC/unpack and sequencer logic are unchanged.


- `0.5.77.experimental`: fixed Logbook floating-window frame continuity by keeping the menu bar inside the dialog layout instead of allowing it to cover the outer border.

## 0.5.77.experimental — Settings open lazy-load
- Deferred heavy Settings pages until selected to reduce open latency.
- Cached rotator connection LED/status until the deferred Rotator tab is built.
- Added Settings opened timing diagnostic.


## 0.5.77.experimental MSK144 reference-style RX chain
- Added reference-style MSK144 RX candidate chain in Msk144RxCore: sync/time/DF search, coherent equalization, MSK40 short-message decode path and status diagnostics.
- No WAV validation was possible at packaging time; live/WAV validation remains required.
