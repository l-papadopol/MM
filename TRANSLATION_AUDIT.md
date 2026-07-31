# MadModem translation audit

Release: **0.5.78**

Runtime UI dictionaries:

- `translations/ui_en.ini`
- `translations/ui_it.ini`
- `translations/ui_fr.ini`
- `translations/ui_de.ini`
- `translations/ui_no.ini`
- `translations/ui_cs.ini`

Each dictionary contains **1730 canonical keys** in identical order, with no
missing, extra, duplicate or empty values. Qt placeholders are preserved.
Standalone tests are intentionally excluded from UI-string harvesting because
their console output is not application UI.

Localized user manuals are under `docs/help/<language>/`.

Run:

```bash
python3 tools/update_ui_translations.py
python3 tools/audit_localization.py
```

The current audit passes parity, order, non-empty values, placeholders and
retired-key checks for all six languages.
