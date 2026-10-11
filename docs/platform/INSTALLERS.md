# MadModem 0.6.1 installation

Close MadModem before running an installer. Interactive installers display
Copyright (C) 2026 Papadopol Lucian-Ioan and the unmodified GNU GPL version 3,
and require acceptance before installing.

## Portable downloads remain available

Installation is optional. The **Build installers** workflow publishes separate
`portable-*` and `installer-*` artifacts from the same source revision:

- Windows: legacy and AVX2/FMA standalone ZIPs with their runtime DLLs. Extract
  into a writable folder and run `bin/MadModem.exe`. Use legacy if unsure; automatic
  CPU selection is provided by Setup.exe, not by the two separate ZIPs.
- Linux: ZIP and TAR.GZ for Ubuntu 24.04 and Fedora 43. Extract and run
  `bin/MadModem`. Matching Qt5 and Hamlib runtime dependencies must already be
  installed. These archives are not self-contained or universal Linux packages.
- macOS: app ZIP and DMG for each architecture, alongside the PKG.

Portable archives are uploaded before installer creation, so they remain
available if a subsequent installer packaging step fails. Keep settings, ADIF
logbooks and recordings when replacing program files. Managed-install markers
are added only to the installer payload, never to the portable archives.

## Windows

Run `MadModem-0.6.1-Windows-x64-Setup.exe`. The installer selects the AVX2 build
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
bash MadModem-0.6.1-1-Ubuntu24.04-amd64-Setup.run
```

Use the Fedora `fc43` installer on Fedora 43. The license appears in a Zenity
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

Open the Intel `x86_64` or Apple Silicon `arm64` PKG for macOS 15 or later. Apple's
Installer presents the license and installs to `/Applications/MadModem.app`.
Subsequent packages update the same bundle. The current packages are ad-hoc
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
