# MM v2.75 — FT post-TX RX slot guard

This patch fixes a timing bug observed after FT8/FT4 transmit.  The RX decoder
was restarted immediately after TX, while UTC time was often still inside the
just-transmitted slot.  The decoder could then collect only the quiet tail of
our own TX period and report `slot skipped, not enough audio`.  More seriously,
a late audio restart could shift the sample vector relative to the real UTC slot.

Changes:

- MainWindow now notifies `Ft8RxDecoder` when an FT TX cycle ends, passing the
  actual transmitted slot boundary.
- If TX ended before the transmitted slot is over, the decoder ignores the
  remainder of that slot and waits for the next UTC boundary.
- If TX/driver/PTT release is slightly late into the next RX slot, the decoder
  keeps the slot but inserts leading zero padding so samples remain aligned to
  the absolute UTC slot, WSJT-X/MSHV style.
- The stale short post-TX tail no longer emits `slot skipped, not enough audio`.

This does not change FT modulation, CAT/PTT, sequencer state, or CW.
