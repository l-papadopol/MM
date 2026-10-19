# MadModem 0.6.3

Revision R6 adds optional, experimental FT8/FT4 RX linear-drift recovery near
the RX marker (Settings → FT8 / FT4, off by default). The normal decoder,
search thresholds, FEC budgets and standard SIC are retained. A bounded local
Costas fit and analytic dechirp add CRC-valid results after the existing passes.
Recent verified drift can predict the next local search for the same DX.
RX/TX markers and CAT are not moved. Real QO-100 recording validation is pending.

QRZ account configuration is now in Settings → QRZ.com; the main station-info
panel remains in place. Username and password persist in the local settings.mad
profile and are reloaded at startup. The profile stores the password without
encryption; session keys stay in memory. Clearing the password removes it.


FT revision R5 keeps the final receive turn open before declaring no response,
delivers validated QSO replies earlier during decoding, and updates QRZ when
the selected FT correspondent changes. FT8/FT4 decoder search and recovery
settings are retained. Public version: 0.6.3.

Updater revision R4 recognizes automatic `build-VERSION-N` tags when promoted
to stable and Latest on GitHub. Tag renaming is no longer required for clients
with this revision installed. A higher public version is still required.

Windows build revision R3 fixes a missing header and timer initialization in the
bundled Hamlib source, plus malformed Autoconf feature checks. UCRT64 packaging,
Native R12, Neural R13 and the FT clock fix are retained. Public version: 0.6.3.

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
