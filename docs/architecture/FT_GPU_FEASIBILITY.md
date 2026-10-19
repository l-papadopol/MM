# FT GPU investigation — R5

## Measured bottleneck in the supplied Windows log

The supplied run uses SSE2, 6 physical / 12 logical processors and a 10-worker
boundary budget. Of the 19 recorded boundary jobs, mean total time is 1297.5 ms:
search 219.4 ms, LDPC 718.9 ms and subtraction 358.5 ms. The 20 gate jobs average
420.7 ms: search 75.4 ms, LDPC 223.3 ms and subtraction 121.8 ms.

Even removing every search cost would remove only about 17% of boundary time.
That is an upper bound for a search-only acceleration, before GPU submission,
transfer and synchronization costs. This profile motivates investigating batched
soft-metric / LDPC work as well as spectral work. It does not establish a GPU
speedup.

## Proposed experimental direction

- Keep candidate admission, hypotheses, thresholds and recovery budgets fixed.
- Evaluate batched tone-energy/soft-metric computation on existing admitted
  candidates, with CPU protocol/CRC/message validation preserved.
- Profile batched LDPC separately: short variable-cost codewords may make
  dispatch and divergent iterations significant.
- Evaluate FFT/spectral work in a persistent device-buffer pipeline rather than
  transferring every individual candidate or decoded signal.
- Measure end-to-end QSO latency, full job duration, CPU usage and exact decoded
  message sets on identical captured RF input. Treat numerical differences near
  weak-signal thresholds as a correctness issue requiring investigation.
- Keep device selection explicit until multiple Windows/macOS/Linux GPU/driver
  combinations have been measured. Preserve the existing CPU path.

No GPU render device is exposed in this verification environment. R5 ships no
GPU decoder implementation or unmeasured GPU switch. The existing OpenGL
waterfall does not establish compute-decoder support or performance. The
previously validated four RF WAV fixtures were unavailable in this workspace
and were not found by exact Library-title lookup. They remain required for a
broad RF sensitivity comparison before any numerical GPU backend is promoted.

## Primary documentation consulted

- Intel, Optimize Offload Performance: account for transfer and execution costs
  together; reuse device allocations and avoid excessive small offloads.
  https://www.intel.com/content/www/us/en/docs/oneapi/programming-guide/2023-2/optimize-offload-performance.html
- Khronos, OpenGL Compute Shader: compute dispatch, workgroups and synchronization;
  compute shaders are core starting with OpenGL 4.3.
  https://wikis.khronos.org/opengl/Compute_Dispatch
- Intel, Minimizing Data Copying: memory placement and host/device transfers.
  https://www.intel.com/content/www/us/en/docs/opencl-sdk/developer-guide-processor-graphics/2019-4/minimizing-data-copying.html
