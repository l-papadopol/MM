# MadModem 0.5.79-lab20 — Radio Telescope rotator safety

This lab refines the receive-only Radio Telescope mode introduced in lab18/lab19.

## Behaviour change

A real Radio Telescope scan now requires a configured and connected rotator.
Without a connected rotator, Start scan is blocked with an explicit status/log message.

The only exception is the explicit **Bench test without rotator** checkbox. It is disabled by default and is intended only for audio/heatmap testing: the antenna does not move, so the generated heatmap is not a real sky/RFI map.

## Alt-Az guard

If the operator selects the Alt-Az sky disk, the selected rotator profile must have elevation enabled. Otherwise the scan is blocked and the operator is asked to choose Azimuth ring or enable the elevation axis in the rotator profile.
