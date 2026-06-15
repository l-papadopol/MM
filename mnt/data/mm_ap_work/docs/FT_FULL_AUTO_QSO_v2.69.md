# MadModem v2.69 - FT FULL AUTO QSO

This patch adds a supervised FT4/FT8 **FULL AUTO QSO** control inspired by
WSJT-Z Auto Call behaviour.

Behaviour:

- The feature is off at startup.
- When enabled, MM listens for decoded CQ messages.
- If no QSO/TX is already active, MM selects the CQ caller, moves RX to the
  decode frequency and, unless Hold TX Frequency is enabled, moves TX there too.
- MM chooses the opposite TX period from the decoded station slot.
- MM sends the normal Tx2 answer (`DX MYCALL MYGRID`).
- The existing WSJT-X-like auto-sequencer then completes report/R-report/RR73/73.
- Auto-log is forced on while FULL AUTO is enabled, so completed QSOs can be
  logged without a modal dialog.
- After completion, MM stays in RX and waits for the next CQ.
- Pressing STOP disables FULL AUTO and cancels pending/active FT TX state.

Safety policy:

This is intended for attended operation only. It deliberately does not add a
background scheduler, unattended time plan, remote-control daemon, or automatic
re-enable at application startup.
