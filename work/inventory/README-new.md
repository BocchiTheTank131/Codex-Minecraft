# Voxel Frontier: Survival World

Voxel Frontier is a compact C++17/OpenGL 3.3 survival voxel sandbox. It uses real cube meshes, perspective projection, depth buffering, streamed chunks, first-person physics, 3D voxel raycasting, procedural textures, and persistent world edits.

## Current gameplay

- Streamed plains, forests, deserts, mountains, rivers, beaches, oceans, ravines, caves, underground lakes, trees, and depth-dependent ore veins
- Sunlight plus propagated torch light, ambient occlusion, dark caves, water transparency, moving sun/moon/stars/clouds, and a visual-only fullbright mode
- Health, hunger, regeneration, starvation, fall damage, death/respawning, sprinting, sneaking, and swimming
- Passive animals, night wolves, drops, animal damage, feeding/breeding, and experience orbs
- Wheat seeds, farmland, four crop stages, harvesting, wheat, bread, and food
- Wooden, stone, and iron pickaxes, axes, and shovels with mining speeds, durability bars, and breakage
- A persistent 36-slot inventory: 27 storage slots plus the nine-slot hotbar
- Stack pickup, merging, swapping, right-click splitting, right-drag single-item distribution, shift-click transfers, and dropping stacks into the world
- A 2x2 player crafting grid and a 3x3 grid when near a placed crafting table
- Recipes for planks, sticks, torches, crafting tables, bread, and all basic tools

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
| `Space` | Jump / swim upward |
| `Left Ctrl` | Sprint |
| `Left Shift` | Sneak / swim downward |
| Hold left mouse | Mine a block; click an animal to attack |
| Right mouse | Place/use selected item, eat, till soil, plant, or feed an animal |
| `1`-`9` / wheel | Select hotbar slot |
| `E` | Open/close inventory and crafting |
| Left click in inventory | Pick up, place, merge, or swap a stack |
| Right click in inventory | Split a stack or place one item |
| Right-drag in inventory | Distribute one item into each visited slot |
| `Shift` + click | Move a stack between storage and hotbar |
| Click outside inventory | Drop the held stack; right click drops one |
| `G` | Toggle visual-only fullbright |
| `F3` | Toggle debug HUD |
| `Escape` | Close inventory or release/capture mouse |

The 3x3 crafting grid is available while the player is within four blocks of a crafting table; otherwise the inventory shows the 2x2 player grid.

## Seeds and saves

The game reads `world_seed.txt`, defaulting to `20260917`. Override it with `--seed 123456` or `--seed=123456`.

- `voxel_world.vxw`: player position and modified voxels, including farmland and crops
- `voxel_inventory.vxi`: all 36 item stacks, crafting slots, cursor stack, selected hotbar slot, quantities, and tool durability
- `voxel_player.vps`: health, hunger, and experience

All files validate the configured seed. Autosaves occur every 12 seconds and on normal exit. Delete the three save files to start a fresh world for the current seed.

## Performance architecture

Terrain generation runs on background workers; OpenGL uploads stay on the main thread. Chunks emit only exposed faces, retain separate opaque/water meshes, are frustum culled, and are integrated/remeshed under frame budgets. Lighting propagation runs only when needed, and creature counts are bounded.

