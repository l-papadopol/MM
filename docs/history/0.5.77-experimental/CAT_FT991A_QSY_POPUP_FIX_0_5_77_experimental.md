# 0.5.77.experimental CAT / FT-991A / popup fix

Fix batch for the Windows/CAT issues reported after the 0.5.77.experimental MSK144/FT test cycle.

## Windows combo popups

The cockpit/fullscreen chrome can interfere with popup stacking on Windows. Combo item views retain an explicit readable palette, remain children of Qt's private popup container, and the real container is raised after it is shown. The item view itself is never promoted to a top-level popup.

## Settings open/close latency

Settings no longer reapplies expensive CAT/rotator configuration unconditionally on every OK.  CAT is reconfigured only when CAT/PTT fields changed; rotator configuration is reapplied only when rotator settings changed.  The duplicate delayed fullscreen enforcement pass in Settings was also removed.

## FT8/FT4 band selector QSY

The FT band selector now queues CAT configuration before the QSY when CAT is enabled but not yet connected, then queues the frequency set to the selected standard FT8/FT4 band frequency.  Selecting a band no longer unnecessarily reconfigures the rotator.

## Yaesu FT-991/FT-991A DATA filter narrowing after TX

Before CAT PTT, the Hamlib controller captures the current RX mode/passband.  After PTT OFF it restores that mode/passband.  When MadModem has to force USB or Data/Pkt for TX, it requests a 3000 Hz passband instead of Hamlib's `RIG_PASSBAND_NORMAL`, which can map to 500 Hz on some Yaesu DATA backends.
