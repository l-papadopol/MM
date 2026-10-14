#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(tr -d '\r\n' < "$root/MADMODEM_VERSION.txt")"
out="${MADMODEM_PACKAGE_OUT_DIR:-$root/dist/packages}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cat "$root/packaging/COPYRIGHT.txt" "$root/COPYING" > "$work/INSTALLER_LICENSE.txt"
for variant in legacy avx2; do
    source_dir="$out/MadModem-$version-Windows-x86_64-$variant-standalone"
    test -f "$source_dir/bin/MadModem.exe"
    mkdir -p "$work/$variant"
    cp -a "$source_dir/." "$work/$variant/"
    printf 'Managed installation; user data is stored outside this directory.\n' > "$work/$variant/bin/managed-install.flag"
    # The installer supplies real shortcuts; remove portable-only instructions.
    rm -f "$work/$variant/RUN_WINDOWS.txt"
done
gcc -O2 -march=x86-64 -mno-avx -mno-avx2 -mno-fma -static \
    "$root/packaging/windows/cpu_probe.c" -o "$work/cpu-probe.exe"
iscc="${ISCC:-/c/Program Files (x86)/Inno Setup 6/ISCC.exe}"
test -x "$iscc"
MSYS2_ARG_CONV_EXCL='*' "$iscc" "/DAppVersion=$version" \
    "/DPayloadRoot=$(cygpath -w "$work")" "/DOutputRoot=$(cygpath -w "$out")" \
    "$(cygpath -w "$root/packaging/windows/MadModem.iss")"
