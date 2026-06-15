# WSJT-X-style FT sequencer / TxPlan notes for MadModem v2.07

MadModem v2.07 keeps the hybrid decoder strategy already present in v2.06, but tightens the FT QSO control plane.

## Design rule

The FT QSO state machine must not infer the next transmitted string from scattered UI text.  The standard message set is generated once from the current QSO context:

- MYCALL from Settings -> User/QTH
- MYGRID from Settings -> User/QTH
- DX call/grid from the active session
- sent report from the active session once a QSO is underway
- TX audio frequency from runtime FT state

That generated set feeds both the table and the scheduled `FtTxPlan`.

## Message ordering

Standard FT messages are interpreted as:

`CALL1 CALL2 PAYLOAD = TO FROM PAYLOAD`

Therefore, a received line where MYCALL is the second token is our own outgoing message or a third-party exchange and must not advance the auto-sequencer.  This rule is retained from v2.06.

## Ordinary auto-sequence

The ordinary WSJT-X-like QSO path implemented by the existing sequencer is:

- grid/locator from DX -> report
- report from DX -> R-report
- R-report from DX -> RR73
- RR73/RRR/73 from DX -> final 73/completion path

`FtStandardMessageSet` ensures that the chosen row text is identical wherever it is consumed.

## Remaining non-goals for this pass

This is not a full import of WSJT-X `MainWindow::processMessage()` and does not claim full parity for contest special messages, compound calls, EU VHF modes, nonstandard calls, or all message 77 variants.  It is the focused v2.07 repair for the ordinary contest FT8/FT4 path that was failing when a received report did not reliably advance the selected TX row.
