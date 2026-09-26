# Voxel Frontier: Living World

A playable Minecraft-style voxel sandbox written in C++17 with OpenGL 3.3, GLFW, GLAD, and GLM. Blocks use real cube triangles, perspective projection, depth buffering, chunk meshes, first-person physics, and grid-accurate 3D interaction.

## Features

- Dynamic circular chunk streaming around the player
- Four background terrain workers with frame-budgeted integration and GPU remeshing
- Automatic distant-chunk unloading and frustum culling
- Plains, forest, desert, and mountain biomes
- Multi-octave Perlin terrain, cross-chunk trees, oceans, and transparent water
- Improved caverns plus winding 3D tunnel fields
- Depth-dependent coal, iron, and gold ore veins generated inside stone
- Grass, dirt, stone, sand, logs, leaves, water, and ore textures in a generated atlas
- Perspective first-person camera, mouse look, jumping, gravity, and substepped AABB collision
- DDA voxel raycasting, instant block breaking, break particles, and selected-block placement
- Nine-slot hotbar with number-key and mouse-wheel selection
- Clean top-left F3 HUD with FPS, player XYZ, chunk XZ, and render distance
- Moving procedural clouds, sun, moon phase, twilight, twinkling stars, and a day/night cycle
- Surface/cave ambient lighting and interpolated per-vertex voxel ambient occlusion
- Versioned world saves containing player position and every modified block
- Configurable deterministic world seed
- Generated footstep, break, and placement sounds on Windows; portable no-op audio fallback elsewhere

## Build prerequisites

- CMake 3.20 or newer
- A C++17 compiler (Visual Studio 2022+, recent GCC, or Clang)
- Git and internet access during the first configure/build
- An OpenGL 3.3-capable graphics driver

CMake downloads pinned GLFW 3.4, GLAD 0.1.36, and GLM 1.0.1 sources using `FetchContent`.

## Windows build and run

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\VoxelFrontier.exe
```

With Ninja from a configured developer shell:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\VoxelFrontier.exe
```

## Linux build and run

On Ubuntu/Debian, install GLFW's platform dependencies first:

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
| Left mouse | Instantly break the targeted block |
| Right mouse | Place the currently selected block |
| `1`–`9` | Select a hotbar slot |
| Mouse wheel | Cycle hotbar selection |
| `F3` | Toggle the debug HUD |
| `Escape` | Release or recapture the mouse |

Hotbar order: grass, dirt, stone, sand, log, leaves, water, coal ore, iron ore. Gold ore generates naturally but is not assigned to the creative hotbar.

## Seeds and saves

The game reads its seed from `world_seed.txt`. If the file is absent, seed `20260917` is used and the file is created. You can also override it:

```powershell
.\build\Release\VoxelFrontier.exe --seed 123456
# or
.\build\Release\VoxelFrontier.exe --seed=123456
```

The selected seed is written back to `world_seed.txt`. Saves are stored in `voxel_world.vxw` in the working directory. The game saves every 12 seconds and on a normal exit. A save is loaded only when its embedded seed matches the configured seed, preventing edits from being applied to unrelated terrain.

Delete `voxel_world.vxw` to start a fresh world with the current seed.

## Performance tuning

- Default render distance: `world.generate(7, spawnPosition)` in `src/main.cpp`
- Supported render-distance range: 2–16 chunks
- Generated chunk integration: up to 4 chunks per frame
- GPU mesh upload budget: 2 chunks per frame
- Day/night length: `cycleSeconds` in `src/Renderer.cpp`

Worker threads only generate CPU block data. OpenGL objects are created, replaced, and destroyed exclusively on the main thread.

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
│   └── World.h
└── src/
    ├── main.cpp
    ├── Noise.cpp
    ├── Player.cpp
    ├── Renderer.cpp
    ├── Sound.cpp
    └── World.cpp
```

