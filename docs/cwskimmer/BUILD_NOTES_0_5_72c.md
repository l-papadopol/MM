# MadModem 0.5.72c — CW/text RX UI hotfix

This patch keeps the 0.5.72b CW skimmer engine and fixes three UI/runtime issues found during live testing.

## Fixes

- RX terminal readability: `highlightCallsignsInTerminal()` no longer resets all text to nearly-black. Ordinary decoded text stays cockpit amber; callsigns remain highlighted green/red.
- Click-to-fill QSO call: RX terminals for CW, RTTY, BPSK/PSK and MFSK now install an event filter. Left-clicking a detected callsign fills the current mode QSO `Call` field.
- CW waterfall OSD contrast: vertical CW skimmer glyphs now draw a dark halo/shadow before the bright glyph, so they remain readable on black/blue/green/yellow waterfall backgrounds.
- Fullscreen Mode menu hardening: Mode and Language popup menus are forced to non-native Qt popups and raised above the frameless fullscreen cockpit window. This targets Linux window managers where a QMenu can fall behind a true fullscreen frameless window.

## Notes

The fullscreen menu fix does not change modem routing or the hidden canonical `cmbMode` list. It only changes popup stacking/activation.
