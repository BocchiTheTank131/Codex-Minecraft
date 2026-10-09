# Weather Update validation - v2.10.0

## Release build and compatibility

MSVC Release, C++17, OpenGL 3.3. Both full and stateless-demo executables were built
through the existing CMake/Inno Setup packaging workflow and tested outside the repository.

Passed automated checks:

- Survival/core regressions and command parser/runtime checks.
- Weather metadata and random-sequence roundtrip, legacy/invalid metadata fallback.
- Three independent installed worlds retain distinct weather state/timers.
- Demo sessions reset to Clear and create no world, inventory, player, mob, seed or settings files.
- Weather settings serialization (including quality, density, flash and toggles).
- Rain/snow/dry biome samples; stone/glass/slab roofs and removed-roof height updates.
- Exposed versus sheltered farmland, rain extinguishing fire, all eight snow depths, torch melting.
- Safe spawning on accumulated snow, appended-block IDs and save/load coverage.
- Cycle durations, cycle toggle, transitions, weather commands/errors/autocomplete.
- Lightning damage, Creative/Spectator immunity, rod recipe/autofill and strike redirection.
- Bounded ignition and delayed thunder dispatch at distance / 343 seconds.
- All 19 supplied OGG files decode; sample-group counts 8/4/3/4, rain clip handoff,
  shelter gain crossfade, SFX mute, positioned lightning voices and audible ranges.
- Real GL rendering at all four quality levels; Off produces zero precipitation particles.
- Captured/inspected rain, snow, thunder, flash and Weather settings screens.
- Actual installers: fresh install, upgrade from 2.9.1, repair, reinstall, direct shortcuts,
  uninstall, and retention of isolated user data. Copies of existing legacy saves loaded;
  original user files were hash-checked and left unchanged.
- Both final executables are GUI-subsystem builds, with system DLL imports only.
- Final executable/checksum verification; supplied audio hashes unchanged.

## Performance

Hardware: Intel i7-14700F, AMD Radeon RX 9060 XT, approximately 32 GB system RAM.
1280x720, seed 123456789, fixed camera/daytime (world time 35), brightness 50%, simulation
distance 8, MSAA/VSync/F3 off, settled chunk queues. Four seconds sampled per quality.
The benchmark sets Rain weather in the origin cold biome, so precipitation here is snow.
RAM is process Working Set, not total system RAM. The same camera is used at each distance.

| RD | Quality | FPS | Mean ms | 99th percentile ms | Worst ms | RAM MiB | Particles |
|---:|---|---:|---:|---:|---:|---:|---:|
| 8 | Off | 3166.2 | 0.316 | 1.306 | 2.357 | 288 | 0 |
| 8 | Low | 3255.6 | 0.307 | 1.118 | 1.929 | 288 | 197 |
| 8 | Medium | 3419.5 | 0.292 | 0.699 | 4.575 | 285 | 3586 |
| 8 | High | 1893.0 | 0.528 | 0.733 | 1.447 | 248 | 11279 |
| 16 | Off | 1499.2 | 0.667 | 0.816 | 1.029 | 340 | 0 |
| 16 | Low | 1490.0 | 0.671 | 0.819 | 7.215 | 341 | 197 |
| 16 | Medium | 1465.6 | 0.682 | 0.836 | 1.491 | 338 | 3586 |
| 16 | High | 1462.8 | 0.684 | 0.914 | 2.003 | 338 | 11280 |
| 32 | Off | 511.5 | 1.955 | 2.131 | 9.452 | 699 | 0 |
| 32 | Low | 513.3 | 1.948 | 2.146 | 2.339 | 701 | 197 |
| 32 | Medium | 511.2 | 1.956 | 2.159 | 8.173 | 699 | 3586 |
| 32 | High | 511.4 | 1.956 | 2.136 | 2.847 | 699 | 11283 |
| 64 | Off | 184.6 | 5.417 | 5.603 | 13.144 | 2131 | 0 |
| 64 | Low | 184.6 | 5.418 | 5.597 | 5.793 | 2134 | 197 |
| 64 | Medium | 184.6 | 5.416 | 5.580 | 10.515 | 2131 | 3586 |
| 64 | High | 184.2 | 5.430 | 5.593 | 5.667 | 2131 | 11284 |

Loaded/rendered chunks: RD8 197/73; RD16 797/266; RD32 3209/1003; RD64 12853/3891.
Precipitation adds one draw when nonempty; Off adds none. Counts are local and do not scale
with render distance. CPU scratch is capped at 150,000 vertices (about 4 MiB). Typical High
precipitation uploads about 1.8 MiB/frame; cached roof data adds 512 bytes per loaded chunk.

The initial fixed-buffer/subdata upload approach caused a reproducible AMD-driver problem
after active weather rendering: RD64 rose to roughly 32 ms and 9,120 MiB Working Set in the
audit fixture. Replacing only the used stream range with glBufferData resolved it: the same
fixture measured about 5.47-5.48 ms and 2.1 GiB at RD64 across all four weather qualities.
The final shipped-binary fixture above corroborates that result. Clear control at RD64 was
5.404 ms; High weather was 5.430 ms. Small differences at RD32/64 are within sample noise.
High has a measurable roughly 0.2 ms cost at RD8; Low/Medium are cheaper. No graphics
quality was reduced to obtain these results.

## Limits and reproducibility

No integrated/low-end GPU was available; these measurements do not establish performance
on that hardware. Four-second samples do not establish long-session leak or tail-latency
guarantees. Engine audio/position/gain checks passed, but no audio-loopback waveform analysis
or subjective listening test was performed. Villager shelter uses existing navigation and
bounded candidate searches; a broad manual village/shelter stress test was not performed.
Optional morning-specific fog and foliage physics were not added; weather fog and local
wind-driven precipitation are implemented.

Run --weather-smoke --seed=123456789 or --weather-benchmark=<output-folder> with an isolated
VOXEL_FRONTIER_DATA_DIR and an executable-only working directory. Test screenshots, logs,
profiles, benchmark CSVs and compiled executables are excluded from Git.

The final local distribution files are:

- VoxelFrontier-v2.10.0-Windows-Standalone.exe
- VoxelFrontier-v2.10.0-Windows-Setup.exe
- SHA256SUMS.txt

Release artifacts are published together with the matching v2.10.0 source tag.
