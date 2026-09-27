# Voxel Frontier

> A Minecraft-inspired voxel sandbox built entirely with **ChatGPT Codex**.

Everything in this project was created entirely with Codex, including the game code, README, GitHub updates, and development prompts.

## About

**Voxel Frontier** is a C++17 / OpenGL 3.3 voxel sandbox featuring Survival, Creative, and Spectator modes.

It includes procedural terrain, streamed chunks, caves, structures, mobs, crafting, farming, lighting, persistent worlds, and first-person voxel interaction.

## Features

- 256-block-tall procedurally generated worlds
- Multi-noise terrain with continents, mountains, rivers, beaches, oceans, caves, ravines, and aquifers
- Plains, deserts, snowy mountains, forests, and other terrain variations
- Villages, pillager outposts, desert pyramids, ruins, wells, and campsites
- Survival, Creative, and Spectator modes
- Dynamic sun, moon, stars, clouds, and day/night cycle
- Sunlight and propagated torch lighting
- Ambient occlusion and transparent water
- Health, hunger, regeneration, starvation, fall damage, and respawning
- Sprinting, sneaking, swimming, Creative flight, and Spectator noclip
- Cows, pigs, sheep, wolves, villagers, and pillagers
- Mob combat, knockback, drops, breeding, and experience
- Farming with farmland, seeds, crops, wheat, bread, and food
- Wooden through diamond tools and swords
- Tool durability, mining speeds, combat cooldowns, and critical hits
- 36-slot inventory with a 9-slot hotbar
- Stack splitting, merging, drag distribution, shift-clicking, and item dropping
- 2x2 player crafting and 3x3 crafting tables
- Recipe book with search, filters, autofill, and batch crafting
- Furnaces with fuel, progress, recipes, and persistent contents
- 27-slot persistent chests
- Persistent world edits, mobs, inventory, player stats, furnaces, and chests

## Crafting

Crafting uses the same recipes for manual grid matching and the Recipe Book. The player grid
accepts recipes up to 2×2; the Crafting Table accepts up to 3×3. Plank recipes accept oak and
birch planks, including mixed planks where the output has no wood variant. Breaking Stone
provides Cobblestone for stone tools and the eight Cobblestone Furnace recipe; smelting
Cobblestone produces Stone for Stone Bricks and Stone Slabs.
Andesite crafts from Diorite and Cobblestone.

