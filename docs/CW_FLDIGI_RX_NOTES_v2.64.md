# MadModem v2.64 CW RX rewrite notes

This patch rewrites `modems/cw/CwDecoder.*` using fldigi's CW modem architecture as the reference model.

Reference inspected from uploaded `fldigi-master.zip`:

- `src/cw_rtty/cw.cxx`
- `src/include/cw.h`
- `src/cw_rtty/morse.cxx`
- `src/include/morse.h`

Relevant fldigi ideas used:

1. Do not decode by chasing the strongest waterfall peak.
2. Mix the selected CW tone to complex baseband.
3. Apply a narrow CW/matched-like filter before key detection.
4. Use AGC/noise floor tracking and hysteresis to decide key-down/key-up.
5. Use a timing state machine for element/character/word spaces.
6. Decode with a fuzzy duration matcher inspired by fldigi's SOM table rather than only hard dot/dash thresholds.

Implementation in MM v2.64:

- AFC OFF is a hard lock to the clicked/user tone. The CW marker must not move.
- AFC ON is local only and cannot leave the configured range around the selected tone.
- The old short-frame Goertzel detector is no longer the primary RX detector; Goertzel is used only for optional local AFC.
- The detector is cheap enough for old PCs: per sample it does DC block, complex NCO, two cascaded low-pass filters, envelope smoothing, and a 4 ms decimated gate.
- Status text now exposes useful CW RX diagnostics: level, noise, threshold, margin, AFC candidate, current pattern, fuzzy winner, fuzzy score, decoded/bad counts.

This is not a byte-for-byte port of fldigi. It is a clean Qt/C++ adaptation of the same RX design so it fits MadModem's current `AudioBlock`/signal architecture.
