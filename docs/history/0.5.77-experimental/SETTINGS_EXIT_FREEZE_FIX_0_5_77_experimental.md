# Settings exit freeze fix — 0.5.77.experimental

This experimental patch reduces UI stalls when closing the Settings dialog.

## Cause

The post-Settings path always performed expensive refresh operations, even when the user only changed an unrelated option:

- full audio input enumeration
- full audio output enumeration
- full serial-port enumeration
- logbook highlight refresh
- QSO map refresh

On Windows, audio/serial enumeration can block the GUI for one or more seconds.

## Fix

- Do not call `refreshDevices()` unconditionally after Settings closes.
- If only the selected device changed, update the visible combo selection without rescanning the OS device list.
- Refresh logbook highlights only when logbook/highlight/watchlist inputs changed.
- Refresh QSO maps only when logbook path or home station location changed.
- Defer optional logbook/map refresh with a short `QTimer::singleShot()` so the Settings window can close cleanly first.
- Add a timing log line:

```text
Settings updated in N ms.
```

Use the explicit device refresh control if a newly plugged audio/serial device must be enumerated.
