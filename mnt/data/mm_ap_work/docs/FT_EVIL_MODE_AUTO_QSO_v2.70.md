# FT Evil Mode Auto CQ / Auto QSO notes - v2.70

WSJT-Z was inspected for the automation model. The relevant behaviour to carry into MM is not a large UI dump, but these operating rules:

- Auto Call / Auto CQ are deliberate automation modes, not normal FT defaults.
- Auto Call and Auto CQ are mutually exclusive.
- Auto Call listens for filtered CQ-type targets and arms a reply/QSO.
- Auto CQ calls CQ repeatedly and lets the normal FT auto-sequencer handle replies.
- STOP must be the hard escape from pending or active automation.

MM v2.70 implements those rules behind a visible safety gate:

1. FT settings show **Evil mode**.
2. Enabling it opens a confirmation popup.
3. The maintainer-defined unlock phrase is required.
4. Only then are **Auto CQ** and **Auto QSO** shown.
5. The unlock is not persisted; it must be done again after restart.

This keeps normal operator workflow clean while still allowing WSJT-Z-style supervised automation for field tests.
