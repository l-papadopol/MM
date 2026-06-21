#!/usr/bin/env bash
set -euo pipefail

APP_NAME="MadModem"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

# v1.08: all-in-one builds compile bundled Hamlib before configuring MadModem.
HAMLIB_LINUX_PREFIX="${HAMLIB_LINUX_PREFIX:-$PWD/third_party/hamlib_lgpl/install-linux-x86_64}"
HAMLIB_WIN_PREFIX="${HAMLIB_WIN_PREFIX:-$PWD/third_party/hamlib_lgpl/install-win64-static}"
MADMODEM_BUILD_LINUX="${MADMODEM_BUILD_LINUX:-on}"
MADMODEM_BUILD_WINDOWS="${MADMODEM_BUILD_WINDOWS:-on}"
MADMODEM_CREATE_MM_ZIP="${MADMODEM_CREATE_MM_ZIP:-on}"
MADMODEM_PACKAGE_DIR="${MADMODEM_PACKAGE_DIR:-$PWD/MM}"
MADMODEM_PACKAGE_ZIP="${MADMODEM_PACKAGE_ZIP:-$PWD/mm.zip}"

# Windows CPU policy:
#   on  = build both public executables:
#         dist/windows/MadModem.exe        -> AVX2/FMA build for modern CPUs
#         dist/windows/MadModem-Legacy.exe -> portable x86-64 build for older CPUs
#   off = build only one Windows executable in build-win64-static.
MADMODEM_WINDOWS_DUAL_CPU_BUILDS="${MADMODEM_WINDOWS_DUAL_CPU_BUILDS:-on}"
MADMODEM_WINDOWS_SINGLE_AVX2="${MADMODEM_WINDOWS_SINGLE_AVX2:-off}"

if [[ -z "${MXE_ROOT:-}" ]]; then
    if [[ -d "/home/iz6nnh/mxe" ]]; then
        MXE_ROOT="/home/iz6nnh/mxe"
    else
        MXE_ROOT="$HOME/mxe"
    fi
fi
MXE_TARGET="${MXE_TARGET:-x86_64-w64-mingw32.static}"
MXE_CMAKE="$MXE_ROOT/usr/bin/${MXE_TARGET}-cmake"

if [[ "$MADMODEM_BUILD_LINUX" != "off" ]]; then
    printf '==> Building bundled Hamlib for Linux\n'
    HAMLIB_PREFIX="$HAMLIB_LINUX_PREFIX" \
    HAMLIB_STATIC=on HAMLIB_SHARED=off JOBS="$JOBS" \
        "$PWD/third_party/hamlib_lgpl/build_hamlib.sh"

    printf '\n==> Building %s for Linux with bundled Hamlib (%s)\n' "$APP_NAME" "$BUILD_TYPE"
    cmake -S . -B build-linux \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DHAMLIB_ROOT="$HAMLIB_LINUX_PREFIX" \
        -DMADMODEM_REQUIRE_HAMLIB=ON
    cmake --build build-linux -j"$JOBS"

    printf '\n==> Installing Linux all-in-one tree to dist/linux\n'
    cmake --install build-linux --prefix "$PWD/dist/linux"
fi

