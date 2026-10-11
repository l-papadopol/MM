# MadModem 0.6.1 installer builds

The `Build installers` workflow (`.github/workflows/build-distribution.yml`)
runs automatically on pushes to `main`/`master` and `v*` tags. It can also be
started through **Actions → Build installers → Run workflow**. It does not
publish a public GitHub Release; completed installers are available as artifacts.

| Job | Build environment | Artifact |
| --- | --- | --- |
| Windows | Windows 2022, MSYS2/MinGW64, Qt5 | One x64 Setup.exe with CPU/OS selection |
| Ubuntu | Ubuntu 24.04, Qt5, system Hamlib | Ubuntu24.04 Setup.run containing DEB |
| Fedora | Fedora 43 container, Qt5, system Hamlib | fc43 Setup.run containing RPM |
| macOS Intel | macos-15-intel, Qt6, Homebrew | x86_64 unsigned.pkg |
| macOS Apple Silicon | macos-15, Qt6, Homebrew | arm64 unsigned.pkg |

Each job builds and runs CTest before packaging. Native smoke checks then install
and reinstall the package, verify user data preservation and launch `--version`.
The Windows check also verifies that the installed executable matches the CPU
variant selected by the probe, starts without the compiler runtime on PATH and
retains user data after uninstall. A runner without AVX2/FMA tests the legacy
executable; the AVX2 variant is still compiled and included.

Upload failures or failed build/test/install steps block that platform's artifact.
The workflow uses read-only repository permissions. Signing/notarization secrets
are not configured. macOS binaries target macOS 15 or later, matching the runners
and their Homebrew libraries.

The separate `source-regression.yml` workflow continues to test Linux Qt5 and Qt6
on pushes and pull requests. Match release tags exactly, for example `v0.6.1`.
Copy the complete source tree, including `.github`, into the repository root.
When updating a repository by copying sources, remove the old tracked verification
folders and obsolete release reports as well: copying over files does not delete
them. Preserve local settings, logs and recordings outside the source tree.

For end-user installation and data migration see [INSTALLERS.md](INSTALLERS.md).
