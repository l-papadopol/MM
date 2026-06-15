# FT WAV diagnostics v2.25

The offline FT WAV analyzer is a regression and troubleshooting tool. v2.25 keeps the RX-only design from v2.24 but adds diagnostic information to the completion log.

The final log line now reports:

- decode count and candidate count;
- total decoder time;
- WAV sample rate, channels and bit depth;
- file duration;
- peak and RMS level after mono conversion;
- clipped sample count;
- number of FT slots analyzed;
- number of slots with zero decode;
- number of emitted reports clamped at -25 dB.

Interpretation:

- Many candidates with zero decodes means the candidate search sees FT-like structure/energy but LDPC/CRC did not validate any message. This can be caused by a wrong mode, truncated or misaligned recording, non-standard audio, bad resampling metadata, or a genuinely undecodable slot.
- A valid WAV should decode consistently in single-pass if signals are not hidden by stronger signals. Deep decode/triple-pass should mainly help crowded or masked slots.
- Many `-25 dB` reports are a sign to inspect the report baseline/clamp path separately; they do not necessarily mean decode failure.

This patch does not alter live FT timing, scheduler/PTT, TX worker, sequencer, or deep-decode behavior.
