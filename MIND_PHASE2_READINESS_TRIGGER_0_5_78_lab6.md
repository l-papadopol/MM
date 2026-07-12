# MIND Phase 2 — Readiness Trigger (0.5.78-lab6)

This lab closes the missing part of Phase 2: MIND must be allowed to learn from a fresh install, but it must not influence the decoder until the active profile is mature.

## What changed

- MIND readiness is now evaluated per active profile.
- FT8 and FT4 no longer borrow readiness from each other.
- Each profile tracks its own:
  - total candidate samples,
  - positive samples,
  - negative samples,
  - validation count,
  - current ranker accuracy,
  - best ranker accuracy.
- UI reports active-profile Pos/Neg instead of global mixed counters.
- New lifecycle states:
  - Cold,
  - Learning,
  - Validating,
  - Assist Ready,
  - Assist Active.

## Gate policy

MIND may score candidates and collect telemetry immediately. It may train continuously in the background. It may only prune/open extra recovery when the active profile passes its readiness trigger.

For FT profiles the assist gate requires:

- enough profile samples,
- enough positive examples,
- enough negative examples,
- enough validation samples,
- stable average ranker accuracy,
- stable best ranker accuracy.

## What did not change

- No LDPC/CRC bypass.
- No message hallucination.
- No intra-frame reliability weighting yet.
- No erasure-like assist yet.
- No multi-period combining yet.

This remains Phase 2. Phase 3 can start only after this trigger is verified on real FT8/FT4 traffic.
