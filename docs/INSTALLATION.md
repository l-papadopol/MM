# Installation — MadModem 0.6.5

Download the installer or portable package matching your system from
[GitHub Releases](https://github.com/l-papadopol/MM/releases). Close MM before installation.

| System | Installer | Portable |
| --- | --- | --- |
| Windows x64 | AutoCPU EXE selects Compatible/AVX2 automatically | Compatible or AVX2/FMA ZIP |
| Ubuntu 24.04 / Mint 22.x x64 | RUN or DEB | TAR.GZ or ZIP |
| Fedora 43 x64 | RUN or RPM | TAR.GZ or ZIP |
| macOS 15+ Intel / Apple Silicon | PKG | app ZIP or DMG |

Linux: run `bash <installer>.run`, `sudo apt install ./<package>.deb` or
`sudo dnf install ./<package>.rpm` as appropriate. RUN wraps the matching native
package. Native packages use system dependencies; portable packages include their
Qt/Hamlib runtime. Mint 21.x, LMDE and other Linux bases require a compatible build.
Extract Linux portable archives into a writable folder and launch `AppRun`.
Windows packages are unsigned; macOS packages are ad-hoc signed.

## Settings, logbook and updates

Managed installations use per-user directories:

| System | Settings | Default logbook |
| --- | --- | --- |
| Windows | `%LOCALAPPDATA%/MadModem/settings.mad` | `%LOCALAPPDATA%/MadModem/logbook.adi` |
| Linux | `~/.config/MadModem/settings.mad` | `~/.local/share/MadModem/logbook.adi` |
| macOS | `~/Library/Preferences/MadModem/settings.mad` | `~/Library/Application Support/MadModem/logbook.adi` |

Linux XDG overrides and explicitly chosen logbook paths are respected. Portable
copies retain executable-adjacent data. Back up settings, ADIF files and recordings
before replacing a portable folder; do not delete the old copy until checked.
Uninstalling MM must not be used to delete personal data.

Help provides update checking and its startup setting. Published prereleases are
included; versions and build numbers determine ordering, independently of Latest.
Downloads require a matching asset and SHA256SUMS. Managed installations use the
installer; portable updates keep a backup and verify the new executable before restart.
Stop transmission and automation before updating. An old updater may need one manual install.

If APT reports an unfinished operation, complete `sudo dpkg --configure -a` before
retrying. The installer does not repair unrelated packages automatically.
