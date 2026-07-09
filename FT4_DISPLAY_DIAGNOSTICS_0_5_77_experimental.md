# MadModem 0.5.77.experimental - FT4/FT8 display diagnostics

This experimental checkpoint adds UI-path diagnostics for FT4/FT8 decodes.

The decoder core is not changed.  The new counters are MainWindow/table-only
telemetry used to prove whether a valid FT4/FT8 message reached the visible RX
list.

Logged per slot:

- decoder decodes: valid messages reported by the FT decoder profiler
- decodeReady signals: decodeReady events received by MainWindow
- rows inserted: new visible rows added to the FT decode table
- duplicate/update: valid messages merged into an existing visible row by the
  duplicate hygiene logic
- blacklisted: decoded rows marked blacklisted and ignored by AutoQSO/sequencer
- table rows before/after: visible table size across the slot

FT4-specific additions:

- every FT4 row insert/update logs a compact `FT4 display path` line
- FT4 live decode over 800 ms emits a warning so slow slots are immediately
  visible in the field log

These changes are diagnostic only: they do not affect candidate search, MIND,
LDPC, CRC, unpacking, deduplication policy, AutoQSO, or the sequencer.
