# MadModem 0.6.0 — CW receiver

Source revision: `0.6.0-source-r1-cw-receiver`.

- Recover the complete qualified startup sequence, including short messages whose first character contains only dots or only dashes. Acquisition can use the configured timing prior after independent carrier confirmation; a mixed short/long pair still acquires an unknown speed.
- Reject startup noise falsely paired with a much stronger real element. Allow strong early elements to survive the reduced coherence while AFC converges.
- Protect the wanted carrier inside the AFC acquisition interval from classification as its own interferer. Filter scanner candidates by local spectral prominence to reduce keying side lobes reported as stations.
- Feed the selected receive filter into the actual CW discriminator. Gate AFC updates on confident, spectrally distinct marks.
- Keep manual WPM fixed both in the relative timing model and in the Bayesian mark-duration model.
- Prevent end-of-recording flush from decoding an unqualified noise fragment.
- Update application, Windows resources, translations and help metadata to 0.6.0.

The existing R22 TX/CAT/FT/RTTY fixes are retained. This revision changes CW reception and release metadata.

## Validation

The existing native CW suite covers speed changes, fading, timing recovery, long gaps and noise. The new `madmodem_cw_integration_regression` exercises the application's CW wrapper and scanner with short messages, AFC offsets from -20 to +20 Hz, a stronger adjacent carrier at +70 Hz, manual timing, 44.1/48/96 kHz input, and actual discriminator filter attenuation. Its shaped test signals use portable seeded noise.

Build and test evidence is included in `verification-060`. Local validation uses Linux/Qt 5; Windows and macOS builds and reception from real radio audio still require testing. Synthetic tests do not establish performance on every propagation or interference condition.

For GitHub, commit the contents of the archive's source folder at the repository root. The release tag matching these sources is `v0.6.0`.
