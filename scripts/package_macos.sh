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
if [[ -z "$QT_PREFIX" || ! -d "$QT_PREFIX" ]]; then
    echo "ERROR: Homebrew Qt prefix not found. Set QT_PREFIX or install: brew install qt" >&2
    exit 1
fi
if [[ ! -x "$MACDEPLOYQT" ]]; then
    echo "ERROR: macdeployqt not found. Expected: $MACDEPLOYQT" >&2
    exit 1
fi

mkdir -p "$DIST_DIR" "$FW_DIR"

# Keep data files in the location expected by the current runtime code.
mkdir -p "$APP_PATH/Contents/MacOS"
[[ -f "$ROOT_DIR/cty.csv" ]] && cp -f "$ROOT_DIR/cty.csv" "$APP_PATH/Contents/MacOS/cty.csv"

# First let Qt's official deploy tool do its normal pass.  Do NOT pass
# -qmldir here: on Homebrew Qt6 it scans too broadly and tries to deploy
# optional modules not installed on the runner (QtStateMachine, Qt3D,
# VirtualKeyboard...), which broke arm64 packaging.  MadModem only needs the
# small QML map files plus QtQuick/QtLocation/QtPositioning imports, copied
# explicitly below.
"$MACDEPLOYQT" "$APP_PATH" -verbose=2 || {
    echo "ERROR: macdeployqt failed" >&2
    exit 1
}

find_qt_qml_dir() {
    local qml_dir=""
    for qmake in "$QT_PREFIX/bin/qmake6" "$QT_PREFIX/bin/qmake" qmake6 qmake; do
        if command -v "$qmake" >/dev/null 2>&1 || [[ -x "$qmake" ]]; then
            qml_dir="$($qmake -query QT_INSTALL_QML 2>/dev/null || true)"
            if [[ -n "$qml_dir" && -d "$qml_dir" ]]; then
                printf '%s
' "$qml_dir"
                return 0
            fi
        fi
    done
    for qml_dir in         "$QT_PREFIX/qml"         "$QT_PREFIX/share/qt/qml"         "$(brew --prefix 2>/dev/null || true)/share/qt/qml"         "/opt/homebrew/share/qt/qml"         "/usr/local/share/qt/qml"; do
        [[ -n "$qml_dir" && -d "$qml_dir" ]] || continue
        printf '%s
' "$qml_dir"
        return 0
    done
    return 1
}

copy_qml_module_minimal() {
    local module="$1"
    local qml_root="$2"
    local src="$qml_root/$module"
    local dst="$APP_PATH/Contents/Resources/qml/$module"
    if [[ ! -d "$src" ]]; then
        echo "ERROR: required QML module not found: $src" >&2
        exit 1
    fi
    echo "==> Bundling minimal QML module: $module"
    mkdir -p "$dst"
    # Copy only the module root files.  Do not recursively copy optional
    # submodules such as Controls, Scene3D, VirtualKeyboard, etc.
    find "$src" -maxdepth 1 -type f         \( -name 'qmldir' -o -name '*.qml' -o -name '*.qmltypes' -o -name '*.metatypes' -o -name '*.dylib' \)         -exec cp -f {} "$dst/" \;
}

QT_QML_DIR="${QT_QML_DIR:-$(find_qt_qml_dir || true)}"
if [[ -z "$QT_QML_DIR" || ! -d "$QT_QML_DIR" ]]; then
    echo "ERROR: Qt QML import directory not found. Set QT_QML_DIR." >&2
    exit 1
