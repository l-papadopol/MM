#!/usr/bin/env bash
set -euo pipefail

# Cross-build bundled Hamlib for the Windows/MXE target used by MadModem.
# Default target is the static MXE triplet so the final Windows executable can
# link Hamlib in directly instead of carrying a separate Hamlib DLL.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

if [[ -z "${MXE_ROOT:-}" ]]; then
    if [[ -d "/home/iz6nnh/mxe" ]]; then
        MXE_ROOT="/home/iz6nnh/mxe"
    else
        MXE_ROOT="$HOME/mxe"
    fi
fi

MXE_TARGET="${MXE_TARGET:-x86_64-w64-mingw32.static}"
MXE_BIN="$MXE_ROOT/usr/bin"
PREFIX="${HAMLIB_PREFIX:-$SCRIPT_DIR/install-win64-static}"

if [[ ! -x "$MXE_BIN/${MXE_TARGET}-gcc" ]]; then
    echo "ERROR: MXE compiler not found: $MXE_BIN/${MXE_TARGET}-gcc" >&2
    echo "Set MXE_ROOT=/path/to/mxe and MXE_TARGET if needed." >&2
    exit 1
fi

export PATH="$MXE_BIN:$PATH"
export HAMLIB_HOST="$MXE_TARGET"
export HAMLIB_PREFIX="$PREFIX"
export HAMLIB_SHARED="${HAMLIB_SHARED:-off}"
export HAMLIB_STATIC="${HAMLIB_STATIC:-on}"
export CC="${CC:-${MXE_TARGET}-gcc}"
export CXX="${CXX:-${MXE_TARGET}-g++}"
export AR="${AR:-${MXE_TARGET}-ar}"
export RANLIB="${RANLIB:-${MXE_TARGET}-ranlib}"
export STRIP="${STRIP:-${MXE_TARGET}-strip}"
export WINDRES="${WINDRES:-${MXE_TARGET}-windres}"
export PKG_CONFIG="${PKG_CONFIG:-${MXE_TARGET}-pkg-config}"

# Make sure configure/pkg-config resolve MXE dependencies instead of native ones.
export PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR:-$MXE_ROOT/usr/$MXE_TARGET/lib/pkgconfig}"
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}"

"$SCRIPT_DIR/build_hamlib.sh"

cat <<MSG
Windows/MXE Hamlib build complete:
  $PREFIX

Use it for MadModem with:
  $MXE_BIN/${MXE_TARGET}-cmake -S "$PROJECT_ROOT" -B build-win64-static \
    -DCMAKE_BUILD_TYPE=Release -DHAMLIB_ROOT="$PREFIX" \
    -DPKG_CONFIG_EXECUTABLE="$MXE_BIN/${MXE_TARGET}-pkg-config" \
    -DMADMODEM_REQUIRE_HAMLIB=ON
MSG
