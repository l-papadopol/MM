#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(tr -d '\r\n' < "$root/MADMODEM_VERSION.txt")"
arch="${MADMODEM_OSX_ARCHITECTURES:-$(uname -m)}"
app="${MADMODEM_APP_PATH:-$root/build-macos-$arch/MadModem.app}"
out="${MADMODEM_MACOS_DIST_DIR:-$root/dist/macos}"
[[ "$(uname -s)" == Darwin ]]
[[ "$arch" == arm64 || "$arch" == x86_64 ]]
test -x "$app/Contents/MacOS/MadModem"
test -d "$app/Contents/Frameworks/QtCore.framework"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/root/Applications" "$out"
ditto "$app" "$work/root/Applications/MadModem.app"
installed="$work/root/Applications/MadModem.app"
mkdir -p "$installed/Contents/Resources/licenses"
for document in COPYING LICENSE.md AUTHORS.md THIRD_PARTY_NOTICES.md; do
    cp "$root/$document" "$installed/Contents/Resources/licenses/$document"
done
printf 'MadModem managed installation\n' > "$installed/Contents/MacOS/managed-install.flag"
# The marker changes the bundle seal. This is ad-hoc signing, not notarization.
python3 "$root/scripts/fix_macos_runtime.py" "$installed" --sign-only
pkgbuild --analyze --root "$work/root" "$work/components.plist"
python3 - "$work/components.plist" <<'PY'
import plistlib, sys
from pathlib import Path
p = Path(sys.argv[1])
components = plistlib.loads(p.read_bytes())
found = False
for component in components:
    if component.get('RootRelativeBundlePath') == 'Applications/MadModem.app':
        component['BundleIsRelocatable'] = False
        # Explicit same-public-version build updates must replace the bundle.
        component['BundleIsVersionChecked'] = False
        # 'update' skips the entire bundle on a fresh Mac. 'upgrade' installs
        # both fresh and existing copies and removes stale program files.
        # The preinstall script preserves legacy portable data outside the bundle.
        component['BundleOverwriteAction'] = 'upgrade'
        found = True
if not found:
    raise SystemExit('Main application missing from component analysis')
p.write_bytes(plistlib.dumps(components))
PY
mkdir -p "$work/resources"
cat "$root/packaging/COPYRIGHT.txt" "$root/COPYING" > "$work/resources/INSTALLER_LICENSE.txt"
mkdir -p "$work/scripts"
cp "$root/packaging/macos/scripts/preinstall" "$work/scripts/preinstall"
# Uploading sources through GitHub's web UI may lose executable file modes.
chmod 755 "$work/scripts/preinstall"
pkgbuild --root "$work/root" --component-plist "$work/components.plist" \
    --scripts "$work/scripts" \
    --identifier org.iz6nnh.madmodem --version "$version" --install-location / \
    "$work/MadModem-component.pkg"
cat > "$work/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>MadModem $version</title>
  <license file="INSTALLER_LICENSE.txt" mime-type="text/plain"/>
  <options customize="never" require-scripts="false" hostArchitectures="$arch"/>
  <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
  <volume-check><allowed-os-versions><os-version min="${MACOSX_DEPLOYMENT_TARGET:-13.0}"/></allowed-os-versions></volume-check>
  <choices-outline><line choice="default"/></choices-outline>
  <choice id="default" visible="false"><pkg-ref id="org.iz6nnh.madmodem"/></choice>
  <pkg-ref id="org.iz6nnh.madmodem" version="$version" onConclusion="none">MadModem-component.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$work/distribution.xml" --package-path "$work" --resources "$work/resources" \
    "$out/MadModem-$version-macOS-$arch-unsigned.pkg"
