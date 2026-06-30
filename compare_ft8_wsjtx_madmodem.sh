#!/usr/bin/env bash
# compare_ft8_wsjtx_madmodem.sh
#
# Run WSJT-X/jt9 FT8 WAV decodes on the MadModem regression WAV set
# and compare the resulting counts with a MadModem FT Auto Test report.
#
# This script does NOT drive the MadModem GUI. It reads the latest
# ~/MadModem_FT_AutoTest_*.txt report, or the report passed with --mm-report.
#
# Usage examples:
#   ./compare_ft8_wsjtx_madmodem.sh
#   ./compare_ft8_wsjtx_madmodem.sh --mm-report ~/MadModem_FT_AutoTest_20260616_064443.txt
#   ./compare_ft8_wsjtx_madmodem.sh --wav-dir ~/MadModem/build-linux/tests/wav --jt9 /usr/bin/jt9
#
# Optional environment:
#   JT9_DEPTH=3
#   JT9_TIMEOUT=90
#
set -Eeuo pipefail

WAV_DIR="${HOME}/MadModem/build-linux/tests/wav"
MM_REPORT=""
OUT_DIR="${HOME}/MadModem/compare_reports"
JT9_BIN="${JT9_CMD:-}"
JT9_DEPTH="${JT9_DEPTH:-3}"
JT9_TIMEOUT="${JT9_TIMEOUT:-90}"
KEEP_GOING=1

usage() {
    sed -n '1,35p' "$0"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wav-dir)
            WAV_DIR="${2:-}"; shift 2 ;;
        --mm-report)
            MM_REPORT="${2:-}"; shift 2 ;;
        --out-dir)
            OUT_DIR="${2:-}"; shift 2 ;;
        --jt9)
            JT9_BIN="${2:-}"; shift 2 ;;
        --depth)
            JT9_DEPTH="${2:-}"; shift 2 ;;
        --timeout)
            JT9_TIMEOUT="${2:-}"; shift 2 ;;
        --strict)
            KEEP_GOING=0; shift ;;
        -h|--help)
            usage; exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2 ;;
    esac
done

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "Missing command: $1" >&2
        exit 1
    }
}

need_cmd python3
need_cmd timeout
need_cmd date
need_cmd find
need_cmd sort

if [[ -z "${JT9_BIN}" ]]; then
    for c in \
        jt9 \
        /usr/bin/jt9 \
        /usr/local/bin/jt9 \
        /usr/lib/wsjtx/jt9 \
        /usr/lib/x86_64-linux-gnu/wsjtx/jt9 \
        /opt/wsjtx/bin/jt9
    do
        if [[ -x "$c" ]] || command -v "$c" >/dev/null 2>&1; then
            JT9_BIN="$(command -v "$c" 2>/dev/null || printf '%s' "$c")"
            break
        fi
    done
fi

if [[ -z "${JT9_BIN}" || ! -x "${JT9_BIN}" ]]; then
    echo "Cannot find executable jt9. Try: --jt9 /path/to/jt9" >&2
    exit 1
fi

if [[ ! -d "${WAV_DIR}" ]]; then
    echo "WAV directory not found: ${WAV_DIR}" >&2
    exit 1
fi

if [[ -z "${MM_REPORT}" ]]; then
    MM_REPORT="$(ls -t "${HOME}"/MadModem_FT_AutoTest_*.txt 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "${MM_REPORT}" || ! -f "${MM_REPORT}" ]]; then
    echo "MadModem report not found." >&2
    echo "Run MadModem Settings -> FT Auto test first, or pass --mm-report /path/report.txt" >&2
    exit 1
fi

STAMP="$(date -u +%Y%m%d_%H%M%S)"
RUN_DIR="${OUT_DIR}/ft8_compare_${STAMP}"
RAW_DIR="${RUN_DIR}/wsjtx_raw"
mkdir -p "${RAW_DIR}"

LOG="${RUN_DIR}/compare.log"
WSJTX_TSV="${RUN_DIR}/wsjtx_decodes.tsv"
SUMMARY_TSV="${RUN_DIR}/summary.tsv"
REPORT_MD="${RUN_DIR}/comparison_report.md"
META="${RUN_DIR}/environment.txt"

printf "file\tdt\tfreq\tsnr\tmessage\traw\n" > "${WSJTX_TSV}"

{
    echo "MadModem / WSJT-X FT8 WAV comparison"
    echo "UTC: $(date -u --iso-8601=seconds)"
    echo "WAV_DIR=${WAV_DIR}"
    echo "MM_REPORT=${MM_REPORT}"
    echo "RUN_DIR=${RUN_DIR}"
    echo "JT9_BIN=${JT9_BIN}"
    echo "JT9_DEPTH=${JT9_DEPTH}"
    echo "JT9_TIMEOUT=${JT9_TIMEOUT}"
    echo
    echo "wsjtx version:"
    if command -v wsjtx >/dev/null 2>&1; then
        wsjtx --version 2>&1 || true
    else
        echo "wsjtx command not on PATH"
    fi
    echo
    echo "jt9 version/help probe:"
    "${JT9_BIN}" -v 2>&1 || "${JT9_BIN}" --version 2>&1 || true
} | tee "${META}" > /dev/null

