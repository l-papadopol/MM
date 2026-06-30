#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MadModem"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
HAMLIB_PREFIX="${HAMLIB_WIN_PREFIX:-$ROOT_DIR/third_party/hamlib_lgpl/install-msys2-mingw64}"
MIND_OPENMP="${MADMODEM_MIND_OPENMP:-ON}"
Q65_FULL="${MADMODEM_Q65_FULL:-OFF}"
BUILD_LEGACY="${MADMODEM_WINDOWS_BUILD_LEGACY:-on}"
BUILD_AVX2="${MADMODEM_WINDOWS_BUILD_AVX2:-on}"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) ;;
    *) echo "ERROR: Windows/MSYS2 build script must run inside an MSYS2 MinGW shell." >&2; exit 1 ;;
esac

cd "$ROOT_DIR"
find "$ROOT_DIR" -type f -name "*.sh" -exec chmod u+rx,go+rx {} + 2>/dev/null || true

if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    echo "ERROR: cmake/ninja not found. Install mingw-w64-x86_64-cmake and mingw-w64-x86_64-ninja." >&2
    exit 1
fi
if ! command -v windeployqt >/dev/null 2>&1; then
    echo "ERROR: windeployqt not found. Install mingw-w64-x86_64-qt5-tools." >&2
    exit 1
fi

printf '==> Building bundled Hamlib for MSYS2/MinGW64\n'
HAMLIB_PREFIX="$HAMLIB_PREFIX" \
HAMLIB_STATIC=on HAMLIB_SHARED=off HAMLIB_AUTORECONF=always JOBS="$JOBS" \
CC="${CC:-gcc}" CXX="${CXX:-g++}" AR="${AR:-ar}" RANLIB="${RANLIB:-ranlib}" \
STRIP="${STRIP:-strip}" WINDRES="${WINDRES:-windres}" \
    bash "$ROOT_DIR/third_party/hamlib_lgpl/build_hamlib.sh"

export PKG_CONFIG_PATH="$HAMLIB_PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

build_variant() {
    local variant="$1"
    local avx2="$2"
    local build_dir="$ROOT_DIR/build-windows-msys2-$variant"

    printf '\n==> Configuring %s Windows %s package (AVX2=%s)\n' "$APP_NAME" "$variant" "$avx2"
    cmake -S "$ROOT_DIR" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_PREFIX_PATH="${MINGW_PREFIX:-/mingw64}" \
        -DHAMLIB_ROOT="$HAMLIB_PREFIX" \
        -DMADMODEM_REQUIRE_HAMLIB=ON \
        -DMADMODEM_AUTOBUILD_HAMLIB=OFF \
        -DMADMODEM_MIND_OPENMP="$MIND_OPENMP" \
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

printf '\nWindows/MSYS2 build complete.\n'
