#!/usr/bin/env python3
"""Score actual streaming probes on externally annotated radio recordings.

SPDX-License-Identifier: GPL-3.0-or-later
No synthesis, tuning search, reference-conditioned decoding, or training.
Reference text stays in the local manifest; public output contains hashes and
metrics, so externally copyrighted transcripts need not be redistributed.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unicodedata
import wave

import numpy as np


def normalize(text):
    return " ".join(unicodedata.normalize("NFKC", text).upper().split())


def edit_counts(reference, hypothesis):
    """Levenshtein counts: deterministic diagonal/deletion/insertion ties."""
    a, b = reference, hypothesis
    previous = [(j, 0, 0, j) for j in range(len(b) + 1)]
    for i, x in enumerate(a, 1):
        row = [(i, 0, i, 0)]
        for j, y in enumerate(b, 1):
            cost, sub, delete, insert = previous[j - 1]
            diagonal = (cost + (x != y), sub + (x != y), delete, insert)
            cost, sub, delete, insert = previous[j]
            deletion = (cost + 1, sub, delete + 1, insert)
            cost, sub, delete, insert = row[j - 1]
            insertion = (cost + 1, sub, delete, insert + 1)
            row.append(min([diagonal, deletion, insertion], key=lambda v: v[0]))
        previous = row
    cost, sub, delete, insert = previous[-1]
    return {"errors": cost, "substitutions": sub, "deletions": delete,
            "insertions": insert, "characters": len(a),
            "cer": cost / len(a) if a else None}


def aligned_words(reference, hypothesis):
    a, b = reference.split(), hypothesis.split()
    dp = np.zeros((len(a) + 1, len(b) + 1), dtype=np.int32)
    dp[:, 0] = np.arange(len(a) + 1)
    dp[0] = np.arange(len(b) + 1)
    for i, x in enumerate(a, 1):
        for j, y in enumerate(b, 1):
            dp[i, j] = min(dp[i - 1, j] + 1, dp[i, j - 1] + 1,
                           dp[i - 1, j - 1] + (x != y))
    i, j = len(a), len(b)
    result = []
    while i or j:
        if i and j and dp[i, j] == dp[i - 1, j - 1] + (a[i - 1] != b[j - 1]):
            result.append((a[i - 1], b[j - 1])); i -= 1; j -= 1
        elif i and dp[i, j] == dp[i - 1, j] + 1:
            result.append((a[i - 1], None)); i -= 1
        else:
            j -= 1
    return result[::-1]


def read_wav(path):
    with wave.open(str(path)) as wav:
        if wav.getsampwidth() != 2 or wav.getnchannels() != 1:
            raise ValueError("benchmark inputs must be mono PCM16 WAV")
        rate = wav.getframerate()
        audio = np.frombuffer(wav.readframes(wav.getnframes()), "<i2")
    return rate, audio.astype(np.float32) / 32768.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", type=Path, required=True)
    ap.add_argument("--native", type=Path, required=True)
    ap.add_argument("--neural", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--private-predictions", type=Path,
                    help="Optional local diagnostic output, not public report")
    args = ap.parse_args()
    manifest = json.loads(args.manifest.read_text())
    probes = {"Native": args.native.resolve(), "Neural_R8": args.neural.resolve()}
    rows, private = [], []
    for item in manifest["recordings"]:
        path = (args.manifest.parent / item["wav"]).resolve()
        rate, raw = read_wav(path)
        source_hash = hashlib.sha256(path.read_bytes()).hexdigest()
        if source_hash != item["wav_sha256"]:
            raise ValueError("source checksum mismatch: " + item["id"])
        start, end = item["start_seconds"], item["end_seconds"]
        if not 0 <= start < end <= len(raw) / rate:
            raise ValueError("invalid annotated interval")
        audio = raw[round(start * rate):round(end * rate)]
        reference = normalize(item["reference"])
        if not reference or "…" in reference:
            raise ValueError("missing or uncertain transcript")
        flush_seconds = manifest.get("flush_seconds", 3.0)
        audio = np.concatenate((audio, np.zeros(round(rate * flush_seconds), np.float32)))
        row = {k: v for k, v in item.items() if k not in ("reference", "wav")}
        row.update({"reference_sha256": hashlib.sha256(reference.encode()).hexdigest(),
                    "input_f32_sha256": hashlib.sha256(audio.astype("<f4").tobytes()).hexdigest(),
                    "rate": rate, "real_audio_seconds": end - start, "metrics": {}})
        hypotheses = {}
        with tempfile.TemporaryDirectory(prefix="mm-cw-radio-") as td:
            input_path = Path(td) / "input.f32"
            audio.astype("<f4").tofile(input_path)
            for engine, probe in probes.items():
                # No transcript or WPM is passed to either decoder.
                hypothesis = normalize(subprocess.check_output(
                    [str(probe), "audio", str(rate), str(item["tone_hz"]), str(input_path), "997"],
                    stderr=subprocess.DEVNULL, text=True, timeout=90))
                hypotheses[engine] = hypothesis
                metric = edit_counts(reference, hypothesis)
                metric["no_space_diagnostic"] = edit_counts(reference.replace(" ", ""), hypothesis.replace(" ", ""))
                metric["exact"] = reference == hypothesis
                metric["hypothesis_sha256"] = hashlib.sha256(hypothesis.encode()).hexdigest()
                pairs = aligned_words(reference, hypothesis)
                for name in ("callsigns", "reports"):
                    targets = set(item.get(name, []))
                    metric[name] = {"correct": sum(x in targets and x == y for x, y in pairs),
                                    "total": sum(x in targets for x, y in pairs)}
                row["metrics"][engine] = metric
        rows.append(row)
        private.append({"id": item["id"], "reference": reference, "hypotheses": hypotheses})
        print("completed", item["id"], flush=True)
    summary = {}
    for engine in probes:
        metrics = [r["metrics"][engine] for r in rows]
        totals = {k: sum(m[k] for m in metrics) for k in
                  ("errors", "substitutions", "deletions", "insertions", "characters")}
        totals["cer"] = totals["errors"] / totals["characters"]
        for field in ("callsigns", "reports"):
            totals[field] = {k: sum(m[field][k] for m in metrics) for k in ("correct", "total")}
        summary[engine] = totals
    result = {"scope": manifest["scope"], "protocol": {
        "mode": "continuous per annotated exchange; reset between recordings only",
        "audio": "supplied mono PCM16 WAV; no filtering, gain normalization or pitch shift by the benchmark",
        "flush_seconds": manifest.get("flush_seconds", 3.0),
        "reference_normalization": "Unicode NFKC, uppercase, collapse whitespace; preserve punctuation and mistakes",
        "score": "Levenshtein CER including spaces; no-space CER is diagnostic only",
        "selection": "published transcripts and acoustic boundaries; no model or parameter changes after inspection",
        "chunk_samples": 997, "default_native_timing": "automatic WPM, no oracle"},
        "probe_sha256": {k: hashlib.sha256(p.read_bytes()).hexdigest() for k, p in probes.items()},
        "summary": summary, "recordings": rows}
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "REAL_COMPARISON.json").write_text(json.dumps(result, indent=2) + "\n")
    with (args.out / "REAL_COMPARISON.csv").open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["recording", "engine", "seconds", "characters", "errors", "CER_percent", "CER_no_spaces_diagnostic_percent", "calls_correct", "calls_total", "reports_correct", "reports_total"])
        for row in rows:
            for engine, m in row["metrics"].items():
                writer.writerow([row["id"], engine, row["real_audio_seconds"], m["characters"], m["errors"],
                    100*m["cer"], 100*m["no_space_diagnostic"]["cer"],
                    m["callsigns"]["correct"], m["callsigns"]["total"], m["reports"]["correct"], m["reports"]["total"]])
    if args.private_predictions:
        args.private_predictions.write_text(json.dumps(private, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
