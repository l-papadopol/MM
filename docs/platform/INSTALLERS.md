# MadModem 0.6.4 installation

Close MadModem before running an installer. Interactive installers display
Copyright (C) 2026 Papadopol Lucian-Ioan and the unmodified GNU GPL version 3,
and require acceptance before installing.

## Portable downloads remain available

Installation is optional. After every platform job succeeds, **Build installers**
publishes the following downloads under **Releases → Assets**, with `SHA256SUMS.txt`.
Version tags (for example `v0.6.4`) publish official releases; builds from a branch
publish numbered prereleases (`build-0.6.4-N`). Manual runs follow the same rule:
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

Run `MadModem-0.6.4-Windows-64bit-Installer-AutoCPU.exe`. The installer selects the AVX2 build
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
bash MadModem-0.6.4-Linux-Ubuntu24.04-64bit-Installer.run
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

MM can discover, download and propose updates from inside the application.
Managed installations apply the update through the native installer.

## In-application updates

At startup MM checks all published releases on l-papadopol/MM on GitHub, including pre-releases. The Latest badge is irrelevant; drafts are excluded. Newer public versions take precedence, then newer distribution build numbers within the same version. The same or older build is not proposed again after installation. Help offers a manual check and a switch for startup checks. If automatic operation is active, the startup check waits for an idle state. The offer shows installed and available version/build and identifies pre-releases. Confirm the download, then confirm closing MM to install. Stop TX and automatic operation first. Downloads require the matching platform package and SHA256SUMS.txt. Managed installations open their native installer. Portable copies keep a full backup, settings and logbooks; the new executable is checked for version and build before restart, with rollback on failure. Windows keeps the portable CPU variant; installers select it automatically. Automatic Linux selection supports Ubuntu 24.04 and Fedora 43. The distribution workflow records the build identity automatically; future branch builds can remain pre-releases without promoting them to Latest or renaming their tags. Install 0.6.4 once manually if your older updater cannot detect it. Subsequent published builds are detected by the new checker.

GitHub HTTPS and SHA-256 protect transfer integrity; the checksum is not an independent publisher signature. Windows remains unsigned and macOS ad-hoc signed. The downloaded installer can require normal OS authorization. A non-writable portable folder or an unsupported distribution requires a manual update. Native Windows/macOS helper behavior must be verified on those systems.

## Linux package-manager interruption

If the installer reports an unfinished dpkg operation, open a terminal and run
`sudo dpkg --configure -a`. When it finishes successfully, run the installer again.
The DEB installer checks `dpkg --audit` and pending dpkg journal files before APT;
it stops with exit code 32 and displays that repair command. It does not repair
or configure unrelated system packages automatically.

## Linux portable automatic update

R12 identifies the existing bundle using `bin/distribution-platform.txt`, then the
older root `BUILD_INFO.txt`, before consulting `/etc/os-release`. This preserves
the selected Ubuntu24.04/Fedora43 ABI on compatible derivatives. A noble-based
distribution is also recognized when bundle metadata is absent. Unsupported or
unknown ABIs are not guessed. Portable selection prefers the matching canonical
TAR.GZ asset, then ZIP, then the historical archive names. Managed installations
continue to select only their matching installer. SHA256SUMS.txt is required.
Both archive formats preserve user state and support rollback after a failed
version/build check. ZIP extraction requires `unzip`.

If an older portable executable cannot identify its matching asset, install R12
manually once, keeping the existing user files. Editing the release title or its
Latest badge is unnecessary. No current release assets are changed by this patch.
