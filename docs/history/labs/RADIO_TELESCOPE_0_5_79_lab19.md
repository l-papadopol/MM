# MadModem 0.5.79-lab19 — Radio Telescope refinement

This lab keeps the lab18 receive-only Radio Telescope mode and refines the scan logic:

- separate rotator settle time from per-tile dwell/integration time;
- default settle time can follow the active rotator profile;
- Alt-Az tile generation now reduces azimuth density near zenith to avoid pole over-sampling;
- hex heatmap interpolation uses a corrected 2D angular distance;
- band-power measurement removes DC bias and applies a Hann window before Goertzel-bank integration;
- stop scan now also sends a rotator stop command when a connected rotator is present.

No FT8/FT4/MSK144/Q65 decoder hot path changes are included in this lab.
