# MSK144 one-shot integration notes — MadModem 0.5.73

## Scope

This build adds an experimental MSK144 mode in one pass instead of splitting the
work into 0.5.73/0.5.74/0.5.75 packages.

Implemented pieces:

- MSK144 entry in the Mode menu and central display stack.
- Dedicated MSK144 RX decode table with columns `UTC`, `dB`, `T`, `DF`, `Message`.
- QSO form for MSK144 with ADIF mode `MSK144`.
- 15 s / 30 s period selector.
- Fast / Normal / Deep bounded coherent frame-search selector.
- RX frequency marker, DF tolerance, short-message/SWL/contest UI controls.
- Standard message generator for CQ, reply, report, RRR and 73 sequences.
- GPL MSHV-derived MSK144 TX generator through `GenMsk::genmsk()`.
- Conservative MSK144 RX frame search around the selected RX frequency.
- LDPC/CRC/77-bit unpack validation through the MSHV GPL `GenMsk` helpers.
- Live ping overlay/status from the RX audio stream.

## Important limits before on-air validation

No local MSK144 WAV corpus was available during integration. Therefore 0.5.73 is
an experimental on-air test build. The decoder only emits messages after the
MSHV LDPC/unpack path validates the frame; however the frequency/time search and
lightweight baseband extraction are MadModem-native and may need tuning against
real pings.

Short-message/SWL controls are present in the UI because they are part of the
MSK144 workflow, but the first promoted decoder path is the normal 77-bit MSK144
message path. Short-message promotion should be validated after ordinary CQ/QSO
messages decode reliably.

The TX center is fixed at 1500 Hz, matching the MSK144 audio carrier convention.
The old diagnostic/non-standard fallback waveform has been removed: if MSHV
message generation fails, TX is inhibited.

## Source attribution

MSK144 protocol and algorithm references come from WSJT-X/MSHV GPL-family code
and the QEX protocol paper. The C++ generator/LDPC/unpack code used here is from
MSHV GPL source supplied by the user and preserved under `third_party/mshv_gpl/`.