fi
mkdir -p "$APP_PATH/Contents/Resources/qml"
copy_qml_module_minimal "QtQuick" "$QT_QML_DIR"
copy_qml_module_minimal "QtLocation" "$QT_QML_DIR"
copy_qml_module_minimal "QtPositioning" "$QT_QML_DIR"
# Keep MadModem's own QML files available beside Qt imports.
mkdir -p "$APP_PATH/Contents/Resources/qml/MadModem"
cp -f "$ROOT_DIR/qml"/*.qml "$APP_PATH/Contents/Resources/qml/MadModem/" 2>/dev/null || true


# Homebrew Qt may deploy optional imageformat plugins that MadModem does not
# need.  The PDF image plugin can reference QtPdf.framework, which is not
# installed by the lean Qt toolchain on GitHub runners.  Do not make the whole
# app package depend on that optional plugin.
if [[ -f "$APP_PATH/Contents/PlugIns/imageformats/libqpdf.dylib" ]]; then
    echo "==> Removing optional imageformats/libqpdf.dylib (avoids QtPdf.framework dependency)"
    rm -f "$APP_PATH/Contents/PlugIns/imageformats/libqpdf.dylib"
fi

# Make the deployed app prefer the QML imports and plugins bundled inside the
# .app, not Homebrew's global Qt installation.
cat > "$APP_PATH/Contents/Resources/qt.conf" <<QTC
[Paths]
Plugins = PlugIns
Qml2Imports = Resources/qml
QTC

framework_name_from_ref() {
    # Extract QtFoo from any of:
    #   /opt/homebrew/.../QtFoo.framework/Versions/A/QtFoo
    #   @rpath/QtFoo.framework/Versions/A/QtFoo
    #   @executable_path/../Frameworks/QtFoo.framework/Versions/A/QtFoo
    printf '%s\n' "$1" | sed -n 's#.*\(Qt[A-Za-z0-9_]*\)\.framework/.*#\1#p'
}

qt_framework_source() {
    local fw="$1"
    local src=""
    for prefix in \
        "$QT_PREFIX" \
        "$(brew --prefix qt 2>/dev/null || true)" \
        "$(brew --prefix qtbase 2>/dev/null || true)" \
        "$(brew --prefix qtmultimedia 2>/dev/null || true)" \
        "$(brew --prefix qtserialport 2>/dev/null || true)" \
        "$(brew --prefix qttools 2>/dev/null || true)" \
        "$(brew --prefix qtdeclarative 2>/dev/null || true)" \
        "$(brew --prefix qtlocation 2>/dev/null || true)" \
        "$(brew --prefix qtsvg 2>/dev/null || true)" \
        "$(brew --prefix qtstatemachine 2>/dev/null || true)" \
        "$(brew --prefix qt3d 2>/dev/null || true)" \
        "$(brew --prefix qtvirtualkeyboard 2>/dev/null || true)"
    do
        [[ -n "$prefix" && -d "$prefix/lib/${fw}.framework" ]] || continue
        src="$prefix/lib/${fw}.framework"
        break
    done
    printf '%s\n' "$src"
}

framework_binary() {
    local fw="$1"
    printf '%s\n' "$FW_DIR/${fw}.framework/Versions/A/${fw}"
}

copy_qt_framework() {
    local fw="$1"
    [[ -n "$fw" ]] || return 0
    local dst="$FW_DIR/${fw}.framework"
    local dst_bin
    dst_bin="$(framework_binary "$fw")"
    if [[ -e "$dst_bin" ]]; then
        return 0
    fi
    local src
    src="$(qt_framework_source "$fw")"
    if [[ -z "$src" || ! -d "$src" ]]; then
        echo "ERROR: cannot locate required Qt framework ${fw}.framework under Homebrew Qt prefixes" >&2
        exit 1
    fi
    echo "==> Force-bundling Qt framework: ${fw}.framework from $src"
    rm -rf "$dst"
    ditto "$src" "$dst"
    if [[ ! -e "$dst_bin" ]]; then
        echo "ERROR: copied ${fw}.framework but binary is missing: $dst_bin" >&2
        find "$dst" -maxdepth 4 -print >&2 || true
        exit 1
    fi
}

collect_macho_files() {
    printf '%s\n' "$APP_EXE"
    find "$FW_DIR" -type f \( -perm -111 -o -name 'Qt*' -o -name '*.dylib' \) -print 2>/dev/null || true
    find "$APP_PATH/Contents/PlugIns" "$APP_PATH/Contents/Resources/qml" -type f -name '*.dylib' -print 2>/dev/null || true
}

qt_refs_for_file() {
    local bin="$1"
    [[ -e "$bin" ]] || return 0
    otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}' | while IFS= read -r ref; do
        framework_name_from_ref "$ref"
    done | sed '/^$/d' | sort -u
}

# Seed with the modules the executable is known to link/use in this project.
# Extra Qt frameworks are acceptable in an unsigned portable bundle; missing one
# is fatal.  This avoids the "empty Contents/Frameworks" class of packages.
required_frameworks=(
    QtCore
    QtGui
    QtWidgets
    QtNetwork
    QtSerialPort
    QtMultimedia
    QtPrintSupport
    QtHelp
    QtSql
    QtQml
    QtQuick
    QtLocation
    QtPositioning
)

# Add every Qt framework referenced by the app and by whatever macdeployqt copied.
while IFS= read -r bin; do
    while IFS= read -r fw; do
        [[ -n "$fw" ]] && required_frameworks+=("$fw")
    done < <(qt_refs_for_file "$bin")
done < <(collect_macho_files)

# Copy recursively: each copied Qt framework may reference further Qt frameworks.
seen=""
queue="$(printf '%s\n' "${required_frameworks[@]}" | sed '/^$/d' | sort -u)"
while [[ -n "$queue" ]]; do
    next_queue=""
    while IFS= read -r fw; do
        [[ -n "$fw" ]] || continue
        case " $seen " in *" $fw "*) continue ;; esac
        seen="$seen $fw"
        copy_qt_framework "$fw"
        while IFS= read -r dep; do
            [[ -n "$dep" ]] || continue
            case " $seen " in *" $dep "*) ;; *) next_queue+="$dep"$'\n' ;; esac
        done < <(qt_refs_for_file "$(framework_binary "$fw")")
    done <<< "$queue"
    queue="$(printf '%s\n' "$next_queue" | sed '/^$/d' | sort -u)"
done

fix_qt_install_names() {
    local bin="$1"
    [[ -e "$bin" ]] || return 0
    local prefix="@executable_path/../Frameworks"
    case "$bin" in
        "$FW_DIR"/*) prefix="@loader_path/../../.." ;;
    esac

    # If this file is itself a Qt framework binary, set a portable install id.
    local self_fw=""
    self_fw="$(printf '%s\n' "$bin" | sed -n 's#.*Frameworks/\(Qt[A-Za-z0-9_]*\)\.framework/Versions/A/\1$#\1#p')"
    if [[ -n "$self_fw" ]]; then
        install_name_tool -id "@rpath/${self_fw}.framework/Versions/A/${self_fw}" "$bin" 2>/dev/null || true
    fi

    otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}' | while IFS= read -r ref; do
        local fw
        fw="$(framework_name_from_ref "$ref")"
        [[ -n "$fw" ]] || continue
        local portable_ref="$prefix/${fw}.framework/Versions/A/${fw}"
        if [[ "$ref" != "$portable_ref" ]]; then
            install_name_tool -change "$ref" "$portable_ref" "$bin" 2>/dev/null || true
        fi
    done
}

# Normalize Qt install names for the executable and every copied framework.
collect_macho_files | while IFS= read -r bin; do
    fix_qt_install_names "$bin"
done

# Do not run dylibbundler here.  On GitHub macOS arm64 it can wipe or
# rewrite Contents/Frameworks after macdeployqt/manual Qt framework deployment,
# producing the exact empty-Frameworks package seen in CI.  macdeployqt already
# copies Homebrew dylibs referenced by the app; the validation below catches
# remaining Qt framework problems before upload.

verify_qt_framework_closure() {
    local missing=0
    echo "==> Verifying bundled Qt framework closure"
    if ! find "$FW_DIR" -mindepth 1 -maxdepth 1 -print -quit | grep -q .; then
        echo "ERROR: macOS bundle Frameworks directory is empty: $FW_DIR" >&2
        missing=1
    fi
    while IFS= read -r bin; do
        [[ -e "$bin" ]] || continue
        while IFS= read -r ref; do
            [[ -n "$ref" ]] || continue
            local fw
            fw="$(framework_name_from_ref "$ref")"
            [[ -n "$fw" ]] || continue
            if [[ "$ref" == /* ]]; then
                echo "ERROR: non-portable external Qt framework reference in $bin: $ref" >&2
                missing=1
            fi
            if [[ ! -e "$(framework_binary "$fw")" ]]; then
                echo "ERROR: missing ${fw}.framework binary required by $bin ($ref)" >&2
                missing=1
            fi
        done < <(otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}' | grep 'Qt.*\.framework' || true)
    done < <(collect_macho_files)
    if [[ ! -e "$(framework_binary QtSerialPort)" ]]; then
        echo "ERROR: QtSerialPort.framework is mandatory for this app but is absent from the bundle" >&2
        missing=1
    fi
    if [[ "$missing" -ne 0 ]]; then
        echo "==> Frameworks directory diagnostic:" >&2
        find "$FW_DIR" -maxdepth 4 -print >&2 || true
        exit 1
    fi
}
verify_qt_framework_closure

cat > "$APP_PATH/Contents/MacOS/BUILD_INFO.txt" <<TXT
MadModem macOS package
======================
Version: $VERSION
Architecture: $STAMP_ARCH
Bundle: $APP_NAME.app
Generated by: scripts/package_macos.sh
Qt prefix: $QT_PREFIX
Frameworks bundled:
$(find "$FW_DIR" -maxdepth 1 -name 'Qt*.framework' -print | sed 's#^.*/##' | sort)
TXT

if [[ "${MADMODEM_ADHOC_SIGN:-on}" == "on" ]]; then
    # Ad-hoc signing is not Apple notarization. It just makes the unsigned CI
    # bundle internally consistent after macdeployqt has copied frameworks.
    codesign --force --deep --sign - "$APP_PATH" || true
fi

rm -f "$ZIP_PATH" "$DMG_PATH"
ditto -c -k --sequesterRsrc --keepParent "$APP_PATH" "$ZIP_PATH"
hdiutil create -volname "$APP_NAME" -srcfolder "$APP_PATH" -ov -format UDZO "$DMG_PATH"

# Validate the ZIP content that GitHub uploads.  A package with an empty
# Frameworks directory must never reach a release again.
if ! unzip -l "$ZIP_PATH" | grep -q 'MadModem.app/Contents/Frameworks/QtSerialPort.framework/Versions/A/QtSerialPort'; then
    echo "ERROR: packaged ZIP does not contain QtSerialPort.framework binary" >&2
    unzip -l "$ZIP_PATH" | grep 'MadModem.app/Contents/Frameworks' >&2 || true
    exit 1
fi

printf 'Packaged ZIP: %s\n' "$ZIP_PATH"
printf 'Packaged DMG: %s\n' "$DMG_PATH"
