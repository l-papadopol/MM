# Release validation — MadModem 0.5.78

## Completed in the preparation environment

- CW relative-timing clean-restart source audit: passed;
- native production CW regression: passed with GCC;
- native production CW regression: passed with Clang;
- AddressSanitizer/UndefinedBehaviorSanitizer: passed;
- ThreadSanitizer: passed;
- obsolete Bayesian/semi-Markov/geometric-rescue source and CMake audit: passed;
- waterfall leveler regression: 6/6;
- waterfall HiDPI viewport and label audits: passed;
- WSJT-X-style waterfall flattening audit: passed;
- FT4 adaptive-runtime audit: passed;
- FT8/FT4 sensitivity source audit: passed;
- macOS portability preflight: passed;
- localization key parity: 1737/1737 keys in all six languages.

## Native CW scenarios

- relative short/long acquisition with a 2.45 human dash ratio;
- exact `CQ CQ DE IZ6NNH 599` at 20 WPM;
- exact `CQ CQ DE TEST` at 35 WPM with an 18 WPM initial hint;
- exact message with additive noise;
- exact message with deep QSB notches that split dashes;
- exact human-jitter message;
- exact message with a stronger known continuous carrier at +70 Hz;
- white/impulsive noise publishes no text.

## Architecture checks

- `CwCarrierDiscriminator` and `CwRelativeTimingDecoder` have independent APIs;
- `CwRelativeTimingTask` owns a dedicated `std::thread`;
- timestamped `CwLogicRun` values are the only discriminator/timing boundary;
- temporal results are drained on the tracker owner thread before callbacks;
- CW Runtime Log is visible and localized in all six UI languages.

## Environment limitation

Qt 5/Qt 6 development packages are unavailable in the preparation container.
CMake therefore stops at `Qt5Config.cmake`; the complete GUI/QtMultimedia build
and live sound-device test must be performed on the target machine. Recorded and
on-air signals remain the release decision for real CW sensitivity.
