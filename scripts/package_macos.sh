#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MadModem"
ARCHS="${MADMODEM_OSX_ARCHITECTURES:-$(uname -m)}"
BUILD_DIR="${MADMODEM_MACOS_BUILD_DIR:-$ROOT_DIR/build-macos-$ARCHS}"
APP_PATH="${MADMODEM_APP_PATH:-$BUILD_DIR/$APP_NAME.app}"
APP_EXE="$APP_PATH/Contents/MacOS/$APP_NAME"
FW_DIR="$APP_PATH/Contents/Frameworks"
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
if [[ ! -x "$APP_EXE" ]]; then
    echo "ERROR: app executable not found: $APP_EXE" >&2
    exit 1
fi

QT_PREFIX="${QT_PREFIX:-$(brew --prefix qt 2>/dev/null || true)}"
MACDEPLOYQT="${MACDEPLOYQT:-$QT_PREFIX/bin/macdeployqt}"
if [[ ! -x "$MACDEPLOYQT" ]]; then
    echo "ERROR: macdeployqt not found. Expected: $MACDEPLOYQT" >&2
    exit 1
fi

mkdir -p "$DIST_DIR" "$FW_DIR"

# Keep the data files in the location expected by the current runtime code.
mkdir -p "$APP_PATH/Contents/MacOS"
[[ -f "$ROOT_DIR/cty.csv" ]] && cp -f "$ROOT_DIR/cty.csv" "$APP_PATH/Contents/MacOS/cty.csv"
if [[ -d "$ROOT_DIR/tests/wav" ]]; then
    mkdir -p "$APP_PATH/Contents/MacOS/tests/wav"
    rsync -a --delete "$ROOT_DIR/tests/wav/" "$APP_PATH/Contents/MacOS/tests/wav/"
fi

"$MACDEPLOYQT" "$APP_PATH" -qmldir="$ROOT_DIR/qml" -verbose=2

collect_macho_files() {
    printf '%s\n' "$APP_EXE"
    find "$FW_DIR" -type f \( -perm -111 -o -name 'Qt*' -o -name '*.dylib' \) -print 2>/dev/null || true
}

copy_qt_framework_if_needed() {
    local fw="$1"
    local src="$QT_PREFIX/lib/${fw}.framework"
    local dst="$FW_DIR/${fw}.framework"
    if [[ -d "$dst" ]]; then
        return 0
    fi
    if [[ ! -d "$src" ]]; then
        echo "ERROR: linked Qt framework not found in Homebrew Qt prefix: $src" >&2
        exit 1
    fi
    echo "==> Bundling missing Qt framework: ${fw}.framework"
    ditto "$src" "$dst"
}

fix_qt_install_names() {
    local bin="$1"
    [[ -f "$bin" ]] || return 0
    local prefix="@executable_path/../Frameworks"
    case "$bin" in
        "$FW_DIR"/*) prefix="@loader_path/../../.." ;;
    esac
    otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}' | grep 'Qt.*\.framework' | while IFS= read -r ref; do
        local fw
        fw="$(printf '%s
' "$ref" | sed -n 's#.*\(Qt[A-Za-z0-9]*\)\.framework/.*##p')"
        [[ -n "$fw" ]] || continue
        local portable_ref="$prefix/${fw}.framework/Versions/A/${fw}"
        if [[ "$ref" != "$portable_ref" ]]; then
            install_name_tool -change "$ref" "$portable_ref" "$bin" 2>/dev/null || true
        fi
    done
}

# macdeployqt may miss frameworks that are linked directly by CMake but not
# pulled through the plugin scanner.  Bundle every Qt*.framework mentioned by
# the executable or already-copied frameworks, including QtSerialPort.
while IFS= read -r fw; do
    [[ -n "$fw" ]] && copy_qt_framework_if_needed "$fw"
done < <(
    collect_macho_files | while IFS= read -r bin; do
        [[ -f "$bin" ]] || continue
        otool -L "$bin" 2>/dev/null | sed -n 's#.*\(Qt[A-Za-z0-9]*\)\.framework/.*#\1#p'
    done | sort -u
)

# Normalize Qt install names for manually copied or imperfectly deployed Qt frameworks.
collect_macho_files | while IFS= read -r bin; do
    fix_qt_install_names "$bin"
done

# Bundle non-Qt Homebrew dylibs too (Hamlib and its dylib dependencies).
# macdeployqt handles Qt frameworks/plugins, but not arbitrary Homebrew libs.
if command -v dylibbundler >/dev/null 2>&1; then
    dylibbundler -od -b \
        -x "$APP_EXE" \
        -d "$FW_DIR" \
        -p "@executable_path/../Frameworks" || true
else
    echo "WARNING: dylibbundler not found; non-Qt Homebrew dylibs may remain external." >&2
fi

verify_qt_framework_closure() {
    local missing=0
    echo "==> Verifying bundled Qt framework closure"
    while IFS= read -r bin; do
        [[ -f "$bin" ]] || continue
        while IFS= read -r ref; do
            [[ -n "$ref" ]] || continue
            fw="$(printf '%s\n' "$ref" | sed -n 's#.*\(Qt[A-Za-z0-9]*\)\.framework/.*#\1#p')"
            [[ -n "$fw" ]] || continue
            if [[ "$ref" == /* ]]; then
                echo "ERROR: non-portable external Qt framework reference in $bin: $ref" >&2
                missing=1
            fi
            if [[ ! -d "$FW_DIR/${fw}.framework" ]]; then
                echo "ERROR: missing ${fw}.framework required by $bin ($ref)" >&2
                missing=1
            fi
        done < <(otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}' | grep 'Qt.*\.framework' || true)
    done < <(collect_macho_files)
    if [[ "$missing" -ne 0 ]]; then
        exit 1
    fi
}
verify_qt_framework_closure

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
