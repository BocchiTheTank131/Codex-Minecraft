# v2.13.0 Equipment & Combat validation

Date: 2026-10-13. Release v2.13.0. Baseline: v2.12.1 commit `4fe7baa`.

## Implementation

- Appended item IDs 148–163 for 16 armor pieces, plus Shield 164; existing IDs 0–147 remain unchanged. No block IDs or terrain generator changed.
- Added four dedicated armor slots and an offhand slot, shared across Survival, Creative, crafting-table, chest and furnace interfaces. Armor slots accept only the correct piece. Offhand accepts normal items. Shift equips into an empty matching slot; occupied armor preserves normal inventory/container transfers. Manual clicks swap pieces, and shift unequips when storage has space.
- Left drag release, right clicks, Q/Ctrl+Q, F swaps and normal dropped-item pickup use actual owned stacks and preserve durability. Creative catalog copies stay unlimited. Armor points are cached and invalidated on equipment mutations/breakage.
- Added original 16×16 icons, all 17 recipes, Recipe Book entries, tooltips, durability bars, armor HUD and offhand display. Shields use the existing held-item batch, with raised presentation and brief BLOCKED feedback; breakage uses existing tool-break audio. No new terrain passes, VAOs, VBOs, shaders or mob sprites.
- Main-hand food, seeds, feeding, placement, tilling and container interactions retain priority. Offhand consumption/wear affects the correct stack. Blocking reduces movement speed by 35%; it never applies to Spectator.
- Routed existing melee, projectile and explosion attacks through one defense path with the actual attack origin. Billboard AI, speeds, climbing, navigation, attack scheduling, spawning, sprites and animations remain unchanged.

### Armor and shields

| Material | Helmet/chest/legs/boots points | Total | Helmet/chest/legs/boots durability |
|---|---|---|---|
| Leather | 1 / 3 / 2 / 1 | 7 | 55 / 80 / 75 / 65 |
| Gold | 2 / 5 / 3 / 1 | 11 | 77 / 112 / 105 / 91 |
| Iron | 2 / 6 / 5 / 2 | 15 | 165 / 240 / 225 / 195 |
| Diamond | 3 / 8 / 6 / 3 | 20 | 363 / 528 / 495 / 429 |

For physical damage D and armor A, effective armor is `clamp(max(0.2*A, A-0.5*D), 0, 20)`. Remaining damage is `D*(1-0.04*effective)`, capped at 80% reduction. Each equipped piece wears by `max(1, floor(D/4))` on an accepted, unblocked hit and clears when exhausted.

Shield durability is 336. Facing protection uses a dot-product threshold of 0.5 against the actual 3D attack direction (a 120-degree front cone). Accepted frontal blocks stop physical damage and its knockback, wearing by `max(1, ceil(D))`. Rear/side attacks and environmental damage bypass the shield. Existing invulnerability is checked before equipment wear; contact hits retain their 0.65-second protection and projectiles retain their independent damage behavior. Creative/Spectator take no equipment damage. Fall, starvation, void, lava/burning and other General environmental damage retain their existing behavior; this update does not add a drowning system.

Sword damage is 4/5/6/4/7 HP for Wood/Stone/Iron/Gold/Diamond. All swords use a shared 0.625-second cooldown. The existing attack timer is checked before hits and is not reset by ordinary clicks or hotbar changes. Existing critical multiplier, knockback, recoil and weapon wear remain. Attacking while blocking is prevented.

## Persistence and loot

Inventory V5 appends five serialized stacks (25 bytes) after the existing inventory/crafting/cursor payload. V4 loads with empty equipment. Loads validate armor slots and commit transactionally; truncated equipment does not partially replace the current bag. Atomic replacement remains in use. Equipment is scoped by the existing per-world inventory path.

World saves retain their header/version and append a 9-byte `VXGEAR1` loot-rule extension. New worlds enable optional worn gear in underground loot. Legacy saves lacking the extension keep their original unexplored loot rules; all new equipment is still craftable there. Existing/modified chests are never rerolled. Terrain generators 1–21 remain unchanged, including generator 21 already used by the baseline.

Equipment loot uses a separate deterministic weighted roll without disturbing historical rolls: Leather common, Shield/Gold/Iron uncommon, deep Diamond helmets very rare, with 35–90% durability. Across 10,000 shallow and 10,000 deep Ancient chest samples, 1,725 / 2,534 contained gear; 19 Diamond helmets occurred in total. This is a fixture result, not a promised global loot percentage.

New/reset inventories and equipment remain empty. Respawn retains equipment consistently with the existing inventory retention rule and clears blocking. Demo persistence stays disabled and privileged commands remain denied.

## Validation

