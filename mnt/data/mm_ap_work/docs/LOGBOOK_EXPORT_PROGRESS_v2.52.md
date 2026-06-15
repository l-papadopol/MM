# Logbook PDF/print progress feedback — v2.52

Large ADIF logs and wide visible-field configurations can take several seconds to export or print because the logbook has to build multiple landscape field-block tables and then render them through Qt's `QPrinter` path.

v2.52 adds an explicit modal progress dialog for these operations:

- `Save PDF...` shows **Preparing logbook PDF...** and then **Rendering PDF file...**.
- `Print...` shows **Preparing logbook printout...** and then **Sending pages to printer...**.
- During table preparation the progress bar advances based on field blocks × QSO rows.
- The user can cancel while MadModem is preparing the table.
- Once rendering is handed to `QPrinter`, Qt does not provide a safe fine-grained cancellation hook here, so that phase is shown as busy/indeterminate.

This change only affects user feedback around logbook PDF/print generation. It does not change ADIF parsing, field preservation, FT decoding, CAT/PTT, or QSO-map rendering.
