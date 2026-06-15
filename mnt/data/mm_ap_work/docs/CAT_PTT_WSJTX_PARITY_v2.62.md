# CAT/PTT WSJT-X parity pass v2.62

MadModem no longer exposes vendor-specific DATA SEND routes in the normal CAT setup UI.

The user-facing model follows WSJT-X more closely:

- PTT method: None / CAT-Hamlib / Serial RTS / Serial DTR
- Radio mode on TX: None / USB / Data/Pkt
- Transmit audio source: Rear/Data / Front/Mic

Internally, CAT PTT remains generic. If Rear/Data is requested, MM first asks Hamlib for DATA PTT. If the rig backend does not accept it, MM falls back to ordinary CAT PTT and reports that fallback in the status text.

Vendor-specific raw CAT commands are deliberately kept out of the normal path.
