# RTTY scope and waterfall text overlay — 0.5.8 R9

- The RTTY tuning scope draws only the tuning trace and the MARK/SPACE references. The former `NOR/REV · source · CAT mode` diagnostic caption is no longer painted inside the instrument.
- The RTTY settings panel provides **Show decoded text on waterfall**. It controls only the selected decoder's vertical live-text trail between MARK and SPACE; CQ/callsign multi-decoder labels remain independent.
- The option is disabled by default and persisted as `RTTY/waterfallTextOverlayEnabled`.
- Disabling the option clears the existing `rtty-live` trail immediately instead of leaving old glyphs on screen.
- The RTTY runtime guard checks the clean scope, the independent checkbox, persistence, translation parity, default state and immediate clearing behavior.

Validation performed on the source package:

- RTTY live/contest/runtime audit
- localization and documentation audits
- five-theme contrast/integrity audit
- runtime-hardening audit
- native CW regression
- native waterfall leveler regression
- FT8/FT4 lifecycle, scheduling, sensitivity and QSO-priority source guards
- macOS portability preflight
