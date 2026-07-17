# MadModem 0.5.78 translation audit

MadModem uses runtime INI dictionaries instead of Qt `.ts` files. The canonical key set is harvested from visible C++/header strings and Qt Designer forms by `tools/update_ui_translations.py`.

## Supported languages

- English (`en`)
- Italian (`it`)
- French (`fr`)
- German (`de`)
- Norwegian (`no`)
- Czech (`cs`)

## 0.5.78 result

Each dictionary contains **1,683 canonical keys**. All six files have identical key order and no missing, extra, duplicate or empty entries. Qt placeholders such as `%1`, `%2` and `%n` are preserved. Retired MIND/DDSP UI vocabulary is absent.

Values identical to English are not automatically errors: mode names, acronyms, callsign/grid terminology, hardware identifiers and protocol labels are often intentionally shared. The current counts of values differing from English are:

| Language | Values differing from English | Shared or pending English values |
|---|---:|---:|
| Italian | 1,390 | 293 |
| French | 1,378 | 305 |
| German | 1,374 | 309 |
| Norwegian | 1,377 | 306 |
| Czech | 1,373 | 310 |

## Maintenance commands

```bash
python3 tools/update_ui_translations.py
python3 tools/audit_ui_translations.py
```

The update script preserves curated canonical translations, migrates useful values from old literal-source keys and removes obsolete extras. The audit fails on structural defects but reports English-identical values only as a review metric.

Localized user manuals are maintained separately in `docs/help/<language>/`. The Qt Help project and collection files use version 0.5.78; embedded HTML remains the fallback when `qhelpgenerator` is unavailable.
