# FT decode performance guard v2.26

The MSHV-style triple-pass/subtraction decoder can be expensive. Offline WAV analysis is intentionally allowed to run full deep decode because it is RX-only and never arms PTT, the slot scheduler or the TX worker.

Live FT RX is different: decode results must arrive early enough for the QSO sequencer to update `FtTxPlan` and for the UTC scheduler to pre-arm the next transmission. v2.26 keeps the existing separated architecture intact and adds a live-only time budget for deep decode.

Current policy:

- FT8 live deep decode budget: about 2200 ms per slot decode job.
- FT4 live deep decode budget: about 900 ms per slot decode job.
- Offline WAV analysis: no hard budget.
- Single-pass decoding: unchanged.

If the budget is hit, MadModem returns the valid decodes already found and skips remaining deep passes. This protects FT timing without disabling the user's optional deep decode feature.
