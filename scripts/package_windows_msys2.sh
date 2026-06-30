#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MadModem"
VERSION="$(cat "$ROOT_DIR/MADMODEM_VERSION.txt" 2>/dev/null || echo dev)"
OUT_DIR="${MADMODEM_PACKAGE_OUT_DIR:-$ROOT_DIR/dist/packages}"
BUILD_LEGACY="${MADMODEM_WINDOWS_BUILD_LEGACY:-on}"
BUILD_AVX2="${MADMODEM_WINDOWS_BUILD_AVX2:-on}"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) ;;
    *) echo "ERROR: Windows/MSYS2 packaging script must run inside an MSYS2 MinGW shell." >&2; exit 1 ;;
esac

if ! command -v windeployqt >/dev/null 2>&1; then
    echo "ERROR: windeployqt not found. Install mingw-w64-x86_64-qt5-tools." >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

copy_common_payload() {
    local package_dir="$1"
    mkdir -p "$package_dir"

    for f in README.md RELEASE_NOTES.md CHANGELOG.md LICENSE.md COPYING AUTHORS.md THIRD_PARTY_NOTICES.md TRANSLATION_AUDIT.md cty.csv; do
        [[ -f "$ROOT_DIR/$f" ]] && cp -f "$ROOT_DIR/$f" "$package_dir/"
    done

    if [[ -d "$ROOT_DIR/tests/wav" ]]; then
        mkdir -p "$package_dir/tests/wav"
        cp -f "$ROOT_DIR/tests/wav"/*.wav "$package_dir/tests/wav/" 2>/dev/null || true
        [[ -f "$ROOT_DIR/tests/wav/README.md" ]] && cp -f "$ROOT_DIR/tests/wav/README.md" "$package_dir/tests/wav/README.md"
    fi

    if [[ -d "$ROOT_DIR/docs/help" ]]; then
        mkdir -p "$package_dir/help"
        cp -f "$ROOT_DIR/docs/help"/*.css "$package_dir/help/" 2>/dev/null || true
        cp -f "$ROOT_DIR/docs/help"/*.qhp "$package_dir/help/" 2>/dev/null || true
        cp -f "$ROOT_DIR/docs/help"/*.qhcp "$package_dir/help/" 2>/dev/null || true
        for lang_dir in "$ROOT_DIR/docs/help"/{en,it,fr,de,no,cs}; do
            if [[ -d "$lang_dir" ]]; then
                mkdir -p "$package_dir/help/$(basename "$lang_dir")"
                cp -f "$lang_dir"/*.html "$package_dir/help/$(basename "$lang_dir")/" 2>/dev/null || true
            fi
        done
    fi

    cat > "$package_dir/RUN_WINDOWS.txt" <<TXT
MadModem Windows package
========================

Run:
  MadModem.exe

This CI package is built with MSYS2/MinGW64 and deployed with windeployqt.
It is unsigned. Windows SmartScreen may warn the first time because the binary
is not Authenticode-signed.
TXT
}

package_variant() {
    local variant="$1"
    local label="$2"
    local build_dir="$ROOT_DIR/build-windows-msys2-$variant"
    local exe_path="$build_dir/$APP_NAME.exe"
    local package_name="$APP_NAME-$VERSION-Windows-x86_64-$label"
    local package_dir="$OUT_DIR/$package_name"
    local zip_path="$OUT_DIR/$package_name.zip"

    if [[ ! -f "$exe_path" ]]; then
        echo "ERROR: executable not found for $variant: $exe_path" >&2
        exit 1
    fi

    rm -rf "$package_dir" "$zip_path"
    mkdir -p "$package_dir"
    cp -f "$exe_path" "$package_dir/$APP_NAME.exe"

    copy_common_payload "$package_dir"

    # Qt deployment: frameworks/plugins/translations and MinGW compiler runtime.
    windeployqt --release --compiler-runtime --qmldir "$ROOT_DIR/qml" "$package_dir/$APP_NAME.exe" || \
        windeployqt --release --compiler-runtime "$package_dir/$APP_NAME.exe"

    # If qhelpgenerator produced .qch files in this build, include them.
    for lang in en it fr de no cs; do
        qch="$build_dir/docs/help/MM_${lang}.qch"
        if [[ -f "$qch" ]]; then
            mkdir -p "$package_dir/help"
            cp -f "$qch" "$package_dir/help/MM_${lang}.qch"
        fi
    done

    (cd "$OUT_DIR" && zip -rq "$zip_path" "$package_name")
    printf 'Windows package complete: %s\n' "$zip_path"
}

if [[ "$BUILD_LEGACY" != "off" ]]; then
    package_variant legacy legacy
fi
if [[ "$BUILD_AVX2" != "off" ]]; then
    package_variant avx2 avx2
fi
