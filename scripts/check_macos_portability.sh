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

if [[ ! -f third_party/mshv_gpl/port/boost/config/compiler/clang.hpp ]]; then
  say_fail "vendored MSHV Boost subset lacks boost/config/compiler/clang.hpp for AppleClang"
fi

# QTextCodec is Qt5-only. The active code intentionally keeps its include and
# call under QT_VERSION < 6 guards in main.cpp. Do not grep blindly here: a
# guarded call is safe, and rejecting it would create false CI failures.

if [[ $fail -ne 0 ]]; then
  exit 1
fi

echo "macOS portability preflight OK"
