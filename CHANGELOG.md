# MadModem changelog

## 0.6.5 — IC8TEM edition

- Add an RX noise blanker before every decoder (Decode › Noise Blanker): NR0V NB2
  style impulse blanker or DL2FW linear prediction, three thresholds; MSK144 pings,
  keyed CW and FT/Q65 tones are never blanked; 48 ms delay compensated in the decode
  times; off by default. Ported from MSHV-IC8TEM.
- Add the MSK144 carrier notch (Decode › MSK144 Carrier Notch (ZAP)).
- Show the edition in the title and the About box; updates come from ic8tem/MM.

## 0.6.5

- Add correlated operational traces for Evil Auto QSO in Runtime log and compressed session logs.
- Show the distribution build number beside the version in the window title.
- Consolidate current documentation and remove obsolete development reports.

Earlier release information is available in [GitHub Releases](https://github.com/l-papadopol/MM/releases).