Both final Release variants passed the Survival, command, fluid, weather, patch272 and menu smoke fixtures. Tests used isolated temporary data profiles; no live player saves were modified.

- All 16 armor recipes and IDs/UVs, all 625 empty/mixed-material combinations, exact reduction and wear, armor/shield/weapon breakage.
- Frontal/rear/side melee, projectile and explosion blocks, environmental bypass, invulnerability before wear, Creative immunity and Spectator immunity.
- Actual arrow collision: 10 frontal arrows dealt no damage/impulse and wore the shield by 30; a rear arrow dealt 3 HP without shield wear; full Iron reduced a real 3 HP arrow to 1.38 HP.
- Manual equipment clicks/swaps, invalid-slot rejection, shift equipping/unequipping, occupied-slot transfer, full-inventory rejection, single/full offhand drops, F round trips, Creative catalog versus owned-slot operations, and worn-shield drop/pickup conservation.
- Mixed equipped armor + shield V5 round trip, truncated-load transaction, V4 migration, independent loot-rule migration, worn-gear chest save/reload and equipped environmental death/respawn.
- Primary-use selection for seeds, Wheat, shovels, food and blocks; offhand tool wear cannot damage the main-hand sword.
- Existing 90-recipe crafting/Recipe Book, billboard roles/physics/combat, fluids, hazards, UI tooltips, menu/world switching, permissions and weather/audio checks passed.
- Underground audit: 168 connected layouts (57 mineshafts, 92 dungeons, 19 ruins), chunk-clipped placement, encounter/chest persistence and deterministic loot passed.
- **2,928 historical chunk hashes for generators 1–20 matched exactly**; historical CSV SHA-256 `a53f68e367c41d5a18e6d9931e8e030810210c35f944ff19f12d6e527c5a68cf` equals the baseline. Generator save/load and unexplored-chunk equality passed for versions 1–21.
- V21 8,192-block terrain/climate maps (512×512 samples) were byte-identical to the prior build for seeds 123456789, 20260917 and 42.
- All 148 historical atlas cells remained pixel-identical. New icons use appended/free cells; existing occupied textures were not replaced.

### In-engine captures

Captured and visually inspected with the actual renderer. The combat view is a controlled preview containing all four original billboards and a test block event; real projectile routing is tested separately above. PNG copies preserve every captured pixel.

- [Equipment inventory](images/equipment-inventory.png)
- [Creative catalog and equipment](images/equipment-creative.png)
- [Shield, armor HUD and billboard sprites](images/equipment-shield.png)
- [Equipment in persistent chest UI](images/equipment-chest.png)

## Performance against v2.12.1

Hardware: i7-14700F (28 logical threads), Radeon RX 9060 XT. Release, seed 123456789, Generator 21, 1280×720, SD8, default morning phase (100 seconds), clear weather/graphics Off, MSAA Off, VSync Off, uncapped. Each RD used sequential baseline/new runs with 20 s stationary load, 8 s 8× flight, 12 s recovery and 20 s teleport loading. Compiler jobs and other game fixtures were not running during timings. Earlier measurements overlapping builds were discarded.

These are **background-launched existing streaming fixture measurements**, not foreground gameplay FPS or isolated GPU timings. They run in Spectator and do not draw armor/shield UI or simulate normal combat. They are useful engine regression checks, not a measured equipment-HUD overhead or proof of low-end GPU performance. Only one seed and this discrete GPU were timed; integrated graphics remain untested.

### Stationary recovery scene

Values below are **v2.12.1 / v2.13.0**. RAM is process working set at phase end, in MiB. The brief initial queue recovery is included.

| RD | Fixture FPS | Mean ms | p99 ms | Worst ms | RAM MiB |
|---|---|---|---|---|---|
| 8 | 4129.1 / 4129.7 | 0.242 / 0.242 | 0.717 / 0.656 | 5.03 / 17.63 | 247.0 / 242.7 |
| 16 | 1287.5 / 1269.2 | 0.777 / 0.788 | 0.993 / 1.141 | 4.90 / 4.04 | 312.7 / 313.4 |
| 32 | 367.9 / 367.3 | 2.718 / 2.723 | 3.016 / 3.002 | 7.94 / 18.15 | 571.2 / 574.3 |

Steady mean-frame differences were approximately −0.01%, +1.44%, +0.18% for RD8/16/32. These small single-pair changes do not establish a performance gain or sustained regression. Steady RAM differences ranged from −4.3 to +3.1 MiB. The expanded icon texture adds exactly 327,680 raw RGBA bytes (0.3125 MiB); equipment storage is fixed size. Streaming allocator/job timing also varies between runs.

### Busy phases

