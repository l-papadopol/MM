# MadModem 0.5.78 release notes

MadModem 0.5.78 is a consolidation release focused on source hygiene, cross-platform UI consistency, multilingual coverage and current documentation.

## What changed

The active MSHV MSK144/MSK40 source path is now Qt 6 compatible on macOS: obsolete `QRegExp` and `QString::midRef()` references were replaced with `QRegularExpression` and `QString::mid()`. The same compatibility correction was applied to the retained upstream 2766 source mirror checked by CI.

The Windows dropdown regression is fixed: a `QComboBox` list now opens normally instead of remaining invisible while the mouse wheel still changes its value. The correction retains Qt's private popup hierarchy and raises only the actual popup container. The list background is now consistent on all platforms and is deliberately a little lighter than the main cockpit background.

The active FT8 decoder no longer contains the retired MIND integration scaffolding. Removed code included callbacks, state flags, passive scores, candidate-ranking branches, training-sample export and associated diagnostics. This cleanup does not introduce a new decoding policy; it makes explicit the classic path that the preceding build already used after MIND was disabled.

Runtime localization was rebuilt from the source tree. Every supported dictionary now contains the same 1,683 canonical keys. English, Italian, French, German, Norwegian and Czech dictionaries pass the new structural audit, including placeholder preservation. Technical acronyms, protocol names and some radio terms intentionally remain identical across languages.

The current user documentation has been consolidated. Multilingual Qt Help now includes Radio Telescope, uses the 0.5.78 namespace/version metadata and remains available through the embedded HTML fallback even when `qhelpgenerator` is absent. Historical lab and point-fix notes remain in `docs/history/` rather than cluttering the package root.

## Compatibility and maturity

Linux remains the primary development and validation platform. Windows and macOS builds are supported by the distribution workflows. FT8, FT4 and RTTY are the most mature paths. MSK144, Q65, CW-skimmer integration, Radio Telescope and some image modes still require continued hardware and on-air validation.

## Upgrade notes

No settings migration is required. Existing station, audio, CAT, rotator, logbook and UI preferences continue to use their established keys. The release number is now consistently reported as `0.5.78`, without the previous `.experimental` suffix.

## Source-package policy

The FULL SOURCE package does not ship test WAV/JPG media, the retired built-in decoder autotest UI or MM Flow Studio. Third-party origins and licenses are documented in `THIRD_PARTY_NOTICES.md` and `docs/SOURCE_AUDIT.md`.
