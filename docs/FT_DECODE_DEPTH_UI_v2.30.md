# FT decode depth UI — v2.30

The FT decoder has one MSHV-derived pipeline with selectable decode depth.
The depth controls are not independent features; they represent three exclusive
states:

1. **Single-pass** — both boxes unchecked.
2. **Deep decode / 3-pass** — only the Deep box checked.
3. **DSP++ / 4-pass decode** — only the DSP++ box checked.

DSP++ internally runs the four-pass path, so the Deep checkbox is not left
selected at the same time.  This avoids user confusion while preserving the
same decoder behavior.

The saved settings remain compatible with previous versions:

- `FT8/deepDecode=true`, `FT8/dspPlusDecode=false` -> Deep 3-pass.
- `FT8/deepDecode=false`, `FT8/dspPlusDecode=true` -> DSP++ 4-pass.
- old v2.29 profiles with both true -> migrated to DSP++ only.

No scheduler, PTT, TxPlan, CAT or logbook behavior is changed by this revision.
