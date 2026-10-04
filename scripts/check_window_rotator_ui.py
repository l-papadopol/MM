#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'main.cpp').read_text()
chrome=(root/'utils/CockpitTheme.cpp').read_text()
assert 'MadModemUi::showMainWindowMaximized(&window);' in main
assert 'showFullScreen' not in main and 'forceMainWindowFullScreen' not in main
assert 'showFullScreen' not in chrome
assert 'MadModemUi::showMainWindowMaximized(m_owner);' in chrome
assert 'setAttribute(Qt::WA_TransparentForMouseEvents, true)' in chrome
panel=(root/'rotator/CatRotatorPanel.cpp').read_text()
recall=panel.split('bool CatRotatorPanel::recallManualPreset',1)[1].split('void CatRotatorPanel::editManualPreset',1)[0]
assert 'setAzEl(' not in recall and 'm_controller->' not in recall
assert 'az>m_spinAz->maximum()' in recall and 'el>m_spinEl->maximum()' in recall
assert 'RotatorManualPresets/%1/%2' in panel
assert 'QOpenGLWidget' not in (root/'rotator/NavballWidget.h').read_text()
print('PASS: normal maximized startup, transparent chrome, profile-specific presets, no motion during recall')
