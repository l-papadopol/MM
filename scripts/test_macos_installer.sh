#!/bin/bash
# Destructive fixtures are restricted to disposable GitHub macOS runners.
set -euo pipefail
[[ "${GITHUB_ACTIONS:-}" == true && "${RUNNER_OS:-}" == macOS ]]
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
arch="${MADMODEM_OSX_ARCHITECTURES:?}"
version="$(tr -d '\r\n' < "$root/MADMODEM_VERSION.txt")"
package="$root/dist/macos/MadModem-$version-macOS-$arch-unsigned.pkg"
app=/Applications/MadModem.app
trap 'echo "macOS installer verification failed at line $LINENO" >&2; pkgutil --pkg-info org.iz6nnh.madmodem || true; ls -ld "$app" "$app/Contents/MacOS" "$app/Contents/MacOS/managed-install.flag" 2>/dev/null || true' ERR
test -f "$package"
# A pre-existing app would conceal update-only packaging bugs.
if [[ -e "$app" ]]; then
    echo 'ERROR: fresh-install test requires a clean runner without /Applications/MadModem.app' >&2
    exit 1
fi
sudo installer -pkg "$package" -target /
test -x "$app/Contents/MacOS/MadModem"
test -f "$app/Contents/MacOS/managed-install.flag"
codesign --verify --deep --strict "$app"
mkdir -p "$HOME/Library/Application Support/MadModem"
printf 'preserve user data\n' > "$HOME/Library/Application Support/MadModem/installer-test.txt"
# Simulate data from an old portable app stored in /Applications.
printf 'preserve portable settings\n' | sudo tee "$app/Contents/MacOS/settings.mad" >/dev/null
printf 'preserve portable logbook\n' | sudo tee "$app/Contents/MacOS/logbook.adi" >/dev/null
sudo touch "$app/Contents/obsolete-program-file"
sudo installer -pkg "$package" -target /
test -f "$app/Contents/MacOS/managed-install.flag"
test ! -e "$app/Contents/obsolete-program-file"
grep -qx 'preserve user data' "$HOME/Library/Application Support/MadModem/installer-test.txt"
backups='/Library/Application Support/MadModem/Portable Backups'
found=0
for backup in "$backups"/*; do
    if [[ -f "$backup/settings.mad" && -f "$backup/logbook.adi" ]]; then
        grep -qx 'preserve portable settings' "$backup/settings.mad"
        grep -qx 'preserve portable logbook' "$backup/logbook.adi"
        found=1
    fi
done
[[ $found -eq 1 ]]
codesign --verify --deep --strict "$app"
"$app/Contents/MacOS/MadModem" --version
echo 'PASS macOS fresh install, reinstall, stale-file removal, portable data preservation and signed-bundle integrity'
