# Logbook field configuration — v2.47

MadModem now separates ADIF preservation from logbook presentation.

The ADIF backend keeps imported fields so round-trip export remains conservative, while the logbook dialog lets the operator choose which fields are displayed and printed.

## UI

Open the logbook and choose:

- Tools -> Visible/print fields...
- the toolbar field icon
- or the table context menu item

The dialog presents ADIF fields in three columns with checkboxes. The setting is saved in `settings.mad`.

## Scope

The selected fields affect:

- logbook table columns,
- CSV copy/export,
- PDF output,
- printer output.

They do not delete ADIF fields from imported records.

## Default hidden fields

These fields are considered noisy/importer-specific and are hidden by default, although the operator may enable them:

- `APP_QRZ_*`
- `LAT`, `LON`
- `MY_*`
- `*_QSO_*`
- `QSL_*`
- `*_QSL_*`
- `RX_PWR`, `TX_PWR`
- `STATION_CALLSIGN`
- `IOTA`, `CNT`, `STATE`
- `CONTEST_ID`
- `SRX`, `STX`, `PFX`

## Print/PDF

Print/PDF output uses landscape orientation and paginates rows as a real table. Long cell values are shortened in the printed table so a QSO remains one row instead of expanding across pages.
