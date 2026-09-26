# Voxel Frontier: Living World

A compact Minecraft-style voxel sandbox written in C++17 with OpenGL 3.3, GLFW, GLAD, and GLM. The game uses real cube triangles, perspective projection, depth buffering, chunk meshes, first-person physics, and grid-accurate 3D interaction.

## World features

- Dynamic circular chunk streaming around the player
- Four background terrain-generation workers; OpenGL uploads stay on the main thread
- Frame-budgeted chunk integration and remeshing to avoid movement stalls
- Automatic unloading beyond the render distance, with player edits restored if a chunk reloads
- Plains, forest, desert, and mountain biomes selected by large-scale noise fields
- Multi-octave Perlin elevation, detailed hills, tall mountain ridges, and 3D cave carving
- Deterministic forest trees that cross chunk borders correctly
- Oceans at sea level with lowered, alpha-blended water surfaces
- Grass, dirt, stone, sand, logs, leaves, and water in a generated pixel-art atlas
- Separate opaque and transparent meshes, hidden-face removal, distance sorting for water, and frustum culling

## Atmosphere and presentation

- 210-second day/night cycle
- Moving sun and moon, procedural stars, sunrise/sunset transitions, and changing sky colors
- Sunlight and moonlight direction affect block faces
- Surface-aware ambient lighting makes enclosed caves significantly darker
- Block-colored breaking particles
- Nine-slot Minecraft-style hotbar with seven usable block types
- Optional pixel-font F3 overlay with FPS, XYZ, chunk, biome, loaded/pending chunks, and render distance

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

Or with Ninja from a configured developer shell:

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
| Left mouse | Break the targeted block |
| Right mouse | Place the selected block |
| `1`–`7` / mouse wheel | Select grass, dirt, stone, sand, log, leaves, or water |
| `F3` | Toggle performance/world debug overlay |
| `Escape` | Release or recapture the mouse |

## Tuning

- Default render distance: `world.generate(7)` in `src/main.cpp`
- Accepted render-distance range: 2–16 chunks
- Chunk integration budget: 4 generated chunks per frame
- Mesh upload budget: 2 chunks per frame (`world.updateStreaming(..., 2)`)
- Day/night length: `cycleSeconds` in `src/Renderer.cpp`
- World seed: the value passed to `World` in `src/main.cpp`

Terrain generation is CPU-only and thread safe. Worker threads never call OpenGL; completed chunks enter a synchronized queue and are uploaded incrementally by the render thread.

## Project layout

```text
VoxelFrontier/
├── CMakeLists.txt
├── include/
│   ├── Block.h
│   ├── Noise.h
│   ├── Player.h
│   ├── Renderer.h
│   └── World.h
└── src/
    ├── main.cpp
    ├── Noise.cpp
    ├── Player.cpp
    ├── Renderer.cpp
    └── World.cpp
```

