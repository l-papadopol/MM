# MadModem 0.5.76 — MSHV RX completion pass

## MSK144

MSK144 RX no longer stops at a single lightweight frame attempt.  The active
MadModem MSK144 decoder now follows the MSHV/WSJT-X depth semantics in the
classical decoder path:

- Fast: candidate time/DF ranking plus single 72 ms frame decode.
- Normal: adds coherent 4-frame averaging.
- Deep: adds coherent 4-, 5-, and 7-frame averaging.

MIND remains strictly outside validation.  It can rank candidate time/DF chunks
and export positive/negative training examples, but final messages are still
accepted only after the classical sync/demod/LDPC/unpack path succeeds.

## Q65

Q65 RX is now wired to the actual MSHV `DecoderQ65` backend when FFTW3 is
available.  CMake first searches system FFTW3; if it is not found, it uses the
bundled MSHV FFTW static library on known Linux/Windows targets.

Active bridge pieces:

- `third_party/mshv_gpl/port/HvDecoderMs/decoderq65.*`
- `third_party/mshv_gpl/port/HvDecoderMs/decoderpom.*`
- `third_party/mshv_gpl/port/Hv_Lib_fftw/`
- `modems/q65/Q65Decoder.*`

MadModem passes the current Q65 submode, period, decode depth, averaging,
AP/max-drift/EME-delay options, QSO words, RX frequency and DF window into
`DecoderQ65::q65_decode()`.  MSHV decode rows are converted into `Q65Decode`
events for the normal MadModem activity table.

If FFTW3 is unavailable and the bundled static library does not match the build
platform, Q65 RX falls back to the safe buffered mode rather than emitting fake
decodes.
