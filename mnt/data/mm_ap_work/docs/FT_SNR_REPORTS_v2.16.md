# FT SNR reports in MadModem v2.16

FT8/FT4 signal reports are not waterfall brightness values. They are conventionally reported as
SNR in a 2500 Hz receiver reference bandwidth.

v2.15 already moved away from the ancient sync-score formula, but it still used the other FT tone
bins of the same symbol as the main noise reference. On a busy waterfall that can be wrong: nearby
FT signals or intermodulation inside those tone bins inflate the noise estimate, so a signal that
looks strong can still be displayed as `-17` or similar.

v2.16 changes the estimator to follow the WSJT-X/MSHV model more closely:

1. Only CRC-valid decodes get a report.
2. The decoded 77-bit message/codeword is mapped back to the exact transmitted FT tone sequence.
3. The receiver measures those decoded tones in the captured slot samples.
4. A robust local spectral baseline is measured around the decoded signal and uses the lower part
   of the local spectrum, rather than the target's own off-tone bins only.
5. The tone-bin SNR is converted to SNR_2500 and clamped to the FT report range.

MSHV's reference decoder uses the same high-level mechanism in `decoderft8.cpp`: reconstruct tones,
measure decoded signal power, compare against `sbase/xbase`, clamp the result.

This is still not a byte-for-byte import of WSJT-X's Fortran/JT9 pipeline or MSHV's complete decoder
class. It is, however, no longer a cosmetic or empirical patch: the displayed report is now derived
from the decoded waveform and a baseline estimator in the same spirit as WSJT-X/MSHV.
