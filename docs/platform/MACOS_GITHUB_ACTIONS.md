# MadModem macOS build via GitHub Actions

This source tree is prepared for a first unsigned macOS CI build. It does not
change the Linux/Windows runtime core. It only adds macOS bundle metadata,
macOS-safe CMake flags, helper scripts and a GitHub Actions workflow.

## What the workflow builds

The workflow `.github/workflows/build-macos.yml` runs manually from GitHub or
automatically when pushing a tag beginning with `v`.

It builds two unsigned artifacts:

- `MadModem-<version>-macOS-arm64-unsigned.zip/.dmg` on `macos-15`
- `MadModem-<version>-macOS-x86_64-unsigned.zip/.dmg` on `macos-15-intel`

Q65A/B/C/D RX and TX use the same bundled implementation as Linux and Windows;
there is no macOS-specific backend switch or external FFT runtime.

## Dependencies installed by CI

The workflow installs with Homebrew:

```bash
brew install cmake ninja qt qtlocation hamlib pkgconf rsync dylibbundler dylibbundler
```

`qtlocation` is installed explicitly so the QSO map backend has a chance to be
available. If Qt Location is missing, the existing CMake fallback still uses the
offline map path.

## Files added for macOS

- `.github/workflows/build-macos.yml`
- `scripts/build_macos.sh`
- `scripts/package_macos.sh`
- `cmake/macos/Info.plist.in`
- `cmake/macos/GenerateMacOSIcon.cmake`

## Bundle notes

`MadModem.app` is generated as a CMake `MACOSX_BUNDLE`. The bundle includes:

- app metadata and microphone permission text in `Info.plist`
- a generated `MadModem.icns` from `icons/madmodem_1024.png`
- `cty.csv` copied to `Contents/MacOS/cty.csv`
- Qt frameworks/plugins deployed by `macdeployqt`
- non-Qt Homebrew dylibs, including Hamlib dependencies, bundled by `dylibbundler` when available

The current runtime still stores `settings.mad` and `logbook.adi` beside the
executable because this is how the Linux/Windows code currently works. A later
macOS-polish pass can migrate those writable files to `QStandardPaths` while
preserving import from the old location.

## Unsigned/notarization warning

The produced `.app/.dmg` is unsigned except for optional ad-hoc signing after
`macdeployqt`. It is not notarized by Apple. A real public Mac release should
later add Developer ID signing and Apple notarization secrets to GitHub Actions.

## Local macOS build

On a Mac with Homebrew:

```bash
brew install cmake ninja qt qtlocation hamlib pkgconf rsync dylibbundler dylibbundler
MADMODEM_OSX_ARCHITECTURES=arm64 bash scripts/build_macos.sh
MADMODEM_OSX_ARCHITECTURES=arm64 bash scripts/package_macos.sh
```

For Intel:

```bash
MADMODEM_OSX_ARCHITECTURES=x86_64 bash scripts/build_macos.sh
MADMODEM_OSX_ARCHITECTURES=x86_64 bash scripts/package_macos.sh
```
