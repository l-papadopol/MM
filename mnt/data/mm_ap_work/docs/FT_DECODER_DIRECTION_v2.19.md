# FT decoder direction from v2.19

MadModem no longer presents MSHV/Decodium/Raptor as selectable complete decode
engines.  In the current code base they were strategy flavours around the same
compact FT decoder pipeline, not two full upstream engines.

The v2.19 direction is:

1. Use one FT receive pipeline in the UI and settings: MSHV-derived native.
2. Remove ambiguity caused by the old selector.
3. Port MSHV algorithms more faithfully into that pipeline, using the GPL source
   already carried in `third_party/mshv_gpl/` when integration is practical.
4. Keep WSJT-X as a correctness reference for slot timing, parser and QSO
   sequencing, but avoid adding another flavour selector unless a real complete
   independent engine is integrated.
5. Do not calculate reports in the UI.  Reports must remain part of the decoder
   result.

This version is an architectural cleanup, not a claim that the compact decoder
is already sensitivity-equivalent to upstream MSHV or WSJT-X.  That equivalence
still requires WAV-based A/B regression tests.
