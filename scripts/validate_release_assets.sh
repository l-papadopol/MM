#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ASSET_DIR="${1:-$ROOT_DIR/release-assets}"
VERSION="$(tr -d '\r\n' < "$ROOT_DIR/MADMODEM_VERSION.txt")"

if [[ ! -d "$ASSET_DIR" ]]; then
    echo "ERROR: release asset directory not found: $ASSET_DIR" >&2
    exit 1
fi

bad=0
count=0
printf 'Expected package version: %s\n' "$VERSION"
find "$ASSET_DIR" -type f -print -exec ls -lh {} \;

while IFS= read -r asset; do
    ((count++)) || true
    base="$(basename "$asset")"

    case "$base" in
      *"$VERSION"*) ;;
      *)
        echo "ERROR: release asset does not contain current version $VERSION: $base" >&2
        bad=1
        ;;
    esac

    case "$base" in
      *Windows*x86_64*legacy-static.zip|*Windows*x86_64*avx2-static.zip|*Linux*x86_64.tar.gz|*macOS*unsigned.zip|*macOS*unsigned.dmg) ;;
      *)
        echo "ERROR: unexpected release asset name/format: $base" >&2
        bad=1
        ;;
    esac

    case "$base" in
      *0.5.76p*|*0.5.76q*|*0.5.76r*|*0.5.76s*|*0.5.76t*|*MSYS2*|*msys2*)
        echo "ERROR: stale/quarantined asset detected: $base" >&2
        bad=1
        ;;
    esac

    if [[ "$base" == *Windows*x86_64*static.zip ]]; then
        if unzip -l "$asset" | grep -Eiq '\.(dll)$'; then
            echo "ERROR: Windows MXE static package contains DLL files: $base" >&2
            unzip -l "$asset" | grep -Ei '\.(dll)$' >&2 || true
            bad=1
        fi
        if unzip -l "$asset" | grep -Eiq 'Qt5.*\.dll|Qt6.*\.dll|lib(gomp|gcc|stdc\+\+|winpthread|icu|pcre|zstd|png|harfbuzz|double-conversion|md4c).*\.dll'; then
            echo "ERROR: Windows package looks like old MSYS2/dynamic package: $base" >&2
            bad=1
        fi
    fi

    if [[ "$base" == *macOS*unsigned.zip ]]; then
        if ! unzip -l "$asset" | grep -q 'MadModem.app/Contents/Frameworks/QtSerialPort.framework/Versions/A/QtSerialPort'; then
            echo "ERROR: macOS ZIP missing QtSerialPort.framework binary: $base" >&2
            unzip -l "$asset" | grep 'MadModem.app/Contents/Frameworks' >&2 || true
            bad=1
        fi
        if ! unzip -l "$asset" | grep -q 'MadModem.app/Contents/MacOS/BUILD_INFO.txt'; then
            echo "ERROR: macOS ZIP missing BUILD_INFO.txt: $base" >&2
            bad=1
        fi
    fi

done < <(find "$ASSET_DIR" -type f | sort)

if [[ "$count" -eq 0 ]]; then
    echo "ERROR: no release assets found in $ASSET_DIR" >&2
    exit 1
fi
if [[ "$bad" -ne 0 ]]; then
    exit 1
fi

{
    echo "MadModem ${VERSION} GitHub release manifest"
    echo "Tag: ${GITHUB_REF_NAME:-manual}"
    echo "Commit: ${GITHUB_SHA:-manual}"
    echo
    find "$ASSET_DIR" -type f -printf '%f\n' | sort
} > "$ASSET_DIR/MadModem-${VERSION}-RELEASE-MANIFEST.txt"

echo "Release asset validation OK."
