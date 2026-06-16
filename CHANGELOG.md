# Changelog

## MadModem 0.5.0 — production consolidation

- Consolidated the validated `0.5.0-alpha.26` FT8 GF(2) OSD full order-1 decoder as the production baseline.
- Expected FT8 Auto Test total remains 88 decodes on the bundled WAV set: 26 / 25 / 16 / 21.
- Rejected later FT8 beta lab experiments (`beta02`..`beta08`) from the production baseline because they did not improve the validated decode count.
- Cleaned production documentation: removed scattered historical v2.x one-off notes and replaced them with consolidated release/architecture notes.
- Included `compare_ft8_wsjtx_madmodem.sh` in the full source tree for optional WSJT-X/jt9 comparison.
- Preserved executable permissions for shell scripts.

## Historical baseline

The decoder core in this release comes from the last validated improvement line:

- `0.5.0-alpha.26_ft_osd_gf2_order1_full`
- GF(2) OSD fallback: order-1 complete over 91 information bits.
- Order-2 search disabled.
- Later micro-sweep/reinject/coherent-metric beta experiments were laboratory-only.
