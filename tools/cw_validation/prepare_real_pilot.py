#!/usr/bin/env python3
"""Acquire the R9 pilot from its public sources; do not redistribute audio.
SPDX-License-Identifier: GPL-3.0-or-later
Requires curl and ffmpeg on PATH. Text is extracted from the operator's site,
never inferred from either evaluated decoder. Original source SHA-256 is pinned.
"""
import argparse
import hashlib
import html
import json
from pathlib import Path
import re
import subprocess


SITE = "https://www.verulam-arc.org.uk/cw-class/"
SOURCES = [
    dict(id="varc3_complete_exchange", session="2025-05-13", number=3,
         source="https://archive.org/download/morse-class-3/Morse%20Class%203.mp3",
         mp3_sha256="f72f451d478acb9d7017561076f083ebea77400496f097950f5680de7d933eea",
         wav_sha256="3b46f2acc2ba9409e4829bc7df3781ac459173b6a5f3e03df44cc094d0b41d91",
         reference_sha256="6ad7cd8282c65dcc0f44dc861c3fcf76a02c6059d595ca9cebb0eaa06d1ca0ad",
         origin=2705, duration=600, start=77.3, end=231.6),
    dict(id="varc7_closing_turn", session="2025-06-10", number=7,
         source="https://archive.org/download/morse-class-7/Morse%20Class%207.mp3",
         mp3_sha256="2bcd15505c23b3bc95d471b5c71bd83ab5a45b25b5fa751e12b2fda3f5ab07b3",
         wav_sha256="f31c22d40d005fc8836451c46240d4693eeb3ef1b7a6922c46258a0ca03f1495",
         reference_sha256="99335c43e0075d770e0ba816aeb9b90646031a5fcc3b28e762102adce989d3b1",
         origin=1110, duration=780, start=655.0, end=728.8),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fetch(url, path):
    subprocess.run(["curl", "--fail", "--silent", "--show-error", "--location",
                    "--max-time", "90", "--output", str(path), url], check=True, timeout=95)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--page", type=Path, help="Use a previously downloaded source HTML")
    ap.add_argument("--allow-decoder-variation", action="store_true",
                    help="Allow a different WAV hash from a different ffmpeg version, while pinning the original MP3")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "real-audio").mkdir(exist_ok=True)
    page = args.page or args.out / "varc.html"
    if not args.page:
        fetch(SITE, page)
    paragraphs = [re.sub("<[^>]+>", " ", html.unescape(p)) for p in
                  re.findall("<p[^>]*>(.*?)</p>", page.read_text(), flags=re.S)]
    paragraphs = [" ".join(p.split()) for p in paragraphs]
    first = next(p for p in paragraphs if p.startswith(
        "Transcript of live QSO with Matt G4IYT (G0OIK)"))
    first = re.sub(r"^Transcript of live QSO with Matt G4IYT\s*", "", first)
    first = first.replace("(G0OIK) – ", "").replace("(G4IYT) – ", "")
    close = next(p for p in paragraphs if p.startswith("BK DE G4IYT OK PETER"))
    references = [first, close]
    rows = []
    for source, reference in zip(SOURCES, references):
        reference = " ".join(reference.upper().split())
        if hashlib.sha256(reference.encode()).hexdigest() != source["reference_sha256"]:
            raise ValueError("published transcript changed; do not silently change benchmark labels")
        mp3 = args.out / "real-audio" / f"VARC-Class{source['number']}.mp3"
        wav = args.out / "real-audio" / f"VARC{source['number']}-QSO.wav"
        if not mp3.exists():
            fetch(source["source"], mp3)
        if sha(mp3) != source["mp3_sha256"]:
            raise ValueError("MP3 checksum mismatch: " + str(mp3))
        if not wav.exists():
            subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                "-ss", str(source["origin"]), "-i", str(mp3), "-t", str(source["duration"]),
                "-ac", "1", "-ar", "8000", str(wav)], check=True, timeout=60)
        actual_hash = sha(wav)
        if actual_hash != source["wav_sha256"] and not args.allow_decoder_variation:
            raise ValueError("Decoded WAV differs from ffmpeg 6.1.1-3ubuntu5. "
                "Use --allow-decoder-variation for a separately hashed rerun, not a bit-identical replay.")
        rows.append({"id": source["id"], "wav": "real-audio/" + wav.name,
            "wav_sha256": actual_hash, "reference": reference,
            "start_seconds": source["start"], "end_seconds": source["end"], "tone_hz": 579.1,
            "callsigns": ["G0OIK", "G4IYT"], "reports": ["599", "5NN", "HE99"],
            "recording_session": source["session"], "source_url": SITE,
            "original_mp3_start_seconds": source["origin"] + source["start"],
            "original_mp3_end_seconds": source["origin"] + source["end"],
            "annotation_status": "Published operator transcript, including sending mistakes; "
                "acoustic boundaries inspected; not independently audited symbol by symbol"})
    result = {"scope": "Pilot on two authentic off-air recordings from two days, one full "
        "three-turn exchange and one closing turn. Published operator references, not independently "
        "audited symbol by symbol. No training or model selection on these recordings. "
        "Not a representative population test and not proof of <5% radio CER.",
        "flush_seconds": 3, "recordings": rows}
    (args.out / "real-manifest-private.json").write_text(json.dumps(result, indent=2) + "\n")
    print("Prepared", len(rows), "recordings; local reference manifest written.")


if __name__ == "__main__":
    main()
