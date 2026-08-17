# MadModem GitHub Actions distribution builds

This tree contains CI scripts for building unsigned MadModem packages directly
on GitHub-hosted runners. The goal is to keep Linux, Windows and macOS package
creation outside the DSP/CAT/runtime code path.

## Workflows

### `.github/workflows/build-distribution.yml`

Manual or tag-driven release factory:

- Linux x86_64 on `ubuntu-24.04`
- Windows x86_64 on `windows-2022` using MSYS2/MinGW64
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
git tag v0.5.8
git push origin v0.5.8
```

The workflow then builds all packages and creates or updates the GitHub Release
for that tag.

Every distribution job runs its configured CTest suite before packaging. A
test failure therefore blocks the corresponding Linux, Windows or macOS
artifact.

### `.github/workflows/source-regression.yml`

Runs on every push and pull request. It performs a portable Linux release
build against the system Hamlib package, then executes the complete CTest
suite, including the native CW/waterfall tests and the FT8/FT4 source guards.

### `.github/workflows/build-macos.yml`

macOS-only workflow, manual-only to avoid duplicate tag builds. Use it when you
want to test only the macOS bundle without waiting for Linux and Windows.

## Generated artifacts

Expected CI output names:

```text
MadModem-<version>-Linux-x86_64.tar.gz
MadModem-<version>-Windows-x86_64-legacy-standalone.zip
MadModem-<version>-Windows-x86_64-avx2-standalone.zip
MadModem-<version>-macOS-arm64-unsigned.zip
MadModem-<version>-macOS-arm64-unsigned.dmg
MadModem-<version>-macOS-x86_64-unsigned.zip
MadModem-<version>-macOS-x86_64-unsigned.dmg
```

## Windows notes

The official Windows release artifacts are built on GitHub with Qt5/MinGW64 in
an MSYS2 runner. The packaging stage follows the complete runtime-DLL closure
and refuses to publish an incomplete standalone package.

The Windows artifacts are built in an MSYS2/MinGW64 runner and include the
complete Qt/MinGW runtime dependency closure. Two standalone ZIP packages are
produced:

- `legacy-standalone`: portable x86_64 baseline, safer for old CPUs
- `avx2-standalone`: AVX2/FMA optimized executable for modern CPUs

The package script recursively discovers and bundles non-system DLLs, writes a
runtime dependency report and rejects an unresolved dependency before creating
the ZIP.

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
Retired MIND build options are not part of 0.5.8; distribution builds do not pass MADMODEM_MIND_OPENMP.
MADMODEM_Q65_FULL=OFF       conservative CI default; can be flipped after runner validation
```

The main modem/DSP/CAT source code is not changed by these scripts.


### 0.5.8 artifact validation

The tag release job validates downloaded artifacts before uploading them to
GitHub Releases. It rejects stale quarantined assets, Windows ZIPs without the
executable/runtime report/Qt runtime/rules file, incomplete Linux tarballs and
macOS ZIPs without the required QtSerialPort framework, build metadata or RTTY
rules. The macOS package script also force-copies required Qt frameworks into
the app bundle and aborts if `Contents/Frameworks` is empty.
