# MadModem 0.5.9-beta

## RTTY RX
- Removed automatic polarity detection and all CAT/signal polarity-probe runtime code.
- Manual Reverse remains the only polarity control.
- Replaced the former Auto polarity checkbox with `Narrow Mark/Space filter`.
- R15 moves Mark/Space selectivity inside `RttyDecoder`: two independent complex baseband channels, each followed by a baud-derived 2nd-order Butterworth low-pass. Normal full channel bandwidth is about 1.5 x baud; Narrow uses about 1.2 x baud.
- The obsolete parallel RTTY twin-bandpass path was removed from `DspConditioner`, so AFC, channel filtering and demodulation now share one frequency owner.
- Added selective-fading Automatic Threshold Correction (ATC) with separate Mark/Space envelopes and noise-floor tracking.
- Replaced the old carrier gate dependency on total wideband input power with a sustained channel-separation metric, preventing strong off-frequency contest signals from desensitizing the selected RTTY decoder.
- Filter changes are applied directly to the RTTY decoder worker path.

## Cabrillo
- Added `File -> Export Cabrillo...` to the Logbook.
- Three-step wizard: QSO scope, contest/exchange settings, validation/preview.
- CQ WW RTTY uses the existing strict contest-specific formatter/validator.
- Generic Cabrillo 3.0 lets the operator select CONTEST plus sent/received ADIF exchange fields; it validates missing station, frequency, UTC and exchange data before saving.
- Removed the old hidden Cabrillo choice from the ordinary ADIF save-file filter.

## Version / guards
- Version advanced to 0.5.9-beta.
- Architecture, UI/contest and release/localization/documentation guard suites pass locally.
- Full Qt compilation is left to CI because the local validation container has no Qt5 development package.

## R13 — CI guard synchronization

- Removed the stale UI audit requirement for the deleted `rtty_contest_macros` localization key.
- Documentation audit now validates the current compact bilingual README instead of requiring the retired alpha-era overview sections.
- Corrected the English README status from `alpha software` to `0.5.9-beta software`.
- Source revision at that stage: `0.5.9-beta-source-r14-rtty-multidecoder-api-fix`.


## R14 build fix

- Restored `RttyDecoder::setVisualizationEnabled(bool)`, which is required by `RttyMultiDecoder` to suppress per-track scope/visualization work.
- The R13 cleanup accidentally removed this public API while leaving both the backing state and the multi-decoder call in place, causing Linux/macOS/Windows compilation to fail.

## R15 — RTTY V2 phase 1 and synthetic testbench

- Added `madmodem_rtty_synthetic_bench`, a deterministic CTest that generates known ITA2/AFSK and measures character error rate under AWGN, selective fading, strong CW interference, retuning and pure noise.
- The benchmark includes a close-CW comparison between Normal and Narrow channel bandwidths instead of relying only on on-air subjective testing.
- RTTY V2 phase 1 implements one Mark/Space channelizer, baud-derived Butterworth filtering, optimized ATC-style threshold correction and a channel-domain carrier gate.
- Shadow decoders used by `RttyMultiDecoder` automatically use the same channelized demodulator, while wideband signal discovery remains unchanged.
- Prototyping against the deterministic synthetic model showed the largest gain under selective fading and strong off-frequency interference; the C++ CTest is the authoritative regression once CI builds it on Qt.


## R16 — long-session UI latency + TX progress highlight

- Restored the live green text-progress indication during RTTY/PSK/MFSK/Hell TX using `QTextEdit::ExtraSelection` overlays instead of repeatedly rewriting the TX document formatting.
- Live RX callsign/contest highlighting now rescans only a bounded 4096-character tail (plus token margin) every 160 ms instead of the complete terminal history.
- Contest duplicate lookup now snapshots the ADIF logbook once per highlighting pass and builds a dupe set, replacing the previous whole-logbook copy/scan for every callsign match.
- Contest RX auto-fill changes are coalesced through the existing highlight timer instead of forcing immediate whole-terminal highlighting for every field update.
- The RTTY waterfall text trail is now fed from the incremental `characterReceived` stream; the GUI no longer receives/copies the decoder's complete growing text history for every character.
- Terminal history and runtime-log pruning now happen in chunks rather than deleting one oldest character/line on every new append after the cap is reached.
- Added static regression checks so the live-tail/highlight and lightweight TX-overlay paths cannot silently regress.
## R17 — text assistance and logbook indexes off the GUI thread

- Added a dedicated `LogbookIndexWorker` thread as the single owner of worked-before and active-contest duplicate indexes. Live lookups are O(1) hash membership checks.
- Exceptional index rebuilds receive only the ADIF path and parse it inside the worker; `MainWindow` no longer copies the complete logbook record vector for highlighting/dupe assistance. Normal QSO logging updates the index incrementally.
- Added a dedicated `TextAssistWorker` thread for callsign/locator recognition and generic contest exchange candidate extraction from bounded RX text snapshots.
- RTTY, CW RX A/B, PSK and MFSK now share this asynchronous path. The GUI only applies returned ranges/metadata; decoders remain unaware of logbook, contest parsing and widgets.
- Added CTest/static regression coverage for worker ownership, O(1) lookups, contest band scope, incremental QSO updates and text/contest parsing.

