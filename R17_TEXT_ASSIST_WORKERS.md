# R17 — background text assistance and logbook indexes

R17 removes live text-analysis and duplicate-logbook work from the GUI thread.

## LogbookIndexWorker

- Dedicated `QThread`; it is the only owner of the live worked/dupe hash indexes.
- Exceptional rebuilds receive only the ADIF file path and parse/load it inside the worker thread. `MainWindow` no longer copies the full `m_logbook.records()` vector just to rebuild assistance indexes.
- Normal QSO additions update the indexes incrementally.
- Global worked-before and active-contest duplicate checks are O(1) hash lookups.
- Contest keys support callsign, callsign+band, callsign+period and callsign+band+period scopes according to the active contest profile.

## TextAssistWorker

- Dedicated `QThread`; receives only bounded RX text snapshots and metadata.
- Owns callsign normalization/recognition, locator recognition and generic contest exchange candidate extraction.
- Has no widget/document dependency and never touches modem decoder state or the logbook.
- Returns document ranges and metadata to `MainWindow`; the GUI only applies the formatting/autofill.

## Modes

The common asynchronous text-assistance path is used by RTTY, CW RX A/RX B, BPSK/QPSK variants and MFSK variants. Feld Hell remains a raster/image decoder rather than a decoded character stream, so there is no textual stream to feed into this worker. FT8/FT4/Q65/MSK144 keep their structured-message path in this revision.

## Regression coverage

- `scripts/check_text_assist_workers.py` guards thread ownership, bounded live snapshots, absence of logbook scanning in the live highlighter, and modem/assist separation.
- `madmodem_text_assist_workers_regression` exercises real `LogbookIndexWorker` and `TextAssistWorker` code through CTest: global lookup, contest band-dupe lookup, incremental add, callsign recognition and contest exchange extraction.
