#!/usr/bin/env bash
set -euo pipefail

APP_NAME="${APP_NAME:-MadModem}"
ROOT_DIR="${MADMODEM_ROOT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
PACKAGE_DIR="${MADMODEM_PACKAGE_DIR:-$ROOT_DIR/MM}"
PACKAGE_ZIP="${MADMODEM_PACKAGE_ZIP:-$ROOT_DIR/mm.zip}"

rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

copied_exe=0

if [[ -f "$ROOT_DIR/dist/linux/bin/$APP_NAME" ]]; then
    cp -f "$ROOT_DIR/dist/linux/bin/$APP_NAME" "$PACKAGE_DIR/${APP_NAME}-linux"
    chmod +x "$PACKAGE_DIR/${APP_NAME}-linux" || true
    copied_exe=$((copied_exe + 1))
else
    echo "WARNING: Linux executable not found: dist/linux/bin/$APP_NAME" >&2
fi

if [[ -f "$ROOT_DIR/dist/windows/$APP_NAME.exe" ]]; then
    cp -f "$ROOT_DIR/dist/windows/$APP_NAME.exe" "$PACKAGE_DIR/$APP_NAME.exe"
    copied_exe=$((copied_exe + 1))
elif [[ -f "$ROOT_DIR/build-win64-static/$APP_NAME.exe" ]]; then
    cp -f "$ROOT_DIR/build-win64-static/$APP_NAME.exe" "$PACKAGE_DIR/$APP_NAME.exe"
    copied_exe=$((copied_exe + 1))
else
    echo "WARNING: Windows executable not found: dist/windows/$APP_NAME.exe" >&2
fi

if [[ "$copied_exe" -eq 0 ]]; then
    echo "ERROR: no executable found to package. Build Linux and/or Windows first." >&2
    exit 1
fi

# Copy the single user-facing README plus legal/support documents.
# Version-specific README_vXX files were removed from the source tree in v1.69
# to avoid shipping dozens of duplicate readme files inside MM.zip.
shopt -s nullglob
for f in \
    "$ROOT_DIR"/README.md \
    "$ROOT_DIR"/CHANGELOG* \
    "$ROOT_DIR"/LICENSE* \
    "$ROOT_DIR"/LICENCE* \
    "$ROOT_DIR"/COPYING* \
    "$ROOT_DIR"/AUTHORS* \
    "$ROOT_DIR"/THIRD_PARTY_NOTICES* \
    "$ROOT_DIR"/TRANSLATION_AUDIT.md \
    "$ROOT_DIR"/compare_ft8_wsjtx_madmodem.sh \
    "$ROOT_DIR"/cty.csv; do
    if [[ -f "$f" ]]; then
        cp -f "$f" "$PACKAGE_DIR/"
        [[ "$(basename "$f")" == "compare_ft8_wsjtx_madmodem.sh" ]] && chmod +x "$PACKAGE_DIR/compare_ft8_wsjtx_madmodem.sh" || true
    fi
done

if [[ -f "$ROOT_DIR/third_party/ggmorse_mit/LICENSE" ]]; then
    mkdir -p "$PACKAGE_DIR/licenses"
    cp -f "$ROOT_DIR/third_party/ggmorse_mit/LICENSE" "$PACKAGE_DIR/licenses/LICENSE-ggmorse-MIT.txt"
fi

# Bundled FT8 Auto test WAV files.  Keep them in the binary distribution so the
# FT panel Auto test button works from installed packages, not only from source.
for wav_dir in \
    "$ROOT_DIR/dist/linux/bin/tests/wav" \
    "$ROOT_DIR/dist/windows/tests/wav" \
    "$ROOT_DIR/build-linux/tests/wav" \
    "$ROOT_DIR/build-win64-static/tests/wav" \
    "$ROOT_DIR/tests/wav"; do
    if [[ -d "$wav_dir" ]]; then
        mkdir -p "$PACKAGE_DIR/tests/wav"
        cp -f "$wav_dir"/*.wav "$PACKAGE_DIR/tests/wav/" 2>/dev/null || true
        [[ -f "$wav_dir/README.md" ]] && cp -f "$wav_dir/README.md" "$PACKAGE_DIR/tests/wav/README.md"
        break
    fi
done
# Ship the localized Qt/HTML help source tree and generated per-language .qch files.
# The runtime help browser looks for MM_en.qch, MM_it.qch, ... and falls back
# to the embedded/resources HTML pages if a .qch is not present.
if [[ -d "$ROOT_DIR/docs/help" ]]; then
    mkdir -p "$PACKAGE_DIR/help"
    cp -f "$ROOT_DIR/docs/help"/*.css "$PACKAGE_DIR/help/" 2>/dev/null || true
    cp -f "$ROOT_DIR/docs/help"/*.qhp "$PACKAGE_DIR/help/" 2>/dev/null || true
    cp -f "$ROOT_DIR/docs/help"/*.qhcp "$PACKAGE_DIR/help/" 2>/dev/null || true
    for lang_dir in "$ROOT_DIR/docs/help"/{en,it,fr,de,no,cs}; do
        if [[ -d "$lang_dir" ]]; then
            mkdir -p "$PACKAGE_DIR/help/$(basename "$lang_dir")"
            cp -f "$lang_dir"/*.html "$PACKAGE_DIR/help/$(basename "$lang_dir")/" 2>/dev/null || true
        fi
    done
fi
for lang in en it fr de no cs; do
    for qch in \
        "$ROOT_DIR/build-linux/docs/help/MM_${lang}.qch" \
        "$ROOT_DIR/build-win64-static/docs/help/MM_${lang}.qch" \
        "$ROOT_DIR/docs/help/MM_${lang}.qch"; do
        if [[ -f "$qch" ]]; then
            mkdir -p "$PACKAGE_DIR/help"
            cp -f "$qch" "$PACKAGE_DIR/help/MM_${lang}.qch"
            break
        fi
    done
done
shopt -u nullglob

rm -f "$PACKAGE_ZIP"
if command -v zip >/dev/null 2>&1; then
    (cd "$(dirname "$PACKAGE_DIR")" && zip -rq "$PACKAGE_ZIP" "$(basename "$PACKAGE_DIR")")
else
    python3 - "$PACKAGE_DIR" "$PACKAGE_ZIP" <<'PY'
import sys, zipfile
from pathlib import Path
src = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as zf:
    for p in sorted(src.rglob('*')):
        if p.is_file():
            zf.write(p, p.relative_to(src.parent))
PY
fi

echo "Packaged: $PACKAGE_DIR"
echo "Zip:      $PACKAGE_ZIP"
