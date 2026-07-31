# MadModem 0.5.77.experimental — Rotator Settings compact layout

This patch keeps the application version at `0.5.77.experimental` and only changes the Rotator settings UI.

## Changes

- Rotator profile page top area is now split horizontally:
  - `Connection` on the left.
  - `Settings` on the right.
- The common Band → Rotator assignment table is now compact and left-aligned.
- Column order is now:
  - `Band`
  - `Auto peak`
  - `Rotator`
  - `Az ±`
  - `El ±`
- The `Rotator` column no longer stretches across the full dialog width.
- The table remains the single authoritative UI for assigning each band to Manual / Rotator 1 / Rotator 2 / Rotator 3.

## Behaviour

No runtime rotator logic was changed. The table still writes back into the existing per-profile settings model so the rest of the application continues to read the same configuration structure.