# Preferred MadModem test WAV order, then any remaining wav files.
mapfile -t ORDERED_WAVS < <(
python3 - "$WAV_DIR" <<'PY'
from pathlib import Path
import sys
wd = Path(sys.argv[1])
preferred = ["websdr_test6.wav", "test_21.wav", "test_18.wav", "test_05.wav"]
seen = set()
for name in preferred:
    p = wd / name
    if p.exists():
        print(p)
        seen.add(p.resolve())
for p in sorted(wd.glob("*.wav")):
    if p.resolve() not in seen:
        print(p)
PY
)

if [[ "${#ORDERED_WAVS[@]}" -eq 0 ]]; then
    echo "No .wav files found in ${WAV_DIR}" >&2
    exit 1
fi

parse_one_raw() {
    local wav_base="$1"
    local raw_file="$2"
    python3 - "$wav_base" "$raw_file" "$WSJTX_TSV" <<'PY'
from pathlib import Path
import sys, re, csv

wav_base = sys.argv[1]
raw_path = Path(sys.argv[2])
out_path = Path(sys.argv[3])

num = r'[-+]?\d+(?:\.\d+)?'
# jt9 direct format seen on WSJT-X 3.0.1 CLI:
#   000000  16  0.2 1256 ~  CQ DM1YS JO30
# meaning UTC, SNR, DT, audio-frequency, '~', message.
jt9_direct = re.compile(rf'^\s*(?P<utc>\d{{6}})\s+(?P<snr>[-+]?\d+(?:\.\d+)?)\s+(?P<dt>{num})\s+(?P<freq>[-+]?\d+)\s*~\s*(?P<msg>\S.*)$')
# Older/simple renderings sometimes use:
#   dt freq snr message
patterns = [
    re.compile(rf'^\s*(?P<dt>{num})\s+(?P<freq>[-+]?\d+)\s+(?P<snr>[-+]?\d+(?:\.\d+)?)\s+(?P<msg>\S.*)$'),
    re.compile(rf'^\s*(?P<dt>{num})\s+(?P<freq>[-+]?\d+)(?P<snr>[+-]\d+(?:\.\d+)?)\s+(?P<msg>\S.*)$'),
]
# WSJT-X ALL.TXT-ish fallback: date/time/band/rx/mode/snr/dt/freq/message
alltxt = re.compile(rf'^\s*\d{{6}}_?\d{{6}}\s+.*?\bFT8\b\s+(?P<snr>[-+]?\d+(?:\.\d+)?)\s+(?P<dt>{num})\s+(?P<freq>[-+]?\d+)\s+(?P<msg>\S.*)$')

def clean_msg(s: str) -> str:
    s = s.strip().strip('`').strip()
    # Remove common jt9/AP annotations not part of text message.
    s = re.sub(r'\s+\?+\s*a\d+\s*$', '', s)
    s = re.sub(r'\s+a\d+\s*$', '', s)
    s = re.sub(r'\s+\?+\s*$', '', s)
    s = re.sub(r'\s+', ' ', s)
    return s.strip()

rows = []
for raw_line in raw_path.read_text(errors='replace').splitlines():
    line = raw_line.strip()
    if not line:
        continue
    m = jt9_direct.match(line)
    if not m:
        m = alltxt.match(line)
    if not m:
        for pat in patterns:
            m = pat.match(line)
            if m:
                break
    if not m:
        continue
    msg = clean_msg(m.group('msg'))
    # Avoid parsing diagnostic text as a decode.
    if len(msg) < 2 or msg.lower().startswith(('error', 'usage', 'warning', 'decode ', 'decoding ')):
        continue
    rows.append([wav_base, m.group('dt'), m.group('freq'), m.group('snr'), msg, raw_line.rstrip('\n')])

with out_path.open('a', newline='') as f:
    w = csv.writer(f, delimiter='\t', lineterminator='\n')
    w.writerows(rows)
print(len(rows))
PY
}

