# MadModem 0.6.4 distribution workflow

`Build installers` (`.github/workflows/build-distribution.yml`) runs on pushes to `main`/`master`, tags `v*`, or manual dispatch. Copy the complete source tree, including `.github`, into the repository root.

| Platform | Build | Downloads |
| --- | --- | --- |
| Windows x64 | MSYS2/UCRT64 (GCC), Qt5; compatible and AVX2/FMA | Two portable ZIPs, one AutoCPU installer |
| Ubuntu 24.04 / Mint 22.x x64 | Qt5, Hamlib | Portable ZIP/TAR.GZ, RUN installer and DEB |
| Fedora 43 x64 | Qt5, Hamlib | Portable ZIP/TAR.GZ, RUN installer and RPM |
| macOS Intel | Qt6/Homebrew, macOS 15 target | App ZIP, DMG and PKG |
| macOS Apple Silicon | Qt6/Homebrew, macOS 15 target | App ZIP, DMG and PKG |

Distribution builds use `BUILD_TESTING=OFF`. They validate runtime dependency closure, relocatable portable packages and installer/reinstall behavior, including user-data preservation. Native smoke checks have bounded process waits. The full decoder/architecture suite is separate and manual: **Actions → Full regression suite (manual)**. It does not run on every package build or source push.

Portable artifacts upload before the installer step. After all required jobs succeed, the publish job collects packages and SHA256 checksums into downloadable GitHub Release assets. It alone requests `contents: write`; compilation jobs use read-only permissions. Repository policy must permit release publication.

A version tag such as `v0.6.4` publishes the official release. A build from a named development ref publishes a numbered prerelease, without overwriting the stable version. Manual official publication must select the correct version tag. Immutable releases cannot be overwritten: use a new version tag.

The publisher gives files descriptive platform/CPU/portable/installer names. The internal staging names can differ from the final download names. Windows packages are unsigned; macOS uses ad-hoc signing without Developer ID or notarization. Optional signing needs the author's credentials and is not configured here.

LINUXPKG1 also retains the DEB/RPM already embedded in each Linux RUN installer,
publishes them as `Linux-Ubuntu24.04-64bit-Package.deb` and
`Linux-Fedora43-64bit-Package.rpm`, and includes both in `SHA256SUMS.txt`.
The publisher refuses to publish if either native package is missing or ambiguous.
See [Linux packages and distribution repository roadmap](LINUX_R31_PACKAGES.md).

When replacing repository sources, remove obsolete tracked verification folders and old revision reports too: copying new files does not delete old files. Preserve user settings, ADIF logs and recordings separately. See [installer, update and migration instructions](INSTALLERS.md).

## Update identity and published pre-releases

The distribution workflow passes `MADMODEM_BUILD_NUMBER=github.run_number` and
`MADMODEM_BUILD_COMMIT=github.sha` to every platform. CMake writes these into
the executable. `MadModem --build-info` reports public version, build and commit;
`--version` keeps its existing output for installer checks. Local builds without
a distribution number use 0 (unknown). Explicit CMake build-number overrides
remain available outside CI.

The publisher stamps machine-readable build/commit comments into release notes
while preserving operator text. Generated `build-VERSION-N` tags also carry the
number directly. Version tags use the stamped identity to distinguish rebuilds.
MM 0.6.4 checks the paged release list, including published pre-releases, with no
Latest requirement. Drafts are excluded. Numeric public versions take precedence,
then build numbers; older public versions are never offered as upgrades.

Branch releases can remain pre-releases. The application asks before downloading
and before closing to install; the pre-release label appears in the offer.
An old 0.6.3 client retains its old detection rules until this version is
installed once, manually if necessary. New releases after that require neither
Latest promotion nor generated-tag renaming for detection.

## Source validation before platform setup

All distribution builds depend on `source-check`, which checks the complete
release identity and exercises intentionally mixed metadata before downloading
platform dependencies. Fix all reported mismatches in one commit. Upload the
whole source-directory contents at the repository root, including `.github`.
Use the new commit's workflow; re-running an old workflow retains its old source.
