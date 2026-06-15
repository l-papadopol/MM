# ADIF full compatibility pass - v2.35

This revision is a safety pass for real-world ADIF logbooks.

The previous preservation pass avoided several destructive behaviours, but a complete ADIF-safe implementation also needs to obey the ADIF length rule: tag lengths are byte counts, not GUI string character counts. v2.35 therefore parses the file as bytes, extracts each field using the byte length specified in the tag, and only then converts the value to UTF-8 text for display.

Normal QSO logging is now append-only. This is intentional: replacing a 66 MB user logbook just to add one QSO is unnecessary and dangerous. Delete and import-merge still rewrite the file because they modify existing content, but those operations use `QSaveFile`.

The UI no longer limits visibility to MadModem's preferred subset. Common columns remain easy to read, and every additional ADIF field found in the file is exposed as a horizontal-scroll column. Export preserves all parsed fields.

Known policy: MadModem treats UTF-8 as the export encoding. Legacy non-UTF8 ADIF files can be loaded field-by-field, but exported files are normalized to UTF-8 with correct ADIF byte lengths.
