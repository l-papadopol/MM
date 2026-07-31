# CW testing

Build and run the pure-C++ regression without Qt:

```bash
scripts/run_cw_native_regression.sh
```

The regression checks:

- exact `CQ CQ DE IZ6NNH 599` at 20 WPM;
- acquisition with a deliberately wrong WPM hint at 30 WPM;
- short deep QSB notches plus noise;
- noise-only input produces no text;
- two full-band carriers 25 Hz apart remain separate scanner lanes.

Real validation is performed with the built-in RX WAV recorder and on-air tests.
A decoder revision is accepted only when it improves real signals without
regressing the existing corpus.
