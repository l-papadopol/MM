# FT/MSK/Q65 decode-table readability — 0.5.78-lab17

0.5.78-lab17 extends the existing decode-table readability controls without changing the decoder hot path.

## Settings

- `Display/decodeTablePreset` — `custom`, `compact`, `normal`, `large`, `low_vision`; default `normal`.
- `Display/decodeTableFontFamily` — optional table-only font family; empty means inherit the global/application UI font.
- `Display/decodeTableFontPointSize` — 8..18 pt; default 9 pt.
- `Display/decodeTableRowHeightPx` — 16..48 px; default 20 px.

## Presets

- Compact: global UI font, 8 pt, 16 px.
- Normal: global UI font, 9 pt, 20 px.
- Large: global UI font, 12 pt, 28 px.
- Low vision: global UI font, 14 pt, 36 px.
- Custom: any manual change to font family, size or row height switches the selector to Custom.

## Application scope

`MainWindow::applyDecodeTableVisualSettings()` applies the settings to FT8/FT4 RX and QSO-history tables, FT/MSK/Q65 standard-message tables, and MSK144/Q65 RX tables. 0.5.78-lab17 also fixes the vertical header section mode so newly inserted RX rows inherit the configured row height while RX is active.

## Localization

The new labels/tooltips are localized in `translations/ui_en.ini`, `ui_it.ini`, `ui_fr.ini`, `ui_de.ini`, `ui_no.ini` and `ui_cs.ini`.
