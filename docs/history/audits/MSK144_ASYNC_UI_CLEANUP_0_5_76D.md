# MSK144 async decode + weak-signal panel cleanup — 0.5.76d

## Reason
User testing of 0.5.76c showed that MSK144 RX now starts, but period-end decoding stalls the UI for about one second and the waterfall shows transient `MSK n dB` labels that are not useful operator markers. The Q65/MSK144 Mode panels also duplicated the global RX/TX controls with per-mode RX/TX/STOP/Clear/Generate buttons.

## Changes

- MSK144 period decode is dispatched to a worker thread.
- The live audio path copies the completed period buffer, immediately trims the streaming buffer, and returns.
- A single in-flight decode guard prevents piling up decode workers if CPU cannot keep up.
- Decode results, MIND stats, and status messages are returned through queued Qt invocation.
- Transient MSK ping labels are no longer drawn on the waterfall; ping detection only updates textual status/log.
- Duplicate per-mode MSK144/Q65 RX/TX/STOP/Clear/Generate controls are hidden and not inserted into the Mode-panel layout. The top RX/TX controls remain authoritative.
- MSK144/Q65 sequence-status group titles now use mode-specific labels, not the old FT8 translation key.

## Notes

This does not claim full SIMD/Eigen vectorization of the MSK144 demodulator yet. The immediate fix is to remove the UI stall by moving period decode off the UI/audio hot path. A later optimization pass can replace the current scalar candidate loops with an Eigen/OpenMP batch path once decode correctness is validated against real MSK144 samples.
