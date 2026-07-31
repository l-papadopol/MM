# Release validation — MadModem 0.5.78

## Completed in the preparation environment

- native CW pure-C++ regression: passed with GCC;
- native CW pure-C++ regression: passed with Clang;
- native CW regression under AddressSanitizer/UndefinedBehaviorSanitizer: passed;
- waterfall leveler regression: 4/4 with GCC and Clang;
- localization parity/order/placeholder audit: 1730/1730 keys in all six languages;
- stale CW source/document/build-reference audit: passed;
- shell and package-source-path audit: passed.

## Native CW scenarios

- exact `CQ CQ DE IZ6NNH 599` at 20 WPM;
- exact message at 30 WPM with an 18 WPM initial hint;
- exact message with short deep-QSB notches and noise;
- noise-only input publishes no text;
- two carrier lanes 25 Hz apart remain separate.

## Environment limitation

Qt 5/Qt 6 development packages are unavailable in the preparation container, so
the complete GUI/QtMultimedia link and live sound-device test must be performed
on the target build machine. Real on-air recordings remain the release decision
for CW sensitivity and deep-QSB behaviour.
