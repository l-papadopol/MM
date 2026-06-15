# Logbook ADIFMaster-style UI — v2.41

The logbook window now follows a more ADIFMaster-like layout while preserving MadModem's conservative ADIF handling.

## Visible interface

The logbook dialog has:

- a menu bar with File, Edit, Search and Tools menus;
- a compact icon toolbar for common commands;
- a spreadsheet-style table with sortable columns and alternating rows;
- a right-click context menu for selected rows;
- a status bar showing Ready, total QSOs, visible rows, selected rows and ADIF extra-field count.

## Supported row actions

Selected rows can be:

- copied as CSV;
- copied as ADIF;
- saved as CSV;
- saved as ADIF;
- deleted after confirmation.

The full logbook and the current search result can still be exported as ADIF. Print/PDF output still uses the existing record-scope chooser.

## ADIF safety

The UI does not normalise or truncate unknown ADIF tags. Exported ADIF still uses the preserving backend introduced in v2.35.

## Large logbooks

Sorting and selection are UI-level operations. When the table is sorted, selected row actions still refer to the correct underlying logbook entries.
