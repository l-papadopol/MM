# MadModem 0.5.9-beta R14 build fix

GitHub CI exposed a compile-time API mismatch in the RTTY multi-decoder.

## Root cause

`RttyMultiDecoder.cpp` still calls:

```cpp
track.decoder->setVisualizationEnabled(false);
```

This is intentional: secondary RTTY decoder tracks must not produce the primary decoder's visualization/scope workload.

During the R12/R13 RTTY cleanup, the public inline setter was accidentally removed from `RttyDecoder.h`, while the backing member `m_visualizationEnabled` and the multi-decoder call both remained.

## Fix

Restored:

```cpp
void setVisualizationEnabled(bool enabled) { m_visualizationEnabled = enabled; }
```

A regression assertion was also added to `scripts/check_rtty_live_contest_runtime.py` so the header API and the multi-decoder call cannot drift apart silently again.

Source revision: `0.5.9-beta-source-r14-rtty-multidecoder-api-fix`
