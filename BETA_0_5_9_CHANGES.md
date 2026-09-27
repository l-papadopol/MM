# MadModem 0.5.9-beta

## RTTY RX
- Removed automatic polarity detection and all CAT/signal polarity-probe runtime code.
- Manual Reverse remains the only polarity control.
- Replaced the former Auto polarity checkbox with `Narrow Mark/Space filter`.
- The filter uses the existing single DSP conditioner path: two cascaded narrow band-pass channels centred on Mark and Space (Q=28), summed without wideband dry mix. This strongly rejects signals outside the two tones and the spectral region between them.
- Filter changes are pushed immediately to the RX worker through `scheduleLiveRxConfig()`.

## Cabrillo
- Added `File -> Export Cabrillo...` to the Logbook.
- Three-step wizard: QSO scope, contest/exchange settings, validation/preview.
- CQ WW RTTY uses the existing strict contest-specific formatter/validator.
- Generic Cabrillo 3.0 lets the operator select CONTEST plus sent/received ADIF exchange fields; it validates missing station, frequency, UTC and exchange data before saving.
- Removed the old hidden Cabrillo choice from the ordinary ADIF save-file filter.

## Version / guards
- Version advanced to 0.5.9-beta.
- Architecture, UI/contest and release/localization/documentation guard suites pass locally.
- Full Qt compilation is left to CI because the local validation container has no Qt5 development package.

## R13 — CI guard synchronization

- Removed the stale UI audit requirement for the deleted `rtty_contest_macros` localization key.
- Documentation audit now validates the current compact bilingual README instead of requiring the retired alpha-era overview sections.
- Corrected the English README status from `alpha software` to `0.5.9-beta software`.
- Source revision: `0.5.9-beta-source-r13-ci-guard-sync`.
