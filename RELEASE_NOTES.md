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

