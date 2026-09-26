# Voxel Frontier: Survival, Creative & Spectator World

Voxel Frontier is a compact C++17/OpenGL 3.3 survival voxel sandbox. It uses real cube meshes, perspective projection, depth buffering, streamed chunks, first-person physics, 3D voxel raycasting, procedural textures, and persistent world edits.

## Current gameplay

- A 256-block-tall world with multi-noise continental terrain, broad biome climates, foothills and mountain ranges, rivers, beaches, oceans, ravines, several cave styles, local aquifers, trees, and depth-dependent ore veins
- Survival and Creative world modes, with an infinite Creative catalog, instant no-drop breaking, invulnerability, and double-tap flight; plus a non-interacting Spectator mode with noclip flight
- Cobblestone, stone bricks, bricks, granite, diorite, andesite, mossy masonry, glass, ice, gravel, clay, mud, snow, oak/birch wood sets, cactus, ladders, chests, furnace, bookshelf, door, and slabs
- Terrain-integrated stone-variant patches, gravel, clay shores, mountain snow/ice, crossed tall grass and flowers, birch trees, and desert cacti
- Seed-deterministic regional structures: modular plains/desert villages with roads, homes and farms; wooden pillager outposts; desert treasure pyramids; and small ruins, wells and campsites. Pieces can span chunks and use deterministic chest loot.
- Sunlight plus propagated torch light, ambient occlusion, dark caves, water transparency, moving sun/moon/stars/clouds, and a visual-only fullbright mode
- Health, hunger, regeneration, starvation, fall damage, death/respawning, sprinting, sneaking, and swimming
- Cows, pigs, sheep, and night/darkness-hostile wolves with bounded local spawning, light and surface checks, sight-aware chasing, local obstacle avoidance, passive fleeing/food-following, health, knockback, drops, feeding/breeding, and experience orbs. Villagers and melee pillagers spawn once from structure markers and persist through saves.
- Occlusion-aware mob combat: the closer visible mob wins over a block target; tools have distinct damage/cooldown profiles and falling attacks can crit
- Wheat seeds, farmland, four crop stages, harvesting, wheat, bread, and food
- Wooden, stone, iron, gold, and diamond tools plus five sword tiers, with mining tiers/speeds, combat profiles, durability bars, and breakage
- A persistent 36-slot inventory: 27 storage slots plus the nine-slot hotbar
- Stack pickup, merging, swapping, right-click splitting, right-drag single-item distribution, shift-click transfers, and dropping stacks into the world
- A 2x2 player crafting grid plus an interactive 3x3 workstation opened by right-clicking a placed crafting table; both include recipe-book autofill and batch crafting
- Per-block furnaces with live input/fuel/output, coal burn time, smelting progress, eight initial recipes, persistence, and stored-item drops
- Per-block 27-slot chests with stacking, splitting, shift transfer, persistence, and stored-item drops
- Recipes for planks, sticks, torches, crafting tables, chests, ladders, bread, every tool/sword tier, furnace, masonry, glass, snow blocks, bookshelf, door, and slabs

## Build prerequisites

- CMake 3.20 or newer
- A C++17 compiler (Visual Studio 2022+, recent GCC, or Clang)
- Git and internet access during the first configure
- An OpenGL 3.3-capable graphics driver

CMake downloads pinned GLFW 3.4, GLAD 0.1.36, and GLM 1.0.1 with `FetchContent`.

## Build and run

