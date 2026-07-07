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
    local app_dir="${2:-$1}"
    mkdir -p "$package_dir" "$app_dir"

    # Human-facing release documents stay in the package root so the user is not
    # greeted by a wall of DLLs. Runtime/data files that the executable resolves
    # relative to applicationDirPath() live beside MadModem.exe inside bin/.
    for f in README.md RELEASE_NOTES.md CHANGELOG.md LICENSE.md COPYING AUTHORS.md THIRD_PARTY_NOTICES.md TRANSLATION_AUDIT.md; do
        [[ -f "$ROOT_DIR/$f" ]] && cp -f "$ROOT_DIR/$f" "$package_dir/"
    done
    [[ -f "$ROOT_DIR/cty.csv" ]] && cp -f "$ROOT_DIR/cty.csv" "$app_dir/"

    if [[ -d "$ROOT_DIR/docs/images" ]]; then
        mkdir -p "$package_dir/docs/images"
        cp -f "$ROOT_DIR/docs/images"/* "$package_dir/docs/images/" 2>/dev/null || true
    elif [[ -d "$ROOT_DIR/docs" ]]; then
        mkdir -p "$package_dir/docs/images"
        cat > "$package_dir/docs/images/README.md" <<TXT
Place MadModem screenshots here for the GitHub README gallery.
TXT
    fi

    if [[ -d "$ROOT_DIR/tests/wav" ]]; then
        mkdir -p "$app_dir/tests/wav"
        cp -f "$ROOT_DIR/tests/wav"/*.wav "$app_dir/tests/wav/" 2>/dev/null || true
        [[ -f "$ROOT_DIR/tests/wav/README.md" ]] && cp -f "$ROOT_DIR/tests/wav/README.md" "$app_dir/tests/wav/README.md"
    fi

    if [[ -d "$ROOT_DIR/docs/help" ]]; then
        mkdir -p "$app_dir/help"
        cp -f "$ROOT_DIR/docs/help"/*.css "$app_dir/help/" 2>/dev/null || true
        cp -f "$ROOT_DIR/docs/help"/*.qhp "$app_dir/help/" 2>/dev/null || true
        cp -f "$ROOT_DIR/docs/help"/*.qhcp "$app_dir/help/" 2>/dev/null || true
        for lang_dir in "$ROOT_DIR/docs/help"/{en,it,fr,de,no,cs}; do
            if [[ -d "$lang_dir" ]]; then
                mkdir -p "$app_dir/help/$(basename "$lang_dir")"
                cp -f "$lang_dir"/*.html "$app_dir/help/$(basename "$lang_dir")/" 2>/dev/null || true
            fi
        done
    fi

    cat > "$package_dir/RUN_WINDOWS.txt" <<TXT
MadModem Windows package
========================

Run MadModem from the shortcut in this folder:

  MadModem.lnk

The executable and all runtime DLLs are stored in bin/ to keep the package root
readable. Do not move MadModem.exe out of bin/: Qt and MinGW runtime DLLs must
remain beside the executable.

This CI package is built with MSYS2/MinGW64 and deployed with windeployqt.
It is standalone/portable and unsigned. Windows SmartScreen may warn the first
time because the binary is not Authenticode-signed.
TXT
}

create_windows_shortcut() {
    local package_dir="$1"
    local shortcut="$package_dir/MadModem.lnk"
    local app_dir="$package_dir/bin"
    local exe="$app_dir/MadModem.exe"

    if ! command -v powershell.exe >/dev/null 2>&1; then
        echo "ERROR: powershell.exe not found; cannot create MadModem.lnk launcher." >&2
        exit 1
    fi
    if ! command -v cygpath >/dev/null 2>&1; then
        echo "ERROR: cygpath not found; cannot create MadModem.lnk launcher." >&2
        exit 1
    fi

    local ps_shortcut ps_exe ps_work
    ps_shortcut="$(cygpath -aw "$shortcut")"
    ps_exe="$(cygpath -aw "$exe")"
    ps_work="$(cygpath -aw "$app_dir")"

    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
        "\$shell = New-Object -ComObject WScript.Shell; \$lnk = \$shell.CreateShortcut('${ps_shortcut}'); \$lnk.TargetPath = '${ps_exe}'; \$lnk.WorkingDirectory = '${ps_work}'; \$lnk.IconLocation = '${ps_exe},0'; \$lnk.Description = 'MadModem - All-in-one digital modem for ham radio'; \$lnk.Save()" >/dev/null

    if [[ ! -f "$shortcut" ]]; then
        echo "ERROR: MadModem.lnk was not created." >&2
        exit 1
    fi
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
        API-MS-WIN-*.DLL|EXT-MS-*.DLL|KERNEL32.DLL|KERNELBASE.DLL|USER32.DLL|GDI32.DLL|ADVAPI32.DLL|COMDLG32.DLL|COMCTL32.DLL|SHELL32.DLL|OLE32.DLL|OLEAUT32.DLL|WS2_32.DLL|WSOCK32.DLL|WINMM.DLL|IMM32.DLL|VERSION.DLL|SETUPAPI.DLL|SHLWAPI.DLL|SHCORE.DLL|RPCRT4.DLL|CRYPT32.DLL|CRYPTSP.DLL|IPHLPAPI.DLL|DNSAPI.DLL|NETAPI32.DLL|WTSAPI32.DLL|UXTHEME.DLL|DWMAPI.DLL|MPR.DLL|CFGMGR32.DLL|EVR.DLL|BCRYPT.DLL|BCRYPTPRIMITIVES.DLL|MSVCRT.DLL|SECHOST.DLL|NTDLL.DLL|WINSPOOL.DRV|AUTHZ.DLL|AVRT.DLL|D3D9.DLL|D3D11.DLL|D3D12.DLL|DWRITE.DLL|DXGI.DLL|DXVA2.DLL|GDIPLUS.DLL|MF.DLL|MFPLAT.DLL|MFREADWRITE.DLL|MSIMG32.DLL|NCRYPT.DLL|ODBC32.DLL|PDH.DLL|PROPSYS.DLL|SECUR32.DLL|USERENV.DLL|USP10.DLL|WINHTTP.DLL|WINTRUST.DLL|WLDAP32.DLL|MSVCP*.DLL|VCRUNTIME*.DLL|UCRTBASE.DLL)
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


# Qt loads OpenSSL at runtime through QSslSocket, so libssl/libcrypto are not
# always visible in the static import table scanned by objdump.  Without these
# DLLs QSslSocket::supportsSsl() is false and the Windows QSO map falls back to
# the offline map even though the normal DLL dependency closure is complete.
bundle_qt_ssl_runtime() {
    local package_dir="$1"
    local mingw_prefix="${MINGW_PREFIX:-/mingw64}"
    local bin_dir="$mingw_prefix/bin"
    local copied_ssl=0
    local copied_crypto=0
    local copied_tls_plugin=0
    local source_path=""
    local base_name=""
    local plugin_dir=""

    echo "==> Bundling Qt/OpenSSL runtime for HTTPS map tiles"

    shopt -s nullglob
    for source_path in \
        "$bin_dir"/libssl*.dll \
        "$bin_dir"/libcrypto*.dll; do
        [[ -f "$source_path" ]] || continue
        base_name="$(basename "$source_path")"
        case "$(printf '%s' "$base_name" | tr '[:lower:]' '[:upper:]')" in
            LIBSSL*.DLL)
                echo "  + Qt SSL DLL: $base_name"
                cp -u "$source_path" "$package_dir/"
                copied_ssl=1
                ;;
            LIBCRYPTO*.DLL)
                echo "  + Qt crypto DLL: $base_name"
                cp -u "$source_path" "$package_dir/"
                copied_crypto=1
                ;;
        esac
    done

    # Qt 5 normally uses the OpenSSL backend built into QtNetwork.  Qt 6 / some
    # distro builds may also ship TLS backend plugins; copy them opportunistically
    # when present.  This is harmless for Qt 5 packages and useful for portability.
    for plugin_dir in \
        "$mingw_prefix/share/qt5/plugins/tls" \
        "$mingw_prefix/lib/qt5/plugins/tls" \
        "$mingw_prefix/share/qt6/plugins/tls" \
        "$mingw_prefix/lib/qt6/plugins/tls"; do
        if [[ -d "$plugin_dir" ]]; then
            mkdir -p "$package_dir/tls"
            for source_path in "$plugin_dir"/*.dll; do
                [[ -f "$source_path" ]] || continue
                echo "  + Qt TLS plugin: $(basename "$source_path")"
                cp -u "$source_path" "$package_dir/tls/"
                copied_tls_plugin=1
            done
        fi
    done
    shopt -u nullglob

    if [[ "$copied_ssl" -eq 0 || "$copied_crypto" -eq 0 ]]; then
        echo "ERROR: OpenSSL runtime DLLs were not found in $bin_dir." >&2
        echo "Qt HTTPS support on Windows requires libssl*.dll and libcrypto*.dll in the standalone package." >&2
        echo "Install the matching MSYS2 MinGW OpenSSL runtime/devel package before packaging." >&2
        exit 1
    fi

    if [[ "$copied_tls_plugin" -eq 0 ]]; then
        echo "  (no separate Qt TLS plugin directory found; normal for Qt 5/OpenSSL builds)"
    fi
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

    {
        echo "MadModem Windows standalone runtime DLL report"
        echo "Package: $(basename "$package_dir")"
        echo
        echo "Bundled DLLs:"
        find "$package_dir" -maxdepth 1 -type f -iname '*.dll' -printf '%f\n' | sort -fu
    } > "$package_dir/runtime-dll-report.txt"

    # These were missing in the broken GitHub package.  Only require them when
    # they are actually part of the dependency graph.
    for wanted in libgomp-1.dll libdouble-conversion.dll libpcre2-16-0.dll libzstd.dll libharfbuzz-0.dll; do
        if collect_binary_dependencies "$package_dir" | grep -Fixq "$wanted"; then
            test -f "$package_dir/$wanted" || { echo "ERROR: expected runtime DLL absent after closure copy: $wanted" >&2; exit 1; }
        fi
    done
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
    mkdir -p "$package_dir/bin"
    cp -f "$exe_path" "$package_dir/bin/$APP_NAME.exe"

    copy_common_payload "$package_dir" "$package_dir/bin"

    # Qt deployment: frameworks/plugins/translations and MinGW compiler runtime.
    "$WINDEPLOYQT_BIN" --release --compiler-runtime --qmldir "$ROOT_DIR/qml" "$package_dir/bin/$APP_NAME.exe" || \
        "$WINDEPLOYQT_BIN" --release --compiler-runtime "$package_dir/bin/$APP_NAME.exe"

    # If qhelpgenerator produced .qch files in this build, include them.
    for lang in en it fr de no cs; do
        qch="$build_dir/docs/help/MM_${lang}.qch"
        if [[ -f "$qch" ]]; then
            mkdir -p "$package_dir/bin/help"
            cp -f "$qch" "$package_dir/bin/help/MM_${lang}.qch"
        fi
    done

    # QSslSocket loads OpenSSL dynamically, so it is not enough to scan the
    # import table.  Bundle OpenSSL explicitly before the generic closure/report.
    bundle_qt_ssl_runtime "$package_dir/bin"

    # windeployqt copies Qt DLLs/plugins, but not the complete indirect MSYS2
    # runtime closure (ICU, PCRE2, zstd, harfbuzz, libgomp, etc.).  Run the
    # recursive objdump-based closure before creating the ZIP.
    bundle_runtime_dll_closure "$package_dir/bin"

    create_windows_shortcut "$package_dir"

    (cd "$OUT_DIR" && zip -rq "$zip_path" "$package_name")
    printf 'Windows package complete: %s\n' "$zip_path"
}

if [[ "$BUILD_LEGACY" != "off" ]]; then
    package_variant legacy legacy-standalone
fi
if [[ "$BUILD_AVX2" != "off" ]]; then
    package_variant avx2 avx2-standalone
fi
