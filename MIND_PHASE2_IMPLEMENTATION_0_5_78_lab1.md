# MIND Phase 2 implementation audit - MadModem 0.5.78-lab1

## Scope

This lab build implements the second step after the MIND Phase 1 audit:
make the current ranker measurable per FT profile and add a real FT4 candidate
hook without changing the final FT validation rules.

## Implemented

- FT8/FT4 MIND scoring callback is now mode-aware.
- DeepDspController receives the active FT mode when scoring native candidates.
- Assist readiness is calculated per active profile instead of borrowing FT8
  readiness for FT4.
- FT4 candidate decode now builds a deterministic `58 x 8` MIND feature view
  from the FT4 data-symbol tone energies.
- FT4 candidates are scored before LDPC for telemetry.
- FT4 positive/negative native samples are exported to MIND training with the
  `FT4` profile label.
- FT4 negative sample export is bounded/deterministic to avoid flooding the
  replay buffer.
- MIND JSON stats include FT8 and FT4 validation counts and per-profile ranker
  accuracy fields.

## Not implemented yet

This build deliberately does **not** implement the later QSB research phases:

- no intra-frame symbol reliability map yet;
- no erasure-like LLR/LDPC modification yet;
- no multi-period soft combining yet;
- no CRC override and no message invention.

Final accepted messages still require:

```text
LDPC + CRC + unpack + parser
```

## Expected test signals

In FT4, the runtime log should start showing the same global MIND ranker line
used by FT8 when scoring is active:

```text
FT MIND Ranker: scored ..., pruned ..., extra ..., unavailable ..., avg success ...%
```

For this lab phase, FT4 `pruned` should normally stay zero. The important value
is `scored`, which proves that FT4 candidates are now reaching the MIND ranker.

## Next phases

- Phase 3: intra-frame reliability map.
- Phase 4: erasure-like LDPC/LLR assist.
- Phase 5: multi-period combining for actual repeated messages only.
