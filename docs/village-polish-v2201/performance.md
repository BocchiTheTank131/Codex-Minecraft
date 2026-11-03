## Matched performance results

Intel i7-14700F (28 logical CPUs), Radeon RX 9060 XT; native 1280x720, seed 20260917, V24 district (5,-1), SD8, unchanged render settings. Original v2.20.0 production-object driver versus the new production-object driver. Six-second warmup and twelve measured seconds per run, four phases (day, night, rain, Trade UI), two repeats in alternating old/new order. Cohorts start at natural roads; ambient spawning/audio are excluded equally. No compiler or other test process ran during timing.

The explicit cap is 60 FPS, but wall intervals include limiter/scheduler/presentation waiting and often exceed 16.7 ms on this host. CPU work excludes the limiter and includes presentation; it is not pure GPU time. GPU queries are collected only when available. Moving NPCs/streaming can produce different resident geometry; exact counts and triangle ranges are retained in performance.json and raw CSVs.

Daytime metrics below are medians of the two per-run summaries (not pooled percentile calculations), in milliseconds. Worst and RAM are maxima across those daytime repeats.

| Version | NPC/RD | Mean CPU | p95 | p99 | Worst | Mean GPU | Mean NPC AI | Peak RAM MiB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| v2.20.0 | 1/8 | 0.967 | 2.030 | 2.948 | 6.690 | 0.190 | 0.001 | 333.8 |
| v2.20.1 | 1/8 | 0.986 | 2.208 | 2.733 | 6.751 | 0.194 | 0.001 | 335.6 |
| v2.20.0 | 10/8 | 0.975 | 2.053 | 2.931 | 6.490 | 0.194 | 0.007 | 334.6 |
| v2.20.1 | 10/8 | 0.945 | 2.146 | 3.060 | 6.753 | 0.209 | 0.007 | 336.1 |
| v2.20.0 | 30/8 | 1.062 | 2.313 | 3.219 | 6.528 | 0.192 | 0.023 | 334.8 |
| v2.20.1 | 30/8 | 1.133 | 2.420 | 3.065 | 6.431 | 0.201 | 0.026 | 334.9 |
| v2.20.0 | 60/8 | 1.222 | 2.440 | 2.996 | 7.503 | 0.209 | 0.049 | 334.4 |
| v2.20.1 | 60/8 | 1.277 | 2.581 | 3.498 | 6.760 | 0.201 | 0.051 | 333.9 |
| v2.20.0 | 30/16 | 1.403 | 2.591 | 4.010 | 6.688 | 0.494 | 0.027 | 402.9 |
| v2.20.1 | 30/16 | 1.546 | 2.987 | 3.816 | 6.552 | 0.487 | 0.028 | 410.1 |
| v2.20.0 | 30/32 | 7.425 | 9.040 | 36.151 | 51.568 | 1.104 | 0.031 | 614.3 |
| v2.20.1 | 30/32 | 6.987 | 8.706 | 25.702 | 28.726 | 1.091 | 0.027 | 614.0 |

30-NPC RD8 p99 CPU work by repeat:
- old: 3.372 ms, 3.067 ms
- new: 3.070 ms, 3.061 ms

The previously recorded 7.759 ms p99 is a reference observation rather than a universal baseline. These repeated results do not establish a specific root cause for that earlier outlier or promise an across-the-board performance gain. This update adds richer geometry while bounding its submission to a shared instance batch.

| New 30-NPC RD8 phase | Mean CPU ms | p99 CPU ms | Mean GPU ms |
| --- | --- | --- | --- |
| Day | 1.133 | 3.065 | 0.201 |
| Night return | 0.973 | 2.531 | 0.204 |
| Rain shelter | 1.139 | 2.625 | 0.202 |
| Trade UI | 1.647 | 3.378 | 0.212 |

Raw per-run save capture/complete durations, snapshot sizes, CPU/frame/GPU timing, resident/triangle counts and navigation work are in benchmark-raw.zip. No physical low-end/iGPU certification or naturally populated simultaneous multi-village performance claim is made.

| 30 NPC / RD8 checkpoints | Capture median ms | Complete median ms | Encode median ms | Commit median ms | Snapshot bytes |
| --- | --- | --- | --- | --- | --- |
| old | 2.141 | 24.825 | 1.962 | 20.133 | 45716, 45717 |
| new | 2.004 | 22.510 | 1.929 | 17.317 | 46436 |

Snapshot sizes reflect the actual saved NPC state, not raw presentation data. New poses have no save extension. These short captures do not establish a save-speed improvement.
