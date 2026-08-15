# MadModem documentation

Current release: **MadModem 0.5.78**.

## Current documents

- `../README.md` — project overview and build instructions.
- `../RELEASE_NOTES.md` — current release notes.
- `../CHANGELOG.md` — concise current changelog.
- `../TRANSLATION_AUDIT.md` — localization status.
- `VERSIONING.md` — version and package naming rules.
- `SOURCE_AUDIT.md` — current compiled/bundled source inventory.
- `RELEASE_VALIDATION_0_5_78.md` — current validation status.
- `DECODER_RECOVERY_0_5_78.md` — active FT and CW decoder policy.
- `FT_CAPTURE_TIMELINE_TWO_STAGE_0_5_78.md` — FT capture timing.
- `WSJTX_3_1_IMPROVED_SOURCE_ANALYSIS_0_5_78.md` — FT reference analysis.
- `cwskimmer/` — the single native CW architecture, API and tests.
- `help/` — localized HTML/Qt Help sources for en/it/fr/de/no/cs.
- `architecture/`, `platform/`, `msk144/`, `q65/` — current subsystem notes.

Obsolete CW branch documentation is intentionally not shipped.

## Checks

```bash
python3 tools/audit_localization.py
python3 tools/audit_documentation.py
bash scripts/ci_release_version_guard.sh
```
