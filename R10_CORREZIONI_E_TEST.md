# R10 - Contest RTTY, Windows CI e long-run UI hardening

- Windows/MSYS2 Python 3.14 architecture guard: explicit UTF-8 reads in check_rx_cat_workers.py.
- Removed the duplicate RTTY Contest macro bank from the side Contest tab.
- The single central RTTY macro bar now switches to the active contest macros while Contest mode is enabled.
- Added a discrete Edit macros control. Standard macros are stored in Text/macroLabels + Text/macroTexts; contest edits are per-profile overrides in QSettings and do not modify the shipped rules files.
- Added a Contest-only RTTY Quick Reply editor with independent TX arrow. It remains writable during TX; pressing its arrow while TX is active queues that reply for the next transmission without overwriting the main TX editor.
- RTTY quick reply uses a TX text override at modulator construction, preserving the main TX buffer.
- Long-run text terminal hardening: RX documents are bounded by actual character count (100000), not only QTextDocument block count. This prevents continuous RTTY/CW streams with few CR/LF characters from growing the GUI document indefinitely.
- Existing waterfall GPU row queue and DSP audio dispatcher were verified already bounded; no destructive DSP reset or arbitrary waterfall clearing was added.
- Updated contest layout guard to enforce the new single-bank architecture.
- Refreshed localization dictionaries after the new UI strings.

Source guard verification performed in this environment:
- scripts/run_guard_suite.py architecture: PASS
- scripts/run_guard_suite.py ui: PASS
- scripts/run_guard_suite.py ft: PASS (Unix-only subchecks skipped by platform rules where applicable)
- scripts/run_guard_suite.py release: PASS (Unix-only version shell check skipped by platform rules)

Full Qt compile was not possible in the packaging environment because Qt5 development CMake packages are not installed there. GitHub CI should perform the authoritative Qt5/MSYS2 compile.
