# MadModem GitHub Actions distribution builds

This tree contains CI scripts for building unsigned MadModem packages directly
on GitHub-hosted runners. The goal is to keep Linux, Windows and macOS package
creation outside the DSP/CAT/runtime code path.

## Workflows

### `.github/workflows/build-distribution.yml`

Manual or tag-driven release factory:

- Linux x86_64 on `ubuntu-24.04`
- Windows x86_64 on `ubuntu-24.04` using MXE static Qt5/MinGW
- macOS arm64 on `macos-15`
- macOS Intel on `macos-15-intel`
- on `v*` tags, uploads the generated artifacts to a GitHub Release

Manual run:

1. Open the repository on GitHub.
2. Go to **Actions**.
3. Select **Build distribution packages**.
4. Click **Run workflow**.
5. Download the artifacts at the bottom of the completed run.

Tag/release run:

```bash
git tag v0.5.76u
git push origin v0.5.76u
```

The workflow then builds all packages and creates or updates the GitHub Release
for that tag.

### `.github/workflows/build-macos.yml`

macOS-only workflow kept from 0.5.76f, now manual-only to avoid duplicate tag builds. Use it when you want to test
only the macOS bundle without waiting for Linux and Windows.

## Generated artifacts

Expected CI output names:

```text
MadModem-<version>-Linux-x86_64.tar.gz
MadModem-<version>-Windows-x86_64-legacy-static.zip
MadModem-<version>-Windows-x86_64-avx2-static.zip
MadModem-<version>-macOS-arm64-unsigned.zip
MadModem-<version>-macOS-arm64-unsigned.dmg
MadModem-<version>-macOS-x86_64-unsigned.zip
MadModem-<version>-macOS-x86_64-unsigned.dmg
```

## Windows notes

The official Windows release artifacts are built on GitHub with MXE static
Qt5/MinGW from an Ubuntu runner, not with a dynamic MSYS2 package. This mirrors
the local MXE release policy while keeping the whole release build inside GitHub
Actions.

Two Windows ZIP packages are produced:

- `legacy-static`: portable x86_64 baseline, safer for old CPUs
- `avx2-static`: AVX2/FMA optimized executable for modern CPUs

The package script rejects Qt/MinGW/Hamlib runtime DLL imports such as Qt5*.dll,
libgcc, libstdc++, libwinpthread, libgomp, ICU, PCRE, zstd and Hamlib. System
Windows DLL imports remain expected.

## Linux notes

The Linux package is a normal native tarball built on the GitHub Ubuntu
runner. It is not an AppImage yet. Users may need the matching Qt runtime
packages installed on their distribution.

## macOS notes

macOS packages are unsigned or ad-hoc signed only. They are not Apple Developer
ID signed and not notarized. Gatekeeper may warn on first launch.

## Build switches used by CI

The CI scripts default to:

```text
Retired MIND build options are not part of 0.5.78; distribution builds do not pass MADMODEM_MIND_OPENMP.
MADMODEM_Q65_FULL=OFF       conservative CI default; can be flipped after runner validation
```

The main modem/DSP/CAT source code is not changed by these scripts.


### 0.5.76u artifact validation

The tag release job validates downloaded artifacts before uploading them to GitHub Releases.
It rejects stale 0.5.76p/q/r/s/t assets, MSYS2/dynamic Windows ZIPs, Windows ZIPs containing DLLs, and macOS ZIPs without `MadModem.app/Contents/Frameworks/QtSerialPort.framework/Versions/A/QtSerialPort`.
The macOS package script also force-copies required Qt frameworks into the app bundle and aborts if `Contents/Frameworks` is empty.
