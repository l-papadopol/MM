
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

This package is an A/B baseline for testing. It is based on the last known MIND Phase 2 code path before Phase 3 reliability telemetry was added. It keeps the readiness trigger and Learning/Assist UI, but removes the Phase 3 reliability telemetry path so FT decode behavior can be compared directly.

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
The Windows standalone package now explicitly includes the OpenSSL runtime DLLs required by Qt Network for HTTPS tile downloads. This prevents the QSO map from falling back to the offline map only because `libssl`/`libcrypto` were not bundled.
 - Final release

- Final source label for the validated 0.5.76 release/tag.
- Keeps the validated Linux/macOS/Windows packaging workflow.
- Keeps the Windows standalone runtime-DLL closure fix.
- Keeps the shutdown lifecycle fix that stops the MIND trainer thread before QObject destruction, preventing the close-time segmentation fault observed on Linux/macOS/Windows.
- No modem/DSP/CAT/decoder feature changes.

## 0.5.76ab - Safe shutdown hotfix

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

# MadModem 0.5.76u

Packaging-only GitHub release hardening over 0.5.76u. This release prevents stale assets from previous releases from remaining downloadable after a re-run: the release job now deletes existing assets before upload and rejects any package whose filename does not contain the current MADMODEM_VERSION.txt value. Windows is GitHub-built MSYS2/MinGW64 standalone dynamic legacy/AVX2. macOS bundles explicitly include and verify QtSerialPort.framework. Linux publishes a single tar.gz. No modem/DSP/CAT/audio/UI runtime code is changed.

# MadModem 0.5.76u

Packaging-only GitHub release correction over 0.5.76p. Windows release artifacts are now built by GitHub using MSYS2/MinGW64 dynamic Qt5, with recursive runtime-DLL closure and separate legacy/AVX2 standalone ZIP packages. macOS packaging verifies that every linked Qt framework is present inside the .app bundle, including QtSerialPort. Linux now publishes a single tar.gz package.

No FT8/FT4 DSP, CAT/PTT, audio, MIND, decoder-core, or UI runtime retune is intended.

# MadModem 0.5.76p

GitHub/macOS arm64 CI hotfix over 0.5.76o. This release fixes the Qt6 removal of `QTextStream::setCodec()` in the logbook CSV export path. Qt6 builds now use `QTextStream::setEncoding(QStringConverter::Utf8)`, while Qt5/Linux/Windows legacy paths keep `setCodec("UTF-8")`. No FT8/FT4 DSP, CAT/PTT, audio, MIND, or UI runtime retune is intended.

GitHub/macOS arm64 CI hotfix over 0.5.76l. This release completes the active MSHV Qt6 API cleanup detected by AppleClang: all compiled uses of `QString::midRef()` in the active MSHV Q65/MSK144 support path were replaced with `QString::mid()`, preserving the same substring/toInt semantics while using an API available in Qt6. The macOS portability preflight now rejects active `midRef`/`QStringRef` usage before CMake/Ninja starts the long build. No FT8/FT4 DSP, CAT/PTT, audio, or UI runtime retune is intended.

# MadModem 0.5.76l

GitHub CI hardening over 0.5.76k. This release performs a broader macOS/Qt6/MSHV portability pass instead of chasing single compiler failures: it adds an AppleClang Boost compatibility shim for the vendored MSHV Boost subset, a macOS source preflight script, and stronger Windows/MSYS2 deploy-tool handling.

GitHub/macOS CI hotfix over 0.5.76j. This release addresses the next macOS arm64 compile failure in the active MSHV MSK144 support code: Qt6 no longer provides the old `QRegExp` API in the same way as the Qt5/Linux path, so the code now uses `QRegularExpression`. The callsign/locator validator was also hardened against Qt6/AppleClang QChar comparison issues.

# MadModem 0.5.76j

GitHub/macOS CI hotfix over 0.5.76i. This release addresses the next macOS compile failure after the `<version>` header-shadow issue: Qt6/AppleClang is stricter about relational operators between `QChar` and ASCII integer/char literals in the active MSHV pack/unpack port. The affected comparisons now explicitly use Latin-1 integer values. CW skimmer remains enabled; its pure-C++ target now opts out of Qt AUTOGEN to keep macOS logs cleaner.

