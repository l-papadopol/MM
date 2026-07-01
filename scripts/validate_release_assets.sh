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
      *Windows*x86_64*legacy-standalone.zip|*Windows*x86_64*avx2-standalone.zip|*Linux*x86_64.tar.gz|*macOS*unsigned.zip|*macOS*unsigned.dmg) ;;
      *)
        echo "ERROR: unexpected release asset name/format: $base" >&2
        bad=1
        ;;
    esac

    case "$base" in
      *0.5.76p*|*0.5.76q*|*0.5.76r*|*0.5.76s*|*0.5.76t*|*0.5.76u*|*0.5.76v*|*0.5.76x*|*0.5.76y*)
        echo "ERROR: stale/quarantined asset detected: $base" >&2
        bad=1
        ;;
    esac

    if [[ "$base" == *Windows*x86_64*standalone.zip ]]; then
        if ! unzip -l "$asset" | grep -q 'MadModem.exe'; then
            echo "ERROR: Windows standalone ZIP missing MadModem.exe: $base" >&2
            bad=1
        fi
        if ! unzip -l "$asset" | grep -q 'runtime-dll-report.txt'; then
            echo "ERROR: Windows standalone ZIP missing runtime-dll-report.txt: $base" >&2
            bad=1
        fi
        if ! unzip -l "$asset" | grep -Eiq 'Qt5Core\.dll|Qt6Core\.dll'; then
            echo "ERROR: Windows standalone ZIP does not contain Qt runtime DLLs: $base" >&2
            bad=1
        fi
        for required_pattern in \
            'libgomp-1\.dll' \
            'libdouble-conversion.*\.dll' \
            'libicuin.*\.dll' \
            'libicuuc.*\.dll' \
            'libpcre2-16-0\.dll' \
            'libzstd.*\.dll' \
            'libharfbuzz.*\.dll' \
            'libmd4c.*\.dll' \
            'libpng16-16\.dll'; do
            if ! unzip -l "$asset" | grep -Eiq "$required_pattern"; then
                echo "ERROR: Windows standalone ZIP missing required runtime DLL pattern: $required_pattern in $base" >&2
                bad=1
            fi
        done
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
