#!/usr/bin/env python3
"""Static architecture guard for outbound QSO UDP logging.

This is part of the consolidated architecture suite, not a separate CTest.
It protects the important ownership rule: ADIF append succeeds first, then one
best-effort WSJT-X/JTDX Logged ADIF notification is emitted from both live QSO
logging paths.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")

errors: list[str] = []
settings_h = text("settings/AppSettings.h")
settings_cpp = text("settings/AppSettings.cpp")
dialog = text("dialogs/AppSettingsDialog.cpp")
main = text("mainwindow.cpp")
broadcaster = text("network/QsoUdpBroadcaster.cpp")
cmake = text("CMakeLists.txt")

checks = [
    ("bool logbookUdpEnabled = false;" in settings_h, "UDP logging must be opt-in"),
    ('QString logbookUdpServer = "127.0.0.1";' in settings_h, "default UDP destination must be loopback"),
    ("int logbookUdpPort = 2237;" in settings_h, "default UDP port must be 2237"),
    ('"Logbook/udpEnabled"' in settings_cpp and '"Logbook/udpServer"' in settings_cpp and '"Logbook/udpPort"' in settings_cpp,
     "UDP settings must be persisted"),
    ("m_chkLogbookUdpEnabled" in dialog and "m_editLogbookUdpServer" in dialog and "m_spinLogbookUdpPort" in dialog,
     "Settings UI must expose enable/server/port controls"),
    ("0xadbccbdaU" in broadcaster and "kWsjtSchema = 3U" in broadcaster and "kWsjtLoggedAdifType = 12U" in broadcaster,
     "broadcaster must emit WSJT-X schema-3 Logged ADIF messages"),
    ("QDataStream::Qt_5_4" in broadcaster and "QDataStream::BigEndian" in broadcaster,
     "WSJT-X QDataStream wire encoding must be explicit"),
    ("AdifLogbook::entryToAdif(entry)" in broadcaster and "<EOH>" in broadcaster,
     "Logged ADIF payload must be a complete ADIF file"),
    ("network/QsoUdpBroadcaster.cpp" in cmake and "network/QsoUdpBroadcaster.h" in cmake,
     "UDP broadcaster must be compiled into MadModem"),
]
for ok, message in checks:
    if not ok:
        errors.append(message)

# There are intentionally exactly two direct runtime append sites: manual/text
# modes and FT auto-log.  Each must notify only after append succeeds.
append_positions = []
start = 0
needle = "if (!m_logbook.append(entry, &error))"
while True:
    pos = main.find(needle, start)
    if pos < 0:
        break
    append_positions.append(pos)
    start = pos + len(needle)
if len(append_positions) != 2:
    errors.append(f"expected exactly two direct QSO append paths, found {len(append_positions)}")
else:
    for index, pos in enumerate(append_positions, 1):
        call = main.find("broadcastLoggedQsoUdp(entry);", pos)
        next_append = append_positions[index] if index < len(append_positions) else len(main)
        if call < 0 or call >= next_append:
            errors.append(f"QSO append path {index} does not broadcast after successful append")

helper_pos = main.find("void MainWindow::broadcastLoggedQsoUdp(const LogbookEntry &entry)")
if helper_pos < 0:
    errors.append("MainWindow UDP broadcast helper is missing")
else:
    helper = main[helper_pos:helper_pos + 2400]
    if "if (!m_settings.logbookUdpEnabled)" not in helper:
        errors.append("UDP helper does not honor the opt-in setting")
    if "QsoUdpBroadcaster::sendLoggedAdif" not in helper:
        errors.append("UDP helper is not wired to the broadcaster")
    if 'uiText("qso_udp_failed"' not in helper:
        errors.append("UDP failure is not reported without undoing local logging")

if errors:
    print("QSO UDP logging guard FAILED:")
    for error in errors:
        print(f"- {error}")
    sys.exit(1)

print("QSO UDP logging guard passed.")
