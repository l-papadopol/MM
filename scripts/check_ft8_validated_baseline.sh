#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 MADMODEM_EXECUTABLE WAV_DIRECTORY" >&2
  exit 2
fi

exe=$1
wav_dir=$2
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
manifest="$root/tests/ft8/validated_baseline.tsv"

if [[ ! -x "$exe" ]]; then
  echo "ERROR: not executable: $exe" >&2
  exit 2
fi
if [[ ! -d "$wav_dir" ]]; then
  echo "ERROR: WAV directory not found: $wav_dir" >&2
  exit 2
fi

wavs=()
declare -A expected
while IFS=$'\t' read -r name digest count; do
  [[ -z "$name" || ${name:0:1} == '#' ]] && continue
  path="$wav_dir/$name"
  if [[ ! -f "$path" ]]; then
    echo "ERROR: missing validated WAV: $path" >&2
    exit 2
  fi
  actual=$(sha256sum "$path" | awk '{print $1}')
  if [[ "$actual" != "$digest" ]]; then
    echo "ERROR: SHA-256 mismatch for $name" >&2
    echo "  expected: $digest" >&2
    echo "  actual:   $actual" >&2
    exit 2
  fi
  wavs+=("$path")
  expected["$name"]=$count
done <"$manifest"

tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen} \
  "$exe" --ft-regression --ft-mode FT8 --ft-depth max "${wavs[@]}" >"$tmp"

printf '%-28s %10s %10s %10s\n' WAV EXPECTED ACTUAL DELTA
status=0
total_expected=0
total_actual=0
for path in "${wavs[@]}"; do
  name=$(basename "$path")
  count=$(awk -F '\t' -v n="$name" '$1=="FTREG" && $4==n {print $6}' "$tmp")
  if [[ -z "$count" ]]; then
    printf '%-28s %10s %10s %10s\n' "$name" "${expected[$name]}" MISSING FAIL
    status=1
    continue
  fi
  delta=$((count - expected[$name]))
  printf '%-28s %10d %10d %+10d\n' "$name" "${expected[$name]}" "$count" "$delta"
  total_expected=$((total_expected + expected[$name]))
  total_actual=$((total_actual + count))
  if (( count < expected[$name] )); then status=1; fi
done
printf '%-28s %10d %10d %+10d\n' TOTAL "$total_expected" "$total_actual" "$((total_actual-total_expected))"

if (( status != 0 )); then
  echo "REGRESSION: validated FT8 baseline is not met" >&2
else
  echo "OK: validated FT8 baseline met or exceeded"
fi
exit "$status"
