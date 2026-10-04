#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="$ROOT_DIR/widgets/WaterfallWidget.cpp"

viewport_count="$(grep -c 'glViewport(0, 0,' "$SOURCE")"
dpr_count="$(grep -c 'const qreal dpr = devicePixelRatioF();' "$SOURCE")"
width_count="$(grep -c 'width()) \* dpr' "$SOURCE")"
height_count="$(grep -c 'height()) \* dpr' "$SOURCE")"

if [[ "$viewport_count" -ne 2 ]]; then
    echo "ERROR: expected exactly 2 WaterfallWidget glViewport paths, found $viewport_count" >&2
    exit 1
fi
if [[ "$dpr_count" -ne 2 || "$width_count" -ne 2 || "$height_count" -ne 2 ]]; then
    echo "ERROR: every WaterfallWidget viewport must use logical size multiplied by devicePixelRatioF()" >&2
    exit 1
fi
if grep -Eq 'glViewport\(0, 0, qMax\(1, width\(\)\), qMax\(1, height\(\)\)\)' "$SOURCE"; then
    echo "ERROR: direct logical-pixel glViewport regression detected" >&2
    exit 1
fi

echo "Waterfall HiDPI viewport source audit passed: 2/2 paths use device-pixel dimensions."
