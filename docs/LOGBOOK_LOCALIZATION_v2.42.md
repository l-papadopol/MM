# Logbook localization v2.42

The v2.41 logbook introduced an ADIFMaster-style UI with menu bar, toolbar,
context menu, status bar and selected-row export/copy commands. v2.42 completes
that UI layer for the existing MadModem runtime dictionary system.

## Scope

Localized elements include:

- File/Edit/Search/Tools menu titles.
- Toolbar actions and tooltips.
- Context-menu actions.
- Advanced search labels and placeholders.
- Worked-call strike-through checkbox.
- Status bar counters: total QSOs, shown rows, selected rows, extra ADIF fields.
- Message boxes for import/export/delete/print/PDF.
- File-dialog titles and filters.
- Print/PDF report labels.

Technical ADIF column identifiers exported as CSV remain canonical where needed
(CALL, GRIDSQUARE, RST_SENT, RST_RCVD, BAND, MODE, FREQ, NAME, QTH, COMMENT).

## Motto removal

The old rotating menu-bar motto has been removed from the visible UI. The menu
bar corner is kept empty so it does not distract from normal operation or waste
horizontal space.
