#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MadModem"
VERSION="$(cat "$ROOT_DIR/MADMODEM_VERSION.txt" 2>/dev/null || echo dev)"
INSTALL_DIR="${MADMODEM_LINUX_INSTALL_DIR:-$ROOT_DIR/dist/linux}"
OUT_DIR="${MADMODEM_PACKAGE_OUT_DIR:-$ROOT_DIR/dist/packages}"
ARCH="${MADMODEM_LINUX_ARCH:-x86_64}"
PACKAGE_NAME="${APP_NAME}-${VERSION}-Linux-${ARCH}"
PACKAGE_DIR="$OUT_DIR/$PACKAGE_NAME"
TAR_PATH="$OUT_DIR/$PACKAGE_NAME.tar.gz"
ZIP_PATH="$OUT_DIR/$PACKAGE_NAME.zip"

if [[ ! -x "$INSTALL_DIR/bin/$APP_NAME" ]]; then
    echo "ERROR: Linux executable not found: $INSTALL_DIR/bin/$APP_NAME" >&2
    exit 1
fi

rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"
cp -a "$INSTALL_DIR/." "$PACKAGE_DIR/"

# User-facing documentation/legal files for the binary package.
for f in README.md RELEASE_NOTES.md CHANGELOG.md LICENSE.md COPYING AUTHORS.md THIRD_PARTY_NOTICES.md TRANSLATION_AUDIT.md cty.csv; do
    [[ -f "$ROOT_DIR/$f" ]] && cp -f "$ROOT_DIR/$f" "$PACKAGE_DIR/"
done

cat > "$PACKAGE_DIR/RUN_LINUX.txt" <<TXT
MadModem Linux package
======================

Run:
  ./bin/MadModem

This CI package is built on the GitHub Actions Ubuntu runner. It is a native
Linux tarball/zip, not an AppImage. If your distribution does not already have
Qt runtime libraries installed, install the matching Qt5 runtime packages first.
TXT

chmod +x "$PACKAGE_DIR/bin/$APP_NAME" || true
rm -f "$TAR_PATH" "$ZIP_PATH"
mkdir -p "$OUT_DIR"
tar -C "$OUT_DIR" -czf "$TAR_PATH" "$PACKAGE_NAME"
if command -v zip >/dev/null 2>&1; then
    (cd "$OUT_DIR" && zip -rq "$ZIP_PATH" "$PACKAGE_NAME")
else
    python3 - "$PACKAGE_DIR" "$ZIP_PATH" <<'PY'
import sys, zipfile
from pathlib import Path
src = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as zf:
    for p in sorted(src.rglob('*')):
        if p.is_file():
            zf.write(p, p.relative_to(src.parent))
PY
fi

printf 'Linux package complete:\n  %s\n  %s\n' "$TAR_PATH" "$ZIP_PATH"
