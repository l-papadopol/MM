# CAT/PTT cleanup v2.62

MadModem now follows the same user-facing abstraction used by WSJT-X:

- Rig model selects the Hamlib backend.
- PTT method selects how PTT is keyed: CAT, RTS, DTR, VOX/audio-only.
- Radio mode on TX selects whether MM asks the rig to stay in the current mode, USB, or Data/Pkt before keying PTT.

The normal UI no longer exposes vendor-specific DATA SEND routes such as Kenwood USB/ACC2/LAN. Those created confusion and made MM look like it needed per-radio controls for something Hamlib and the rig backend should normally abstract.

For CAT PTT, MM now uses the backend's ordinary CAT PTT call. If `Radio mode on TX` is `Data/Pkt`, MM first asks Hamlib to set a packet/data USB mode. It then keys ordinary CAT PTT. This mirrors the WSJT-X style separation between mode and PTT.

Legacy v2.59/v2.60 settings are migrated automatically:

- `data`
- `force_data_usb`
- `kenwood_usb`
- `kenwood_acc2`
- `kenwood_lan`

become:

- `data_pkt`

If a specific transceiver still needs raw CAT commands, they should be added later as hidden Advanced/Debug overrides, not as normal UI choices.
