# MSK144 MSHV link fix - 0.5.77.experimental

Fixes two integration issues in the MSHV MSK144/MSK40 adapter:

- Removed the duplicate Qt AUTOMOC ownership of `Msk144Decoder.h` from the main `MadModem` target. The Q_OBJECT header is now owned only by the `madmodem_msk144` static library, avoiding duplicate `Msk144Decoder::staticMetaObject` and signal symbols at final link.
- Added the missing `DecoderMs::SetPerodTime(int)` shim in `decoderms_msk_helpers.cpp`. Upstream MSHV keeps the historical misspelling; MadModem calls the same API to set 15/30 s MSK144 periods.

No decoder logic was changed.
