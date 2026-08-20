#!/usr/bin/env python3
"""Guard the CW six-button macro bank and its single contest ownership path."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
source = (root / "mainwindow.cpp").read_text(encoding="utf-8")

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
    ("CW standard bank yields to contest bank", "button->setEnabled(!cwContestActive);" in refresh),
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

print("CW macro UI guard passed.")
