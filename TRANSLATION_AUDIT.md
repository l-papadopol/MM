# MadModem translation audit

Release: **0.5.8**

Runtime UI dictionaries:

- `translations/ui_en.ini`
- `translations/ui_it.ini`
- `translations/ui_fr.ini`
- `translations/ui_de.ini`
- `translations/ui_no.ini`
- `translations/ui_cs.ini`

Each dictionary contains **1821 canonical keys** in identical order, with no
missing, extra, duplicate or empty values. Qt placeholders are preserved.

Current reviewed-value coverage:

| Language | Localized values | Shared technical/coherent English values |
|---|---:|---:|
| Italian | 1457 | 364 |
| French | 1399 | 422 |
| German | 1414 | 407 |
| Norwegian | 1379 | 442 |
| Czech | 1418 | 403 |

The 0.5.8 R10 review removed the old word-by-word substitution fallback. It
could turn an English sentence into an unreadable mixture of two languages.
Translations are now complete, reviewed sentences stored by stable key. When a
new sentence has not yet been reviewed, MadModem keeps coherent English rather
than inventing a hybrid translation.

Operator-facing FT, slot/TX state, audio-device, PTT safety, file/image,
scheduler, rotator and QSO controls have exact translations in Italian, French,
German, Norwegian and Czech. Protocol names, callsign fields, units and developer
diagnostics may remain shared technical English by design. Standalone tests are
excluded because their console output is not application UI.

Localized user manuals are under `docs/help/<language>/`.

Run:

```bash
python3 tools/update_ui_translations.py
python3 tools/audit_ui_translations.py
python3 tools/audit_localization.py
```

The audit checks parity, order, non-empty values, placeholders, canonical
English, retired keys, reviewed operator coverage and mixed-language sentence
fragments for all six languages.