| RD | Phase | Mean ms before / after | p99 ms before / after | Worst ms before / after |
|---|---|---|---|---|
| 8 | flight8x | 0.298 / 0.289 | 1.204 / 0.939 | 20.29 / 20.44 |
| 8 | teleport | 0.498 / 0.503 | 0.789 / 0.818 | 11.63 / 3.92 |
| 16 | flight8x | 1.030 / 1.034 | 4.431 / 4.333 | 31.32 / 17.46 |
| 16 | teleport | 1.104 / 1.153 | 1.708 / 2.233 | 14.31 / 10.92 |
| 32 | flight8x | 3.097 / 3.097 | 6.324 / 6.353 | 19.55 / 18.23 |
| 32 | teleport | 2.713 / 2.857 | 4.796 / 4.908 | 15.28 / 29.87 |

The first teleport pair was slower after the update by 4.5% at RD16 and 5.3% at RD32. Worst frames varied in both directions; this is not a demonstrated speedup. A repeat using the final build produced:

| RD32 repeat phase | FPS before / after | Mean ms before / after | p99 ms before / after | Worst ms before / after | RAM MiB before / after |
|---|---|---|---|---|---|
| Recovery | 371.9 / 373.9 | 2.689 / 2.675 | 2.888 / 2.866 | 7.39 / 6.26 | 573.6 / 573.8 |
| Teleport | 391.4 / 380.2 | 2.555 / 2.630 | 4.182 / 4.117 | 9.73 / 19.52 | 657.9 / 658.1 |

The repeated teleport mean was 2.9% slower, while its p99 improved slightly and steady recovery was 0.5% faster. A small loading-phase slowdown remains observed in these pairs; it is not hidden or claimed as a gain. The fixture does not execute equipment simulation/UI, so these measurements do not establish a defense-system cause. No broad steady-frame or memory regression appeared. Additional foreground measurements would be needed to separate driver/scheduling variation from a small persistent loading cost.

The fixture's opaque submission counts are asynchronous snapshots and vary by a few submissions; no terrain submission algorithm changed. Its terrain buffer accounting also remains close, with no new terrain geometry or buffers. Dedicated GPU pass timings, representative equipped-combat FPS and comprehensive manual OS-level click/drag sequences were not measured. UI logic/ownership was tested automatically and captures were inspected; an extended human playtest remains useful.

Raw results: `build/v2130-validation/performance-comparison.csv`, per-RD `clean-*-rd*/streaming.csv`, `streaming-samples.csv`, and repeated RD32 files. The benchmark reports p99 frame time, not average-of-slowest-1% FPS.

## Changed files

- New focused modules: `src/Equipment.cpp`, `src/EquipmentInput.cpp`, `src/EquipmentValidation.cpp`, `src/EquipmentPreview.cpp`.
- Items, recipes, stacks and combat: `include/Survival.h`, `src/Survival.cpp`, `src/Definitions.cpp`, `include/Player.h`, `src/Player.cpp`, `include/Game.h`, `src/Game.cpp`, `src/ContentValidation.cpp`.
- UI/input: `include/UiLayout.h`, `include/InventoryTooltip.h`, `src/InventoryUI.cpp`, `include/UIManager.h`, `src/UIManager.cpp`, `include/InputManager.h`, `src/InputManager.cpp`, `include/Renderer.h`, `src/Renderer.cpp`.
- Loot/save rule: `include/ExplorationLoot.h`, `src/ExplorationLoot.cpp`, `include/World.h`, `src/World.cpp`.
- Build/docs/art: `CMakeLists.txt`, `README.md`, this report and four capture PNGs; `tools/generate_equipment_assets.py`, 17 new armor/shield inventory PNGs, `assets/inventory_atlas_16.png`, `assets/item_icons_expansion.png`, `assets/item_icons_expansion.rgba`.

## Release outputs

Both Release variants and packaging succeeded; remaining compiler warnings were recorded in `build/v2130-validation/package-final-input.log` (no build errors). Installer generation succeeded and SHA-256 sums match `dist/SHA256SUMS.txt`.

Final standalone SHA-256: `1f8f755f90bdeee9c43dfc118d691effab34834b1b8933328e21af7e03665dfb`.
Final installer SHA-256: `48497f28038dffd8b4de69af0dea031a506e5f966907144f2fb1dc202a1e9e4e`.

- Full game: `build/installed-release/Release/VoxelFrontier.exe`.
- Full installer: `dist/VoxelFrontier-v2.13.0-Windows-Setup.exe`.
- Standalone demo: `dist/VoxelFrontier-v2.13.0-Windows-Standalone.exe`.

Release publication: [Voxel Frontier v2.13.0](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.13.0).
