# Q65 in MadModem 0.5.8

Q65 is a developing RX/TX mode with selectable submode, period and decode depth.
The full receive bridge uses GPL-compatible MSHV-derived decoder components and
FFTW3 when the required headers/library are available. Without that full backend,
Q65 TX remains available but RX is explicitly unavailable; MadModem does not
pretend that buffered audio is a functioning decoder.

Relevant source:

- `modems/q65/`;
- Q65 support selected in `CMakeLists.txt`;
- bundled/system FFTW3 discovery in the Q65 CMake section.

Reference-WAV and on-air validation remain required before claiming complete Q65
receive parity.
