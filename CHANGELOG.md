# MadModem changelog

## 0.5.78 — 2026-07-16

### Interface and platform fixes

- Corrected Windows combo-box dropdown visibility while preserving the internal Qt popup container.
- Unified dropdown styling on Linux, Windows and macOS: dark cockpit background, slightly lighter than the surrounding panel, amber text and high-contrast selection.
- Kept the fullscreen cockpit window controls and safe application-exit path.

### Decoder and source cleanup

- Removed retired MIND callback, state, feature-ranking, training-sample and diagnostic remnants from the active FT8 decoder source.
- Preserved the classic FT decoder thresholds and recovery policy that were already selected by the previous hard-bypass path.
- Kept MSHV-derived FT8/FT4, MSK144 and Q65 protocol components and their licensing notices.
- Kept production packages free of bundled WAV/JPG test media, obsolete built-in autotest UI and MM Flow Studio.

### Localization

- Regenerated the runtime UI dictionaries from the canonical source harvest.
- Synchronized English, Italian, French, German, Norwegian and Czech at 1,683 keys each.
- Migrated useful translations from obsolete literal-source keys and removed duplicate stale entries.
- Added release-specific translations for recent Radio Telescope, rotator, logbook, MSK144/Q65 and settings controls.
- Added an automated audit for key parity/order, duplicate and empty values, Qt placeholders and retired MIND/DDSP vocabulary.

### Documentation

- Updated all public version metadata to 0.5.78.
- Rewrote the README, release notes, documentation index, versioning guide and translation audit.
- Added a localized Radio Telescope help page in all six supported languages.
- Updated Qt Help namespaces, filter attributes, contents, keywords and collection start pages.
- Moved old lab, audit and one-off point-fix notes to `docs/history/` instead of leaving them in the package root.

## Earlier development history

The detailed pre-0.5.78 changelog and point-fix notes are preserved under `docs/history/` for source archaeology. They are not the current operational documentation.
