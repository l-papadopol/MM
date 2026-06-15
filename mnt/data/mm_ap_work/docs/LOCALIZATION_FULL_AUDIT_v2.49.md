# Localization full audit — v2.49

The v2.49 pass performs a full-source localization audit over the current MadModem tree.

## Scope

The audit covers strings from:

- Qt Designer `.ui` files.
- Explicit `uiText(key, fallback)` calls.
- Dialog-local `L("...")` translator calls.
- Common user-visible C++ calls such as:
  - `appendLog(...)`
  - `QMessageBox::...`
  - `setText(...)`
  - `setWindowTitle(...)`
  - `setToolTip(...)`
  - `setStatusTip(...)`
  - `addItem(...)`
  - `addAction(...)`
  - `addMenu(...)`
  - common table/header string patterns.

## Runtime dictionaries

All runtime language dictionaries now contain the same harvested key set:

- `translations/ui_en.ini`
- `translations/ui_it.ini`
- `translations/ui_fr.ini`
- `translations/ui_de.ini`
- `translations/ui_no.ini`
- `translations/ui_cs.ini`

The key count is 1003 for each language in this revision.

## Intentional unchanged strings

Some values deliberately remain identical across languages because translating them would be incorrect or confusing:

- Mode names and acronyms: FT8, FT4, RTTY, SSTV, WEFAX, PSK, QPSK, MFSK, CW, CAT, PTT, QSO, QTH, ADIF.
- Units and labels: Hz, kHz, MHz, dB, ppm, px, WPM, baud.
- Ham-radio band labels: 160m, 80m, 40m, 20m, 70cm, etc.
- Brand/model text such as Kenwood DATA SEND routes.
- Internal HTML fragments used to build rich labels.
- Test harness/debug-only strings that are not part of normal UI.

## Runtime log handling

MadModem still preserves dynamic values such as file paths, device names, callsigns, frequencies and raw backend error text. Stable prefixes and common runtime phrases are localized while the dynamic tail is left untouched.

Example:

- English: `Audio input: IN TS-890 (USB AUDIO CODEC)`
- Italian: `Ingresso audio: IN TS-890 (USB AUDIO CODEC)`

This avoids corrupting device names, callsigns and ADIF values while still localizing the visible UI/log sentence.