Bookshelves have no survival crafting recipe yet because Books are not implemented. Bricks
likewise need brick items, which are not implemented. Glass and cooked food use the Furnace,
not the crafting grid. Mossy blocks and Snow Blocks retain Voxel Frontier's existing adapted
recipes because vines and snowballs are not implemented. The recipe set follows the
[Minecraft Wiki crafting reference](https://minecraft.wiki/w/Crafting) where the game's
ingredients exist.

## Windows Release

The Windows x64 release provides a standalone executable and an installer. The standalone
download is one `VoxelFrontier.exe`; the installer adds Start Menu and optional Desktop
shortcuts and registers Voxel Frontier in Windows Installed Apps.

The executable embeds the item atlas and all OGG sound effects. Shaders are compiled into the
program. It needs no external asset folder, sound folder, or Visual C++ Redistributable.

The game stores worlds, settings, seed metadata, and screenshots in
`%LOCALAPPDATA%\VoxelFrontier\`. This location survives installer upgrades and uninstall.
Release builds write startup and runtime output to `logs\VoxelFrontier.log` there.
If an older standalone copy has saves beside it, launching the new executable from that folder
copies those files into the new location when no profile exists there. The originals remain in
place. The `VOXEL_FRONTIER_DATA_DIR` environment variable can select a different data folder.

The installer uses `%LOCALAPPDATA%\Programs\Voxel Frontier\` by default and needs no
administrator privileges. Running a newer installer upgrades the existing installation. Running
the same version offers Repair/Reinstall, Uninstall, or Cancel; an older installer blocks a
downgrade. Uninstall removes program files and shortcuts while keeping personal data.

### Requirements

- Windows 10 or newer, x64
- OpenGL 3.3-capable GPU and graphics driver

### Build both release artifacts

Install Inno Setup 7 once, then run the packaging script from PowerShell:

```powershell
winget install --id JRSoftware.InnoSetup.7 -e --scope user --silent --accept-package-agreements --accept-source-agreements
.\package_release.ps1
```

If using Visual Studio 2022, pass `-Generator 'Visual Studio 17 2022'`. The script builds in
Release mode and writes `dist/VoxelFrontier.exe`, the versioned Windows Setup executable, and
`dist/SHA256SUMS.txt`. The installer contains only the game executable.

## Building from Source

### Requirements

- CMake 3.20+
- C++17 compiler
- Git
- OpenGL 3.3-capable graphics driver

CMake automatically downloads:

- GLFW 3.4
- GLAD 0.1.36
- GLM 1.0.1

### Windows

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\VoxelFrontier.exe
```

### Standalone Windows Build

```powershell
cmake -S . -B build/standalone -G "Visual Studio 17 2022" -A x64 -DVOXEL_STANDALONE=ON
cmake --build build/standalone --config Release
```

The ordinary CMake build above still copies `assets/` and `sounds/` beside its executable and
uses its working directory for saves. The standalone build embeds those runtime assets and uses
the Windows user-data directory.

Output:

```text
build/standalone/Release/VoxelFrontier.exe
```

The standalone build statically links the MSVC runtime and embeds runtime assets into the executable.

### Linux

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
| `Space` | Jump / swim upward / fly upward |
| `Left Ctrl` | Sprint / faster flight |
| `Left Shift` | Sneak / swim downward / fly downward |
| Left Mouse | Attack or mine |
| Right Mouse | Place/use item or interact |
| `Shift + Right Mouse` | Place blocks against interactive blocks |
| Middle Mouse / `R` | Pick targeted block |
| `1-9` / Mouse Wheel | Select hotbar slot |
| `Q` | Drop one item |
| `Ctrl + Q` | Drop entire stack |
| `E` | Open inventory |
| `Esc` | Pause / close interface |
| `G` | Toggle Fullbright |
| Hold `C` | Smooth zoom |
| `F2` | Take screenshot |
| `F3` | Toggle debug overlay |
| `F4` | Show extra world-generation debug data |

Double-tap `Space` in Creative mode to toggle flight.

While holding `C`, use the mouse wheel to adjust zoom strength.

Keybindings can be changed from:

**Pause Menu → Settings → Controls**

## World Seeds

World configuration is stored in:

```text
world_seed.txt
```

The default seed is:

```text
20260917
```

Command-line options are also supported:

```bash
VoxelFrontier.exe --seed=123456
VoxelFrontier.exe --creative
VoxelFrontier.exe --survival
VoxelFrontier.exe --spectator
```

World-generation testing:

```bash
VoxelFrontier.exe --worldgen-test --seed=123456
```

This generates an overhead development preview and exits automatically.

## Saves

Voxel Frontier uses separate persistent save files:

```text
voxel_world.vxw
voxel_inventory.vxi
voxel_player.vps
voxel_mobs.vxm
```

They store world edits, player state, inventory, mobs, furnaces, chests, and other persistent data.

Autosaves occur every **12 seconds** and when the game exits normally.

Delete the save files to generate a fresh world using the configured seed.

Older worlds retain their original terrain-generator version so newly generated chunks remain compatible with previously explored terrain.

## World Reset

The pause menu includes **Reset World**, allowing you to:

- Enter a new seed
- Choose Survival, Creative, or Spectator
- Generate a completely fresh world

Resetting removes gameplay save data while preserving settings, screenshots, and unrelated files.

## Rendering & Performance

Voxel Frontier uses a chunk-based rendering architecture designed for high render distances.

- Terrain generation runs on background worker threads
- OpenGL uploads remain on the main thread
- Only exposed voxel faces are generated
- Opaque and transparent geometry use separate meshes
- Frustum culling skips invisible chunks
- Chunk generation and remeshing operate under frame budgets
- Block edits only rebuild affected and neighboring chunks
- Lighting updates are localized
- Sunlight and block light share compact packed storage

Render distance can be configured from **2 to 64 chunks**, while simulation distance can be configured independently from **2 to 32 chunks**.

## Visuals

The game includes:

- Procedural pixel-art item sprites
- Block and mob targeting outlines
- Held-item animation
- Camera bob
- Sprint FOV effects
- Smooth adjustable zoom
- Underwater fog and tint
- Damage flash
- Ambient occlusion
- MSAA
- Fullbright mode

Normal distance fog is disabled so loaded terrain remains visible at long render distances.

The F3 debug overlay can display FPS, coordinates, chunk information, biome, facing direction, world mode, seed, lighting, frame time, render distances, generation queues, and mesh statistics.

## Settings

Settings are saved to:

```text
voxel_settings.cfg
```

Available options include:

- Render distance
- Simulation distance
- Field of view
- Mouse sensitivity
- Master volume
- Off / 2x / 4x MSAA
- Fullscreen
- VSync
- FPS display
- Coordinate display
- Custom keybindings

Most settings apply immediately.

## Source Structure

The project is split into focused systems:

- `main.cpp` - application entry point
- `Game` - initialization, main loop, simulation, rendering, saves, and shutdown
- `World` - chunks, terrain, blocks, lighting, and world data
- `Renderer` - OpenGL rendering and mesh handling
- `Player` - movement and first-person physics
- `SurvivalWorld` - survival mechanics and entities
- `FarmingSystem` - crops and farming
- `SoundSystem` - audio
- `InputManager` - keyboard and mouse input
- `UIManager` - inventory, crafting, pause menu, and settings
- `StructureGenerator` - villages, outposts, pyramids, ruins, and loot
- `CanyonCarver` - deterministic canyon and cave generation
- `Screenshot` - framebuffer capture and BMP output

## Tech

- **Language:** C++17
- **Graphics:** OpenGL 3.3
- **Window/Input:** GLFW
- **OpenGL Loader:** GLAD
- **Math:** GLM
- **Build System:** CMake

---

**Voxel Frontier** is an experimental voxel sandbox focused on building Minecraft-style systems from scratch in modern C++ and OpenGL, with development driven heavily by ChatGPT Codex.