# Try a small set of jt9 invocations. Different WSJT-X packages accept different aliases.
run_jt9_variant() {
    local variant="$1"
    local wav="$2"
    case "$variant" in
        depth_ft8_long) timeout "${JT9_TIMEOUT}" "${JT9_BIN}" -d "${JT9_DEPTH}" --ft8 "$wav" ;;
        depth_ft8_short) timeout "${JT9_TIMEOUT}" "${JT9_BIN}" -d "${JT9_DEPTH}" -8 "$wav" ;;
        ft8_long) timeout "${JT9_TIMEOUT}" "${JT9_BIN}" --ft8 "$wav" ;;
        ft8_short) timeout "${JT9_TIMEOUT}" "${JT9_BIN}" -8 "$wav" ;;
        plain) timeout "${JT9_TIMEOUT}" "${JT9_BIN}" "$wav" ;;
        *) return 2 ;;
    esac
}

JT9_VARIANT=""
VARIANTS=(depth_ft8_long depth_ft8_short ft8_long ft8_short plain)

echo "Selecting jt9 invocation..." | tee -a "${LOG}"
first_wav="${ORDERED_WAVS[0]}"
first_base="$(basename "$first_wav")"
for v in "${VARIANTS[@]}"; do
    probe_raw="${RAW_DIR}/.${first_base}.${v}.probe.txt"
    set +e
    start_ns="$(date +%s%N)"
    run_jt9_variant "$v" "$first_wav" > "${probe_raw}" 2>&1
    rc=$?
    end_ns="$(date +%s%N)"
    set -e
    count="$(parse_one_raw "${first_base}" "${probe_raw}" 2>/dev/null || echo 0)"
    # Remove probe parses from TSV.
    printf "file\tdt\tfreq\tsnr\tmessage\traw\n" > "${WSJTX_TSV}"
    elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
    echo "  variant=${v} rc=${rc} parsed=${count} ms=${elapsed_ms}" | tee -a "${LOG}"
    if [[ "$rc" -eq 0 && "$count" -gt 0 ]]; then
        JT9_VARIANT="$v"
        break
    fi
done

if [[ -z "$JT9_VARIANT" ]]; then
    echo "No jt9 invocation produced parseable decodes on ${first_base}." | tee -a "${LOG}" >&2
    echo "Raw probes are in ${RAW_DIR}; inspect them and set JT9_CMD/JT9_DEPTH or edit VARIANTS." | tee -a "${LOG}" >&2
    exit 1
fi

echo "Selected jt9 variant: ${JT9_VARIANT}" | tee -a "${LOG}"

printf "file\tjt9_rc\tjt9_ms\traw_file\tparsed_decodes\n" > "${RUN_DIR}/wsjtx_runs.tsv"

for wav in "${ORDERED_WAVS[@]}"; do
    base="$(basename "$wav")"
    raw="${RAW_DIR}/${base}.jt9.txt"
    echo "Running jt9 on ${base}..." | tee -a "${LOG}"
    set +e
    start_ns="$(date +%s%N)"
    run_jt9_variant "$JT9_VARIANT" "$wav" > "$raw" 2>&1
    rc=$?
    end_ns="$(date +%s%N)"
    set -e
    elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
    parsed="$(parse_one_raw "$base" "$raw" || echo 0)"
    printf "%s\t%s\t%s\t%s\t%s\n" "$base" "$rc" "$elapsed_ms" "$raw" "$parsed" >> "${RUN_DIR}/wsjtx_runs.tsv"
    if [[ "$rc" -ne 0 && "$KEEP_GOING" -eq 0 ]]; then
        echo "jt9 failed on ${base}, rc=${rc}" >&2
        exit "$rc"
    fi
done

python3 - "$WSJTX_TSV" "$RUN_DIR/wsjtx_runs.tsv" "$MM_REPORT" "$SUMMARY_TSV" "$REPORT_MD" "$META" <<'PY'
from pathlib import Path
import sys, csv, re, datetime

wsjtx_tsv = Path(sys.argv[1])
runs_tsv = Path(sys.argv[2])
mm_report = Path(sys.argv[3])
summary_tsv = Path(sys.argv[4])
report_md = Path(sys.argv[5])
meta = Path(sys.argv[6])

def norm_msg(s: str) -> str:
    s = re.sub(r'\s+', ' ', s.strip())
    s = re.sub(r'\s+\?+\s*a\d+\s*$', '', s)
    s = re.sub(r'\s+a\d+\s*$', '', s)
    s = re.sub(r'\s+\?+\s*$', '', s)
    return s.strip()

ws = {}
with wsjtx_tsv.open(newline='') as f:
    for r in csv.DictReader(f, delimiter='\t'):
        fn = r['file']
        ws.setdefault(fn, []).append({
            'dt': r['dt'], 'freq': r['freq'], 'snr': r['snr'],
            'message': norm_msg(r['message']),
            'raw': r['raw'],
        })

runs = {}
with runs_tsv.open(newline='') as f:
    for r in csv.DictReader(f, delimiter='\t'):
        runs[r['file']] = r

# Parse MadModem TSV auto-test report.
mm_rows = {}
lines = mm_report.read_text(errors='replace').splitlines()
header_idx = None
for i, line in enumerate(lines):
    if line.startswith('File\tMode\tOK\tReference\tDecodes\t'):
        header_idx = i
        break

