# Kenwood TS-890S/TS-990S DATA SEND PTT

The TS-890S/TS-990S can transmit digital audio through the DATA SEND path using PC commands.
For USB audio digital modes, the reliable CAT PTT sequence is:

1. Select DATA SEND audio input path, normally USB Audio: `MS102;`.
2. Key DATA SEND transmit: `TX1;`.
3. Return to receive: `RX;`.

MadModem v2.59 keeps Hamlib as the CAT transport but uses these Kenwood vendor commands for modern Kenwood data rigs when a DATA/USB route is selected.
This avoids relying only on generic Hamlib `RIG_PTT_ON_DATA`, which may not map to Kenwood `TX1;` on every backend/build.
