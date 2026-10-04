#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
find "$ROOT" -type f -name "*.sh" -exec chmod 755 {} +
chmod 755 "$ROOT/build_all.sh" "$ROOT/run_linux.sh" 2>/dev/null || true
printf 'MadModem script permissions repaired. You can now run ./build_all.sh\n'