if [[ "$MADMODEM_BUILD_WINDOWS" != "off" ]]; then
    printf '\n==> Building bundled Hamlib for Windows via MXE (%s)\n' "$MXE_TARGET"
    if [[ ! -x "$MXE_CMAKE" ]]; then
        echo "ERROR: MXE CMake wrapper not found or not executable: $MXE_CMAKE" >&2
        echo "Set MXE_ROOT=/path/to/mxe or MXE_TARGET=x86_64-w64-mingw32.static if needed." >&2
        exit 1
    fi
    MXE_PKG_CONFIG="$MXE_ROOT/usr/bin/$MXE_TARGET-pkg-config"
    if ! "$MXE_PKG_CONFIG" --exists openssl 2>/dev/null; then
        echo "WARNING: MXE target does not report OpenSSL via pkg-config." >&2
        echo "         Windows online OSM HTTPS tiles require Qt SSL/OpenSSL; MM will fall back to the bundled PNG map if SSL is missing." >&2
        echo "         Suggested rebuild: cd $MXE_ROOT && make MXE_TARGETS=$MXE_TARGET openssl qtbase.clean qtbase qttools" >&2
    else
        echo "MXE OpenSSL: $("$MXE_PKG_CONFIG" --modversion openssl 2>/dev/null || echo unknown)"
    fi
    QT_QCONFIG="$MXE_ROOT/usr/$MXE_TARGET/qt5/mkspecs/qconfig.pri"
    if [[ -f "$QT_QCONFIG" ]]; then
        if grep -Eiq '(^| )[A-Za-z0-9_+-]*ssl|openssl' "$QT_QCONFIG"; then
            echo "MXE Qt qconfig: SSL/OpenSSL markers found."
        else
            echo "WARNING: MXE Qt qconfig does not show obvious SSL/OpenSSL markers: $QT_QCONFIG" >&2
            echo "         If Windows OSM still says SSL handshake failed, force qtbase.clean + qtbase rebuild after openssl." >&2
        fi
    else
        echo "WARNING: Cannot find MXE Qt qconfig.pri at $QT_QCONFIG" >&2
    fi
    MXE_ROOT="$MXE_ROOT" MXE_TARGET="$MXE_TARGET" \
    HAMLIB_PREFIX="$HAMLIB_WIN_PREFIX" JOBS="$JOBS" \
        "$PWD/third_party/hamlib_lgpl/build_hamlib_mxe.sh"

    export PATH="$MXE_ROOT/usr/bin:$PATH"
    mkdir -p "$PWD/dist/windows"

    build_windows_variant() {
        local variant_name="$1"
        local build_dir="$2"
        local avx2_flag="$3"
        local output_exe="$4"

        printf '\n==> Building %s for Windows via MXE: %s (%s, MADMODEM_AVX2_BUILD=%s)\n' \
            "$APP_NAME" "$variant_name" "$BUILD_TYPE" "$avx2_flag"
        "$MXE_CMAKE" -S . -B "$build_dir" \
            -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
            -DHAMLIB_ROOT="$HAMLIB_WIN_PREFIX" \
            -DPKG_CONFIG_EXECUTABLE="$MXE_ROOT/usr/bin/$MXE_TARGET-pkg-config" \
            -DMADMODEM_REQUIRE_HAMLIB=ON \
            -DMADMODEM_AVX2_BUILD="$avx2_flag"
        cmake --build "$build_dir" -j"$JOBS"
        cp -f "$build_dir/${APP_NAME}.exe" "$PWD/dist/windows/$output_exe"
    }

    if [[ "$MADMODEM_WINDOWS_DUAL_CPU_BUILDS" != "off" ]]; then
        # Public Windows package policy:
        #   MadModem.exe        = AVX2/FMA optimized executable for modern CPUs.
        #   MadModem-Legacy.exe = portable executable for older CPUs such as Xeon X5680.
        build_windows_variant "AVX2/FMA" "build-win64-static-avx2" ON "${APP_NAME}.exe"
        build_windows_variant "Legacy portable" "build-win64-static-legacy" OFF "${APP_NAME}-Legacy.exe"
    else
        if [[ "$MADMODEM_WINDOWS_SINGLE_AVX2" == "on" ]]; then
            build_windows_variant "single AVX2/FMA" "build-win64-static" ON "${APP_NAME}.exe"
        else
            build_windows_variant "single portable" "build-win64-static" OFF "${APP_NAME}.exe"
        fi
    fi
fi

if [[ "$MADMODEM_CREATE_MM_ZIP" != "off" ]]; then
    printf '\n==> Creating distributable MM folder and mm.zip\n'
    APP_NAME="$APP_NAME" \
    MADMODEM_ROOT_DIR="$PWD" \
    MADMODEM_PACKAGE_DIR="$MADMODEM_PACKAGE_DIR" \
    MADMODEM_PACKAGE_ZIP="$MADMODEM_PACKAGE_ZIP" \
        "$PWD/tools/package_mm_zip.sh"
fi

printf '\nBuild complete.\n'
[[ "$MADMODEM_BUILD_LINUX" == "off" ]] || printf 'Linux:   dist/linux/bin/%s\n' "$APP_NAME"
if [[ "$MADMODEM_BUILD_WINDOWS" != "off" ]]; then
    if [[ "$MADMODEM_WINDOWS_DUAL_CPU_BUILDS" != "off" ]]; then
        printf 'Windows: dist/windows/%s.exe        (AVX2/FMA)\n' "$APP_NAME"
        printf 'Windows: dist/windows/%s-Legacy.exe (portable legacy CPU)\n' "$APP_NAME"
    else
        printf 'Windows: dist/windows/%s.exe\n' "$APP_NAME"
    fi
fi
[[ "$MADMODEM_CREATE_MM_ZIP" == "off" ]] || printf 'Package: %s\n' "$MADMODEM_PACKAGE_ZIP"
