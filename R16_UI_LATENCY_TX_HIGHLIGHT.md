# R16 — UI latency and text TX progress

This revision targets two issues reported during extended CQ WW RTTY operation.

## TX progress

Text TX progress is rendered with `QTextEdit::ExtraSelection`: the transmitted prefix is green and the next character uses the accent underline. The overlay does not mutate the underlying QTextDocument on every audio callback.

## Long-session UI slowdown

The previous live terminal highlighter ran roughly every 160 ms and rescanned/reformatted the entire accumulated RX terminal. In contest mode each callsign match also iterated `m_logbook.records()`; because `records()` returns by value, this copied and rescanned the complete ADIF logbook once per detected callsign. Both costs therefore increased throughout a contest session.

R16 changes live highlighting to a bounded tail, constructs the active contest-dupe set once per pass, coalesces auto-fill refreshes, feeds waterfall text from incremental characters, and batch-prunes bounded histories. DSP ownership and the R15 RTTY V2 receive algorithm are unchanged.

## Validation

The consolidated UI, architecture, release, FT and waterfall guard suites pass locally. Full Qt compilation and the native RTTY synthetic CTest remain CI responsibilities.
