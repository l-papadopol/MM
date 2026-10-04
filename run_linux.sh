#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -x "$ROOT/build-linux/MadModem" ]]; then
    exec "$ROOT/build-linux/MadModem" "$@"
fi
if [[ -x "$ROOT/dist/linux/bin/MadModem" ]]; then
    exec "$ROOT/dist/linux/bin/MadModem" "$@"
fi
if [[ -x "$ROOT/dist/linux/MadModem" ]]; then
    exec "$ROOT/dist/linux/MadModem" "$@"
fi
echo "MadModem Linux executable not found. Build first with ./build_all.sh or cmake --build build-linux." >&2
exit 1
