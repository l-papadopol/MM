# macOS Qt 6 MSHV complex macro fix — 0.5.78

The MSK144/MSK40 compatibility shim previously defined generic macros such as `I` and `complex`. In Qt 6 MOC/unity translation units the `I` macro leaked into Qt headers and rewrote template parameter names in `QSize`, causing AppleClang errors that appeared to originate inside Qt itself.

The active MSHV port now uses the prefixed `mshv_complex` type and `mshv_i`, `mshv_creal`, `mshv_cimag`, `mshv_conj`, and `mshv_cabs` inline helpers. No generic complex macro remains in active port headers. The macOS portability preflight now rejects future reintroduction of those macros.
