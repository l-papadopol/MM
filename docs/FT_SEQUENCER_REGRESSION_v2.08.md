# MadModem FT sequencer regression notes — v2.08

This build adds a small optional QtCore self-test target for the FT parser and sequencer.
It is intentionally independent from MainWindow, CAT, audio, waterfall and scheduler code.

Enable and run it with:

```bash
cmake -S . -B build-ft-tests -DMADMODEM_BUILD_FT_TESTS=ON
cmake --build build-ft-tests --target MadModemFtSequencerSelfTest
./build-ft-tests/MadModemFtSequencerSelfTest
```

Covered regression cases:

- normal locator reply -> Tx3 report;
- normal report reply -> Tx4 R-report;
- R-report -> Tx5 RR73;
- RR73/RRR/73 -> Tx6 final 73;
- repeated RR73 while Tx6 is already armed must not complete/log prematurely;
- own transmitted echo `DX MYCALL payload` is ignored;
- compound addressed calls such as `EA/IZ6NNH` match base call `IZ6NNH`;
- bracketed/hash calls such as `<IZ6NNH>` are normalized before parsing;
- directed CQ such as `CQ DX K1ABC FN42` extracts the real caller and grid;
- RR79/RRxx traps are not accepted as Maidenhead locators;
- simple contest/field-day payloads are recognized as directed exchanges instead of being misparsed as locator/report/free text;
- contest acknowledgement form `MYCALL DXCALL R GRID` drives the RR73 phase.

The test checks the sequencer core only. It does not prove RF-level decode, audio timing, CAT/PTT timing or full WSJT-X processMessage equivalence.
