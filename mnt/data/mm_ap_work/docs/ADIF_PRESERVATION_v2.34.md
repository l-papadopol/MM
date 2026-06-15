# ADIF preservation in v2.34

Older MadModem revisions parsed a minimal subset of ADIF fields into `LogbookEntry` and rewrote the logbook on application exit.  This was unsafe with real ADIF files from external loggers: a 60+ MB file could be rewritten as a much smaller file because unsupported tags were discarded.

v2.34 changes the policy:

1. Loading parses common columns for UI/filtering but keeps every ADIF tag in each QSO record.
2. Closing MadModem no longer rewrites the logbook file.
3. Switching logbook path no longer forces a save of the previous path.
4. Append/import/delete still save because they are real modifications.
5. Writes use `QSaveFile` so a failed write cannot leave a truncated ADIF file.
6. The logbook table and PDF output expose common ADIF metadata plus a summary of preserved extra fields.

This is not a full ADIF editor yet.  It is an ADIF-preserving browser/import/export backend intended to avoid data loss while MM grows more complete logbook UI columns over time.
