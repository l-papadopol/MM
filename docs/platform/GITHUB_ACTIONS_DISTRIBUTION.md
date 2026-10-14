# MadModem 0.6.2 distribution workflow

`Build installers` (`.github/workflows/build-distribution.yml`) runs on pushes to `main`/`master`, tags `v*`, or manual dispatch. Copy the complete source tree, including `.github`, into the repository root.

| Platform | Build | Downloads |
| --- | --- | --- |
| Windows x64 | MSYS2/MinGW64, Qt5; compatible and AVX2/FMA | Two portable ZIPs, one AutoCPU installer |
| Ubuntu 24.04 x64 | Qt5, Hamlib | Portable ZIP/TAR.GZ and native-package installer |
| Fedora 43 x64 | Qt5, Hamlib | Portable ZIP/TAR.GZ and native-package installer |
| macOS Intel | Qt6/Homebrew, macOS 15 target | App ZIP, DMG and PKG |
| macOS Apple Silicon | Qt6/Homebrew, macOS 15 target | App ZIP, DMG and PKG |

Distribution builds use `BUILD_TESTING=OFF`. They validate runtime dependency closure, relocatable portable packages and installer/reinstall behavior, including user-data preservation. Native smoke checks have bounded process waits. The full decoder/architecture suite is separate and manual: **Actions → Full regression suite (manual)**. It does not run on every package build or source push.

Portable artifacts upload before the installer step. After all required jobs succeed, the publish job collects packages and SHA256 checksums into downloadable GitHub Release assets. It alone requests `contents: write`; compilation jobs use read-only permissions. Repository policy must permit release publication.

A version tag such as `v0.6.2` publishes the official release. A build from a named development ref publishes a numbered prerelease, without overwriting the stable version. Manual official publication must select the correct version tag. Immutable releases cannot be overwritten: use a new version tag.

The publisher gives files descriptive platform/CPU/portable/installer names. The internal staging names can differ from the final download names. Windows packages are unsigned; macOS uses ad-hoc signing without Developer ID or notarization. Optional signing needs the author's credentials and is not configured here.

When replacing repository sources, remove obsolete tracked verification folders and old revision reports too: copying new files does not delete old files. Preserve user settings, ADIF logs and recordings separately. See [installer, update and migration instructions](INSTALLERS.md).
