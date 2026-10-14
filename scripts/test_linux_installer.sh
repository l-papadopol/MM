#!/usr/bin/env bash
# CI only: installs the package on a disposable native runner/container.
set -euo pipefail
installer="$(realpath "$1")"
bash "$installer" --help
# An unattended caller that has not accepted the GPL must not install anything.
if env -u DISPLAY -u WAYLAND_DISPLAY bash "$installer" </dev/null; then
    echo 'ERROR: license acceptance was bypassed' >&2; exit 1
fi
bash "$installer" --accept-gpl3
test -x /usr/bin/MadModem
test -f /usr/lib/madmodem/bin/managed-install.flag
test -f /usr/share/applications/MadModem.desktop
user_data="${XDG_DATA_HOME:-$HOME/.local/share}/MadModem"
mkdir -p "$user_data"
printf 'preserve user data\n' > "$user_data/installer-test.txt"
bash "$installer" --accept-gpl3
grep -qx 'preserve user data' "$user_data/installer-test.txt"
QT_QPA_PLATFORM=offscreen /usr/bin/MadModem --version
