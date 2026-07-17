# Q65 in MadModem 0.5.78

Q65 is a developing RX/TX mode with selectable submode, period and decode depth. The full receive bridge uses MSHV-derived decoder components and FFTW3 when the required headers/library are available; otherwise CMake retains the safe buffered fallback path described by its configuration warnings.

Relevant source:

- `modems/q65/`
- MSHV-derived Q65 support selected in `CMakeLists.txt`
- bundled/system FFTW3 discovery in the Q65 CMake section

Q65 still requires continued reference-WAV and on-air validation. Historical compile-port and layout notes are archived in `docs/history/subsystems/q65/`.
