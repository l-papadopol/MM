# MadModem 0.6.3 installation

Close MadModem before running an installer. Interactive installers display
Copyright (C) 2026 Papadopol Lucian-Ioan and the unmodified GNU GPL version 3,
and require acceptance before installing.

## Portable downloads remain available

Installation is optional. After every platform job succeeds, **Build installers**
publishes the following downloads under **Releases → Assets**, with `SHA256SUMS.txt`.
Version tags (for example `v0.6.3`) publish official releases; builds from a branch
publish numbered prereleases (`build-0.6.3-N`). Manual runs follow the same rule:
select the version tag to publish an official release. Existing release notes are
preserved when uploading to an existing release. The workflow also retains the
separate `portable-*` and `installer-*` Actions artifacts from the same revision.
Publication uses the built-in GitHub token with `contents: write` only in the
publish job; no personal access token is required. Repository or organization
policies must allow this permission. Existing immutable releases cannot be
overwritten; publish a new version tag instead.

Downloads:

- Windows: `Portable-Compatible` (legacy) and `Portable-AVX2-FMA` ZIPs with their runtime DLLs. Extract
  into a writable folder and run `bin/MadModem.exe`. Use `Compatible` if unsure; automatic
  CPU selection is provided by `Installer-AutoCPU.exe`, not by the two separate ZIPs.
- Linux: ZIP and TAR.GZ for Ubuntu 24.04 and Fedora 43. Extract and run
  `AppRun` (or `bin/MadModem`). Qt5, Hamlib, Qt plugins, QML modules, OpenSSL
  and their non-system dependencies are bundled. The launcher uses only the
  private library/plugin directories; do not start `MadModem.bin` directly.
  Select the matching distribution: glibc and graphics drivers remain supplied
  by the host. These are not universal Linux binaries. Runtime provenance and
  license texts are included. CI checks the relocated archive in a clean
  distribution container without Qt or Hamlib installed.
- macOS: `Intel` or `AppleSilicon` identifies the processor family. `Installer.pkg`
  installs the app; `Portable.zip` contains the app bundle; `DiskImage.dmg` is a
  mountable disk image. Apple Silicon means Macs with Apple M-series processors.

Portable archives are uploaded before installer creation, so they remain
available if a subsequent installer packaging step fails. Keep settings, ADIF
logbooks and recordings when replacing program files. Managed-install markers
are added only to the installer payload, never to the portable archives.

## Windows

Run `MadModem-0.6.3-Windows-64bit-Installer-AutoCPU.exe`. The installer selects the AVX2 build
only when the CPU supports SSE4.2, FMA, XSAVE, OSXSAVE, AVX and AVX2, and the
operating system enables the XMM/YMM register state. Otherwise it selects legacy.
Both variants are 64-bit; legacy does not mean 32-bit Windows.

The default location is `%LOCALAPPDATA%\Programs\MadModem`. The installer creates
a Start-menu shortcut, an optional desktop shortcut and an uninstall entry. A
stable application ID and install path allow later installers to update the same
installation. Reinstalling the same version also replaces its program files.
User data is not included in the uninstall file list.

## Linux

Choose the installer matching the distribution, then run:

```bash
bash MadModem-0.6.3-Linux-Ubuntu24.04-64bit-Installer.run
```

Use the `Linux-Fedora43-64bit-Installer.run` package on Fedora 43. The license appears in a Zenity
window when available, otherwise in the terminal. The installer then requests
installation privileges and delegates dependency resolution and updates to APT
or DNF. No manual extraction directory is needed. Start MM from the applications
menu or with `MadModem`. Use `sudo apt remove madmodem` or `sudo dnf remove madmodem`
to uninstall. Packages target their named distribution; they are not universal
Linux binaries.

For unattended deployment, `--accept-gpl3` explicitly records acceptance by the
caller. An unattended run without this option refuses to install. Internet access
may be required by the package manager to obtain missing dependencies.

## macOS

Open the `Intel-Installer.pkg` or `AppleSilicon-Installer.pkg` for macOS 15 or later. Apple's
Installer presents the license and installs to `/Applications/MadModem.app`.
Subsequent packages replace the same bundle, removing obsolete program files.
Executable-adjacent portable data is preserved under
`/Library/Application Support/MadModem/Portable Backups` before replacement.
MM migrates the saved default settings/logbook on first launch if user-folder
copies do not already exist; custom files remain available in the backup. The current packages are ad-hoc
signed, without Developer ID signing or notarization. Windows installers are
also unsigned; operating-system trust warnings may appear.

## Preferences and logbooks

Managed installs contain `managed-install.flag` beside the executable. They use
Qt's per-user configuration/data directories:

| System | settings.mad | Default logbook.adi |
| --- | --- | --- |
| Windows | `%LOCALAPPDATA%\MadModem` | `%LOCALAPPDATA%\MadModem` |
| Linux | `~/.config/MadModem` | `~/.local/share/MadModem` |
| macOS | `~/Library/Preferences/MadModem` | `~/Library/Application Support/MadModem` |

Linux XDG overrides are respected. A logbook path explicitly chosen by the user
remains unchanged. When old settings or a default logbook are found beside the
installed executable, MM copies them into the user directory only if no user
file already exists; the originals are retained. Failed migration keeps access
to the original instead of silently loading an empty log.

A portable copy in an unrelated folder cannot be discovered safely. Before the
first managed launch, copy its `settings.mad` into the configuration directory
above, keeping a backup. Keep its ADIF file in a permanent user folder and select
that file in the logbook. Never delete the old portable folder until settings and
QSO counts have been checked. Portable builds without the marker keep their
existing path behavior.

Updates are applied by running the next installer. This release does not include
an automatic download/update service.

## In-application updates

At startup MM checks stable releases on l-papadopol/MM on GitHub. Help offers a manual check and a switch to disable startup checks. Drafts, prereleases and versions equal to or older than the running version are ignored. Confirm the download and then confirm closing MM to apply it. Stop TX and automatic operation first. Downloads require a matching package and SHA256SUMS.txt. Managed installations open their native installer; license and system permission dialogs still apply. Portable copies stay portable: a complete backup is kept beside the program folder, settings and logbooks are preserved, and MM restarts after a bounded version check. If that check fails, the helper restores the previous folder. Keep the backup until the update has been tested. Windows keeps the current portable CPU variant; installers choose automatically. Automatic Linux selection supports Ubuntu 24.04 and Fedora 43 only. Starting with source R4, automatic tags such as `build-0.6.4-N` are also accepted after you clear pre-release and mark the release Latest. Tag renaming is unnecessary. A higher public version is still required; another build of the installed version does not trigger an update. Older clients need a numeric tag or one manual installation to acquire the new parser.

GitHub HTTPS and SHA-256 protect transfer integrity; the checksum is not an independent publisher signature. Windows remains unsigned and macOS ad-hoc signed. The downloaded installer can require normal OS authorization. A non-writable portable folder or an unsupported distribution requires a manual update. Native Windows/macOS helper behavior must be verified on those systems.
