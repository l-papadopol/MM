# MadModem 0.5.74a — Q65/MSK144 MSHV include-path hotfix

This hotfix keeps the 0.5.74 Q65 assimilation work and corrects remaining relative include paths inherited from the original MSHV source tree layout.

Fixed paths:

- `third_party/mshv_gpl/port/HvGenMsk/genmesage_msk.cpp` now includes `../../nhash.h`.
- `third_party/mshv_gpl/port/HvPackUnpackMsg/pack_msg.cpp` now includes `../../pfx_sfx.h`.
- `third_party/mshv_gpl/port/HvPackUnpackMsg/unpack_msg.cpp` now includes `../../pfx_sfx.h`.

The CW skimmer warnings shown during compilation are non-fatal warnings and are not addressed in this hotfix.

Q65 finalization remains a separate step after the global build is green: the staged MSHV `DecoderQ65` source must be wired into MadModem's period audio buffer and decode worker instead of remaining as reference-only code.
