# MSK144 MIND domain gate — 0.5.75a

0.5.75a fixes the misleading cold-start MIND panel for MSK144.

The previous 0.5.75 UI reused the already-trained FT8/FT4 ranker accuracy while the active profile was MSK144. This made the panel show Ranker/Best values above 80% even with `MSK144 samples = 0`.

The fix separates the readiness gate:

- FT8/FT4 keep the existing FT candidate-ranker statistics.
- MSK144 Assist is not allowed until real MSK144 ping/chunk positive/negative samples and MSK144 validation history exist.
- The UI now shows `--` and `MIND MSK144 0%` while MSK144 has no training data.
- Training mode may collect MSK144 samples, but Assist cannot be enabled by FT-only history.

MIND still never validates an MSK144 message. The final decode remains native sync/demod/FEC/CRC/unpack.
