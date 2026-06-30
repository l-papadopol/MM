# MadModem GitHub Actions distribution builds

This tree contains CI scripts for building unsigned MadModem packages directly
on GitHub-hosted runners. The goal is to keep Linux, Windows and macOS package
creation outside the DSP/CAT/runtime code path.

## Workflows

### `.github/workflows/build-distribution.yml`

Manual or tag-driven release factory:

- Linux x86_64 on `ubuntu-24.04`
- Windows x86_64 on `windows-2025` using MSYS2/MinGW64
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
git tag v0.5.76q
git push origin v0.5.76q
```

The workflow then builds all packages and creates or updates the GitHub Release
for that tag.

### `.github/workflows/build-macos.yml`

macOS-only workflow kept from 0.5.76q, now manual-only to avoid duplicate tag builds. Use it when you want to test
only the macOS bundle without waiting for Linux and Windows.

## Generated artifacts

Expected CI output names:

```text
MadModem-<version>-Linux-x86_64.tar.gz
MadModem-<version>-Linux-x86_64.zip
MadModem-<version>-Windows-x86_64-legacy.zip
MadModem-<version>-Windows-x86_64-avx2.zip
MadModem-<version>-macOS-arm64-unsigned.zip
MadModem-<version>-macOS-arm64-unsigned.dmg
MadModem-<version>-macOS-x86_64-unsigned.zip
MadModem-<version>-macOS-x86_64-unsigned.dmg
```

## Windows notes

The Windows CI build uses native MSYS2/MinGW64 packages instead of MXE. This is
intentional for GitHub Actions: MSYS2 provides ready-made Qt packages and a
GitHub Action to install them quickly on hosted Windows runners.

The Windows packages are deployed with `windeployqt` and include Qt DLLs,
plugins and compiler runtime files. They are not Authenticode-signed.

Two Windows executables are produced:

- `legacy`: portable x86_64 baseline, safer for old CPUs
- `avx2`: AVX2/FMA optimized executable for modern CPUs

## Linux notes

The Linux package is a normal native tarball/zip built on the GitHub Ubuntu
runner. It is not an AppImage yet. Users may need the matching Qt runtime
packages installed on their distribution.

## macOS notes

macOS packages are unsigned or ad-hoc signed only. They are not Apple Developer
ID signed and not notarized. Gatekeeper may warn on first launch.

## Build switches used by CI

The CI scripts default to:

```text
MADMODEM_MIND_OPENMP=ON     for Linux/Windows
MADMODEM_MIND_OPENMP=OFF    for macOS, because Apple Clang/OpenMP needs extra handling
MADMODEM_Q65_FULL=OFF       conservative CI default; can be flipped after runner validation
```

The main modem/DSP/CAT source code is not changed by these scripts.
