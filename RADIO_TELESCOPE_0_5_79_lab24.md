# MadModem 0.5.79-lab24 — Radio Telescope rotator-required RX guard

This patch makes Radio Telescope behaviour explicit when no rotator is connected.

- Pressing RX in Radio Telescope mode now checks the selected rotator profile before opening audio.
- If the rotator is disabled, disconnected or lacks the required elevation axis for Alt-Az mode, RX is blocked and the Mode tab status shows the reason.
- The existing explicit "Bench test without rotator" checkbox remains the only way to run the audio/heatmap path without a rotator.
- Start scan keeps the same rotator guard, so real sky maps cannot be created silently with a stationary antenna.
