# MadModem changelog

## 0.6.6

- Speed up the FT8 Costas search with reusable real FFT plans and precomputed frequency interpolation.
- Add FT8/FT4 display filters for continent, DXCC, CQ and ITU zones, WAP references and CQ traffic.
- Recover weak FT8 CQs using recently decoded tracks from the same RX parity. Estimate the current timing, frequency and drift from Costas symbols and confirm the codeword from the current slot.
- Recover repeated FT8 and FT4 CQs from matched soft decisions in consecutive RX periods of the same parity.
- Add NB2 Auto to analyze candidate impulse windows with NB2. The audio delay
  stays constant during automatic bypass and when switching to manual NB2.
- Apply the configured USB or Data/Pkt mode when entering FT8, FT4, Q65 or
  MSK144 with Hamlib CAT, and when tuning a standard FT band.
- Speed up NB2 and migrate saved LPC selections to NB2 Normal. Document NB2 in
  CW, RTTY and both skimmers.

- Keep STOP latched on FT8/FT4 call-again. Apply the SWR lock to serial CW and
  require continuous valid SWR readings above the threshold.
- Confirm reduced Tune power before PTT and restore power after PTT OFF.
- Check the TX deadline through the first audio pull for MSK144 and Q65.
- Publish only after the selected macOS builds and installer checks pass.

- Integrate IC8TEM's receiver, CAT, station and logbook tools; see
  [release notes](RELEASE_NOTES.md) and [operator guide](docs/FEATURES.md).
- Restore the original Windows installer identity and per-user data locations.
- Restore update checks and download documentation to `l-papadopol/MM`.
- Credit Costantino Cerrotta, IC8TEM, in About and AUTHORS; consolidate GitHub documentation.

## 0.6.5

- Add correlated Evil Auto QSO traces to Runtime log and compressed session logs.
- Show the distribution build number beside the version in the window title.
- Consolidate documentation and remove obsolete development reports.

Earlier releases: [GitHub Releases](https://github.com/l-papadopol/MM/releases).
