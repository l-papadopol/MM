# MadModem FT8 bundled WAV benchmark

These four FT8 WAV files are bundled so the FT decoder can be regression-tested
from the FT decode tab with the **Auto test** button.

Expected reference decode counts used by the report:

| WAV | Expected reference decodes |
| --- | ---: |
| websdr_test6.wav | 30 |
| test_21.wav | 34 |
| test_18.wav | 20 |
| test_05.wav | 32 |

v3.32 decoder note:

- Fast remains one pass.
- Deep remains two pass with fast continuous-GFSK subtraction.
- FT8 now applies the WSJT-X/MSHV `ft8b()` hard Costas sync bail-out before LDPC: candidates with `nsync <= 6` are rejected as sync-gate failures instead of being sent to LDPC.
- The report includes `Sync gate` and `LDPC tried` so regressions are visible.
- The Deep candidate budget is expanded after the sync gate, using saved LDPC time to inspect more sync-ranked candidates without using LDPC as a trash filter.

No benchmark WAV is used during normal RX unless Auto test or manual WAV analysis is started.

MadModem v3.33 note:
Deep mode keeps the v3.31/v3.32 fast two-pass subtract path and adds WSJT-X/MSHV-style multi-metric FT8 soft-decision recovery only after the hard Costas sync gate. Fast remains the legacy single-metric path.
