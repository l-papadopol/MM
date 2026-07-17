# MSHV 2.76.6 MSK144/MSK40 upstream snapshot

This directory contains the unmodified upstream MSHV MSK144/MSK40 decoder files used as the reference source for the MadModem `0.5.78` MSK144 RX adapter.

Active adapter code is in:

- `third_party/mshv_gpl/port/HvDecoderMsMsk/`
- `modems/msk144/MshvMsk144Adapter.*`

The adapter intentionally avoids linking the full upstream `DecoderMs` constructor because that constructor instantiates all MSHV decoders. Only the MSK144/MSK40 bodies are compiled against a minimal `DecoderMs` shim.
