# MSK144 in MadModem 0.5.78

MSK144 is a developing RX/TX mode. The active source uses the MadModem adapter around the in-tree MSHV-derived MSK144/MSK40 decoder components. Receive work is separated from the GUI so long decode operations do not intentionally block the interface.

The mode exposes period and decode-depth controls and accepts only messages that pass the protocol validation path. Hardware and on-air parity with established reference applications still require continued testing.

Relevant source:

- `modems/msk144/`
- `third_party/mshv_gpl/port/HvDecoderMsMsk/`
- `third_party/mshv_gpl/upstream_2766/` (reference snapshot)

Historical porting/build notes are archived in `docs/history/subsystems/msk144/`.
