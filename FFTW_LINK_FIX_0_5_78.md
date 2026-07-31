# FFTW3 link correction — MadModem 0.5.78

`decoderpom.cpp` belongs to the active `madmodem_msk144` target and directly calls FFTW3. Previously FFTW3 was discovered and linked only inside the optional full-Q65 CMake branch. With Q65 disabled in CI, compilation succeeded but the final executable failed to link with unresolved `fftw_plan_*`, `fftw_execute`, and `fftw_destroy_plan` symbols.

The correction is build-system only:

- FFTW3 is discovered once before the MSK144 target is defined.
- `madmodem_msk144` publicly propagates the FFTW3 link requirement.
- The optional full Q65 bridge reuses the same dependency.
- Linux and macOS GitHub jobs install the FFTW3 development package; Windows already did.

No decoder algorithm or source behavior was changed by this correction.
