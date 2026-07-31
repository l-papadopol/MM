# 0.5.76e FT UI readability hotfix

## Period selectors

The FT first/second-period radio buttons were being disabled while a QSO/TX plan was armed. The cockpit theme painted disabled checked radio indicators almost black, making the selected period appear to disappear.

Fix: keep disabled checked `QRadioButton`/`QCheckBox` indicators visibly green in the cockpit theme. This preserves the lock semantics while keeping the active period readable.

## Decode table readability

Added persistent display settings:

- `Display/decodeTableFontPointSize` — 8..18 pt, default 9 pt.
- `Display/decodeTableRowHeightPx` — 16..48 px, default 20 px.

The settings are exposed in Settings → Logbook / FT colours → Decode table readability and applied to:

- FT received decodes;
- FT QSO/activity table;
- FT standard-message table;
- MSK144 RX and message tables;
- Q65 RX and message tables.

Lower row heights show more received decodes at once; larger font/row values are available for operators who need high readability.
