#!/usr/bin/env python3
"""Guard the CW operator UI: macro bank plus explicit RX A/RX B/TX waterfall ownership."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
source = (root / "mainwindow.cpp").read_text(encoding="utf-8")
header = (root / "mainwindow.h").read_text(encoding="utf-8")
waterfall_h = (root / "widgets" / "WaterfallWidget.h").read_text(encoding="utf-8")
waterfall_cpp = (root / "widgets" / "WaterfallWidget.cpp").read_text(encoding="utf-8")
settings_h = (root / "settings" / "AppSettings.h").read_text(encoding="utf-8")
settings_cpp = (root / "settings" / "AppSettings.cpp").read_text(encoding="utf-8")

errors = []

def section(start: str, end: str) -> str:
    i = source.find(start)
    if i < 0:
        errors.append(f"missing section start: {start}")
        return ""
    j = source.find(end, i + len(start))
    if j < 0:
        errors.append(f"missing section end after: {start}")
        return source[i:]
    return source[i:j]

cw_page = section("QWidget *MainWindow::createCwTerminalPage()", "void MainWindow::placeQsoFormInModePanel")
setup = section("void MainWindow::setupTextTerminalPages()", "void MainWindow::updateCentralDisplayForMode")
connections = section("void MainWindow::setupUiConnections()", "void MainWindow::populateWeatherFaxLinePresets")
refresh = section("void MainWindow::refreshTextMacroButtons()", "QString MainWindow::expandTextTemplate")

checks = [
    ("CW macro layout exists", "QGridLayout *macroLayout = new QGridLayout();" in cw_page),
    ("CW macro bank is initialized once in its page", "m_cwMacroButtons.clear();" in cw_page),
    ("CW macro bank contains six buttons", "for (int i = 0; i < 6; ++i)" in cw_page),
    ("CW macro buttons are retained", "m_cwMacroButtons.append(button);" in cw_page),
    ("CW macro row is visible above TX", "layout->addLayout(macroLayout);" in cw_page and cw_page.find("layout->addLayout(macroLayout);") < cw_page.find("layout->addLayout(inputLayout);")),
    ("setup does not destroy CW macro bank", "m_cwMacroButtons.clear();" not in setup),
    ("CW macro buttons transmit shared text macros", "for (int i = 0; i < m_cwMacroButtons.size(); ++i)" in connections and "sendTextMacro(i);" in connections),
    ("CW macro labels use shared macro settings", "applyLabels(m_cwMacroButtons);" in refresh),
    ("CW macro bank remains available outside Contest", "applyLabels(m_cwMacroButtons);" in refresh and "button->setEnabled(true);" in refresh),
    ("CW macro bank switches to contest profile when Contest is active", "applyRttyContestLabels(m_cwMacroButtons);" in refresh),
    ("CW macro clicks route to contest macros only while CW Contest is active", "sendRttyContestMacro(index);" in source and "cwContestActive" in source),
    ("CW TX tone has a dedicated editable control", "m_spinCwTxToneHz = new QSpinBox" in source and "cw_tx_tone" in source),
    ("CW TX tone is persisted independently", "int cwTxToneHz = 1000;" in settings_h and "CW/txToneHz" in settings_cpp),
    ("CW waterfall exposes a red TX marker", 'tx.label = QStringLiteral("TX")' in source and "m_spinCwTxToneHz" in source and "QColor(255, 80, 80)" in source),
    ("CW transmitter uses the dedicated TX tone", "new CwTransmitter" in source and "m_spinCwTxToneHz != nullptr ? static_cast<double>(m_spinCwTxToneHz->value())" in source),
    ("CW enables opt-in waterfall chord detection", "setChordClickEnabled(cwMode);" in source and "frequencyChordClicked" in waterfall_h),
    ("CW TX chord is wired to the dedicated handler", "frequencyChordClicked" in source and "handleWaterfallFrequencyChordClicked" in header),
    ("waterfall chord requires both left and right buttons", "buttons.testFlag(Qt::LeftButton) && buttons.testFlag(Qt::RightButton)" in waterfall_cpp),
    ("CW single clicks are delayed only for chord disambiguation", "m_chordClickEnabled" in waterfall_cpp and "m_chordClickTimer.start();" in waterfall_cpp),
]

for label, ok in checks:
    if ok:
        print(f"[OK] {label}")
    else:
        errors.append(label)

if errors:
    print("CW macro UI guard FAILED:")
    for error in errors:
        print(f"- {error}")
    sys.exit(1)

print("CW operator UI guard passed.")
