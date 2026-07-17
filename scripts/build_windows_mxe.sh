#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MadModem"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
MXE_ROOT="${MXE_ROOT:-/usr/lib/mxe}"
MXE_TARGET="${MXE_TARGET:-x86_64-w64-mingw32.static}"
MXE_BIN="$MXE_ROOT/usr/bin"
MXE_CMAKE="$MXE_BIN/${MXE_TARGET}-cmake"
MXE_PKG_CONFIG="$MXE_BIN/${MXE_TARGET}-pkg-config"
HAMLIB_PREFIX="${HAMLIB_WIN_PREFIX:-$ROOT_DIR/third_party/hamlib_lgpl/install-win64-static}"
Q65_FULL="${MADMODEM_Q65_FULL:-OFF}"
BUILD_LEGACY="${MADMODEM_WINDOWS_BUILD_LEGACY:-on}"
BUILD_AVX2="${MADMODEM_WINDOWS_BUILD_AVX2:-on}"

case "$(uname -s)" in
    Linux*) ;;
    *) echo "ERROR: MXE Windows cross-build must run on Linux." >&2; exit 1 ;;
esac

cd "$ROOT_DIR"
rm -f "$ROOT_DIR/VERSION" "$ROOT_DIR/version" 2>/dev/null || true
find "$ROOT_DIR" -type f -name "*.sh" -exec chmod u+rx,go+rx {} + 2>/dev/null || true

if [[ ! -x "$MXE_CMAKE" ]]; then
    echo "ERROR: MXE CMake wrapper not found: $MXE_CMAKE" >&2
    echo "Set MXE_ROOT=/path/to/mxe and MXE_TARGET=$MXE_TARGET, or install MXE packages on GitHub." >&2
    exit 1
fi
if [[ ! -x "$MXE_BIN/${MXE_TARGET}-g++" ]]; then
    echo "ERROR: MXE compiler not found: $MXE_BIN/${MXE_TARGET}-g++" >&2
    exit 1
fi
if [[ ! -x "$MXE_PKG_CONFIG" ]]; then
    echo "ERROR: MXE pkg-config not found: $MXE_PKG_CONFIG" >&2
    exit 1
fi

export MXE_ROOT MXE_TARGET
export PATH="$MXE_BIN:$PATH"
export PKG_CONFIG_EXECUTABLE="$MXE_PKG_CONFIG"
# Keep target pkg-config isolated from native Ubuntu .pc files.
export PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR:-$MXE_ROOT/usr/$MXE_TARGET/lib/pkgconfig}"
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}"

printf '==> MXE root:   %s\n' "$MXE_ROOT"
printf '==> MXE target: %s\n' "$MXE_TARGET"
printf '==> MXE CMake:  %s\n' "$MXE_CMAKE"
"$MXE_BIN/${MXE_TARGET}-g++" --version | head -1 || true

printf '==> Building bundled Hamlib for Windows/MXE static\n'
MXE_ROOT="$MXE_ROOT" MXE_TARGET="$MXE_TARGET" \
HAMLIB_PREFIX="$HAMLIB_PREFIX" HAMLIB_STATIC=on HAMLIB_SHARED=off HAMLIB_AUTORECONF=always JOBS="$JOBS" \
    bash "$ROOT_DIR/third_party/hamlib_lgpl/build_hamlib_mxe.sh"

build_variant() {
    local variant="$1"
    local avx2="$2"
    local build_dir="$ROOT_DIR/build-windows-mxe-$variant"

    printf '\n==> Configuring %s Windows MXE static %s (AVX2=%s)\n' "$APP_NAME" "$variant" "$avx2"
    "$MXE_CMAKE" -S "$ROOT_DIR" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DHAMLIB_ROOT="$HAMLIB_PREFIX" \
        -DPKG_CONFIG_EXECUTABLE="$MXE_PKG_CONFIG" \
        -DMADMODEM_REQUIRE_HAMLIB=ON \
        -DMADMODEM_AUTOBUILD_HAMLIB=OFF \
        -DMADMODEM_ENABLE_Q65_FULL_MSHV_DECODER="$Q65_FULL" \
        -DMADMODEM_AVX2_BUILD="$avx2"

    cmake --build "$build_dir" --config "$BUILD_TYPE" --parallel "$JOBS"

    if [[ ! -f "$build_dir/$APP_NAME.exe" ]]; then
        echo "ERROR: expected Windows executable not found: $build_dir/$APP_NAME.exe" >&2
        exit 1
    fi
}

if [[ "$BUILD_LEGACY" != "off" ]]; then
    build_variant legacy OFF
fi
if [[ "$BUILD_AVX2" != "off" ]]; then
    build_variant avx2 ON
fi

printf '\nWindows/MXE static build complete.\n'
