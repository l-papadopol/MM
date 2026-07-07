# Q65/MSK144 UI layout update - 0.5.74c

This patch keeps the MSHV assimilation work from 0.5.74b and only reorganizes
operator-facing controls.

## Changes

- The main Mode menu now exposes one `Q65` entry only.
- Q65 submode selection (`A`, `B`, `C`, `D`) is in the Q65 right-side Mode panel.
- QSO Info for MSK144 and Q65 has been moved out of the central activity page and
  into the right-side Mode panel, matching the CW/RTTY layout.
- The standard TX message table/sequencer for MSK144 and Q65 has been moved to
  the right-side Mode panel, matching the FT4/FT8 workflow.
- Q65 settings were changed from a cramped four-column grid to a compact
  two-column form to avoid clipped labels in fullscreen/cockpit mode.

## Not changed

- Q65 TX generation still uses the assimilated MSHV Q65 generator.
- Q65 full RX decode backend remains behind the existing build guard and does
  not emit fake decodes.
- MSK144 settings/decode-depth behavior is unchanged except for layout.
