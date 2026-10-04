#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  compare_ft_wav_regression.sh OLD_MADMODEM NEW_MADMODEM MODE WAV_OR_DIR [...]

MODE is FT8 or FT4. Both executables are run with the same WAV files and
--ft-depth max. The script compares per-file decode counts and reports any
regression. WAV files are intentionally not bundled in release archives.
USAGE
}

if [[ $# -lt 4 ]]; then
  usage >&2
  exit 2
fi

old_exe=$1
new_exe=$2
mode=${3^^}
shift 3

if [[ ! -x "$old_exe" || ! -x "$new_exe" ]]; then
  echo "ERROR: both MadModem paths must be executable" >&2
  exit 2
fi
if [[ "$mode" != FT8 && "$mode" != FT4 ]]; then
  echo "ERROR: MODE must be FT8 or FT4" >&2
  exit 2
fi

wavs=()
for item in "$@"; do
  if [[ -d "$item" ]]; then
    while IFS= read -r -d '' wav; do wavs+=("$wav"); done < <(find "$item" -maxdepth 1 -type f \( -iname '*.wav' -o -iname '*.wave' \) -print0 | sort -z)
  elif [[ -f "$item" ]]; then
    wavs+=("$item")
  else
    echo "WARNING: skipping missing path: $item" >&2
  fi
done
if [[ ${#wavs[@]} -eq 0 ]]; then
  echo "ERROR: no WAV files found" >&2
  exit 2
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

run_one() {
  local exe=$1 out=$2
  QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen} \
    "$exe" --ft-regression --ft-mode "$mode" --ft-depth max "${wavs[@]}" >"$out"
}

run_one "$old_exe" "$tmp/old.tsv"
run_one "$new_exe" "$tmp/new.tsv"

awk -F '\t' '$1=="FTREG" {print $4 "\t" $6}' "$tmp/old.tsv" | sort >"$tmp/old.counts"
awk -F '\t' '$1=="FTREG" {print $4 "\t" $6}' "$tmp/new.tsv" | sort >"$tmp/new.counts"

printf '%-28s %10s %10s %10s\n' WAV OLD NEW DELTA
status=0
while IFS=$'\t' read -r wav old_count; do
  new_count=$(awk -F '\t' -v w="$wav" '$1==w {print $2}' "$tmp/new.counts")
  if [[ -z "$new_count" ]]; then
    printf '%-28s %10s %10s %10s\n' "$wav" "$old_count" MISSING FAIL
    status=1
    continue
  fi
  delta=$((new_count - old_count))
  printf '%-28s %10d %10d %+10d\n' "$wav" "$old_count" "$new_count" "$delta"
  if (( delta < 0 )); then status=1; fi
done <"$tmp/old.counts"

if (( status != 0 )); then
  echo "REGRESSION: one or more files decoded fewer messages" >&2
else
  echo "OK: no per-file decode-count regression"
fi

exit "$status"
