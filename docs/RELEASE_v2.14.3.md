# Voxel Frontier v2.14.3 - Checkpoint Integrity, Audio & Settings Polish

## Critical checkpoint fix
- Fixes the reproduced v2.14.2 regression where switching game mode wrote `world_seed.txt` outside the checkpoint, invalidated its checksum, and caused a same-process session-lock conflict.
- Mode, time and weather changes now remain in memory until a coordinated checkpoint saves them with player/world state. Standalone writes to checkpoint-managed files are refused.
- Background save requests coalesce into a fresh snapshot after the pending commit. World-selection cheats changes reserve the world and update fresh verified metadata transactionally.
- Integrity errors identify the component and expected/actual checksum details. Permanent failures stop automatic retries. Journaling, strict validation, atomic replacement and verified backup recovery remain enabled.

**Affected v2.14.2 worlds:** back up the entire world folder, including manifests, journals and backups. In Select World, select the unavailable world and press **R twice within five seconds** to recover a verified previous checkpoint into a **separate world**. Missing or invalid backups are refused; the original is never silently repaired or overwritten. Progress after the last complete checkpoint may not be recoverable.

## Audio and settings
- Updated sound effect and background audio, including all 24 background songs. A small top-right Now Playing label uses the current song's filename without `.ogg`.
- Fresh profiles and the standalone demo default to music **50%**, render distance **8**, simulation distance **8**, occlusion **Auto**, terrain LOD **Low**, and F3 closed. Existing installed-game saved preferences continue to load normally.
- Removed the extra out-of-panel Back button in Video settings; the normal bottom Back button remains.
- Antialiasing resolves the 3D scene before HUD, inventory, menus, maps and text, keeping UI separate. Disabling AA no longer blits an old multisample target. Underwater tint is applied before HUD rendering.

## Validation and scope
The checkpoint-correction build passed **20 full/demo regression suites**, **80 focused assertions**, persistence stress (400 concurrent writes and 200 paired checkpoints), interrupted metadata commits, legacy import and safe-copy recovery. Both variants matched **663 historical chunk hashes** each and played all **24** background songs through decoder/shuffle transitions.

The final settings/UI follow-up was rebuilt in both Release variants and checked through source/build inspection and distribution checksums. The complete suite was **not rerun after those final settings/UI changes**; retained checkpoint-test hashes identify the earlier checkpoint-correction build. Final package hashes are published separately.

World generation, billboard-mob behavior and existing v2.14.2 stability mechanisms are preserved. The standalone demo remains stateless with privileged commands disabled. The separate intermittent native Windows replacement error's underlying cause remains unresolved; recovery needs a complete verified checkpoint. No physical power-loss, multi-hour soak or physical low-end-hardware certification is claimed.

See [checkpoint validation and recovery](https://github.com/BocchiTheTank131/Codex-Minecraft/blob/v2.14.3/docs/CHECKPOINT_v2.14.3_VALIDATION.md), [settings/UI scope](https://github.com/BocchiTheTank131/Codex-Minecraft/blob/v2.14.3/docs/V2143_SETTINGS_POLISH.md), and [final artifact hashes](https://github.com/BocchiTheTank131/Codex-Minecraft/blob/v2.14.3/docs/v2143-release-artifacts.json).

## Downloads
- **VoxelFrontier-v2.14.3-Windows-Setup.exe**: full game installer with persistent worlds.
- **VoxelFrontier-v2.14.3-Windows-Standalone.exe**: single-file stateless demo.
- **SHA256SUMS.txt**: SHA-256 checksums for both downloads.
