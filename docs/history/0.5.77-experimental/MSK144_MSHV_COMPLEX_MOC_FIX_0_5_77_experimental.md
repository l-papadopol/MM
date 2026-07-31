# MSK144 MSHV complex/MOC compile fix — 0.5.77.experimental

Fixes a Linux/Qt MOC compile failure in the MSK144 MSHV adapter where `decoderms.h`
used `creal()`/`cimag()` before the local MSHV complex compatibility macros had
been included by dependent headers.

Changes:
- include `mshv_complex_compat.h` explicitly before the local `cabs()` shim;
- implement the local `madmodem_mshv_cabs()` helper using GCC complex
  `__real__` / `__imag__` operators directly;
- keep the upstream MSK144/MSK40 source calls to `cabs(...)` routed through the
  local helper.

This keeps the port local to `third_party/mshv_gpl/port/HvDecoderMsMsk/` and does
not affect the rest of MadModem.
