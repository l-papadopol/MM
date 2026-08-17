# RTTY Contest macro UI fix — MadModem 0.5.79

The Contest mode tab and the normal RTTY terminal now have distinct macro banks with distinct responsibilities.

- The normal RTTY macro buttons always show and transmit the user's standard text macros.
- When Contest mode is enabled, the normal RTTY macro bank is disabled to avoid accidental non-contest transmissions.
- The Contest mode tab contains the only contest macro bank.
- Contest macro labels and text are loaded dynamically from the selected `rtty_rules` profile.
- Contest macro buttons use a dedicated `sendRttyContestMacro()` path; normal macros use `sendTextMacro()` and are never reinterpreted as contest macros.
- The duplicated Contest QSO fields remain synchronized views of the single RTTY QSO state; no second QSO state machine was introduced.
