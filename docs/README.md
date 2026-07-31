# MadModem documentation

Current release: **MadModem 0.5.78**.

## Current documents

- `../README.md` — project overview, build requirements and safety notes.
- `../RELEASE_NOTES.md` — changes and upgrade information for 0.5.78.
- `../CHANGELOG.md` — concise release history.
- `../TRANSLATION_AUDIT.md` — multilingual dictionary status and audit procedure.
- `VERSIONING.md` — public versioning and package naming.
- `SOURCE_AUDIT.md` — current compiled/bundled source-origin inventory.
- `RELEASE_VALIDATION_0_5_78.md` — source-package checks and environment limitations.
- `help/` — localized HTML and Qt Help projects for en/it/fr/de/no/cs.
- `architecture/` — current architecture summaries.
- `platform/` — distribution and CI notes.
- `msk144/`, `q65/`, `cwskimmer/` — subsystem implementation notes.

## Historical material

Old point-fix reports, lab branches, retired audits and the long pre-0.5.78 changelog are stored in `history/`. They are retained for source archaeology and must not be treated as current operating instructions.

## Documentation and localization checks

```bash
python3 tools/audit_ui_translations.py
python3 tools/audit_documentation.py
bash scripts/ci_release_version_guard.sh
```
