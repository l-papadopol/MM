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
    "FT split setting is persisted and exposed in Radio/CAT UI": (
        "hamlibSplitOperation" in all_relevant
        and "Hamlib/splitOperation" in all_relevant
        and 'QStringLiteral("fake_it")' in all_relevant
        and 'QStringLiteral("rig")' in all_relevant
        and "Split operation" in all_relevant
    ),
    "FT split keeps actual TX AF in the 1500..2000 Hz corridor": (
        "while (audioHz < 1500)" in main
        and "audioHz += 500;" in main
        and "shift -= 500;" in main
        and "while (audioHz > 2000)" in main
        and "audioHz -= 500;" in main
        and "shift += 500;" in main
    ),
    "FT split CAT transaction is prepared before PTT and restored after PTT": (
        "prepareFtSplitForTx()" in main
        and main.find("prepareFtSplitForTx()") < main.find("m_pendingFt8PttKeyed = keyPttForTx();", main.find("void MainWindow::prearmFtPreparedSlotTransmit"))
        and "restoreFtSplitAfterTx();" in main
        and "beginFtSplitTx" in controller
        and "endFtSplitTx" in controller
    ),
    "Fake It transient CAT frequency is hidden from ordinary CAT polling": (
        "if (m_ftSplitTxActive)" in controller
        and "CAT frequency change blocked while an FT split TX transaction is active" in controller
    ),
    "Rig split follows Hamlib/WSJT-X logical VFO abstraction with no non-split fallback": (
        "rig_get_split_vfo" in controller
        and "rig_set_split_vfo" in controller
        and "rig_set_split_freq" in controller
        and "rig_get_split_mode" in controller
        and "rig_set_split_mode" in controller
        and "wsjtLikeFtRigSplitTxVfo" in controller
        and "rig->state.vfo_list" in controller
        and "ScopedHamlibTxVfo" in controller
        and "pairedSplitVfo" not in controller
        and "rig_set_freq(rig, txVfo" not in controller
        and "refusing a second concurrent owner" in controller
        and "no non-split fallback was used" in main
    ),
    "Rig split TX mode changes are fail-closed unless the previous split mode is restorable": (
        controller.count("splitTx && !m_havePreTxMode") >= 2
        and "could not snapshot the split TX mode/passband before changing the transmit audio route; TX aborted" in controller
        and "rig_get_split_mode" in controller
        and "rig_set_split_mode" in controller
    ),
    "Split is opt-in and the stable default CAT path remains unchanged": (
        'QString hamlibSplitOperation = "none";' in all_relevant
        and 'if (operation == QStringLiteral("none")) return true;' in main
    ),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'OK' if ok else 'FAIL'}] {name}")

if failed:
    print(f"CAT/FT band sync audit failed: {len(failed)} check(s).", file=sys.stderr)
    raise SystemExit(1)

print("CAT/FT band sync audit passed.")
