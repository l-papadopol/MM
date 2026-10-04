# FT8 live runtime, adaptive resources and OpenGL waterfall — MadModem 0.5.78

## Scope

This checkpoint extends the `FT8_NO_SILENT_SLOT_DROP` baseline without changing
FT8 protocol thresholds, the native CW receiver, CAT/PTT policy or the atomic
sequencer. The work targets the field symptoms observed on Linux: an invalid
first UTC slot after RX start, occasional application stalls during that start,
a fixed eight-worker ceiling on many-core CPUs and visibly uneven waterfall
scrolling while a busy FT8 boundary decode completes.

## 1. First partial slot is synchronization-only

Every `AudioEngine::startInput()` increments a capture generation carried by
`AudioBlock`. On the first timestamped slot of that generation, any non-zero UTC
prefix that was not actually captured marks the period as partial. It remains
available to the waterfall and timing logic but is never sent to Costas search,
LDPC or OSD. Later slots also reject missing prefixes beyond ordinary callback
jitter.

The final boundary check is based on real captured samples:

```text
real captured = available samples - intentional UTC pre-padding
```

A complete FT8/FT4 frame must fit in those real samples before candidate search
is allowed.

## 2. Capture-generation and timestamp protection

The FT timeline rejects blocks from an older capture generation, non-monotonic
capture sequences and timestamps that move behind the accepted stream. Large
forward jumps discard the partial period and realign directly to the new UTC
slot; MadModem does not manufacture and decode intermediate zero-filled slots.
Large intra-slot gaps or overlaps invalidate that period.

Changing capture generation also invalidates an in-flight decode generation.
Candidate, residual and OSD loops observe that cancellation and relinquish CPU
instead of delaying the new RX session with obsolete work.

## 3. Dynamic system-resource controller

`SystemResourceManager` discovers the processors available to the process rather
than applying an eight-worker constant.

- Linux: `sched_getaffinity()` plus sysfs physical-core topology and hybrid
  `core_type` where exposed.
- Windows: processor groups, process group affinity, logical processors,
  physical cores and efficiency classes. Persistent FT worker threads are spread
  across visible processor groups on systems larger than one group.
- macOS: scheduler-visible logical/physical CPU counts and performance-level
  topology where available.
- Portable fallback: Qt/std hardware concurrency.

The persistent FT pool is created once and may contain all scheduler-visible
logical processors. The live controller reserves capacity for audio, GUI and
other interactive work, then assigns separate budgets to:

- the early gate;
- the complete boundary;
- concurrent OSD recovery;
- offline analysis.

The live target is adjusted only between jobs. Audio queue latency, waterfall
presentation/render latency, queued waterfall rows, system CPU load and useful
worker saturation can reduce or gradually increase the next job's concurrency.
There is no fixed eight- or twelve-worker live ceiling.

## 4. Persistent decode coordinator

A single persistent coordinator thread replaces per-slot `std::async` creation.
It accepts one bounded job, uses the persistent candidate pool and posts
completion housekeeping to the decoder QObject thread. A complete boundary can
still be retained as the one newest pending snapshot when a gate is active; no
unbounded backlog is allowed.

## 5. Atomic GUI presentation

Decoder rows remain one atomic batch for sequencer decisions. They are also
presented as one GUI transaction:

- table repaints and sorting are temporarily suspended;
- rows are inserted together;
- overlays, reports and performance panels update once;
- decode log lines are appended as one block;
- auto-scroll runs once.

The measured GUI batch time is fed back to the resource controller.

## 6. Circular OpenGL waterfall

Downward scrolling now uses a persistent RGBA texture. Each FFT line is converted
to one colour row and uploaded with `glTexSubImage2D`; a ring index and shader
select the chronological vertical order. The full image is no longer moved in
CPU memory for each row. Shader variants cover OpenGL ES, desktop core and
compatibility profiles.

A bounded row queue protects the GUI if presentation is temporarily delayed.
The existing QImage implementation remains the fallback when OpenGL setup fails
and remains the implementation for rightward scrolling.

## 7. Runtime telemetry

The runtime log reports:

- physical cores and logical processors available to the process;
- SMT width, P/E-core counts and Windows processor groups when known;
- persistent pool capacity and current gate/boundary/OSD targets;
- SIMD backend selected at runtime;
- per-slot capture generation, stale blocks, timestamp jumps and invalid slots;
- audio queue latency;
- GUI batch/presentation latency;
- waterfall render time, queued/dropped rows and GPU/CPU backend;
- automatic worker-budget changes.

## 8. FFT decision

The current checkpoint keeps the internal FFT engine. The associated source
audit is in `FT8_FFT_RUNTIME_AUDIT_0_5_78.md`. Candidate-search timing in the
supplied live log was materially smaller than LDPC plus subtraction time, and no
same-machine FFTW comparison was available in the preparation environment.
Changing FFT libraries without that measurement would add packaging and nested
threading risk without a demonstrated benefit.

## Preserved invariants

- one native CW RX implementation only;
- no legacy/fallback CW branch;
- atomic FT sequencer decision per completed batch;
- no truncated FT waveform when a deadline is missed;
- bounded pending decode state;
- fast shutdown and side-effect-free Settings Cancel;
- GitHub distribution and macOS workflows retained.

## Required live test

Compile cleanly, start RX at several positions inside a UTC period and confirm
that the first partial period is logged as skipped before candidate search. On a
busy band, verify continuous gate/boundary processing, worker targets greater
than eight where hardware permits, smooth waterfall scrolling and no stale
capture or unexplained timestamp-jump messages. Repeat on Linux and Windows
legacy; the preparation container cannot perform the Qt/QtMultimedia runtime
test.
