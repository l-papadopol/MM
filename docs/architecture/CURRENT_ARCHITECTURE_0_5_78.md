# MadModem 0.5.78 architecture summary

MadModem is a Qt Widgets desktop application with one integrated main window and mode-specific widgets/workers. Audio capture, transmit generation and decoding are separated from the GUI through workers and queued Qt delivery where the relevant mode requires asynchronous processing.

Key subsystems:

- runtime multilingual dictionaries in `translations/`;
- audio engines in `audio/`;
- mode implementations in `modems/`;
- FT8/FT4 protocol and decoder code under `modems/ft8/` with MSHV-derived support in `third_party/mshv_gpl/port/`;
- CW skimmer implementation under `modems/cw/skimmer/`;
- CAT/PTT through Hamlib in `rig/` and bundled Hamlib build support;
- rotator profiles/control in `rotator/`;
- settings, dialogs, logbook and map widgets in their corresponding directories;
- localized embedded HTML/optional Qt Help in `docs/help/`.

The retired MIND subsystem is not part of the 0.5.78 active decoder architecture. Historical design and test notes are archived under `docs/history/`.
