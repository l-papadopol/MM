#!/usr/bin/env python3
"""Guard compact operator copy and runtime localization boundaries."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "mainwindow.cpp").read_text(encoding="utf-8", errors="replace")


def require(fragment: str, label: str, errors: list[str]) -> None:
    if fragment not in MAIN:
        errors.append(label)


def main() -> int:
    errors: list[str] = []

    require("constexpr int kMaximumHintCharacters = 132;",
            "long hover/status help is not bounded", errors)
    require('m_lblFt8TxBanner->setMaximumWidth(360);',
            "FT banner can overflow the narrow side panel", errors)
    require('m_lblFt8TxBanner->setText(uiText("ft_tx_banner_rx", "RX"));',
            "idle FT banner is not compact", errors)
    require('m_lblFt8WindowStatus->setText(QStringLiteral("%1 · %2 · %3 s")',
            "FT slot line is not the compact period/state/countdown form", errors)
    require('uiText("decoder_ready_short", "%1 ready")',
            "decoder state still uses verbose per-mode prose", errors)
    require('uiText("rtty_waterfall_text_overlay", "Show decoded text on waterfall")',
            "RTTY waterfall text does not have an operator toggle", errors)

    # Text assigned after the initial object-tree translation must be routed
    # through uiText()/MadModemI18n. Pure meter/scale tokens are language-neutral.
    neutral_dynamic = {"WAV", "-inf dB"}
    dynamic_literal = re.compile(
        r"(?:setText|setToolTip|setStatusTip|setPlaceholderText|setWindowTitle)"
        r"\(\s*(?:QStringLiteral\()?\"([^\"]+)\""
    )
    for path in [ROOT / "mainwindow.cpp", *sorted((ROOT / "dialogs").glob("*.cpp"))]:
        source = path.read_text(encoding="utf-8", errors="replace")
        for match in dynamic_literal.finditer(source):
            value = match.group(1)
            if (value in neutral_dynamic or
                    re.fullmatch(r"[x%0-9 .·|/+\\n-]*(?:Hz|dB|m|s)?", value)):
                continue
            if not any(ch.isalpha() for ch in value):
                continue
            line = source.count("\n", 0, match.start()) + 1
            errors.append(f"{path.relative_to(ROOT)}:{line}: runtime text bypasses localization: {value!r}")

    # File/message/input/color dialogs are created after the language pass, so
    # a standalone quoted argument inside the call would remain English.
    standalone_literal = re.compile(r'^\s*"([A-Za-z][^"\\]*)"\s*,?\s*$', re.MULTILINE)
    for path in [ROOT / "mainwindow.cpp", *sorted((ROOT / "dialogs").glob("*.cpp"))]:
        source = path.read_text(encoding="utf-8", errors="replace")
        lines = source.splitlines()
        for index, line_text in enumerate(lines):
            if not re.search(r"Q(?:File|Message|Input|Color)Dialog::[A-Za-z]+\s*\(", line_text):
                continue
            # The project's dialog calls put user-facing literal arguments on
            # their own lines. Inspect a bounded call window; a translated call
            # never contains such a standalone raw string.
            call_text = "\n".join(lines[index:index + 12])
            bad = standalone_literal.search(call_text)
            if bad:
                line_number = index + call_text[:bad.start()].count("\n") + 1
                errors.append(f"{path.relative_to(ROOT)}:{line_number}: dialog text bypasses localization: {bad.group(1)!r}")

    if errors:
        print("Operator UI copy guard FAILED:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Operator UI copy guard passed: compact FT state and runtime localization are enforced.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
