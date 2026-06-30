#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MadModem"
ARCHS="${MADMODEM_OSX_ARCHITECTURES:-$(uname -m)}"
BUILD_DIR="${MADMODEM_MACOS_BUILD_DIR:-$ROOT_DIR/build-macos-$ARCHS}"
APP_PATH="${MADMODEM_APP_PATH:-$BUILD_DIR/$APP_NAME.app}"
DIST_DIR="${MADMODEM_MACOS_DIST_DIR:-$ROOT_DIR/dist/macos}"
VERSION="$(cat "$ROOT_DIR/MADMODEM_VERSION.txt" 2>/dev/null || echo dev)"
STAMP_ARCH="${ARCHS//;/+}"
ZIP_PATH="$DIST_DIR/${APP_NAME}-${VERSION}-macOS-${STAMP_ARCH}-unsigned.zip"
DMG_PATH="$DIST_DIR/${APP_NAME}-${VERSION}-macOS-${STAMP_ARCH}-unsigned.dmg"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: macOS packaging must run on macOS/Darwin." >&2
    exit 1
fi
if [[ ! -d "$APP_PATH" ]]; then
    echo "ERROR: app bundle not found: $APP_PATH" >&2
    exit 1
fi

QT_PREFIX="${QT_PREFIX:-$(brew --prefix qt 2>/dev/null || true)}"
MACDEPLOYQT="${MACDEPLOYQT:-$QT_PREFIX/bin/macdeployqt}"
if [[ ! -x "$MACDEPLOYQT" ]]; then
    echo "ERROR: macdeployqt not found. Expected: $MACDEPLOYQT" >&2
    exit 1
fi

mkdir -p "$DIST_DIR"

# Keep the data files in the location expected by the current runtime code.
mkdir -p "$APP_PATH/Contents/MacOS"
[[ -f "$ROOT_DIR/cty.csv" ]] && cp -f "$ROOT_DIR/cty.csv" "$APP_PATH/Contents/MacOS/cty.csv"
if [[ -d "$ROOT_DIR/tests/wav" ]]; then
    mkdir -p "$APP_PATH/Contents/MacOS/tests/wav"
    rsync -a --delete "$ROOT_DIR/tests/wav/" "$APP_PATH/Contents/MacOS/tests/wav/"
fi

"$MACDEPLOYQT" "$APP_PATH" -qmldir="$ROOT_DIR/qml" -verbose=2

# Bundle non-Qt Homebrew dylibs too (Hamlib and its dylib dependencies).
# macdeployqt handles Qt frameworks/plugins, but not arbitrary Homebrew libs.
if command -v dylibbundler >/dev/null 2>&1; then
    dylibbundler -od -b \
        -x "$APP_PATH/Contents/MacOS/$APP_NAME" \
        -d "$APP_PATH/Contents/Frameworks" \
        -p "@executable_path/../Frameworks" || true
else
    echo "WARNING: dylibbundler not found; non-Qt Homebrew dylibs may remain external." >&2
fi

if [[ "${MADMODEM_ADHOC_SIGN:-on}" == "on" ]]; then
    # Ad-hoc signing is not Apple notarization. It just makes the unsigned CI
    # bundle internally consistent after macdeployqt has copied frameworks.
    codesign --force --deep --sign - "$APP_PATH" || true
fi

rm -f "$ZIP_PATH" "$DMG_PATH"
ditto -c -k --sequesterRsrc --keepParent "$APP_PATH" "$ZIP_PATH"
hdiutil create -volname "$APP_NAME" -srcfolder "$APP_PATH" -ov -format UDZO "$DMG_PATH"

echo "Packaged ZIP: $ZIP_PATH"
echo "Packaged DMG: $DMG_PATH"
