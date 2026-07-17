#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MadModem"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
BUILD_DIR="${MADMODEM_LINUX_BUILD_DIR:-$ROOT_DIR/build-linux-github}"
INSTALL_DIR="${MADMODEM_LINUX_INSTALL_DIR:-$ROOT_DIR/dist/linux}"
HAMLIB_PREFIX="${HAMLIB_LINUX_PREFIX:-$ROOT_DIR/third_party/hamlib_lgpl/install-linux-x86_64}"
Q65_FULL="${MADMODEM_Q65_FULL:-OFF}"
FFTW_ROOT="${MADMODEM_FFTW3_ROOT:-}"

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "ERROR: Linux build script must run on Linux, for example GitHub Actions ubuntu-24.04." >&2
    exit 1
fi

cd "$ROOT_DIR"
# Remove stale legacy VERSION/version files from copy-over repository updates.
# On case-insensitive filesystems they can shadow the C++17 <version> header.
rm -f "$ROOT_DIR/VERSION" "$ROOT_DIR/version" 2>/dev/null || true
find "$ROOT_DIR" -type f -name "*.sh" -exec chmod u+rx,go+rx {} + 2>/dev/null || true

if [[ ! -d "$ROOT_DIR/third_party/hamlib_lgpl/source" ]]; then
    echo "ERROR: bundled Hamlib source not found: third_party/hamlib_lgpl/source" >&2
    exit 1
fi
if ! pkg-config --exists fftw3 2>/dev/null; then
    echo "ERROR: FFTW3 development files not found. Install libfftw3-dev." >&2
    exit 1
fi

printf '==> Building bundled Hamlib for Linux\n'
HAMLIB_PREFIX="$HAMLIB_PREFIX" \
HAMLIB_STATIC=on HAMLIB_SHARED=off JOBS="$JOBS" \
    bash "$ROOT_DIR/third_party/hamlib_lgpl/build_hamlib.sh"

printf '\n==> Configuring %s for Linux GitHub package\n' "$APP_NAME"
rm -rf "$INSTALL_DIR"
FFTW_CMAKE_ARGS=()
if [[ -n "$FFTW_ROOT" ]]; then
    FFTW_CMAKE_ARGS+=("-DMADMODEM_FFTW3_ROOT=$FFTW_ROOT")
fi
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DHAMLIB_ROOT="$HAMLIB_PREFIX" \
    -DMADMODEM_REQUIRE_HAMLIB=ON \
    -DMADMODEM_AUTOBUILD_HAMLIB=OFF \
    -DMADMODEM_ENABLE_Q65_FULL_MSHV_DECODER="$Q65_FULL" \
    "${FFTW_CMAKE_ARGS[@]}" \
    "$@"

cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$JOBS"
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"

if [[ ! -x "$INSTALL_DIR/bin/$APP_NAME" ]]; then
    echo "ERROR: expected Linux executable was not installed: $INSTALL_DIR/bin/$APP_NAME" >&2
    exit 1
fi

printf '\nLinux build complete:\n  %s\n' "$INSTALL_DIR/bin/$APP_NAME"
