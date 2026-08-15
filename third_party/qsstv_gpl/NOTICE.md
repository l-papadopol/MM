# QSSTV notice

MadModem v0.89 includes a QSSTV-derived sound-card clock calibration concept and keeps relevant QSSTV source files as reference material.

QSSTV copyright and attribution:

- QSSTV by Johan Maes, ON4QZ
- Source reference: QSSTV project source tree provided by the user as `QSSTV-main.zip`
- QSSTV is distributed under the GNU General Public License, version 3 or later, as indicated by the QSSTV license files included here.

The MadModem calibration dialog is a Qt Multimedia adaptation of the QSSTV idea of counting RX/TX sound-card frames against a monotonic clock. It is not a direct VCL/ALSA/PulseAudio UI copy, because MadModem already has its own Qt audio engine and cross-platform device handling.

Relevant original QSSTV files are retained under `reference/` for traceability and future deeper SSTV RX assimilation.
