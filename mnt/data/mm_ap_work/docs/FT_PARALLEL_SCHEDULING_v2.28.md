# FT parallel scheduling — v2.28

v2.27 already moved FT8 candidate search to a MSHV-style spectral matrix and
brought WAV timing down from roughly 10 seconds to a few hundred milliseconds on
the user's test files.

v2.28 keeps that algorithm and changes the threading model inside the decoder
worker:

1. Candidate search no longer assigns start indices by `worker, worker+nWorkers,
   ...`; workers pull the next DT/start hypothesis from an atomic counter.
2. Candidate LDPC decode no longer assigns candidates by static modulo; workers
   pull the next candidate from an atomic counter.
3. Offline WAV analysis can use all detected cores.
4. Live RX reserves one core for the rest of MadModem: audio input, UI, CAT/PTT,
   scheduler and TX pre-arm.

This preserves the divide-and-conquer architecture:

```text
live audio / WAV slot
        ↓
FT decoder worker
        ↓
parallel candidate search + candidate decode
        ↓
Decode list / sequencer / logbook
```

Scheduler, PTT and TX audio remain outside this parallel work.
