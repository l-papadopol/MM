#!/usr/bin/env python3
"""Audit MadModem runtime UI dictionaries.

Checks canonical key parity/order, duplicate or empty entries, Qt placeholder
preservation and removal of retired MIND/DDSP UI vocabulary.  English-identical
values are reported as a maintenance metric but are not automatically errors,
because protocol names and radio terminology are intentionally shared.
"""
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

from update_ui_translations import LANGS, TRANS_DIR, harvest_keys, read_ini

PLACEHOLDER_RE = re.compile(r"%(?:L?\d+|n)")
RETIRED_RE = re.compile(r"(?:\bMIND\b|\bDeepDsp\b|\bDDSP\b)", re.IGNORECASE)


def raw_keys(path: Path) -> list[str]:
    keys: list[str] = []
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("[") or "=" not in line:
            continue
        keys.append(line.split("=", 1)[0].strip())
    return keys


def placeholders(text: str) -> Counter[str]:
    return Counter(PLACEHOLDER_RE.findall(text))


def main() -> int:
    canonical = harvest_keys()
    canonical_keys = list(canonical.keys())
    errors: list[str] = []
    dictionaries: dict[str, dict[str, str]] = {}

    for lang in LANGS:
        path = TRANS_DIR / f"ui_{lang}.ini"
        if not path.exists():
            errors.append(f"{lang}: missing dictionary {path}")
            continue

        keys = raw_keys(path)
        duplicates = sorted(k for k, count in Counter(keys).items() if count > 1)
        if duplicates:
            errors.append(f"{lang}: duplicate keys: {', '.join(duplicates[:12])}")

        data = read_ini(path)
        dictionaries[lang] = data
        actual_keys = list(data.keys())
        missing = [k for k in canonical_keys if k not in data]
        extra = [k for k in actual_keys if k not in canonical]
        if missing:
            errors.append(f"{lang}: {len(missing)} missing canonical keys; first: {missing[:8]}")
        if extra:
            errors.append(f"{lang}: {len(extra)} obsolete/extra keys; first: {extra[:8]}")
        if actual_keys != canonical_keys:
            errors.append(f"{lang}: key order differs from the canonical harvest")

        empty = [k for k, value in data.items() if not value.strip()]
        if empty:
            errors.append(f"{lang}: {len(empty)} empty values; first: {empty[:8]}")

        retired = [k for k, value in data.items() if RETIRED_RE.search(k) or RETIRED_RE.search(value)]
        if retired:
            errors.append(f"{lang}: retired MIND/DDSP vocabulary remains; first: {retired[:8]}")

        placeholder_errors: list[str] = []
        for key, english in canonical.items():
            value = data.get(key, "")
            if placeholders(english) != placeholders(value):
                placeholder_errors.append(key)
        if placeholder_errors:
            errors.append(
                f"{lang}: placeholder mismatch in {len(placeholder_errors)} entries; "
                f"first: {placeholder_errors[:8]}"
            )

    if "en" in dictionaries:
        en = dictionaries["en"]
        print(f"Canonical keys: {len(canonical_keys)}")
        for lang in LANGS:
            data = dictionaries.get(lang)
            if data is None:
                continue
            identical = sum(1 for key in canonical_keys if data.get(key) == en.get(key))
            translated = len(canonical_keys) - identical if lang != "en" else 0
            if lang == "en":
                print(f"{lang}: {len(data)} keys; canonical source dictionary")
            else:
                print(
                    f"{lang}: {len(data)} keys; {translated} values differ from English; "
                    f"{identical} intentionally/shared-or-pending English values"
                )

    if errors:
        print("\nTranslation audit FAILED:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Translation audit PASSED: parity, order, non-empty values, placeholders and retired-key checks are clean.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
