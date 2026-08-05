# CW testing

Run the pure-C++ production-path regression:

```bash
scripts/run_cw_native_regression.sh
```

The suite compiles the same discriminator, temporal task, timing decoder, Morse
beam, selected-tone tracker and lane scanner used by the application. It checks:

- replay of a leading dash after a deliberately wrong initial clock;
- bounded Bayesian posterior metadata and discriminator-probability export;
- relative-pair acquisition with a non-ideal human dash ratio;
- clean 20 WPM and 35 WPM with a wrong 18 WPM hint;
- the live `CQ CQ OG50YL...` case at 30.5 WPM with 8.8-dit word gaps;
- 38 WPM with 11-dit Farnsworth word gaps and an 18 WPM initial hint;
- 12 WPM with a deliberately wrong 28 WPM initial hint;
- additive noise and deep QSB notches;
- human MARK/SPACE jitter and a 2.45 dash ratio;
- a stronger known carrier at +70 Hz;
- valid CW followed by beating broad narrow-band noise;
- micro-run storms, white/impulsive noise and no false text;
- reset timestamps, duplicate tone updates and sample-rate restart integrity.

For this checkpoint the production CW path is validated with GCC and Clang
using `-Wall -Wextra -Werror`, plus AddressSanitizer and
UndefinedBehaviorSanitizer. Exact timing and memory figures are recorded in
`CW_BAYESIAN_BEAM_VALIDATION.txt` for the packaged build.
The full Qt application cannot be configured in the preparation container
because Qt5 development packages are absent. On-air and recorded-audio testing
remain mandatory because synthetic generators cannot reproduce every receiver
AGC, operator and QRM condition.
