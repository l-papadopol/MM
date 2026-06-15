# CW adaptive decoder — v2.58

The previous CW path used a narrow fixed-tone quadrature detector and was too fragile in real reception.

This revision uses a short-frame adaptive detector inspired by the reviewed STM32 FFT and Goertzel CW decoders:

1. audio is processed in approximately 4 ms frames;
2. each frame is DC-removed and Blackman-windowed;
3. a small Goertzel bank scans 250–2000 Hz in 25 Hz steps;
4. the strongest bin becomes the current CW tone estimate;
5. noise and signal floors are tracked in dB;
6. hysteresis and a 4-frame debounce produce stable key transitions;
7. the existing adaptive Morse timing decoder converts transitions into characters.

The decoder remains RX-only. It does not touch PTT, TX, FT slot timing, CAT, ADIF, or map code.

Expected benefit: tuning and audio level are much less critical, and CW should decode from the dominant sidetone even when the user has not manually placed the marker exactly on the tone.
