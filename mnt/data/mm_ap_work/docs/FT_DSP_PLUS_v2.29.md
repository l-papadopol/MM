# FT DSP++ / 4-pass decode — v2.29

`DSP++ / 4-pass decode` is an optional higher-effort FT decode mode.

The normal FT decode levels are now:

1. **Single-pass** — fastest, timing-safe default.
2. **Deep decode / triple pass** — MSHV-style three-pass subtract/rescan.
3. **DSP++ / 4-pass decode** — adds a fourth rescue pass for hard captures.

DSP++ is not a separate decoder engine and does not reintroduce the old flavour selector. MadModem continues to use the single MSHV-derived pipeline.

DSP++ is intended for:

- difficult WAV/offline analysis;
- crowded bands;
- strong PCs where the additional CPU cost is acceptable.

It should remain optional because contest/live FT timing is more important than trying endless decode passes. The FT scheduler, PTT, TX worker and QSO sequencer are not touched by this setting.
