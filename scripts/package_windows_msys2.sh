#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MadModem"
VERSION="$(cat "$ROOT_DIR/MADMODEM_VERSION.txt" 2>/dev/null || echo dev)"
OUT_DIR="${MADMODEM_PACKAGE_OUT_DIR:-$ROOT_DIR/dist/packages}"
BUILD_LEGACY="${MADMODEM_WINDOWS_BUILD_LEGACY:-on}"
BUILD_AVX2="${MADMODEM_WINDOWS_BUILD_AVX2:-on}"

export PATH="${MINGW_PREFIX:-/mingw64}/bin:/usr/bin:$PATH"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) ;;
    *) echo "ERROR: Windows/MSYS2 packaging script must run inside an MSYS2 MinGW shell." >&2; exit 1 ;;
esac

find_windeployqt() {
    local candidate
    for candidate in \
        "${WINDEPLOYQT:-}" \
        "${MINGW_PREFIX:-/mingw64}/bin/windeployqt.exe" \
        "${MINGW_PREFIX:-/mingw64}/bin/windeployqt-qt5.exe" \
        "${MINGW_PREFIX:-/mingw64}/bin/windeployqt6.exe" \
        windeployqt windeployqt-qt5 windeployqt6; do
        [[ -z "$candidate" ]] && continue
        if command -v "$candidate" >/dev/null 2>&1; then
            command -v "$candidate"
            return 0
        fi
        if [[ -x "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

WINDEPLOYQT_BIN="$(find_windeployqt || true)"
if [[ -z "$WINDEPLOYQT_BIN" ]]; then
    echo "ERROR: windeployqt not found." >&2
    echo "Checked windeployqt, windeployqt-qt5, windeployqt6 and ${MINGW_PREFIX:-/mingw64}/bin." >&2
    echo "Install/verify mingw-w64-x86_64-qt5-tools or the matching Qt tools package." >&2
    echo "Available deploy tools:" >&2
    ls -la "${MINGW_PREFIX:-/mingw64}/bin"/*deploy*qt* 2>/dev/null || true
    exit 1
fi
printf '==> Qt deploy tool: %s\n' "$WINDEPLOYQT_BIN"

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
It is standalone/portable: Qt, MinGW and MSYS2 runtime DLL dependencies are
bundled into this directory. See runtime-dll-report.txt.
It is unsigned. Windows SmartScreen may warn the first time because the binary
is not Authenticode-signed.
TXT
}


# Recursively copy every non-system DLL needed by the EXE, Qt plugins and all
# already bundled DLLs.  windeployqt does not always copy indirect MinGW/MSYS2
# dependencies (ICU, pcre2, zstd, harfbuzz, libgomp, double-conversion, etc.).
# This is adapted from the proven workflow pattern supplied by the user.
is_system_dll() {
    local dll_name="$1"
    local upper_name=""
    upper_name="$(printf '%s' "$dll_name" | tr '[:lower:]' '[:upper:]')"
    case "$upper_name" in
        API-MS-WIN-*.DLL|EXT-MS-*.DLL|KERNEL32.DLL|KERNELBASE.DLL|USER32.DLL|GDI32.DLL|ADVAPI32.DLL|COMDLG32.DLL|COMCTL32.DLL|SHELL32.DLL|OLE32.DLL|OLEAUT32.DLL|WS2_32.DLL|WSOCK32.DLL|WINMM.DLL|IMM32.DLL|VERSION.DLL|SETUPAPI.DLL|SHLWAPI.DLL|SHCORE.DLL|RPCRT4.DLL|CRYPT32.DLL|CRYPTSP.DLL|IPHLPAPI.DLL|DNSAPI.DLL|NETAPI32.DLL|WTSAPI32.DLL|UXTHEME.DLL|DWMAPI.DLL|MPR.DLL|BCRYPT.DLL|BCRYPTPRIMITIVES.DLL|MSVCRT.DLL|SECHOST.DLL|NTDLL.DLL|WINSPOOL.DRV|AUTHZ.DLL|AVRT.DLL|D3D9.DLL|D3D11.DLL|D3D12.DLL|DWRITE.DLL|DXGI.DLL|DXVA2.DLL|GDIPLUS.DLL|MF.DLL|MFPLAT.DLL|MFREADWRITE.DLL|MSIMG32.DLL|NCRYPT.DLL|ODBC32.DLL|PDH.DLL|PROPSYS.DLL|SECUR32.DLL|USERENV.DLL|USP10.DLL|WINHTTP.DLL|WINTRUST.DLL|WLDAP32.DLL|MSVCP*.DLL|VCRUNTIME*.DLL|UCRTBASE.DLL)
            return 0
            ;;
    esac
    return 1
}

is_optional_runtime_dll() {
    local dll_name="$1"
    local upper_name=""
    upper_name="$(printf '%s' "$dll_name" | tr '[:lower:]' '[:upper:]')"
    case "$upper_name" in
        FBCLIENT.DLL|LIBMARIADB.DLL|LIBPQ.DLL)
            return 0
            ;;
    esac
    return 1
}

runtime_search_dirs() {
    printf '%s\n' \
        "${MINGW_PREFIX:-/mingw64}/bin" \
        "${HAMLIB_WIN_PREFIX:-$ROOT_DIR/third_party/hamlib_lgpl/install-msys2-mingw64}/bin" \
        "$ROOT_DIR/third_party/hamlib_lgpl/install-msys2-mingw64/bin"
}

find_dll_in_search_dirs() {
    local dll_name="$1"
    local search_dir=""
    local match=""
    while IFS= read -r search_dir; do
        [[ -n "$search_dir" && -d "$search_dir" ]] || continue
        match="$(find "$search_dir" -maxdepth 1 -type f -iname "$dll_name" | head -n1 || true)"
        if [[ -n "$match" ]]; then
            printf '%s\n' "$match"
            return 0
        fi
    done < <(runtime_search_dirs)
    return 1
}


copy_matching_runtime_glob() {
    local package_dir="$1"
    local pattern="$2"
    local search_dir=""
    local copied=0
    shopt -s nullglob nocaseglob
    while IFS= read -r search_dir; do
        [[ -n "$search_dir" && -d "$search_dir" ]] || continue
        local dll_path
        for dll_path in "$search_dir"/$pattern; do
            [[ -f "$dll_path" ]] || continue
            if ! find "$package_dir" -maxdepth 1 -type f -iname "$(basename "$dll_path")" | grep -q .; then
                echo "  + runtime glob: $(basename "$dll_path")"
                cp -u "$dll_path" "$package_dir/"
                copied=1
            fi
        done
    done < <(runtime_search_dirs)
    shopt -u nullglob nocaseglob
    return "$copied"
}

copy_msys2_runtime_safety_net() {
    local package_dir="$1"
    echo "==> Copying MSYS2/MinGW runtime safety-net DLLs"

    # These are not optional on the GitHub/MSYS2 dynamic Qt5 build.  windeployqt
    # copies Qt DLLs and plugins, but it can leave the MinGW/ICU/pcre/zstd/font
    # stack behind.  Copy by glob so ICU minor updates do not break the package.
    local patterns=(
        'libgcc_s_seh-1.dll'
        'libstdc++-6.dll'
        'libwinpthread-1.dll'
        'libgomp-1.dll'
        'libdouble-conversion*.dll'
        'libicu*.dll'
        'libpcre2-16-0.dll'
        'libpcre2-8-0.dll'
        'libzstd*.dll'
        'libharfbuzz*.dll'
        'libgraphite2*.dll'
        'libmd4c*.dll'
        'libpng16-16.dll'
        'zlib1.dll'
        'libfreetype*.dll'
        'libbrotli*.dll'
        'libbz2*.dll'
        'libglib-2.0-0.dll'
        'libgthread-2.0-0.dll'
        'libintl-8.dll'
        'libiconv-2.dll'
        'libxml2-2.dll'
        'liblzma-5.dll'
        'libjpeg-*.dll'
        'libtiff-*.dll'
        'libwebp*.dll'
        'libsqlite3-0.dll'
        'libssl-*.dll'
        'libcrypto-*.dll'
        'libcurl-*.dll'
        'libssh2-*.dll'
        'libnghttp2-*.dll'
        'libidn2-*.dll'
        'libunistring-*.dll'
    )

    local pattern
    for pattern in "${patterns[@]}"; do
        copy_matching_runtime_glob "$package_dir" "$pattern" || true
    done
}

collect_binary_dependencies() {
    local package_dir="$1"
    find "$package_dir" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0 \
        | while IFS= read -r -d '' binary_path; do
              objdump -p "$binary_path" 2>/dev/null | awk '/DLL Name:/ {print $3}'
          done \
        | sort -fu
}

copy_runtime_dll() {
    local package_dir="$1"
    local dll_name="$2"
    local source_path=""
    [[ -n "$dll_name" ]] || return 1
    if is_system_dll "$dll_name"; then
        return 1
    fi
    if is_optional_runtime_dll "$dll_name"; then
        return 1
    fi
    if find "$package_dir" -type f -iname "$dll_name" | grep -q .; then
        return 1
    fi
    source_path="$(find_dll_in_search_dirs "$dll_name" || true)"
    if [[ -z "$source_path" ]]; then
        return 2
    fi
    echo "  + runtime DLL: $(basename "$source_path")"
    cp -u "$source_path" "$package_dir/"
    return 0
}

bundle_runtime_dll_closure() {
    local package_dir="$1"
    local copied_new=1
    local dll_name=""
    local unresolved_dlls=()

    if ! command -v objdump >/dev/null 2>&1; then
        echo "ERROR: objdump not found; cannot validate Windows DLL dependency closure." >&2
        exit 1
    fi

    copy_msys2_runtime_safety_net "$package_dir"

    echo "==> Bundling recursive Windows runtime DLL closure"
    copied_new=1
    while [[ "$copied_new" -ne 0 ]]; do
        copied_new=0
        while IFS= read -r dll_name; do
            [[ -n "$dll_name" ]] || continue
            if copy_runtime_dll "$package_dir" "$dll_name"; then
                copied_new=1
            fi
        done < <(collect_binary_dependencies "$package_dir")
    done

    while IFS= read -r dll_name; do
        [[ -n "$dll_name" ]] || continue
        if is_system_dll "$dll_name" || is_optional_runtime_dll "$dll_name"; then
            continue
        fi
        if ! find "$package_dir" -type f -iname "$dll_name" | grep -q .; then
            unresolved_dlls+=("$dll_name")
        fi
    done < <(collect_binary_dependencies "$package_dir")

    if [[ "${#unresolved_dlls[@]}" -ne 0 ]]; then
        echo "ERROR: unresolved Windows runtime DLL dependencies detected:" >&2
        printf '  %s\n' "${unresolved_dlls[@]}" >&2
        echo "Search directories were:" >&2
        runtime_search_dirs >&2
        exit 1
    fi

    # Hard validation for the DLL family that broke on real Windows/Wine. If Qt5Core
    # is present, these indirect runtime DLLs must be packaged too.
    if find "$package_dir" -maxdepth 1 -type f -iname 'Qt5Core.dll' | grep -q .; then
        local must_have_globs=(
            'libdouble-conversion*.dll'
            'libicuin*.dll'
            'libicuuc*.dll'
            'libpcre2-16-0.dll'
            'libzstd*.dll'
            'libharfbuzz*.dll'
            'libpng16-16.dll'
        )
        local glob
        for glob in "${must_have_globs[@]}"; do
            if ! find "$package_dir" -maxdepth 1 -type f -iname "$glob" | grep -q .; then
                echo "ERROR: expected Qt/MinGW runtime DLL family missing after packaging: $glob" >&2
                echo "Package root contains:" >&2
                find "$package_dir" -maxdepth 1 -type f -iname '*.dll' -printf '  %f\n' | sort -fu >&2
                exit 1
            fi
        done
    fi

    if collect_binary_dependencies "$package_dir" | grep -Fixq 'libgomp-1.dll'; then
        test -f "$package_dir/libgomp-1.dll" || { echo "ERROR: expected OpenMP runtime DLL absent after packaging: libgomp-1.dll" >&2; exit 1; }
    fi

    {
        echo "MadModem Windows standalone runtime DLL report"
        echo "Package: $(basename "$package_dir")"
        echo
        echo "Bundled root DLLs:"
        find "$package_dir" -maxdepth 1 -type f -iname '*.dll' -printf '%f\n' | sort -fu
        echo
        echo "Resolved dependency names seen by objdump:"
        collect_binary_dependencies "$package_dir"
    } > "$package_dir/runtime-dll-report.txt"
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
    "$WINDEPLOYQT_BIN" --release --compiler-runtime --qmldir "$ROOT_DIR/qml" "$package_dir/$APP_NAME.exe" || \
        "$WINDEPLOYQT_BIN" --release --compiler-runtime "$package_dir/$APP_NAME.exe"

    # If qhelpgenerator produced .qch files in this build, include them.
    for lang in en it fr de no cs; do
        qch="$build_dir/docs/help/MM_${lang}.qch"
        if [[ -f "$qch" ]]; then
            mkdir -p "$package_dir/help"
            cp -f "$qch" "$package_dir/help/MM_${lang}.qch"
        fi
    done

    # windeployqt copies Qt DLLs/plugins but can miss indirect MSYS2 runtime
    # dependencies. Build a complete standalone closure and fail before upload if
    # any required non-system DLL is absent.
    bundle_runtime_dll_closure "$package_dir"

    (cd "$OUT_DIR" && zip -rq "$zip_path" "$package_name")
    printf 'Windows package complete: %s\n' "$zip_path"
}

if [[ "$BUILD_LEGACY" != "off" ]]; then
    package_variant legacy legacy-standalone
fi
if [[ "$BUILD_AVX2" != "off" ]]; then
    package_variant avx2 avx2-standalone
fi
