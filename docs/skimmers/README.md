# CW and RTTY skimmers — MadModem 0.6.5

## Ownership and data flow

`runtime/RxDecoderWorker` owns continuous RX processing on its worker. RTTY's selected receiver receives the conditioned pair; `RttyMultiDecoder` receives the full input block, so the selected narrow software filter cannot hide the other stations. The radio's physical passband still limits both.

`CwBandSkimmer` and `RttyMultiDecoder` each resample once to 12 kHz using the anti-aliased `LinearResampler`. RTTY lane DSP remains in the existing RX worker. CW band DSP belongs to one separate `CwBandSkimmerWorker` thread; there is no operating-system thread per band station. CW selected RX A/B keep their dedicated timing tasks. The CW band receivers use `CwRelativeTimingTask` inline on their band worker.

The CW queue holds at most 0.75 seconds or 96 blocks. Overflow invalidates only band state and reduces its capacity; A/B remain continuous. Wall-time load adapts the cap between 2 and 16 channels, with slow recovery after sustained headroom. Generation checks discard stale results after reset. Only bounded value snapshots cross the mutex; DSP and thread joins never run under that mutex. The worker is joined on destruction. Offline input can outpace this live skimmer; use real-time pacing when evaluating it.

This is simultaneous stream decoding, not unlimited CPU parallelism. Channel counts, pre-roll, text history and display rate are bounded. Weak-signal decoder jobs, TX preparation and CAT remain in their existing workers.

## CW acquisition

`CwSkimmerEngine` discovers persistent carrier lanes; it does not decode text itself. `CwBandSkimmer` assigns up to 16 lanes to `SelectedToneCwTracker` instances with independent automatic WPM, bounded AFC and timing state. The discriminator produces timed MARK/SPACE evidence, `CwRelativeTimingDecoder` estimates timing families, and `CwMorseBeamDecoder` commits credible prefixes. RX A and RX B use the same native receive pipeline with their own controls.

## RTTY acquisition

`RttyMultiDecoder` scans paired tone energy across 100–3500 Hz every 350 ms. The selected baud, shift and polarity apply to every lane. Local paired peaks, sideband rejection and confirmation across successive scans reduce duplicate/transient receivers. Each lane owns a `RttyDecoder`, including its ITA2 letters/figures and character framing state. Small frequency corrections move both tones together. Inactive lanes expire after four seconds.

Limits: 2–16 lanes, 12 by default; one second of input pre-roll; 192 characters per lane; snapshots at no more than five per second. Tracking is not a mixed-format classifier. Overlapping pairs, strong adjacent stations and radio filters can prevent acquisition. There is no guarantee that every visible pair is independently decodable.

## Shared display and CQ recognition

`dsp/text/SkimmerSpot.h` is the queued value snapshot. `SkimmerCqTracker` consumes committed per-station tokens, requires two matching callsigns in CQ context, rejects locator tokens and expires stale calls. It also accepts repeated CQ cycles carrying the same call. It does not infer a confirmed QSO from text.

`widgets/SkimmerOverlay` places stable vertical text columns or stacked CQ labels. Text placement reserves neighbouring carrier/Mark/Space positions. If no column fits within a bounded distance, the renderer reports hidden columns with `+N · zoom` rather than overlap. Font metrics and viewport zoom determine actual capacity.

The GUI updates from snapshots only. Skimmer output never starts TX, changes the selected correspondent, controls a rotator or writes the logbook. The selected RTTY waterfall trail is suppressed while its skimmer is enabled.

## Lifecycle

Reset on RX discontinuity, format change, CAT retune and local TX resume; changing mode clears the shared overlay. The RTTY scan history is discarded with its decoders. Existing queued worker operations remain ordered; QObject children are deleted on their owner thread, without waiting for deferred-delete delivery during offline input.

## Validation

Use focused CW/RTTY checks when these paths change. `tests/RttyContestRegression.cpp` exercises independent ITA2 input, letters/figures, AFC, multistation CQ and logbook integration. `tests/CwIntegrationRegression.cpp` and `scripts/run_cw_native_regression.sh` cover the CW production paths. The full suite is manual, separate from distribution builds. Keep generated audio, images, logs and build trees outside the source package. On-air recordings remain necessary for receiver AGC, QRM and fading behavior.
