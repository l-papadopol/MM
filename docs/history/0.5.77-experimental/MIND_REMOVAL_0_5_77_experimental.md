# MadModem 0.5.77.experimental — MIND removal

This experimental package removes the MIND/DeepDsp neural candidate-ranker from the application runtime.

## Removed from build/runtime

- `ai/DeepDspController.*`
- `ai/DeepDspTinyNet.*`
- `widgets/DdspPanelWidget.*`
- MIND side-tab creation
- MIND trainer thread startup/shutdown
- MIND FT8/FT4 callback wiring
- MIND MSK144 callback wiring
- MIND UI translation entries
- MIND OpenMP CMake option and build-script forwarding
- MIND icons from resources

## Decoder status

FT8, FT4, MSK144, CW and RTTY now run only through classical DSP/LDPC/Baudot/Morse paths. RTTY was not actually using MIND in the live chain, but its cleanup was verified while removing the runtime.

## Historical note

Some internal FT performance structs still keep legacy zeroed diagnostic field names for ABI/local refactor safety. They are not wired to a neural model, no trainer is instantiated, no candidate-ranker callback exists, and no MIND UI is exposed.