if header_idx is not None:
    header = lines[header_idx].split('\t')
    for line in lines[header_idx+1:]:
        if not line.strip() or '\t' not in line:
            continue
        parts = line.split('\t')
        if len(parts) < len(header):
            parts += [''] * (len(header) - len(parts))
        row = dict(zip(header, parts))
        if row.get('File'):
            mm_rows[row['File']] = row

all_files = sorted(set(ws) | set(mm_rows) | set(runs))
preferred = ["websdr_test6.wav", "test_21.wav", "test_18.wav", "test_05.wav"]
all_files = [x for x in preferred if x in all_files] + [x for x in all_files if x not in preferred]

cols = [
    'File', 'WSJTX decodes', 'MM decodes', 'MM-WSJTX',
    'JT9 ms', 'MM total ms', 'MM search ms', 'MM LDPC ms', 'MM subtract ms',
    'MM OSD tried', 'MM OSD recovered', 'MM OSD ms',
    'MM sync gate', 'MM sync detail', 'MM sweep detail',
]
with summary_tsv.open('w', newline='') as f:
    w = csv.writer(f, delimiter='\t', lineterminator='\n')
    w.writerow(cols)
    for fn in all_files:
        ws_count = len(ws.get(fn, []))
        mm = mm_rows.get(fn, {})
        mm_dec = mm.get('Decodes', '')
        try:
            delta = int(mm_dec) - ws_count
        except Exception:
            delta = ''
        run = runs.get(fn, {})
        w.writerow([
            fn, ws_count, mm_dec, delta,
            run.get('jt9_ms', ''),
            mm.get('Total ms', ''), mm.get('Search ms', ''), mm.get('LDPC ms', ''), mm.get('Subtract ms', ''),
            mm.get('OSD GF2 tried', ''), mm.get('OSD GF2 recovered', ''), mm.get('OSD GF2 ms', ''),
            mm.get('Sync gate', ''), mm.get('Sync gate detail', ''), mm.get('Sync sweep detail', ''),
        ])

# Markdown report with per-file WSJT-X messages.
now = datetime.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
md = []
md.append("# MadModem / WSJT-X FT8 WAV comparison")
md.append("")
md.append(f"UTC: `{now}`")
md.append("")
md.append(f"MadModem report: `{mm_report}`")
md.append(f"Environment: `{meta}`")
md.append("")
md.append("## Count summary")
md.append("")
md.append("| File | WSJT-X/jt9 decodes | MadModem decodes | MM-WSJTX | JT9 ms | MM total ms | OSD rec | Sync gate |")
md.append("|---|---:|---:|---:|---:|---:|---:|---:|")
for fn in all_files:
    ws_count = len(ws.get(fn, []))
    mm = mm_rows.get(fn, {})
    mm_dec = mm.get('Decodes', '')
    try:
        delta = int(mm_dec) - ws_count
    except Exception:
        delta = ''
    run = runs.get(fn, {})
    md.append(f"| {fn} | {ws_count} | {mm_dec} | {delta} | {run.get('jt9_ms','')} | {mm.get('Total ms','')} | {mm.get('OSD GF2 recovered','')} | {mm.get('Sync gate','')} |")

md.append("")
md.append("## Notes")
md.append("")
md.append("- This compares MadModem Auto Test counts against WSJT-X `jt9` command-line WAV decodes.")
md.append("- MadModem Auto Test reports currently do not contain the individual decoded message list, so message-level missing/extra comparison is only possible after MadModem exports per-decode rows.")
md.append("- WSJT-X/jt9 messages below are normalized by removing AP suffixes such as `a1` and trailing `?`.")
md.append("")
md.append("## WSJT-X/jt9 decoded messages")
for fn in all_files:
    md.append("")
    md.append(f"### {fn}")
    msgs = ws.get(fn, [])
    if not msgs:
        md.append("")
        md.append("_No parsed WSJT-X/jt9 decodes._")
        continue
    md.append("")
    md.append("| DT | Freq | SNR | Message |")
    md.append("|---:|---:|---:|---|")
    for d in msgs:
        msg = d['message'].replace('|', '\\|')
        md.append(f"| {d['dt']} | {d['freq']} | {d['snr']} | `{msg}` |")

report_md.write_text("\n".join(md) + "\n")
print(f"Wrote {summary_tsv}")
print(f"Wrote {report_md}")
PY

echo
echo "Done."
echo "Run directory: ${RUN_DIR}"
echo "Main report:   ${REPORT_MD}"
echo "Summary TSV:   ${SUMMARY_TSV}"
echo "WSJT-X TSV:    ${WSJTX_TSV}"
echo "Raw jt9 logs:  ${RAW_DIR}"
