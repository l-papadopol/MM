#!/usr/bin/env python3
"""Source guard for the narrow RTTY contest side-panel layout."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "mainwindow.cpp").read_text(encoding="utf-8")

start = MAIN.index("m_tabRttyContest = new QWidget")
end = MAIN.index("connect(m_chkRttyContestMode", start)
contest = MAIN[start:end]

checks = {
    "contest page follows the narrow viewport": (
        "setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff)" in contest
        and "setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred)" in contest
    ),
    "only vertical scrolling remains": (
        "setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded)" in contest
    ),
    "contest header is a two-column form": (
        "addWidget(m_chkRttyContestMode, 0, 0, 1, 2)" in contest
        and "addWidget(m_cmbRttyContest, 1, 1)" in contest
        and "addWidget(m_btnRttyContestReloadRules, 1, 2)" not in contest
        and "addWidget(m_btnRttyContestNewSession, 2, 2)" not in contest
    ),
    "service buttons keep text-sized width": (
        "setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed)" in contest
        and "addWidget(m_btnRttyContestReloadRules, 3, 0, 1, 2, Qt::AlignRight)" in contest
        and "addWidget(m_btnRttyContestNewSession, 4, 0, 1, 2, Qt::AlignRight)" in contest
    ),
    "QSO editor is a single label-field column": (
        "addWidget(m_editRttyContestQsoMode, 2, 1)" in contest
        and "addWidget(m_editRttyContestQsoRstReceived, 4, 1)" in contest
        and "setColumnStretch(3, 1)" not in contest
        and "addWidget(m_editRttyContestQsoMode, 1, 3)" not in contest
    ),
    "QSO controls may shrink inside 300 px": (
        "edit->setMinimumWidth(0)" in contest
        and "edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed)" in contest
        and "label->setWordWrap(true)" in MAIN
    ),
    "contest chrome uses compact padding": (
        'setProperty("cockpitCompactPanel", true)' in contest
        and "padding-left: 4px" in contest
        and "setContentsMargins(4, 4, 4, 4)" in contest
    ),
    "macro buttons have no forced horizontal minimum": (
        "button->setMinimumWidth(0)" in contest
        and "button->setMinimumHeight(24)" in contest
        and "addWidget(button, i / 3, i % 3)" in contest
    ),
    "short tab caption is used": (
        'insertTab(insertIndex, m_tabRttyContest, uiText("rtty_contest", "Contest"))' in contest
        and 'setTabText(i, uiText("rtty_contest", "Contest"))' in MAIN
    ),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'OK' if ok else 'FAIL'}] {name}")

if failed:
    print(f"RTTY contest layout audit failed: {len(failed)} check(s).", file=sys.stderr)
    raise SystemExit(1)

print("RTTY contest narrow-layout audit passed.")
