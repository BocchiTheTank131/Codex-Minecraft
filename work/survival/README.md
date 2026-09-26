# Voxel Frontier: Survival World

A compact survival voxel sandbox written in C++17 with OpenGL 3.3, GLFW, GLAD, and GLM. The game uses real cube geometry, perspective projection, depth buffering, streamed chunk meshes, first-person physics, and 3D voxel raycasting.

## Survival systems

- 20-point player health with HUD display
- Distance-based fall damage, hostile-mob contact damage, death, and automatic respawning
- Hold-to-mine block breaking with visible progress
- Wooden, stone, and iron pickaxes, axes, and shovels
- Tool class and material tier affect block-breaking time
- Physical block drops with gravity, ground collision, pickup range, and despawning
- Nine-slot persistent hotbar with item counts
- Crafting recipes for planks, sticks, crafting tables, and nine basic tools
- Tool recipes require a placed crafting table within four blocks
- Consumable block and torch placement
- Seed-validated world and inventory saving

## Voxel lighting

- Every loaded chunk owns separate 0–15 sunlight and block-light volumes
- Sunlight is calculated vertically through air, water, and foliage
- Torches seed a breadth-first block-light propagation pass across loaded chunk boundaries
- Placing or removing blocks recalculates affected sunlight and schedules emitted-light propagation
- Chunk meshes interpolate sunlight, warm torch light, and per-corner ambient occlusion
- Enclosed caves are dark unless opened to the sky or illuminated with torches

## World generation

- Dynamically streamed chunks generated on four background workers
- Plains, forest, desert, and mountain biomes
- Layered Perlin terrain with sharper mountain ridges
- Procedural cross-chunk trees
- Rivers, beaches, oceans, ravines, winding caves, large caverns, and underground lakes
- Depth-dependent coal, copper, iron, gold, and diamond veins
- Transparent water rendered separately and sorted by chunk distance
- Persistent player edits restored when chunks reload

## Creatures

- Cows, pigs, and sheep spawn naturally in suitable loaded biomes
- Passive animals wander with inexpensive timer-based steering
- Animals use gravity, block avoidance, jumping, and AABB collision
- Wolves can pursue and damage the player at night
- Distant creatures despawn to keep simulation costs bounded
- Animals and dropped items use a shared low-poly cuboid renderer

## Atmosphere and feedback

- Moving sun, moon phase, twilight, procedural stars, and layered moving clouds
- Day/night directional lighting
- Block-colored breaking and placement particles
- Generated footstep, mining, placement, damage, and splash sounds on Windows
- Portable no-op audio fallback on other platforms
- F3 overlay with FPS, coordinates, current chunk, render distance, and health

## Build prerequisites

- CMake 3.20 or newer
- A C++17 compiler (Visual Studio 2022+, recent GCC, or Clang)
- Git and internet access during the first configure/build
- An OpenGL 3.3-capable graphics driver

CMake downloads pinned GLFW 3.4, GLAD 0.1.36, and GLM 1.0.1 sources with `FetchContent`.

## Windows build and run

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\VoxelFrontier.exe
```

## Linux build and run

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
| Mouse | Look around |
| `Space` | Jump |
| Hold left mouse | Mine the targeted block |
| Right mouse | Place the selected block or torch |
| `1`–`9` / wheel | Select a hotbar slot |
| `C` | Open or close crafting panel |
| `R` | Select next recipe |
| `V` | Craft selected recipe |
| `F3` | Toggle debug HUD |
| `Escape` | Release or capture the mouse |

Hotbar order: grass, stone, logs, planks, crafting table, torches, wooden pickaxe, stone pickaxe, and iron pickaxe. Empty items remain assigned but appear dim until obtained or crafted.

## Seeds and saves

The game reads `world_seed.txt`, falling back to seed `20260917`. Override it with:

```powershell
.\build\Release\VoxelFrontier.exe --seed 123456
# or
.\build\Release\VoxelFrontier.exe --seed=123456
```

- `voxel_world.vxw` stores player position and every modified voxel.
- `voxel_inventory.vxi` stores item counts and selected hotbar slot.
- Both files validate their embedded seed before loading.
- Automatic saves occur every 12 seconds and on normal exit.

Delete both save files to start fresh with the configured seed.

## Performance architecture

- Terrain generation is CPU-only and thread safe.
- OpenGL allocation and upload stays on the main thread.
- Chunk integration and remeshing are frame-budgeted.
- Only exposed voxel faces are emitted.
- Opaque and transparent geometry use separate buffers.
- Frustum culling skips off-screen chunk meshes.
- Light propagation runs only when edits change the light field.
- Creature count and simulation distance are capped.

## Project layout

```text
VoxelFrontier/
├── CMakeLists.txt
├── include/
│   ├── Block.h
│   ├── Noise.h
│   ├── Player.h
│   ├── Renderer.h
│   ├── Sound.h
│   ├── Survival.h
│   └── World.h
└── src/
    ├── main.cpp
    ├── Noise.cpp
    ├── Player.cpp
    ├── Renderer.cpp
    ├── Sound.cpp
    ├── Survival.cpp
    └── World.cpp
```

