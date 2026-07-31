# MadModem 0.5.74b — Q65/MSK144 build cleanup

This package keeps the 0.5.74 Q65 assimilation and applies the cleanup required after the first compile pass.

## Fixed build issues

The MSHV ported sources must not keep relative includes that only work in the original MSHV tree. The active `third_party/mshv_gpl/port/` files now include shared helpers through the MadModem port layout:

- `HvGenMsk/genmesage_msk.cpp` -> `../../nhash.h`
- `HvPackUnpackMsg/pack_msg.cpp` -> `../../pfx_sfx.h`
- `HvPackUnpackMsg/unpack_msg.cpp` -> `../../pfx_sfx.h`
- `HvPackUnpackMsg/pack_unpack_msg.h` -> `../../HvTxW/hvqthloc.h`

## Warning cleanup

The CW skimmer warnings reported by GCC were cleaned in source instead of being hidden by broader compiler flags:

- histogram loops now use signed index types where the bounds are signed;
- `size_t` loops are used when iterating container/sample counts;
- debug-only branches have braces, so disabled `DEBUG_PRINTF` no longer leaves empty `if` bodies;
- debug helper arguments are explicitly marked unused when file logging/debug printing is disabled;
- the deprecated `Eigen::initParallel()` call was removed.

## Q65 RX finalization status

Q65 TX is already generated through the assimilated MSHV `GenQ65` code. Q65 RX buffering/UI/period handling is connected in MadModem, but the full MSHV `DecoderQ65` backend depends on FFTW3. The CMake option remains explicit:

```bash
cmake -DMADMODEM_ENABLE_Q65_FULL_MSHV_DECODER=ON ..
```

If FFTW3 is not found, the build remains green and Q65 RX does not emit fake decodes.
