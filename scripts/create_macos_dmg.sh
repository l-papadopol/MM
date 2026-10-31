#!/usr/bin/env bash
set -euo pipefail
app="${1:?app bundle required}"
output="${2:?DMG output path required}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/volume"
# Use a private, stable source directory instead of the active build bundle.
ditto "$app" "$work/volume/$(basename "$app")"
ln -s /Applications "$work/volume/Applications"
# Ask Spotlight not to index the temporary image contents.
touch "$work/volume/.metadata_never_index"
for attempt in 1 2 3; do
    echo "==> Creating DMG (attempt $attempt/3)"
    rm -f "$work/package.dmg"
    if hdiutil create -volname MadModem -srcfolder "$work/volume" -fs HFS+ \
        -ov -format UDZO "$work/package.dmg" > "$work/hdiutil.log" 2>&1; then
        cat "$work/hdiutil.log"
        hdiutil verify "$work/package.dmg"
        mv -f "$work/package.dmg" "$output"
        exit 0
    fi
    cat "$work/hdiutil.log" >&2
    if [[ "$attempt" -eq 3 ]] || ! grep -qi 'Resource busy' "$work/hdiutil.log"; then
        echo 'ERROR: DMG creation failed; no unverified image will be published' >&2
        exit 1
    fi
    sleep "$((attempt * 3))"
done
