# Localization completion pass — v2.32

MadModem uses a lightweight runtime INI dictionary instead of Qt `.ts/.qm` catalogs.  This pass makes that system more complete and easier to maintain.

## What is covered

The dictionary generator now harvests these user-visible sources:

- explicit `uiText("key", "English fallback")` calls;
- dialog-local `L("English source")` calls;
- Qt Designer `.ui` visible strings;
- common widget construction and property setters such as `QLabel`, `QPushButton`, `QCheckBox`, `QGroupBox`, `QAction`, `setText`, `setToolTip`, `setStatusTip`, `setWindowTitle`, `addItem`;
- static `appendLog()` messages where they can be harvested safely;
- common runtime `Caption: value` log patterns, translating the caption while preserving dynamic device names, paths, callsigns, frequencies and error strings.

All six runtime dictionaries are synchronized at 725 keys.

## Intentional non-translation policy

Some tokens are deliberately kept unchanged because they are radio/software standard terms or compact protocol labels:

- modes: FT8, FT4, SSTV, WEFAX, RTTY, CW, Hell, PSK, MFSK;
- radio workflow tokens: RX, TX, PTT, CAT, QSO, QTH, ADIF, UTC;
- units: Hz, kHz, MHz, dB, ppm, WPM, LPM;
- callsigns, locators, file extensions and serial/CAT backend identifiers.

## Maintenance workflow

After adding visible UI/log/dialog text:

```bash
python3 tools/update_ui_translations.py
python3 tools/audit_localization.py
```

`audit_localization.py` verifies that every language file contains the same harvested key set.  It also reports non-technical English fallbacks so they can be reviewed gradually without breaking technical labels.

## Dialogs

`LogbookDialog` now supports the same translator callback already used by other settings dialogs.  MainWindow passes `uiTextFromSource("text", source)` into the dialog, so labels, message boxes and import/export status strings follow the selected runtime language.

## Current limitation

The audit is static and conservative. It cannot infer every dynamically formatted string built from multiple fragments, so runtime logs still use caption/prefix localization for many diagnostic lines while preserving the dynamic details verbatim.
