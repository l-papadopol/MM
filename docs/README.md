# MadModem 0.6.1 documentation

Operating help and maintained developer references for MadModem 0.6.1. Generated verification output is kept outside the source tree.

## User help

MadModem includes twelve Qt Help pages in six languages: English, Italian,
French, German, Norwegian and Czech. Open them from **Help → MadModem Help**.

| Topic | What it covers |
| --- | --- |
| Quick start | First-run audio, RX, waterfall and safe TX setup |
| Audio, CAT and PTT | Devices, Hamlib, serial PTT and hardware tests |
| Waterfall and markers | Tuning, zoom, pan and mode-specific markers |
| Text modes | RTTY, PSK/QPSK, MFSK and Feld Hell |
| CW / Morse | RX A/RX B, Auto-WPM, AFC and diagnostics |
| FT4 / FT8 | Decodes, bands, QSO sequencing, timing and runtime log |
| Rotator | Profiles, geometry, pointing, tracking and calibration |
| Radio Telescope | Receive-only scanning, measurements and safety |
| Scheduler | UTC band changes and CAT behaviour |
| Logbook and map | ADIF, DXCC, filters, layers and paths |
| Troubleshooting | Checks for audio, timing, CAT, PTT and rotators |

The source pages live under [`help/`](help/). Each language has its own Qt Help
project and is packaged with the application.

## Project and release information

- [`../README.md`](../README.md) — public project overview, features and build
  instructions.
- [`../RELEASE_NOTES.md`](../RELEASE_NOTES.md) — user-visible changes in the
  current release.
- [`../CHANGELOG.md`](../CHANGELOG.md) — chronological engineering history.
- [`../TRANSLATION_AUDIT.md`](../TRANSLATION_AUDIT.md) — translation policy and
  audit results.
- [`VERSIONING.md`](VERSIONING.md) — version and package naming rules.
- [`SOURCE_AUDIT.md`](SOURCE_AUDIT.md) — compiled and bundled source inventory.

## Developer references

- [Installer and migration instructions](platform/INSTALLERS.md).
- [GitHub build workflow](platform/GITHUB_ACTIONS_DISTRIBUTION.md).
- [Current architecture](architecture/CURRENT_ARCHITECTURE.md).
- [CW implementation and tests](cwskimmer/README.md).
- [MSK144](msk144/README.md) and [Q65](q65/README.md).

## Validation commands

```bash
python3 tools/audit_localization.py
python3 tools/audit_documentation.py
bash scripts/ci_release_version_guard.sh
ctest --test-dir build --output-on-failure
```

The localization audit checks key order, placeholders, semantic mixing and the
reviewed operator-facing strings. The documentation audit checks the six help
projects, local links, embedded resources and version metadata.
