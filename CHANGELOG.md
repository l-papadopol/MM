# MadModem changelog

## 0.6.6

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
