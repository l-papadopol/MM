# Logbook and QSO map behaviour for large ADIF files — v2.40

Large ADIF logs can contain tens of thousands of QSOs.  Plotting every QSO as an individual marker makes the map unreadable and slows the UI.

MadModem now keeps the full ADIF log in the logbook, but the map has its own display policy:

- by default the map shows the current UTC day and current mode;
- large historical logs can be inspected with the **Map display...** button;
- filters can be applied by band, mode and date window;
- the recommended setting for very large logs is **Show only latest QSO per Maidenhead square**;
- the map also has a maximum marker count.

The logbook itself remains ADIF-preserving: unsupported fields are kept for import/export round-trips and QSO appends are not supposed to rewrite huge ADIF files unnecessarily.

## Worked-before indicators

The logbook window has a persistent **Strike through worked calls** checkbox.  When enabled, callsigns that already exist in the ADIF log are marked in RX/decode views with a strike-through line.  This matches the common contest/digital-mode convention where already-worked calls are visually distinguishable without blocking the operator.
