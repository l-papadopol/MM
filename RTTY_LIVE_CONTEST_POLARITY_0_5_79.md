# MadModem 0.5.79 — RTTY live scope, contest tab and runtime rules placement

This checkpoint keeps the contest engine data-driven from the external `rtty_rules` file and adds three UI/runtime changes.

## RTTY live tuning scope

The CRT-style RTTY tuning scope now displays the tail of the live decoded ITA2 text in the centre between the logical MARK and SPACE captions. The lower line shows the resolved polarity (`NOR`/`REV`), the decision source, and the CAT mode when available.

Automatic polarity has one resolver. Direct Hamlib CAT polling reads the active rig mode together with frequency. USB/LSB/RTTY/RTTYR provides an initial polarity prior; two lightweight framing probes simultaneously evaluate normal and reversed ITA2 hypotheses. Strong framing/text evidence can override the CAT prior. When Auto polarity is disabled, the Reverse checkbox remains authoritative.

## Dedicated Contest mode tab

RTTY gains a `Contest mode` side tab immediately after `Mode`; the existing Rotator tab is appended after it. The contest tab is visible only while RTTY is selected.

The tab contains the external-rules selector/reload, contest session and serial controls, rule-defined exchange fields, live score, contest macro buttons, and a mirrored QSO form. The mirrored form is deliberately not a second QSO state: edits are synchronized bidirectionally with `m_rttyQsoForm`, and Add to log calls the existing `addQsoToLogFromForm(m_rttyQsoForm)` path. Serial increment/scoring/logging therefore retain one owner.

## `rtty_rules` runtime placement

The runtime intentionally loads only:

`QCoreApplication::applicationDirPath()/rtty_rules`

No source-tree or embedded fallback was added. Packaging/install rules now put the file beside the executable:

- Linux: `bin/rtty_rules`
- Windows MSYS2: `bin/rtty_rules`
- Windows MXE static: `rtty_rules` beside the root executable
- macOS app bundle: `MadModem.app/Contents/MacOS/rtty_rules`

Release validation now checks the packaged runtime location.
