#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MadModem"
BUILD_TYPE="${BUILD_TYPE:-Release}"
ARCHS="${MADMODEM_OSX_ARCHITECTURES:-$(uname -m)}"
BUILD_DIR="${MADMODEM_MACOS_BUILD_DIR:-$ROOT_DIR/build-macos-$ARCHS}"
DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
MIND_OPENMP="${MADMODEM_MIND_OPENMP:-OFF}"
Q65_FULL="${MADMODEM_Q65_FULL:-OFF}"

# macOS runners use a case-insensitive filesystem.  A stale root VERSION/version
# file shadows the C++17 standard <version> header when any target has the
# project root in its include path.  v0.5.76h replaced VERSION with
# MADMODEM_VERSION.txt, but repositories updated by copy-over may still keep
# the old tracked file.  Remove it defensively in CI/local builds.
rm -f "$ROOT_DIR/VERSION" "$ROOT_DIR/version" 2>/dev/null || true

# Stop immediately on known source-level portability traps instead of burning
# CI minutes and surfacing one compiler error at a time.
bash "$ROOT_DIR/scripts/check_macos_portability.sh"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: macOS build must run on macOS/Darwin, for example GitHub Actions macos-15." >&2
    exit 1
fi

if ! command -v brew >/dev/null 2>&1; then
    echo "ERROR: Homebrew not found. Install Homebrew or use the GitHub Actions workflow." >&2
    exit 1
fi

QT_PREFIX="${QT_PREFIX:-$(brew --prefix qt 2>/dev/null || true)}"
HAMLIB_PREFIX="${HAMLIB_PREFIX:-$(brew --prefix hamlib 2>/dev/null || true)}"

if [[ -z "$QT_PREFIX" || ! -d "$QT_PREFIX" ]]; then
    echo "ERROR: Qt not found. Run: brew install qt qtlocation" >&2
    exit 1
fi
if [[ -z "$HAMLIB_PREFIX" || ! -d "$HAMLIB_PREFIX" ]]; then
    echo "ERROR: Hamlib not found. Run: brew install hamlib" >&2
    exit 1
fi

CMAKE_PREFIX_PATH_VALUE="$QT_PREFIX"
for formula in qtlocation qtmultimedia qtserialport qtdeclarative; do
    prefix="$(brew --prefix "$formula" 2>/dev/null || true)"
    if [[ -n "$prefix" && -d "$prefix" ]]; then
        CMAKE_PREFIX_PATH_VALUE="$CMAKE_PREFIX_PATH_VALUE;$prefix"
    fi
done

export PATH="$QT_PREFIX/bin:$PATH"
export PKG_CONFIG_PATH="$HAMLIB_PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH_VALUE" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DHAMLIB_ROOT="$HAMLIB_PREFIX" \
    -DMADMODEM_REQUIRE_HAMLIB=ON \
    -DMADMODEM_MACOS_BUNDLE=ON \
    -DMADMODEM_MIND_OPENMP="$MIND_OPENMP" \
    -DMADMODEM_ENABLE_Q65_FULL_MSHV_DECODER="$Q65_FULL" \
    "$@"

cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "${JOBS:-$(sysctl -n hw.ncpu)}"

echo "Built: $BUILD_DIR/$APP_NAME.app"
