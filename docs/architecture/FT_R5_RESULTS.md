# FT response / QRZ source R5 verification

## Reproduced defects

In the supplied on-air log, LA8ENA and LZ2KSQ are declared unanswered immediately
after the third TX ends. Their later RR73 decodes are then treated as a new QSO
without the saved WaitingFinal73 state, so automatic final 73 is lost. R5 consumes
each attempt once, retains the session through the opposite receive slot and
closes an unanswered session after that slot's final decode. A bounded fallback
timer allows 2500 ms after FT8 RX end or 2000 ms after FT4 RX end when the final
batch is unavailable. New exchange generations and STOP invalidate old timers.

The screenshot shows G0JEI in the active TX plan while QRZ still displays LZ2KSQ.
The sequencer deliberately blocks DX-field Qt signals; QRZ previously depended
on that signal. R5 explicitly refreshes selected-station state with the
sequencer UI and gives the FT field priority in FT modes.

## Preserved decoder behavior

R4's complete FT8/FT4 candidate finders and per-candidate decode functions are
byte-identical. Search thresholds, candidate limits, pass matrix, LDPC/OSD
budgets, subtraction gain/reference model and worker topology are preserved.

During an active QSO, accepted results are published between existing passes
and at accepted FT8 residual recovery. Later work continues; already emitted
rows are deduplicated before the final batch. No extra focused decoder or GPU
backend is added. Callsign/marker updates use a nonblocking configuration lock
and coalesce on the QObject owner thread; they apply after the current decode
releases its snapshot, before a deferred final job launches.

FT8 cancellation bounds are computed once and QVector detaches once per signal.
The exact arithmetic is retained. FT4 cancellation remains byte-identical.

## Controlled comparison

Five alternating before/after runs on the same host; median milliseconds. The
fixture is deterministic synthetic audio, not an RF recording: six actual
MSHV-generated messages, generated at 48 kHz and decimated to 12 kHz, with seed
771 Gaussian noise. The active RR73 at 1033 Hz overlaps a stronger CQ at 1050 Hz.
Both versions recover exactly the same six messages in all runs. Baseline uses
the R4-equivalent frozen decoder object and R4 headers; revised uses R5.

| Mode | QSO result ready, R4 → R5 ms | Full job, R4 → R5 ms | Results |
|---|---:|---:|---|
| FT8 | 545.2 → 393.3 | 545.1 → 547.3 | 6 → 6, identical messages |
| FT4 | 235.4 → 153.0 | 235.3 → 239.2 | 6 → 6, identical messages |

This establishes earlier result availability in this fixture. Full decode CPU
time is effectively unchanged within run-to-run variation; it does not establish
a broad throughput improvement or increased RF sensitivity. The preceding
real-WAV baseline files were not available, so the historical 26/25/16/21-decode
RF corpus was not rerun.

## Passed checks

- Full Linux Qt5 Release application build; public version 0.6.3.
- CTest FT parser/sequencer regression, extended for 15 s / 7.5 s periods, one
  and three attempts, final RX ownership, late RR73, stale callbacks and
  rearming identical text before audio.
- Actual MainWindow GUI probes in FT8 and FT4: last-attempt reply → final 73,
  no extra attempt, obsolete timeout ignored, genuine no-response stop, and
  cached QRZ selection/text LZ2KSQ → G0JEI during automatic report reply.
- Actual asynchronous decoder/coordinator signal path: six unique results and
  exactly one final boundary batch in each mode. Callsign/marker update bursts
  during a running decode complete without waiting for its mutex; latest
  values are applied after completion.
- 86 cancellation output buffers, including clipped starts/ends, produce the
  same SHA-256 before/after:
  `f62cae7818a7ed26c129f9fe87a79a6a02177050bf706e02919176de0bea430b`.
- FT source/algorithm guard suite: 6/6 passed.
- Release/localization/documentation/portability guards: 7/7 passed.

The old FT source guard's clock-placement and waterfall-comment checks were
stale on R4 itself. They now inspect the actual freeze branch and the already
pinned 0.6.3 clock; candidate-finder fingerprints remain unchanged. Translation
keys were reordered to match moved source text; every translation value is
preserved.

No authenticated QRZ request, native Windows/macOS build, connected radio test
or GPU hardware benchmark was performed here. R3 UCRT64/Hamlib fixes, R4 updater,
CW Native R12 / Neural R13 and the FT clock remain in the package.

The optional probes are included under tests. On a GCC/Clang Linux build with
compile_commands.json and Makefile-generator link.txt, run:

```sh
python3 tools/run_ft_response_probes.py /path/to/build
```

For a frozen older build, `--baseline` compiles only the latency probe against
that build's headers and linked objects; `--repeat 5` repeats its timing run.
Set the usual Qt/Hamlib runtime library paths for the selected build. These
probes are optional development executables, not application startup checks.
