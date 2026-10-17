# Voxel Frontier v2.14.2 - Stability, Reliability & Performance

## Improvements
- Safer world saves: checksummed, generation-numbered checkpoints; retained previous checkpoints; interrupted-save recovery; lifetime per-world session protection; and explicit failures instead of silently replacing corrupt sidecars with defaults.
- World selection isolates damaged saves. Select an unavailable world and press **R twice within five seconds** to recover a verified previous checkpoint into a separate world when available. Keep external backups.
- Death saves preserve terrain, containers, inventory and entities consistently. Survival health and hunger retain their saved values.
- Non-Spectator players wait safely for unloaded collision terrain. Distant teleports use asynchronous streaming; far-coordinate distances no longer overflow or bypass simulation limits.
- Corrected negative-Y/far-coordinate farming, partial-block targeting, numeric validation, mob/drop save bounds and transactional loading.
- Bounded 120 Hz simulation retains elapsed time at low frame rates, with explicit overload diagnostics and a more precise world clock.
- Incremental crop discovery reduces scan spikes, distant passive/drop rendering avoids unnecessary geometry preparation, and unreferenced map records can be reclaimed without reusing referenced identities.
- Autosave commits immutable staged files on one background worker. Capture and serialization remain synchronous. Improved save and terrain-retirement diagnostics expose remaining costs.
- Preserves Generator V22 and all historical generators, world geography, item/block IDs, equipment, boats, navigation and the four billboard mobs. The standalone demo remains stateless with privileged commands disabled.

## Validation
Both Release variants built and passed **20 full/demo regression suites**. Each matched all **663 historical chunk hashes**. Tests include corrupt sidecars, coherent death saves, interrupted checkpoint rollback, deliberate Windows file locks, simultaneous sessions, 5,001 mobs/drops, high-altitude saves, negative-Y crops, unloaded collision and low-FPS timing.

Actual foreground Survival walking and boating passed at RD8/16/32 with weather, equipment, maps, save/reload and injected frame spikes. An isolated v2.14.1-to-v2.14.2 installer upgrade/uninstall preserved fixture saves and left the production installation unchanged. RD64 received a brief stress check.

## Known limitations
All 25 audit findings are accounted for: **15 Fixed, 6 Improved, 3 Deferred, 1 Unresolved**. Teleport retirement stalls remain unresolved; a large sample was localized to shared-page trimming/destruction. The underlying cause of the original intermittent Windows replacement error also remains unresolved, although tested failure/recovery safeguards are improved.

Main-thread capture/serialization, global simulation/furnace traversal, edit-history compaction, moving-camera occlusion and true geometric terrain LOD remain limited or deferred. GPU memory targets remain soft; RD64 can exceed them. Legacy animal persistence flags cannot be reconstructed and are preserved conservatively. The live map cap remains 128. Older clients do not honor the new session reservation.

Matched performance was broadly similar, with mixed frame-time differences; **no general FPS or RAM speedup is claimed**. Physical low-end hardware, physical power loss, multi-hour sessions and an upgrade over the user's actual installation were not tested.

See the [detailed validation report](https://github.com/BocchiTheTank131/Codex-Minecraft/blob/v2.14.2/docs/STABILITY_VALIDATION.md) for evidence, measurements, screenshots and all finding classifications.

## Downloads
- **VoxelFrontier-v2.14.2-Windows-Setup.exe**: full game installer with persistent worlds.
- **VoxelFrontier-v2.14.2-Windows-Standalone.exe**: single-file, stateless demo.
- **SHA256SUMS.txt**: SHA-256 checksums for both downloads.
