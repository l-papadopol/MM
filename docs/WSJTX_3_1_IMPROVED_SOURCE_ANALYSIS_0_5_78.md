# WSJT-X 3.1 Improved source analysis for MadModem 0.5.78

## Source examined

Archive supplied for this audit:

- `wsjtx-3.1.0_improved_PLUS_260522.tgz`
- SHA-256: `f159cde764ababb2c63ee8f307f18f29db0db322e152622cc5b4e2c7575fb5a0`

The archive contains the standard FT8 decoder and the Improved multithreaded
`ft8var` decoder. The findings below are source-level observations, not claims
about runtime speed on a particular computer.

## What the standard decoder actually does

`lib/ft8_decode.f90` defines `MAXCAND=1000`, keeps up to 200 early decodes, and
runs staged work at `nzhsym=41`, `47` and `50`. Early CRC-valid signals are
saved with their tones, frequency and DT, then reconstructed and subtracted
before later work. The normal/deep path runs up to three passes; the shallow
path runs two. Candidate sync thresholds are not merely a single UI setting:
the standard deep path uses a lower threshold than the shallow path.

The important architectural point is that early work is not discarded. It
changes the signal presented to the final stage by successive interference
cancellation.

## What the Improved MTD decoder adds

The active Improved path is in `lib/ft8var/`:

- `ft8_decodevar.f90`
- `sync8var.f90`
- `ft8_downsamplevar.f90`
- `ft8bvar.f90`
- `subtractft8var.f90`
- `bpdecode174_91var.f90`
- `osd174_91var.f90`

Concrete parameters found in the supplied source:

- `nft8cycles=1/2/3` maps to 3/6/9 passes.
- The broad timing search uses 40 ms steps over approximately +/-2.5 s.
- The frequency candidate grid is 3.125 Hz.
- Low-threshold pass groups use sync thresholds around 1.225, 1.3 and 1.1.
- Candidates within about 4 Hz and 0.1 s are deduplicated.
- Candidates close to the QSO frequency are promoted and allowed a 1.1 sync
  threshold; virtual QSO candidates at +/-5 s are also inserted.
- `ft8_downsamplevar.f90` uses a 192000-point transform (0.0625 Hz bins),
  extracts the candidate region and returns complex data at 200 Hz, 32 samples
  per symbol. It also creates half-sample timing variants.
- `ft8bvar.f90` searches approximately +/- one quarter symbol around the
  initial DT estimate before demodulation.
- Belief propagation is followed by OSD with deeper settings for selected QSO
  and repeat/Again cases.
- CRC-valid waveforms are refined in DT/frequency and subtracted from the shared
  signal so subsequent candidates see a cleaner residual.

This explains why the MTD path can consume substantially more CPU: it is not
just the same decoder compiled less efficiently. It evaluates more pass types,
more timing representations, AP/QSO hypotheses and repeated residual searches.

## Comparison with MadModem before this checkpoint

MadModem already had several good foundations:

- 3.125 Hz live frequency grid;
- approximately 40 ms timing grid in adaptive mode;
- local DT/frequency refinement before LDPC;
- multithreaded candidate decoding;
- deterministic LDPC, CRC and message validation;
- waveform subtraction and a residual scan;
- GF(2) OSD and OSD-lite recovery;
- detailed rejection/performance counters.

The principal differences were policy and coverage:

1. The live gate pass had a roughly 480 ms adaptive budget.
2. Candidate, residual and OSD caps were substantially smaller than the
   available WSJT-X search breadth.
3. The full-slot boundary pass was intentionally skipped whenever the earlier
   gate pass had already run.
4. Deep/residual work could be disabled by heuristics even though the complete
   15-second signal was now available.
5. Audio blocks were assigned to UTC slots using decoder processing time rather
   than a capture-time sample timeline.

Therefore the observed ~600 ms decode time was not proof that MadModem performed
the same search 20 times faster. A material part of the speed came from doing
less final-stage work and, in live operation, from skipping the complete-slot
pass.

## Changes made in this checkpoint

- The early gate pass is retained for low UI/sequencer latency.
- A second full-slot boundary pass is always scheduled from an immutable
  snapshot and is no longer suppressed because the gate pass succeeded.
- The boundary pass has an independent wideband deep policy, larger candidate
  caps, up to three pass groups, successive subtraction and a broader residual
  budget. It is not conditional on an active QSO or CQ heuristic.
- Messages already emitted by the gate pass are deduplicated, so the boundary
  pass only adds newly recovered messages.
- The gate pass remains bounded and responsive; the boundary pass may continue
  while capture of the next UTC slot proceeds.

## What is deliberately not claimed yet

This is not a complete port of `ft8var`. In particular MadModem does not yet
replicate all AP subpasses, the 0.0625 Hz long-FFT candidate downsampler, all
half-sample variants, or the full WSJT-X OSD policy.

The next comparison must use the same WAV files and publish, for each pass:

- raw Costas candidates;
- candidates surviving sync gates;
- LDPC attempts and failures;
- OSD attempts/recoveries;
- CRC-valid messages;
- subtraction count;
- residual candidates and added decodes;
- wall time by phase.

The headless `--ft-regression` command now emits an `FTPERF` record for every
analyzed slot with candidate/pass counts, search/LDPC/subtraction time, sync
rejects, LDPC failures, OSD attempts/recoveries, residual candidates/recoveries
and the adaptive stop decision.

Only those measurements can show whether the remaining 88-versus-114 gap is
mainly candidate generation, demodulation/LLR quality, LDPC/OSD recovery or
successive cancellation.

## FT4 source findings

The supplied source also shows that FT4 is not treated as a single cheap decode
attempt. In `lib/ft4_decode.f90` the standard path keeps up to 200 candidates,
uses a sync threshold of about 1.18 for the normal 7.5-second mode (1.0 for the
3.75-second variant), and performs successive subtraction when depth permits.
The deep path runs three outer subtraction stages. Inside each candidate it
tries five independent bit-metric representations before any a-priori passes;
the normal QSO states add two AP passes and the final state can add three. Thus a
candidate can receive seven or eight decoding attempts, with OSD depth raised
near the QSO frequency.

FT4 also performs a two-level time/frequency refinement. Candidate discovery is
followed by a coarse DT/frequency search over three time segments, then a fine
search around the best position. The supplied source includes a three-point
parabolic sub-sample DT correction before bit-metric extraction. CRC-valid
signals are reconstructed and removed with `subtractft4`, allowing later outer
stages to search a cleaner residual.

For MadModem this means that FT4 parity cannot be judged only from final wall
time. The meaningful counters are candidate coverage, sync-quality rejects,
number of metric/AP attempts, LDPC/OSD outcomes and decodes added after each
subtraction stage. The new capture timeline and immutable full-slot snapshot are
shared foundations for FT8 and FT4, but FT4 still requires its own source-level
comparison before its search policy is enlarged.