Windows (PowerShell):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\VoxelFrontier.exe
```

Linux:

```bash
sudo apt install build-essential cmake git xorg-dev libglu1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/VoxelFrontier
```

## Controls

| Input | Action |
|---|---|
| `W A S D` | Move |
| Mouse | Look |
| `Space` | Jump / swim upward; double-tap in Creative to toggle flight; fly upward in Creative or Spectator |
| `Left Ctrl` | Sprint; faster flight in Creative or Spectator |
| `Left Shift` | Sneak and stay on supported block/slab edges; swim downward; fly downward in Creative or Spectator |
| Hold left mouse | Attack a targeted mob or mine; Creative breaks blocks instantly on click |
| Right mouse | Open a targeted crafting table, furnace, or chest; place/use the selected item, eat, till soil, plant, or feed an animal |
| `Shift` + right mouse | Place a held block against a crafting table, furnace, or chest without opening it |
| Middle mouse / `R` | Pick the targeted block: select its hotbar slot in Survival, or obtain it in Creative |
| `1`-`9` / wheel | Select hotbar slot; in Spectator, wheel adjusts flight speed |
| `Q` | Throw one item from the selected slot; `Ctrl+Q` throws the whole stack |
| `E` | Open/close the 2x2 player inventory; close a crafting table, furnace, or chest |
| `Esc` | Close gameplay UI, or open/resume the true single-player pause menu |
| `G` | Toggle visual-only Fullbright |
| Hold `C` | Smooth camera zoom; use the wheel while held to adjust and remember zoom strength for the session |
| `F2` | Save a BMP screenshot in `screenshots/` |
| `F3` | Toggle detailed debug information |
| `F4` | Toggle additional world-generation values in the F3 overlay |
| Left click in inventory | Pick up, place, merge, or swap a stack |
| Right click in inventory | Split a stack or place one item |
| Right-drag in inventory | Distribute one item into each visited slot |
| `Shift` + click | Move a stack between storage and hotbar; Shift-click crafting output to make as many as fit |
| Double-click an item | Gather matching stacks into the cursor, up to the stack limit |
| Click outside inventory | Drop the held stack; right click drops one |

Right-click a placed crafting table within reach to open its shaped 3x3 grid, a furnace to smelt, or a chest for 27 persistent storage slots. `E` always opens the personal 2x2 grid. Inventory and crafting-table screens include a recipe book with categories, search, a craftable-only filter, and ingredient autofill; Shift-click a recipe to fill as many batches as materials permit. These interfaces release the mouse and leave day/night, mobs, drops, crops, water, furnaces, and player survival simulation running; only the Esc pause menu freezes the world. Gameplay keybindings can be changed in Pause Menu → Settings → Controls; conflicting bindings are swapped, and Reset to Defaults restores the original layout.

## Seeds and saves

The game reads the seed and world mode from `world_seed.txt`, defaulting to seed `20260917` in Survival. Override startup mode with `--creative`, `--survival`, or `--spectator`, and override the seed with `--seed 123456` or `--seed=123456`.

- `voxel_world.vxw`: player position, modified voxels, furnace state, and chest contents; the block-entity section is append-only so older saves still load
- `voxel_inventory.vxi`: all 36 item stacks, crafting slots, cursor stack, selected hotbar slot, quantities, and tool durability
- `voxel_player.vps`: health, hunger, and experience
- `voxel_mobs.vxm`: living mob species, position, health, and age

All files validate the configured seed. Autosaves occur every 12 seconds and on normal exit. Delete the four save files to start a fresh world for the current seed.

Existing world saves retain their original terrain generator so explored areas and newly streamed chunks meet without generator-version seams. Fresh worlds use the newer multi-noise generator. The world save records its generator version; the original version-1 world files still load.

Fresh version-5 worlds use sparse, finite canyon and branching cave carvers instead of the old continuous surface-ravine contour. Near-surface cave protection now fades gradually, while selected finite cave paths form occasional ground or hillside mouths. Most cavities remain underground. Older saved worlds retain their original generation version, including version-4 cave placement. Press `F4` while playing to show extra world-generation values within the F3 overlay. For an overhead, save-free development preview, run `VoxelFrontier.exe --worldgen-test --seed=123456`; it exits after taking a screenshot.

Use **Reset World** from the pause menu to enter a new seed, choose Survival, Creative, or Spectator, and confirm a fresh world. The pause menu also cycles through these three modes without resetting. Spectator can fly through terrain, cannot interact with blocks, mobs, or items, and leaves the inventory view read-only. Switching back finds a nearby safe position before restoring collision. Reset removes only the current world, inventory, player, and mob save files; settings, screenshots, assets, and unrelated files are preserved.

The complete sun/moon and gameplay day/night cycle is synchronized to 420 seconds, twice the previous duration.

## Source organization

- `main.cpp` is the small process entry point.
- `Game` owns initialization, the main loop, persistence coordination, per-frame simulation, rendering orchestration, and shutdown.
- `InputManager` centralizes GLFW keyboard/mouse state, edge detection, mouse deltas, scrolling, and cursor capture.
- `UIManager` owns the `GameState` state machine and inventory, crafting-table, pause, and settings interaction state.
- `Screenshot` contains framebuffer capture and BMP writing.
- `World`, `Renderer`, `Player`, `SurvivalWorld`, `FarmingSystem`, and `SoundSystem` retain their existing domain responsibilities.
- `StructureGenerator` owns regional placement, reusable rotated/mirrored pieces, terrain adaptation, markers, loot tables, and chunk-local application; `World` only invokes it during new-world generation.
- `CanyonCarver` owns sparse deterministic canyon and branching tunnel paths, applying only each chunk's intersecting portion without shared generation state.

Only `PauseMenu`, `Settings`, and the `ResetWorld` confirmation pause simulation. `Inventory` and `CraftingTable` remain live gameplay states. The checked-in `.clang-format` keeps the source consistently readable.

## Performance architecture

Terrain generation runs on background workers; OpenGL uploads stay on the main thread. Creative mode reuses the same chunk, lighting, interaction, and persistence architecture as Survival. Chunks emit only exposed faces, retain separate opaque/water meshes, are frustum culled, and are integrated/remeshed under frame budgets. Player edits rebuild only their owning and face-adjacent boundary chunks, sunlight updates one column, torch lighting is recalculated locally, and sunlight/block light share a packed byte to reduce high-distance memory use.


## Pixel-art assets and presentation

`assets/voxel_expansion_items_generated.png` is the generated source sheet for this expansion. `assets/item_icons_expansion.png` is the 10x10 preview atlas and `assets/item_icons_expansion.rgba` is the dependency-free runtime atlas. Earlier source/atlas files remain in place for provenance and compatibility. Every obtainable block, ore, material, crop, food item, torch, workstation, tool, and sword has a distinct sprite. The same sprites are used in the hotbar, inventories, workstation outputs, cursor stack, held-item overlay, and dropped-item billboards.

Gameplay presentation includes targeted-block and targeted-mob outlines, held-item swing, camera bob, sprint FOV easing, adjustable hold-to-zoom, underwater fog/tint, damage flash, and a readable Fullbright indicator. Zoom hides the HUD and held item until released. Normal distance fog is disabled so loaded terrain remains visible. F3 displays configurable FPS/coordinates plus chunk, biome, facing, world mode, flying state, seed, light values, distances, frame time, loaded/rendered chunks, generation/mesh queues, rebuild time, and edit time.

## Pause and settings

`Esc` pauses the single-player simulation and releases the cursor only when the pause menu is open. Gameplay interfaces do not pause. The pause menu provides Resume Game, Settings, a Survival/Creative/Spectator mode toggle, Reset World, Save World, Save & Quit, and Exit Game. Reset World opens a separate confirmation page with seed and mode selection. Settings apply immediately where practical and include independently saved render distance (2-64 chunks), simulation distance (2-32 chunks), FOV, mouse sensitivity, master volume, Off/2x/4x MSAA, fullscreen, VSync, FPS visibility, and coordinate visibility. Numeric settings use mouse-controlled sliders and display their current value.

Settings persist in `voxel_settings.cfg`. CMake copies the required runtime atlas beside the executable after every build, so launching from either the project folder or the Release folder works.
