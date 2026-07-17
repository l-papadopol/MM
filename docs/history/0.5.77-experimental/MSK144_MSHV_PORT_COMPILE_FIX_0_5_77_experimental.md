# MSK144 MSHV port compile fix — 0.5.77.experimental

Fixes the first Linux build errors observed after importing the MSK144/MSK40 MSHV adapter.

Changes:

- Removed the accidental `config_rpt_msk40.h` include from the adapter `decoderms.h` wrapper. That header defines `s8ms`/`s8`, while upstream `decodermsk144.cpp` also defines them locally; including it through the common header caused duplicate static definitions in the same translation unit.
- Kept `config_rpt_msk40.h` included only by `decodermsk40.cpp`, where `s8r` and `rpt_msk40` are actually used.
- Added a local C99-complex magnitude helper for the adapter and mapped upstream `cabs(...)` calls to it. Some C++/libstdc++ builds do not expose the C `cabs` symbol in the global namespace for `double complex`.
- Added the missing `DecoderMs::dot_product_da_da(...)` helper required by the imported MSK144 upstream code.

This patch only addresses the reported build errors in `third_party/mshv_gpl/port/HvDecoderMsMsk/decodermsk144.cpp` and `decodermsk40.cpp`. It does not change FT8/FT4, Radio Telescope, UI, or MIND removal state.
