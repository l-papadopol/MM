#!/usr/bin/env python3
"""Static localization coverage audit for MadModem runtime dictionaries.

The authoritative harvester is tools/update_ui_translations.py.  This audit imports
that harvester, checks that every generated key exists in every runtime language,
and reports entries that still fall back to English in non-English dictionaries.
"""
from __future__ import annotations
import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LANGS = ["en", "it", "fr", "de", "no", "cs"]


def load_update_module():
    path = ROOT / "tools" / "update_ui_translations.py"
    spec = importlib.util.spec_from_file_location("update_ui_translations", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load update_ui_translations.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)  # type: ignore[attr-defined]
    return module


def read_ini(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    if not path.exists():
        return out
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("[") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[k.strip()] = v.strip()
    return out


def main() -> int:
    updater = load_update_module()
    harvested = updater.harvest_keys()
    dictionaries = {lang: read_ini(ROOT / "translations" / f"ui_{lang}.ini") for lang in LANGS}
    expected = set(harvested.keys())
    errors: list[str] = []
    for lang in LANGS:
        keys = set(dictionaries[lang].keys())
        missing = sorted(expected - keys)
        extra = sorted(keys - expected)
        if missing:
            errors.append(f"{lang}: missing {len(missing)} harvested keys, e.g. {missing[:10]}")
        if extra:
            errors.append(f"{lang}: extra {len(extra)} keys not produced by harvester, e.g. {extra[:10]}")

    en = dictionaries["en"]
    # Some labels are intentionally identical in many languages because they are
    # mode names, abbreviations, units, HTML fragments, ham-radio acronyms or
    # brand/model labels.  Do not report them as untranslated UI.
    same_ok_values = {
        "File", "Info", "Freq", "Dir", "Handshake", "Test TX", "Test PTT",
        "CAT / Hamlib", "MSHV-derived native decoder", "Next TX%1: %2",
        "Avg DT", "dB/Tag", "Hell", "Feld Hell", "Preset", "Baud",
        "Shift", "Mark", "Tone", "Center", "Variant", "Font", "Decoder",
        "Log", "PulseAudio", "MM v2.49", "MM v1.59", "-inf dB",
        "Freq: -- Hz", "UTC</div>", "msg=", "s", "ppm", "px",
        "Kenwood USB Audio via DATA SEND", "Kenwood ACC 2 audio via DATA SEND",
        "Kenwood LAN audio via DATA SEND",
        # Radio/GUI acronyms, protocol labels, icons and words whose spelling is
        # intentionally identical in at least one supported language.
        "Audio / PTT / CAT", "Audio / PTT", "Radio / CAT", "Data/Pkt",
        "Hardware RTS/CTS", "Serial RTS", "Serial DTR", "Front/Mic",
        "Auto WPM", "standard FT", "UTC interval", "APT start", "FT8 slot: --",
        "RTTY contest DSP", "TX report", "Live max 10", "SSTV editor",
        "Send SSTV", "Hellschreiber TX", "Start RX", "Tune",
        "Mode", "Source", "Index", "Palette", "Message", "Convergence",
        "Terminal", "Variable", "Type", "Note", "Image", "Station",
        "Text", "Model", "Band", "Name", "QSOs", "Status", "Scheduler",
        "No", "Locator", "Auto QSO", "MM Flow Studio",
        "Yaesu 450° overlap (N-E-S-W-N-E)", "Az min / max", "El min / max",
        "Az Park", "El Park", "Az", "El", "Az %1° / El %2°",
        "QSO: %1 %2 — %3° — %4 km", "● TX", "… WAV", "■ RX",
        "▶ RX", "■ TX", "6m", "2m", ":</td><td>",
        "MeteoFax / HF WEFAX RX", "MadModem 0.5.1", "𝑥 Variable",
        "R skew", "B skew", "Antenna", "Macro %1", "Macros",
        "{MYCALL}, {MYNAME}, {MYQTH}, {LOC}, {CALL}, {NAME}, {QTH}, {RST},",
    }
    def intentionally_same(key: str, value: str) -> bool:
        if value in same_ok_values:
            return True
        if key.startswith("placeholder."):
            return True
        if key.startswith("button.transport_"):
            return True
        if key.startswith("log.ft_osd_gf2_lab_"):
            return True
        if key.startswith("text.") and (value.startswith("<") or "background-color:" in value):
            return True
        if any(token in key for token in ["bpsk", "qpsk", "mfsk", "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "70cm", "23cm"]):
            return True
        return False

    fallback_counts = {}
    for lang in LANGS:
        if lang == "en":
            continue
        same = [k for k, v in dictionaries[lang].items()
                if en.get(k) == v and any(ch.isalpha() for ch in v)
                and not updater.TECH_RE.match(v)
                and not intentionally_same(k, v)]
        fallback_counts[lang] = len(same)

    print("Localization audit")
    print(f"  harvested keys: {len(expected)}")
    for lang in LANGS:
        print(f"  {lang}: {len(dictionaries[lang])} keys")
    for lang, count in fallback_counts.items():
        print(f"  {lang}: {count} non-technical English fallbacks")
    if errors:
        print("FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print("OK: all runtime dictionaries match the harvested key set.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
