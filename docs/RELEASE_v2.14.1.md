# Voxel Frontier v2.14.1 - Transportation & Navigation Polish

## Improvements
- Safer saves: serialized writers, staged per-world recovery checkpoints, actionable Windows failure diagnostics, and bounded retries scheduled across frames. Failed saves are reported and preserve recoverable previous data.
- Boats: spatial collision filtering, swept shoreline collision handling, bounded catch-up simulation, and dismount checks against actual rotated boat hulls and exposed water surfaces.
- Maps: clearer markers and labels, known-area refresh with F6 where the key is available, forest-canopy surveying correction, and zoom feedback with protection against repeated Paper consumption.
- Waypoints: coordinate validation, clearer selection feedback, and deletion confirmation.
- Preserves Generator V22 and all historical generators, existing save formats, equipment, gameplay, and all four billboard mobs. The standalone demo remains stateless.

## Validation
Both Release variants built and passed the relevant regression suites: 13 core suites plus full/demo menu checks. All 663 historical terrain hashes matched in each variant. Validation includes v2.14.0 save loading, concurrent saves, interrupted-process checkpoint recovery, locked-file recovery, maps and inventory transfers, and boat simulation at 15/30/60/120/240 FPS and frame spikes.

## Known limitations
The original intermittent Windows replacement failure's underlying cause remains unresolved; recovery paths and improved diagnostics are tested. Save serialization remains synchronous, with maximum navigation-data saves reaching approximately 20 ms in stress runs. Performance comparisons were mixed: no general FPS or RAM speedup is claimed, and small frame-time overhead was measured in some scenes. Teleport stalls remain unresolved (approximately 37-63 ms in the focused comparison). Physical low-end hardware and installation over an existing user installation were not tested.

See the [full validation report](https://github.com/BocchiTheTank131/Codex-Minecraft/blob/v2.14.1/docs/TRANSPORTATION_POLISH_VALIDATION.md) for measurements, screenshots, and remaining limitations.

## Downloads
- **Windows-Setup.exe**: full game installer with persistent worlds.
- **Windows-Standalone.exe**: standalone stateless demo.
- **SHA256SUMS.txt**: SHA-256 checksums for both downloads.
