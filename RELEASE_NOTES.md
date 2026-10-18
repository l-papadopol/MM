# MadModem 0.6.3

Windows packaging revision R2 moves the GitHub build to MSYS2 UCRT64, including
Qt5, OpenSSL, ANGLE and Hamlib. Runtime DLLs are resolved from the same environment.
The application version remains 0.6.3; CW engines and the FT clock fix are unchanged.

## 0.6.3 — Native R12 and Neural R13

- Two CW RX A/B choices only: Native R12 (default) and Neural R13 (experimental). Native algorithms, R13 numerical weights and R13 CTC search parameters are unchanged.
- Removed the older neural runtime paths and their bundled weights. Old saved CTC/Envelope selections migrate to Neural R13; unknown settings fall back to Native R12.
- Pinned the FT clock outside the mode settings scroll area, above QRZ. Removed the nested FT sequencer summary and duplicated period/next-message labels above the clock. Sequencer details remain in the clock tooltip; the live TX banner and message selection continue to update.
- Updated public version, Windows resources, localized help and UI strings to 0.6.3. Removed obsolete training files, caches and generated build products from this source delivery.
- Validation details: `docs/architecture/RELEASE_0_6_3.md`. Historical R12/R13 benchmark reports describe those earlier source packages.


Native Windows/macOS builds and tests on a connected radio remain to be run.
