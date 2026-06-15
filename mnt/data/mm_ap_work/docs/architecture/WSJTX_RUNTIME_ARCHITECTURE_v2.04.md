# WSJT-X runtime alignment notes for MadModem v2.04

## Problem observed in MadModem

Testing showed that on old dual-core hardware MadModem could sit at low CPU load
for most of the RX period and then jump to 70-100% around the slot transition.
That is a bad FT architecture: the slow decode work then competes with the
sequencer, TX message selection, PTT and audio start.

## WSJT-X architecture point used here

WSJT-X does not treat the UI timer as the decoder engine. Audio is accumulated
continuously, symbol counters (`m_ihsym`) advance as audio arrives, and the slow
`jt9` decoder is invoked at specific symbol-count gates. In the WSJT-X 3.0.1
source:

- FT8 normal decode gate uses `m_hsymStop = 49`.
- FT4 uses `m_hsymStop = 21`.
- One hsym step corresponds to about 0.288 s.
- For FT8, WSJT-X also signals the decoder side so old work can bail out rather
  than stack up as stale jobs.

MadModem does not yet embed the full separate `jt9` shared-memory process, but
v2.04 adopts the same runtime division: continuous slot collection, a deliberate
single decode gate, and no stacking of overlapping stale decoder passes.

## MadModem v2.04 implementation

`Ft8RxDecoder` remains a worker object in its own QThread. `MainWindow` only
queues raw FT audio blocks to it.

`Ft8RxDecoder::wsjtxDecodeGateSamples()` maps WSJT-X's gates to the native 12 kHz
internal decoder rate:

- FT8: `49 * 0.288 * 12000 = 169344 samples`, about 14.112 s.
- FT4: `21 * 0.288 * 12000 = 72576 samples`, about 6.048 s.

`maybeStartStreamingDecodeSlot()` now launches at most one decode for the current
slot at that gate. `finishCurrentSlot()` does not start another decode if the
WSJT-X-gated decode was already launched.

## Why this is architectural rather than a local bug fix

The old policy was decode-pass proliferation: live pass, more live passes, then
final/boundary pass. That can look attractive on a fast CPU but is exactly the
wrong shape on old hardware because it creates an end-of-period work pile-up.
The v2.04 policy moves MadModem toward the WSJT-X model: one slow decoder job at
a known point in the RX period, with sequencer/TX pre-arm left enough time before
the next transmit slot.

## Remaining architecture work

The next pass should move QSO progression/logbook context into a dedicated FT QSO
session object so `MainWindow` no longer owns individual state variables such as
`m_ft8SeqDxCall`, `m_ft8SeqReportSent`, pending retry strings and log fields.
