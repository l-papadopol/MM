# Hamfist Assimilation Notes

## Source components retained

Retained and ported:

- `cw_dsp.*` — FFT, per-bin noise floor, adaptive threshold, hysteresis, channel observation collection;
- `cw_classifier.*` — mark/off duration histograms, peak/valley timing classification;
- `cw_decode.*` — beam search decoder, Morse tree, language/callsign bias;
- `cw_data.*` — Morse/prosign/dictionary data;
- `dictionary.*` — autocorrect/prefix utilities;
- `fft.*`, `utils.*` — fixed-point FFT and magnitude helper.

Not retained:

- Arduino `.ino` application loop;
- RP2040 ADC/PWM audio classes;
- TFT/display/UI code;
- buttons/touch/display fonts/splash;
- CW TX encoder path for this RX-skimmer phase.

## Fixes applied during port

### 1. `set_threshold()` no-op

Original bug:

```cpp
void set_threshold(float thresh_mult) { thresh_mult = thresh_mult; }
```

Ported fix:

```cpp
void set_threshold(float threshold_multiplier) { thresh_mult = threshold_multiplier; }
```

### 2. FFT imaginary buffer reset

The original code wrote new real samples into `i[]` but did not reset the corresponding `q[]` entries before each FFT frame. Since the previous FFT overwrites `q[]`, stale imaginary values could contaminate later frames.

Ported fix:

```cpp
i[frequency_count] = sample;
q[frequency_count] = 0;
frequency_count++;
```

### 3. Channel bin range

Original code computed:

```cpp
stop_bin = start_bin + CHANNEL_SIZE - 1;
for (idx = start_bin; idx < stop_bin; ++idx)
```

That scans only four bins when `CHANNEL_SIZE` is five.

Ported fix:

```cpp
stop_bin = start_bin + CHANNEL_SIZE;
```

### 4. Variable-length arrays

Original `cw_decode.cpp` used C99-style variable-length arrays:

```cpp
float on_durations[num_observations];
float off_durations[num_observations];
```

Ported fix: `std::vector<float>`.

### 5. Per-instance noise initialization

Original `process_frame()` used static locals for initialization/gate counters. This is unsafe if multiple engines exist.

Ported fix: moved state into `c_cw_dsp` instance members.

### 6. Standalone FFT initialization

Original application called `fft_initialise()` from Arduino `setup()`.

Ported fix: the DSP constructor initializes the FFT table, so the library is self-contained.

### 7. Minimal robustness guards

Added guards for empty/gibberish partials, zero histogram counts, and no-candidate beam iterations.

## Known limitations

- Current FFT/channel plan is still the original six fixed channel groups.
- The decoder needs enough mark/space observations before it becomes useful; early characters may be missed while the timing classifier trains.
- Confidence is currently a pragmatic wrapper score, not a mathematically calibrated probability.
- The library has not yet been validated against real on-air MadModem audio.
