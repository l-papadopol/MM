#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

fail=0
say_fail() { echo "ERROR: $*" >&2; fail=1; }

if [[ -e VERSION || -e version ]]; then
  say_fail "root VERSION/version shadows the C++17 <version> header on case-insensitive macOS filesystems"
fi

if grep -R --include='*.cpp' --include='*.h' -n '\bQRegExp\b' third_party/mshv_gpl \
      | grep -v '/reference/' | grep -v '/upstream_2765/' >/tmp/madmodem_qregexp_hits.$$ 2>/dev/null; then
  cat /tmp/madmodem_qregexp_hits.$$ >&2
  say_fail "active MSHV sources still use QRegExp; Qt6 removed it"
fi
rm -f /tmp/madmodem_qregexp_hits.$$ 2>/dev/null || true


if grep -R --include='*.cpp' --include='*.h' -n '\bmidRef\s*(' third_party/mshv_gpl \
      | grep -v '/reference/' | grep -v '/upstream_2765/' >/tmp/madmodem_midref_hits.$$ 2>/dev/null; then
  cat /tmp/madmodem_midref_hits.$$ >&2
  say_fail "active MSHV sources still use QString::midRef(); Qt6 removed it, use mid()"
fi
rm -f /tmp/madmodem_midref_hits.$$ 2>/dev/null || true

if grep -R --include='*.cpp' --include='*.h' -n '\bQStringRef\b' third_party/mshv_gpl \
      | grep -v '/reference/' | grep -v '/upstream_2765/' >/tmp/madmodem_qstringref_hits.$$ 2>/dev/null; then
  cat /tmp/madmodem_qstringref_hits.$$ >&2
  say_fail "active MSHV sources still use QStringRef; Qt6 removed it"
fi
rm -f /tmp/madmodem_qstringref_hits.$$ 2>/dev/null || true

if [[ ! -f third_party/mshv_gpl/port/boost/config/compiler/clang.hpp ]]; then
  say_fail "vendored MSHV Boost subset lacks boost/config/compiler/clang.hpp for AppleClang"
fi


python3 - <<'PYCHK'
from pathlib import Path
import re, sys
fail_hits = []
for p in Path('.').rglob('*'):
    if p.suffix not in ('.cpp', '.h', '.hpp', '.cc'):
        continue
    if any(part in ('reference', 'upstream_2765') for part in p.parts):
        continue
    lines = p.read_text(errors='ignore').splitlines()
    for i, line in enumerate(lines):
        if re.search(r'q(?:Bound|Max|Min)<qint64>\s*\(\s*[0-9]', line):
            fail_hits.append(f"{p}:{i+1}:{line.strip()}")
        if 'qBound<qint64>(' in line:
            for j in range(i + 1, min(i + 5, len(lines))):
                if re.match(r'\s*[0-9]+\s*[,)]', lines[j]):
                    fail_hits.append(f"{p}:{j+1}:{lines[j].strip()}")
if fail_hits:
    print('ERROR: qint64 Qt min/max/bound calls still use bare integer literals; Qt6/AppleClang can make these overloads ambiguous.', file=sys.stderr)
    print('Use qint64{0}/qint64{1}/static_cast<qint64>(...) consistently.', file=sys.stderr)
    for h in fail_hits:
        print(h, file=sys.stderr)
    sys.exit(1)
PYCHK

# QTextCodec is Qt5-only. The active code intentionally keeps its include and
# call under QT_VERSION < 6 guards in main.cpp. Do not grep blindly here: a
# guarded call is safe, and rejecting it would create false CI failures.

if [[ $fail -ne 0 ]]; then
  exit 1
fi

echo "macOS portability preflight OK"
