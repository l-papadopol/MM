# FT8 FFT runtime audit — MadModem 0.5.78

## Material examined

- the current MadModem `Ft8RxDecoder.cpp`;
- bundled MSHV upstream `decoderft8.cpp` and its FFT support tree;
- the existing source-level audit of the user-supplied
  `wsjtx-3.1.0_improved_PLUS_260522.tgz`;
- the live Linux profiler log supplied for this checkpoint.

This is a source and profiler audit. It is not a claim that one FFT library is
universally faster.

## Current MadModem path

MadModem uses an in-tree iterative radix-2 complex FFT. FT8 candidate discovery
uses 4096-point transforms for cached Costas spectra at each timing hypothesis.
The decoder then parallelizes candidate refinement/LLR/LDPC work externally.
Signal subtraction deliberately uses a time-domain envelope/filter path rather
than paying a large forward/inverse FFT for every decoded signal.

This arrangement has two useful properties for live operation:

1. no planner creation or external FFT runtime is needed in the critical path;
2. the adaptive FT pool is the only owner of candidate parallelism, avoiding
   accidental `N worker threads × M FFT threads` oversubscription.

## MSHV reference

The bundled MSHV upstream decoder performs large transforms through its `four2a`
abstraction and uses FFT-based filtering/downsampling in the reference chain. It
also applies an inexpensive hard Costas-sync bail-out before LDPC and performs
subtraction only after a valid decode. These policy decisions are more important
to MadModem's current bottleneck than replacing a 4096-point radix-2 transform
in isolation.

## WSJT-X Improved reference

The previously completed source audit records that the Improved FT8 path uses a
192000-point candidate downsampler, 0.0625 Hz bins, multiple timing variants,
more pass groups, AP/QSO hypotheses, OSD and successive cancellation. Its CPU
cost therefore reflects a substantially broader search, not merely a different
FFT routine.

## Live profiler evidence

In the supplied Linux test, early-gate search was commonly about 29–72 ms and
complete-boundary search about 266–305 ms. Boundary LDPC plus subtraction was
usually larger, while 19–31 messages were recovered in roughly 0.78–0.89 s.
This does not prove the internal FFT is optimal, but it does show that an FFT
library replacement is not yet the dominant or safest optimisation.

## Decision for this checkpoint

- Keep the current internal FFT.
- Do not enable FFTW threading inside the FT candidate pool.
- Do not introduce CUDA/OpenCL for FT8 candidate search.
- Use the GPU for the workload with a demonstrated benefit: waterfall storage,
  scrolling and composition.
- Preserve FFTW as the existing optional dependency where other modes require
  it; do not make it a new mandatory FT8 runtime dependency.

## Measurement required before a future change

A future comparison must run in the same executable, on the same captured slot,
with persistent plans and identical candidate policy. Record separately:

```text
normalisation
Costas cache construction
FFT execution
candidate extraction/NMS
LLR and LDPC
OSD
subtraction/residual scan
total wall time
messages recovered
```

Test at least the internal radix-2 backend and single-threaded FFTW under the
same external worker budget. A threaded FFTW plan must not be nested under the
candidate pool. Replace the engine only when the same live slots show a stable
wall-time improvement without lost decodes, increased queue latency or poorer
Windows packaging/legacy compatibility.
