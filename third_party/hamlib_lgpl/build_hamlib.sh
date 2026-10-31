#!/usr/bin/env bash
set -euo pipefail

# Build the bundled Hamlib source tree into a private prefix that MadModem can
# link against.  This script supports both native builds and cross-builds
# (e.g. MXE/MinGW for Windows) via environment variables.
#
# Native Linux example:
#   ./third_party/hamlib_lgpl/build_hamlib.sh
#
# Cross Windows example, normally called by build_hamlib_mxe.sh:
#   HAMLIB_HOST=x86_64-w64-mingw32.static CC=x86_64-w64-mingw32.static-gcc \
#     HAMLIB_PREFIX=$PWD/third_party/hamlib_lgpl/install-win64-static \
#     HAMLIB_SHARED=off HAMLIB_STATIC=on ./third_party/hamlib_lgpl/build_hamlib.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/source"
HOST_TAG="${HAMLIB_HOST:-native}"
HOST_TAG_SAFE="${HOST_TAG//[^A-Za-z0-9_.-]/_}"
BUILD_DIR="${HAMLIB_BUILD_DIR:-$SCRIPT_DIR/build-$HOST_TAG_SAFE}"

if [[ -n "${HAMLIB_PREFIX:-}" ]]; then
    PREFIX="$HAMLIB_PREFIX"
elif [[ -n "${HAMLIB_HOST:-}" ]]; then
    PREFIX="$SCRIPT_DIR/install-$HOST_TAG_SAFE"
else
    PREFIX="$SCRIPT_DIR/install-linux-x86_64"
fi

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
HAMLIB_SHARED="${HAMLIB_SHARED:-off}"
HAMLIB_STATIC="${HAMLIB_STATIC:-on}"

if [[ ! -d "$SRC_DIR" ]]; then
    echo "ERROR: Hamlib source directory not found: $SRC_DIR" >&2
    exit 1
fi

cd "$SRC_DIR"

# Hamlib is bundled from a source snapshot, not always from a pristine release
# tarball.  Some snapshots contain generated autotools files made with a newer
# libtool than the one installed on the build host.  That fails later during
# make with messages like:
#   libtool: Version mismatch error ... LT_INIT comes from libtool 2.5.4
#   You should recreate aclocal.m4 with macros from libtool 2.4.7 ...
# Therefore the safe default is to regenerate the autotools/libtool files with
# the local toolchain before configuring.  Set HAMLIB_AUTORECONF=never only if
# you intentionally want to use a pristine upstream release tarball as-is.
HAMLIB_AUTORECONF="${HAMLIB_AUTORECONF:-always}"
NEED_AUTORECONF=0

case "$HAMLIB_AUTORECONF" in
    always|yes|on|1)
        NEED_AUTORECONF=1
        ;;
    never|no|off|0)
        NEED_AUTORECONF=0
        ;;
    auto)
        if [[ ! -x ./configure ]]; then
            NEED_AUTORECONF=1
        fi
        for aux in ltmain.sh config.guess config.sub ar-lib missing install-sh compile; do
            if [[ ! -f "build-aux/$aux" ]]; then
                NEED_AUTORECONF=1
                break
            fi
        done
        ;;
    *)
        echo "ERROR: invalid HAMLIB_AUTORECONF=$HAMLIB_AUTORECONF (use always, auto, or never)" >&2
        exit 1
        ;;
esac

if [[ "$NEED_AUTORECONF" -eq 1 ]]; then
    echo "==> Regenerating Hamlib autotools/libtool files with local toolchain"
    rm -rf autom4te.cache
    # Remove stale generated files that may contain macros from another libtool
    # version.  libtoolize/autoreconf will recreate them consistently.
    rm -f aclocal.m4 configure libtool
    rm -f build-aux/ltmain.sh build-aux/config.guess build-aux/config.sub           build-aux/ar-lib build-aux/missing build-aux/install-sh build-aux/compile

    # Do not call Hamlib's bootstrap here: it runs a non-forced autoreconf pass
    # and can leave stale macro state in repacked snapshots.  A single forced
    # local pass is both faster and avoids libtool version mismatches.
    libtoolize --force --copy || true
    autoreconf -fiv

    # An out-of-tree build directory created by an earlier broken configure can
    # contain a stale generated libtool script.  Drop it after regeneration.
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

CONFIGURE_ARGS=(
    "--prefix=$PREFIX"
    "--disable-dependency-tracking"
    "--without-readline"
    "--without-indi"
    "--without-libusb"
    "--disable-html-matrix"
    "--disable-winradio"
)

if [[ "$HAMLIB_SHARED" == "on" || "$HAMLIB_SHARED" == "yes" || "$HAMLIB_SHARED" == "1" ]]; then
    CONFIGURE_ARGS+=("--enable-shared")
else
    CONFIGURE_ARGS+=("--disable-shared")
fi

if [[ "$HAMLIB_STATIC" == "off" || "$HAMLIB_STATIC" == "no" || "$HAMLIB_STATIC" == "0" ]]; then
    CONFIGURE_ARGS+=("--disable-static")
else
    CONFIGURE_ARGS+=("--enable-static")
fi

if [[ -n "${HAMLIB_HOST:-}" ]]; then
    CONFIGURE_ARGS+=("--host=$HAMLIB_HOST")
    if command -v gcc >/dev/null 2>&1; then
        CONFIGURE_ARGS+=("--build=$(gcc -dumpmachine)")
    fi
fi

export CFLAGS="${CFLAGS:--O3 -pipe -ffunction-sections -fdata-sections}"
export CXXFLAGS="${CXXFLAGS:--O3 -pipe -ffunction-sections -fdata-sections}"

cat <<MSG
==> Building bundled Hamlib
    source:  $SRC_DIR
    build:   $BUILD_DIR
    prefix:  $PREFIX
    host:    ${HAMLIB_HOST:-native}
    static:  $HAMLIB_STATIC
    shared:  $HAMLIB_SHARED
    jobs:    $JOBS
MSG

"$SRC_DIR/configure" "${CONFIGURE_ARGS[@]}"
make -j"$JOBS"
make install

cat <<MSG

Bundled Hamlib installed to:
  $PREFIX

Use it for MadModem with:
  cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release \
    -DHAMLIB_ROOT="$PREFIX" -DMADMODEM_REQUIRE_HAMLIB=ON

MSG
