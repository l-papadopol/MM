#!/usr/bin/env python3
"""Source audit for the 0.5.78 linear FT sequencer integration.

This is intentionally structural: the full Qt application must still be built by
CI on Linux/Windows/macOS.  The audit prevents the removed fallback paths from
being reintroduced silently.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"FAIL: {message}")
        raise SystemExit(1)
    print(f"PASS: {message}")

main = read("mainwindow.cpp")
main_h = read("mainwindow.h")
seq_h = read("modems/ft8/FtQsoSequencer.h")
seq_cpp = read("modems/ft8/FtQsoSequencer.cpp")
rx_h = read("modems/ft8/Ft8RxDecoder.h")
rx_cpp = read("modems/ft8/Ft8RxDecoder.cpp")
rig = read("rig/HamlibController.cpp")
resources_h = read("utils/SystemResourceManager.h")
resources_cpp = read("utils/SystemResourceManager.cpp")
test = read("tests/FtSequencerSelfTest.cpp")

require("reclaimFt8ActiveQsoAwayFromQrm" not in main + main_h,
        "AutoQSO reclaim/fallback path removed")

history_start = main.index("void MainWindow::handleFt8QsoHistoryDoubleClicked")
history_end = main.index("void MainWindow::handleFt8DecodeDoubleClicked", history_start)
history = main[history_start:history_end]
require("scheduleFt8SequencerMessage" not in history,
        "QSO activity/history selection cannot arm TX")
require("press TX to transmit" in history,
        "QSO activity/history selection explicitly requires TX")

require("InterruptAndArmRow" in seq_h and "context.txActive ? Action::InterruptAndArmRow" in seq_cpp,
        "terminal decode can interrupt an obsolete active FT frame")
require("final acknowledgement received while final 73 is on air; no additional TX scheduled" in seq_cpp,
        "final 73 cannot create another 73 retry")
require("RR73 during obsolete R-report TX -> interrupt and arm one 73" in test,
        "sequencer regression covers late RR73 during TX")
require("repeated RR73 while final 73 is on air -> no additional TX" in test,
        "sequencer regression covers repeated terminal decode")

require("setLiveInputEnabled" in rx_h and "if (!m_liveInputEnabled)" in rx_cpp,
        "FT decoder owns an explicit live-input gate")
require(re.search(r"connect\(m_audioEngine, &AudioEngine::audioBlockReady,\s*m_ft8RxDecoder, &Ft8RxDecoder::processAudioBlock", main, re.S) is not None,
        "AudioEngine routes FT blocks directly to the decoder thread")
ft_branch_start = main.index("if (Ft8Mode::isFamilyMode(modeName))", main.index("void MainWindow::handleRxAudioBlock"))
ft_branch = main[ft_branch_start:main.index("if (!m_rxRunning", ft_branch_start)]
require("QMetaObject::invokeMethod" not in ft_branch and "m_ft8RxDecoder->processAudioBlock" not in ft_branch,
        "MainWindow no longer duplicates FT audio transport")

require("m_ftTxWorker, &FtTxWorker::audioBlockReady" not in main,
        "generated FT TX audio is not injected into the RX waterfall")
require("Keep the last RX waterfall frame frozen throughout FT TX" in main,
        "generic TX monitor also freezes for FT family modes")

require("maybeAdjustLocked" not in resources_h + resources_cpp and
        "m_liveWorkerTarget" not in resources_h + resources_cpp,
        "adaptive live-worker controller and mutable target removed")
require("m_maxLiveWorkers = qMax(1, logical - m_reservedLogicalProcessors)" in resources_cpp and
        "static_cast<double>(live) * 0.40" in resources_cpp and
        "fixed live topology budget" in resources_cpp,
        "live worker budget uses the proven topology while telemetry continues")

require('if (m_config.txAudioRoute != QStringLiteral("default"))' in rig,
        "default CAT TX route does not query/restore rig mode")
require("m_txModeChangedByMadModem" in rig,
        "rig mode is restored only when MadModem actually changed it")

clock_setup_start = main.index("void MainWindow::setupFt8Page")
clock_setup_end = main.index("void MainWindow::setupProcessingConnections", clock_setup_start)
clock_setup = main[clock_setup_start:clock_setup_end]
require("sequencerLayout->addWidget(clockGroup)" in clock_setup,
        "FT8/FT4 period clock is embedded under the sequencer")
require("statusTabLayout->insertWidget" not in clock_setup,
        "FT period clock is no longer placed in the Status tab")
require("m_lblFt8PeriodStatus" not in main + main_h and "utcCaption" not in clock_setup,
        "redundant I/II range and UTC caption labels removed from the FT clock")

require("m_pendingFt8TxTag.trimmed().toUpper() == txTag" not in main,
        "TX/SEQ/RETRY tag is metadata, not pending-plan identity")

print("FT linear sequencer source audit: PASS")