# MadModem 0.5.76i

CI hotfix for GitHub distribution builds. This release does not change DSP, CAT/PTT, FT, MIND, MSK144/Q65 runtime logic, or UI behaviour. It only hardens Windows/MSYS2 deployment-tool detection and macOS stale VERSION cleanup.

# MadModem 0.5.76h — macOS CI header-shadow fix


## 0.5.76h - macOS CI header-shadow fix

- Renamed the root plain-text version file from `VERSION` to `MADMODEM_VERSION.txt`.
- Fixes AppleClang/libc++ failing on macOS CI because `<version>` resolved to the project root `VERSION` file on case-insensitive APFS runners.
- No decoder/DSP/CAT/UI runtime logic changed.

This release-preparation step extends the previous macOS CI work to a full GitHub-hosted package factory. It adds Linux, Windows/MSYS2 and macOS artifact generation plus optional GitHub Release upload on `v*` tags. The modem runtime core is intentionally unchanged.

# MadModem 0.5.76f — macOS GitHub Actions preparation

- Adds a conservative macOS `.app` bundle target for CMake builds.
- Adds GitHub Actions workflow `Build macOS unsigned` for Apple Silicon and Intel artifacts.
- Adds unsigned ZIP/DMG packaging scripts using `macdeployqt`; non-Qt Homebrew dylibs are bundled best-effort with `dylibbundler`.
- Adds macOS `Info.plist`, generated `.icns` icon support and microphone permission text.
- Keeps modem DSP, decoders, CAT/PTT, logbook and normal Linux/Windows build paths unchanged.

# MadModem 0.5.76e — FT period indicator and decode-table readability

- Keeps the selected FT first/second-period indicator green even when the active QSO/TX plan temporarily locks period controls.
- Adds Settings controls for decode table font size and row height.
- Applies the table readability settings to FT, MSK144 and Q65 decode/activity tables.


# MadModem 0.5.76d — MSK144 async RX and mode panel cleanup

- Fixed the RX start guard that still rejected MSK144 and Q65 as “not implemented yet”.
- MSK144 RX now calls `applyMsk144Settings()`, resets `Msk144Decoder`, feeds live audio blocks to the decoder and flushes the period on STOP.
- Q65 RX now calls `applyQ65Settings()`, resets `Q65Decoder`, feeds live audio blocks to the DecoderQ65 bridge and flushes/clears averages from the Mode panel.
- Added compact `Sequence status` groups to MSK144 and Q65 Mode panels, placed next to QSO Info and Standard messages like the FT8/FT4 flow.
- Decode handlers now auto-select the next likely standard message row after directed messages, reports, R-reports and final acknowledgements.

# MadModem 0.5.76b — Q65 DecoderQ65 math/FFTW compile hotfix

This hotfix continues the 0.5.76 Q65 full RX bridge work and fixes the next GCC/C++ compile failure in the MSHV-derived `DecoderQ65`/`decoderpom` code.

Changes:

- Adds explicit `<cmath>`/`<cstdlib>` includes and `std::` math imports where the original MSHV port used unqualified C math functions.
- Fixes missing `log10`, `fabs`, `atan`, `cos`, `sqrt`, `pow`, `sin`, `fmod`, and `tanh` symbols in C++ builds.
- Keeps the previous complex/FFTW compatibility shim from 0.5.76a.
- No UI/behavior changes.


## 0.5.76d notes

- MSK144 period decode now runs in a worker thread to avoid UI stalls at period end.
- Transient MSK dB labels are no longer drawn on the waterfall.
- Duplicate per-mode RX/TX/STOP/Clear/Generate buttons were hidden from MSK144/Q65 Mode panels.
## 0.5.76n CI hotfix

This package fixes the macOS arm64 Qt6/AppleClang ambiguity reported in `mainwindow.cpp` around `qBound<qint64>`. The fix is intentionally mechanical: explicit qint64 bounds/constants in Qt min/max/bound calls plus a preflight check to prevent the same class of CI failure from reappearing. No runtime DSP/CAT/audio retune is intended.

