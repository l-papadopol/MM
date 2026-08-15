# FT4 adaptive runtime parity — MadModem 0.5.78

## Field symptom

The supplied live log showed the process-wide FT worker target decreasing from
14 workers to one during FT8, despite low system CPU and an OpenGL waterfall
normally below 2 ms. After changing mode, FT4 inherited that one-worker target.
Its 81% gate and boundary then each ran four complete candidate passes (480
candidate attempts), taking about 1.4 s and 1.8 s respectively. The long gate
also caused boundary deferral.

## Corrections

1. Every new AudioEngine FT capture session restores the topology-derived live
   worker budget and clears capture-local audio-pressure history.
2. Resource adaptation samples current queue latency when the job is queued,
   rather than the maximum spike observed anywhere in the slot.
3. Worker reduction requires sustained queue pressure. A normal 4096-frame
   backend cadence or one transient scheduling delay is not enough.
4. Post-decode table insertion time is retained as telemetry but does not by
   itself reduce candidate workers; actual waterfall backlog still can.
5. The FT4 81% gate is one pass only.
6. The complete FT4 boundary is limited to three decode-driven passes and a live
   pass budget.
7. FT4 candidate decoding uses atomic work stealing in the persistent pool, as
   FT8 already does.
8. FT4 rows use the common table/overlay GUI batch without per-row runtime-log
   writes. Half-slot diagnostic keys are normalized before comparison.

## Unchanged

FT4 tone spacing and sync patterns, candidate frequency/DT grid, LDPC(174,91),
CRC, message unpacking, SNR estimation, TX generation, audio capture, QSO
sequencer and waterfall DSP are unchanged.

## Validation available in this package

- `scripts/check_ft4_adaptive_runtime.sh`
- `scripts/check_waterfall_hidpi_viewport.sh`
- `scripts/check_waterfall_hidpi_labels.sh`
- `scripts/run_waterfall_leveler_regression.sh`
- `scripts/run_cw_native_regression.sh`
- localization and documentation audits

A complete Qt build and live FT4 timing test must be performed on the target
machine because the preparation container does not contain Qt development
packages or an audio device.
