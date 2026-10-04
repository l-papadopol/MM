# R18 - FT logbook lookup moved off the GUI thread

This revision extends the R17 `LogbookIndexWorker` so FT4/FT8 no longer scan or synchronously query the ADIF logbook in live GUI paths.

## Worker-owned FT indexes

`LogbookIndexWorker` now maintains hash indexes for:

- callsign worked-before membership;
- latest QSO time by callsign;
- latest QSO time by callsign + band + mode;
- DXCC worked globally, by band and by mode;
- Maidenhead 4-character grid worked globally, by band and by mode;
- existing contest-dupe keys from R17.

Normal logged QSOs update the indexes incrementally. Exceptional rebuilds still parse the ADIF file in the worker thread.

## FT GUI/runtime changes

The following FT paths now use queued `lookupFt()` requests and an asynchronous GUI-side result cache:

- FT decode-table worked/needed highlighting;
- new-DXCC outline state;
- AutoQSO `never_worked` and `recent` duplicate policies;
- AutoQSO new-country/new-grid/new-band/new-mode priority metadata;
- FT auto-log 10-minute duplicate protection.

No `m_logbook.records()` or `m_logbook.containsCallsign()` remains in those FT live paths.

Worker replies are coalesced before refreshing the decode table so a busy FT slot does not trigger one full table pass per reply.

## Regression coverage

`madmodem_text_assist_workers_regression` now verifies the FT hash indexes (worked, recent, call+band+mode, DXCC and grid), and `check_text_assist_workers.py` rejects synchronous FT GUI logbook access in the protected live paths.
