#!/usr/bin/env python3
"""Source-level guard for the bidirectional CAT/FT band contract."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


main = read("mainwindow.cpp")
controller = read("rig/HamlibController.cpp")
all_relevant = "\n".join(
    read(path)
    for path in (
        "mainwindow.cpp",
        "settings/AppSettings.h",
        "settings/AppSettings.cpp",
        "rig/HamlibController.h",
        "rig/HamlibController.cpp",
        "dialogs/RigControlSettingsDialog.h",
        "dialogs/RigControlSettingsDialog.cpp",
        "dialogs/AppSettingsDialog.cpp",
    )
)

checks = {
    "startup applies saved CAT settings": (
        "refreshDevices();\n    applyPersistentSettingsToRuntime();" in main
        and "if (catChanged) {\n        invokeRigConfigureFromSettings();" in main
    ),
    "saved CAT settings auto-connect when enabled": (
        "cfg.catEnabled = settings.hamlibCatEnabled;" in controller
        and "if (m_config.catEnabled) {\n        connectRig();" in controller
    ),
    "FT band selector commands a CAT QSY": (
        "qsyRigToSelectedFtBand();" in main
        and "invokeRigSetFrequency(targetHz);" in main
    ),
    "CAT frequency always synchronizes the FT band": (
        "void MainWindow::handleRigFrequencyChanged(double frequencyHz)" in main
        and "const QSignalBlocker blockBand(m_cmbFt8Band);" in main
        and "FT band synchronized" in main
    ),
    "obsolete optional CAT-to-band gate is absent": all(
        token not in all_relevant
        for token in (
            "hamlibUpdateFt8Band",
            "updateFt8BandFromCat",
            "m_chkUpdateFt8Band",
        )
    ),
    "QSY publishes actual Hamlib readback": (
        "m_lastFrequencyHz = 0.0;\n    pollNow();" in controller
        and "emit frequencyChanged(frequencyHz);" not in controller
    ),
    "QSY publishes actual HRD readback": (
        "m_lastFrequencyHz = 0.0;\n        return pollHrd();" in controller
    ),
    "CAT remains open and is polled": (
        "m_pollTimer->start();\n        pollNow();" in controller
        and "if (m_rig != nullptr)" in controller
    ),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'OK' if ok else 'FAIL'}] {name}")

if failed:
    print(f"CAT/FT band sync audit failed: {len(failed)} check(s).", file=sys.stderr)
    raise SystemExit(1)

print("CAT/FT band sync audit passed.")
