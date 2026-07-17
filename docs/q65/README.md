# Q65 in MadModem 0.5.78

Q65 is a developing RX/TX mode with selectable submode, period and decode depth. The full receive bridge uses MSHV-derived decoder components and the same FFTW3 dependency already required by the active MSK144/MSK40 decoder. The Q65 full bridge itself remains controlled by its CMake option.

Relevant source:

- `modems/q65/`
- MSHV-derived Q65 support selected in `CMakeLists.txt`
- shared system FFTW3 discovery used by MSK144/MSK40 and the optional Q65 bridge

Q65 still requires continued reference-WAV and on-air validation. Historical compile-port and layout notes are archived in `docs/history/subsystems/q65/`.
