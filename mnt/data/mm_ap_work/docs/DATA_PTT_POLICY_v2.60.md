# DATA PTT policy — v2.60

The old v2.59 fix was too Kenwood-specific in wording.  v2.60 makes the rule generic:

1. If the selected TX route is a digital/DATA route (`data`, `force_data_usb`, `kenwood_usb`, `kenwood_acc2`, `kenwood_lan`), MadModem requests Hamlib `RIG_PTT_ON_DATA`.
2. This is the normal path for radios/backends that expose a dedicated digital/data PTT state.
3. If Hamlib rejects DATA PTT and the rig is a known modern Kenwood data rig (TS-890S/TS-990S), MadModem falls back to the documented Kenwood DATA SEND commands: `TX1;` for TX and `RX;` for RX.
4. If DATA PTT was requested and no supported DATA PTT path works, MadModem reports an error instead of silently falling back to ordinary microphone CAT PTT.

This avoids a Kenwood-only workaround while still keeping the documented Kenwood fallback for the rigs that need it.
