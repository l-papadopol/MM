# MSK144 build fix 0.5.73b

This hotfix corrects a relative include path in the MSHV-derived pack/unpack code.

## Error fixed

The Linux build stopped in `madmodem_msk144` while compiling:

- `third_party/mshv_gpl/port/HvPackUnpackMsg/pack_msg.cpp`
- `third_party/mshv_gpl/port/HvPackUnpackMsg/unpack_msg.cpp`
- `third_party/mshv_gpl/port/HvGenMsk/genmesage_msk.cpp`
- `modems/msk144/tx/Msk144Transmitter.cpp`
- `modems/msk144/Msk144Decoder.cpp`

The failing header was:

```cpp
#include "../../../HvTxW/hvqthloc.h"
```

That path was valid in upstream MSHV because `HvPackUnpackMsg` lived under `src/HvMsPlayer/libsound/`. In MadModem the same code was intentionally moved under `third_party/mshv_gpl/port/HvPackUnpackMsg/`, so the correct relative path is:

```cpp
#include "../../HvTxW/hvqthloc.h"
```

No MSK144 algorithm or UI behaviour was changed by this hotfix.
