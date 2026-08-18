# Q65 implementation target

Q65 is not a public MadModem feature until the complete engine described below
is implemented and validated. A TX-only mode or an RX path hidden behind an
optional build switch does not meet the release requirement.

## Definition of done

- Q65A, Q65B, Q65C and Q65D provide complete interoperable RX and TX.
- The engine is included in every normal MadModem build; no user-selectable
  CMake option may produce a crippled Q65 mode.
- One owned realtime decoder path handles UTC period assembly, candidate search,
  synchronization, demodulation, FEC decoding, message unpacking and averaging.
- The transmitter generates complete protocol-compliant frames for every
  supported submode and period.
- UI controls, standard messages, sequencing, CAT/PTT timing, logging and
  diagnostics operate consistently with the other weak-signal modes.
- Linux, Windows and macOS builds run the same implementation. A missing
  required dependency is a build error, not a runtime RX-unavailable state.
- Recorded reference signals and live-radio tests cover all four submodes,
  multiple signal levels, frequency offsets, drift, QRM and averaging.

## Current source status

`modems/q65/` currently contains useful protocol/TX work and a conditional
receive bridge, but it does not yet satisfy the definition above. The future
implementation must consolidate that work into the single mandatory engine;
it must not add another fallback decoder or preserve competing runtime paths.
