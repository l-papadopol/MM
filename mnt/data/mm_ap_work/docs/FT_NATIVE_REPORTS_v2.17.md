# FT native report handling in v2.17

## Policy

The FT report shown by MadModem must come from the decoder engine that produced
the valid FT8/FT4 message.

The UI must not estimate SNR from waterfall intensity, sync score, decode list
sorting score or a second scan over the received audio buffer.

## Implementation in this version

`Ft8RxDecoder::decodeCandidate()` and `Ft8RxDecoder::decodeFt4Candidate()` now
retain the symbol/tone energy matrix already calculated for the LDPC soft bits.
After LDPC and CRC succeed, the decoder reconstructs the expected tone for each
symbol from the decoded codeword and computes a native matched-symbol report
from those already-existing energies.

This means:

- no post-decode buffer scan;
- no extra Goertzel/FFT pass just for the report;
- no sync-score-to-report conversion;
- no UI-side report estimator;
- `Decode::snrDb` is a decoder-owned field.

## Future external backends

If MM later wires a full external MSHV, WSJT-X/JT9-derived or Decodium/Raptor
backend that already emits a report, that report must be copied directly to
`Ft8RxDecoder::Decode::snrDb`.

Only if a backend returns a decoded message without any native report may MM use
a clearly-marked fallback.  That fallback must stay inside the backend adapter,
not in `MainWindow`, not in the waterfall and not in the sequencer.
