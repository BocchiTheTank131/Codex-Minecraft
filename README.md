# Voxel Frontier

> A Minecraft-inspired voxel sandbox built entirely with **ChatGPT Codex**.

Everything in this project was created entirely with Codex, including the game code, README, GitHub updates, and development prompts.

## Contents

- [Features](#features)
- [Windows downloads](#windows-release)
- [Controls](#controls)
- [Commands](#commands)
- [Mobs](#mobs)
- [Crafting](#crafting)
- [World seeds](#world-seeds) and [saves](#saves)
- [Settings and audio](#settings)
- [Rendering and performance](#rendering--performance)
- [Building from source](#building-from-source)
- [Source structure](#source-structure)

## About

**Voxel Frontier** is a C++17 / OpenGL 3.3 voxel sandbox featuring Survival, Creative, and Spectator modes.

It includes procedural terrain, streamed chunks, caves, structures, mobs, crafting, farming, lighting, persistent worlds, and first-person voxel interaction.

Launch to the moving panorama main menu: **PLAY** opens **Select World** in the installed
game, **SETTINGS** opens Video, Audio, Controls, and Weather categories, and **QUIT** closes the game.
Select a saved world and press PLAY (or double-click it), or choose **CREATE NEW WORLD**.
New worlds have a name, an optional numeric seed, and Survival or Creative mode.
The standalone demo skips world selection and starts a fresh temporary world.
The world starts only after PLAY. From the pause menu, **SAVE & RETURN TO MENU** saves the
installed game; the standalone demo's **RETURN TO MENU** discards its temporary world.
Pressing PLAY again in the demo starts a fresh gameplay session.

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
- Cows, pigs, sheep, villagers, pillagers, and four distinct billboard hostiles
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
- In-game commands with history, autocomplete, relative coordinates, and bounded world edits
- Streaming shuffle-bag background music and independent audio categories
- Persistent world edits, mobs, inventory, player stats, furnaces, and chests

## Crafting

Crafting uses the same recipes for manual grid matching and the Recipe Book. The player grid
accepts recipes up to 2×2; the Crafting Table accepts up to 3×3. Plank recipes accept oak and
birch planks, including mixed planks where the output has no wood variant. Breaking Stone
provides Cobblestone for stone tools and the eight Cobblestone Furnace recipe; smelting
Cobblestone produces Stone for Stone Bricks and Stone Slabs.
Andesite crafts from Diorite and Cobblestone.

Version **2.10.1** has **102 inventory items**, **56 placeable block families**, **120 block
IDs/states including Air**, **62 crafting recipes**, and **13 furnace recipes**. Existing item
and block IDs remain unchanged; all new IDs are appended.

### Crafting expansion

New materials: **Paper, Book, Clay Ball, Brick, Snowball, Charcoal, Sugar Cane, Vine**.
New block families: **Coal Block, Iron Block, Gold Block, Copper Block, Diamond Block, Hay Bale,
Sandstone, Polished Granite, Polished Diorite, Polished Andesite, Sugar Cane, Vine**.

| Ingredients / layout | Output | Grid |
|---|---|---|
| One Log / Birch Log | 4 Planks / Birch Planks | 2×2 |
| Two generic Planks vertically | 4 Sticks | 2×2 |
| Four generic Planks in a square | Crafting Table | 2×2 |
| Eight Cobblestone around an empty center | Furnace | 3×3 |
| Eight generic Planks around an empty center | Chest | 3×3 |
| Seven Sticks in an H pattern | 3 Ladders | 3×3 |
| Six matching Planks in two columns | 3 Wooden Doors | 3×3 |
| Three generic Planks / Stone horizontally | 6 Wooden Slabs / Stone Slabs | 3×3 |
| Coal or Charcoal above a Stick | 4 Torches | 2×2 |
| Three Sugar Cane horizontally | 3 Paper | 3×3 |
| Three Paper + Leather, shapeless | Book | 2×2 |
| Three Books between two rows of generic Planks | Bookshelf | 3×3 |
| Four Clay Balls in a square | Clay block | 2×2 |
| Four Bricks in a square | Bricks block | 2×2 |
| Four Snowballs in a square | Snow Block | 2×2 |
| Three Snow Blocks horizontally | 6 Snow layers | 3×3 |
| Nine Coal / Iron Ingots / Gold Ingots / Copper Ingots / Diamonds / Wheat | Corresponding storage block / Hay Bale | 3×3 |
| One storage block / Hay Bale | 9 of its original material | 2×2 |
| Four Sand in a square | Sandstone | 2×2 |
| Four Granite / Diorite / Andesite in a square | 4 matching polished blocks | 2×2 |
| Four Stone in a square | 4 Stone Bricks | 2×2 |
| Cobblestone + Vine, shapeless | Mossy Cobblestone | 2×2 |
| Stone Bricks + Vine, shapeless | Mossy Stone Bricks | 2×2 |
| Three Wheat horizontally | Bread | 3×3 |
| Three Copper Ingots vertically | Lightning Rod | 3×3 |

Generic plank ingredients accept default/oak and Birch, including mixed planks. Wooden Doors
require six matching planks; both wood families produce the existing Wooden Door item.
Manual recipes and Recipe Book previews/search/categories/autofill/batch crafting share one
registry. Tool recipes and mirrored axes remain supported.

### Furnace recipes and fuel

| Input | Output |
|---|---|
| Cobblestone | Stone |
| Sand | Glass |
| Clay Ball | Brick |
| Coal Ore | Coal |
| Iron Ore | Iron Ingot |
| Gold Ore | Gold Ingot |
| Copper Ore | Copper Ingot |
| Log / Birch Log | Charcoal |
| Raw Beef / Raw Meat | Cooked Beef |
| Raw Pork | Cooked Pork |
| Raw Mutton | Cooked Mutton |

Each smelt takes the existing **5 seconds**. Coal and Charcoal each burn for **40 seconds**
(8 smelts); a Coal Block burns for **400 seconds** (80 smelts). Charcoal is a distinct item
and also crafts Torches. Cooking and smelting remain separate from crafting.

Breaking Clay in Survival drops **4 Clay Balls**; Snow drops one Snowball per accumulated
layer. Crafted Snow is 1/8 block high; weather can accumulate up to eight layers. Proper Book, Brick, Snowball, and Vine chains replace the previous
missing/adapted recipes. All new items are available in Creative and through `/give`.

### Plants and original textures

New generator-version-8 worlds contain deterministic Sugar Cane patches on Grass/Dirt/Sand
shorelines with cardinally adjacent exposed water at the ground level. Each patch anchor selects
up to five base positions and plants one to three blocks high, subject to actual air clearance,
solid footing and structure exclusion. Vines still appear on some forest tree trunks. Cane can be placed on suitable
ground adjacent to water or on another Cane block; it has no automatic growth in this release.
Vines attach to four wall directions. Both plants drop their own item and have no player
collision or voxel AO contribution. Existing worlds keep their saved generator version and
terrain; start a new world to obtain the new natural plant generation.

The expansion includes **20 original 16×16 textures**, including separate Hay Bale and
Sandstone surfaces and transparent material/plant sprites. Legacy atlas slots are preserved.
To regenerate these assets, install Pillow for development and run
`python tools/generate_crafting_textures.py`. Pillow is not required to build or run the game;
the generated runtime atlases are committed and embedded in release executables.

### v2.7.1 texture refresh

All **101 inventory icons** have been regenerated as original **16×16 pixel art**:
56 block/plant icons and 45 other item sprites. Full blocks share one isometric projection
and lighting direction, sampling the actual placed-block materials. Tools and swords share
consistent silhouettes across their material variants. Clean binary transparency, a native-pixel
transparent border, nearest-neighbor scaling, and an inward UV inset prevent fringe and
neighboring-cell artifacts. Glass remains readable on light and dark backgrounds.

The native atlas is `assets/inventory_atlas_16.png` (160×176); the runtime atlas remains
640×704 with 64×64 cells. Every existing sprite registration is preserved. Individual native
icons are in `assets/inventory/`. Regenerate the full set with
`python tools/generate_inventory_textures.py` (development-only Pillow and a C++17 compiler;
`--compiler` defaults to `g++`). Run it after regenerating world materials. This visual update
does not change recipes, world textures, IDs, save formats, or full/demo persistence behavior;
v2.7.0 saves remain compatible.

The recipe set follows the
[Minecraft Wiki crafting reference](https://minecraft.wiki/w/Crafting) where the game's
ingredients exist.

### v2.7.2 world generation and gameplay fixes

- Hovering occupied inventory, Creative, crafting/Recipe Book, chest and furnace slots
  shows only the item's display name in a small pixel-font tooltip. Inventory hotbar slots
  are included; tooltips stay within the window and empty slots show nothing.
- Death respawns near the original world origin rather than the last loaded player position.
  A bounded 48-block outward search finds a solid, clear surface with safe neighboring footing,
  accounting for world edits. Velocity, fall/swim/flight/sprint and death state are reset;
  a two-second respawn protection period prevents immediate combat deaths. Local collision
  terrain is made available before physics resumes, with normal asynchronous mesh/streaming.
- **Terrain generator v8** gives new worlds stronger regional mountain ranges, ridgelines,
  basins and valleys. Erosion controls relief; lowlands, coastlines and existing river masks
  remain useful. Column noise is reused and async generation is retained.
- Existing v2.7.1 and older saves retain their stored generator version, including unexplored
  chunks. No item/block IDs, recipes, world-save layout or full/demo persistence rules changed.

Developer validation: `--patch272-smoke` checks eight seeds (including `123456789`),
compares v7/v8 CPU chunk-generation time, validates shoreline Cane and old-world generation,
and exercises respawn and tooltip bounds. It exports `terrain272.csv` only in that explicit
validation mode. `--worldgen-test --seed=<seed> --preview-x=<x> --preview-z=<z>` captures a
terrain preview; `--crafting-preview` exercises the actual inventory/container tooltip UI.

## Windows Release

### v2.10.1 - Billboard Movement Fix

- Swept full-hitbox collision prevents billboard enemies tunneling through terrain.
- Reliable descents and landings across ledges, slabs, corners, and chunk boundaries.
- Unloaded terrain remains blocked; Ryo retains wall climbing without walking into unsafe drops.
- Includes 52 terrain-physics regression cases, with existing combat and save compatibility preserved.

### v2.10.0 - The Weather Update

Weather follows the documented [Weather](https://minecraft.wiki/w/Weather) mechanics,
adapted to Voxel Frontier. Clear lasts 4-12 minutes, Rain 2-5, and Thunderstorms 1-3.

- Smooth sky/cloud/daylight transitions; dry biomes stay dry and cold regions receive snow.
- Local batched precipitation, roof clipping, ground splashes, wind and optional weather fog.
- Snow accumulates up to eight layers and melts in warm biomes or near emitted light.
- Exposed farmland hydrates in rain; exposed lightning fire is extinguished.
- Lightning damages nearby entities, can ignite wood, and attracts to exposed rods within 32 blocks.
- Craft a Lightning Rod from three Copper Ingots vertically in a Crafting Table.
- Dark storms allow daytime hostile spawning; villagers seek nearby shelter.
- All 19 supplied weather/impact clips are embedded. Outdoor/roof rain crossfades;
  positional thunder is delayed by strike distance, with close positional impact sounds.
- Weather commands and a dedicated Weather settings page.
- Each installed world saves weather state, timers, intensity and random sequence.
  Legacy metadata defaults to Clear; the standalone demo remains stateless.

### v2.9.1 performance update

- Reduced repeated rendering work and reused temporary mesh/UI buffers.
- Accelerated block-light rebuilds without changing light values.
- Batched opaque terrain draws through shared buffer pages for better high-distance performance.
- Preserved terrain generation, transparency, gameplay, and save compatibility.

### v2.9.0 menus and multiple worlds

- Polished, responsive menu buttons with smooth hover feedback.
- The embedded panorama pans slowly and wraps continuously behind menu pages.
- Installed PLAY opens a paginated world list showing names, modes, seeds, and last-played times.
- Create separate Survival or Creative worlds with an optional seed; blank seeds are randomized.
- Each world keeps its own terrain, player, inventory, mobs, and day/night time.
- Legacy single-world saves are safely imported as **Imported World**, preserving the original files.
- Settings are organized into Video, Audio, Controls, and Weather; settings remain global.
- Save & Return to Menu saves and unloads the full game. The standalone demo discards its
  temporary world and starts fresh when PLAY is pressed again, without world-selection screens.

Download from [GitHub Releases](https://github.com/BocchiTheTank131/Codex-Minecraft/releases).

| v2.10.1 Windows build | Purpose | Persistence |
|---|---|---|
| `VoxelFrontier-v2.10.1-Windows-Standalone.exe` | Run the single-file demo directly | Fresh session every launch |
| `VoxelFrontier-v2.10.1-Windows-Setup.exe` | Install the full game | Saves worlds and settings |
| `SHA256SUMS.txt` | Verify download integrity | Not a game executable |

The Windows x64 release provides a stateless standalone demo and a full installer. The
standalone download is one `VoxelFrontier-vX.Y.Z-Windows-Standalone.exe`: it starts a fresh
session each time, ignores existing saves, and does not save worlds, player state, inventory,
mobs, seed, or settings. Its Save World button explains that saving is unavailable. The
installer adds Start Menu and optional Desktop shortcuts and registers the full game in
Windows Installed Apps.

Packaged executables embed the item atlas, four hostile PNG sprites, OGG sound effects, and
background music. Shaders are compiled into the program. They need no external asset folder,
sound folder, or Visual C++ Redistributable. Windows Release builds use the GUI subsystem and
do not open a console; Debug builds may retain one for diagnostics.

The installed game stores worlds, settings, seed metadata, and screenshots in
`%LOCALAPPDATA%\VoxelFrontier\`. This location survives installer upgrades and uninstall.
Release builds write startup and runtime output to `logs\VoxelFrontier.log` there. The demo
may also write logs and explicit F2 screenshots there, but no game state or settings. The full
game can migrate saves from an older copy beside its executable when no profile exists in the
new location. The originals remain in place. The `VOXEL_FRONTIER_DATA_DIR` environment
variable can select a different data folder.

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
Release mode for both full and demo variants and writes the versioned standalone demo and
Windows Setup executables plus `dist/SHA256SUMS.txt`. The installer contains the full game
executable.

### Content verification

Run developer checks with a disposable user-data directory, outside the repository:

```powershell
$env:VOXEL_FRONTIER_DATA_DIR = Join-Path $env:TEMP 'VoxelFrontier-content-check'
.\build\installed-release\Release\VoxelFrontier.exe --survival-smoke --seed=20260917
.\build\installed-release\Release\VoxelFrontier.exe --command-smoke --seed=20260917
.\build\installed-release\Release\VoxelFrontier.exe --crafting-preview --seed=20260917
Remove-Item Env:\VOXEL_FRONTIER_DATA_DIR
```

Content checks cover all 61 recipes, mixed planks, grid size restrictions, Recipe Book
search/autofill, batch crafting, stack limits, all 13 furnace inputs, fuel capacity, generated
plant determinism, new block meshes/UVs, command lookup, and item/block save/reload. The
opt-in preview exports four screenshots of world models, Creative inventory, player crafting,
and a mixed-plank Bookshelf recipe. Tests never run during normal gameplay. Screenshots and
logs are exports; the standalone demo still bypasses save/config persistence.

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
cmake -S . -B build/standalone -G "Visual Studio 17 2022" -A x64 -DVOXEL_STANDALONE=ON -DVOXELFRONTIER_STANDALONE_DEMO=ON
cmake --build build/standalone --config Release
```

The ordinary CMake build above still copies `assets/`, `sounds/`, and `sprites/` beside its executable and
uses its working directory for saves. The standalone build embeds those runtime assets and uses
the Windows user-data directory.

Output:

```text
build/standalone/Release/VoxelFrontier.exe
```

The demo build statically links the MSVC runtime and embeds runtime assets into the executable.
Omit `-DVOXELFRONTIER_STANDALONE_DEMO=ON` for a full persistent build. The release packaging
script builds both configurations separately.

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
| `/` | Open command input with `/` already entered |
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

## Commands

Press `/` during gameplay to open the command overlay. **Enter** executes and closes input;
**Escape** cancels. The world keeps simulating while you type, but movement, hotbar selection,
and gameplay shortcuts are suppressed. Commands work in Survival, Creative, and Spectator.

| Input while typing | Action |
|---|---|
| Up / Down | Browse the last 100 session commands |
| Tab | Complete the first matching suggestion |
| Left / Right, Home / End | Move the caret |
| Backspace / Delete | Remove text |
| Ctrl + V | Paste clipboard text |

Input is limited to 256 characters. Feedback and history are session-only. Command, item,
block, and mob names are case-insensitive. Multiword item names accept underscores such as
`stone_bricks` or quoted text such as `"stone bricks"`.

### Complete command reference

Square brackets mean optional arguments; angle brackets mean required arguments. Do not type
the brackets themselves.

| Command | Effect | Example |
|---|---|---|
| `/help [command]` | List commands or show usage for one | `/help give` |
| `/gamemode <survival\|creative\|spectator>` | Change mode; `s` and `c` are also accepted | `/gamemode creative` |
| `/time set <day\|noon\|night\|midnight\|seconds>` | Set the cycle position | `/time set night` |
| `/time add <seconds>` | Advance or offset world time | `/time add 30` |
| `/give [@s] <item> [count]` | Add items, defaulting to one; report overflow | `/give @s torch 64` |
| `/clear [item] [count]` | Clear all inventory or a selected item/quantity | `/clear cobblestone 16` |
| `/tp <x> <y> <z>` | Teleport and reset movement/fall state | `/tp ~ ~10 ~` |
| `/teleport <x> <y> <z>` | Alias for `/tp` | `/teleport 100 70 -250` |
| `/kill [@s]` | Kill the current player, even when explicitly used in Creative/Spectator | `/kill` |
| `/summon <mob> [x y z]` | Spawn a mob; defaults to three blocks along the player's look direction | `/summon NijikaIjichi` |
| `/weather <clear\|rain\|thunder> [seconds]` | Set weather for up to 86,400 seconds, or use a random duration | `/weather thunder 120` |
| `/weather query` | Show current weather and remaining duration | `/weather query` |
| `/summon lightning_bolt [x y z]` | Strike at the specified loaded position (relative coordinates supported) | `/summon lightning_bolt ~10 ~ ~10` |
| `/seed` | Show the current session seed | `/seed` |
| `/setblock <x> <y> <z> <block>` | Replace one block in a loaded chunk | `/setblock ~ ~-1 ~ stone` |
| `/fill <x1> <y1> <z1> <x2> <y2> <z2> <block>` | Fill an inclusive region, up to 4,096 blocks | `/fill 0 60 0 4 64 4 stone` |

- `@s` and `@p` both select the single local player for `/give` and `/kill`. Other selectors,
  including `@e`, are not implemented.
- Coordinates accept decimals and relative offsets: `~` keeps that coordinate, `~10` adds 10.
  Local `^` coordinates are not supported. Block coordinates are rounded down.
- `/give` accepts counts from **1 to 2,304**, respects stack limits, and reports anything that
  does not fit rather than silently discarding it.
- `/setblock` and `/fill` require loaded chunks and valid world-height coordinates. `/fill`
  rejects oversized or partly unloaded regions before editing. Use `air` to remove blocks.
- Time uses **seconds**, not Minecraft ticks. The cycle wraps every **420 seconds**:
  Day = 35, Noon = 105, Night = 245, Midnight = 315.
- Gameplay-changing commands have an internal cheat permission check. Cheats currently default
  to enabled; no world-creation permission toggle is exposed yet. `/help` and `/seed` remain
  available if permission is disabled.
- `/locate`, `/difficulty`, `/gamerule`, multiplayer selectors, and Minecraft's full command
  catalog are not implemented.

Installed-world changes use the normal save system. Demo commands affect only the current
session. Command-summoned hostiles may appear in daytime; natural spawning remains night-only except during sufficiently dark thunderstorms.

## Mobs

Voxel Frontier currently has **nine mob types**:

| Mob | Role / behavior |
|---|---|
| Cow | Passive animal |
| Pig | Passive animal |
| Sheep | Passive animal |
| Villager | Village-associated non-hostile mob |
| Pillager | Separate hostile mob associated with outposts |
| HitoriGotoh | Silent explosive hunter. Begins a roughly 1.5-second fuse near a visible Survival player; moving away can cancel it. Explosions damage entities and destructible terrain. |
| KitaIkuyo | Melee hunter with 35-block detection and brief last-known-position memory. Attacks deal 2 HP / one heart with a cooldown. |
| NijikaIjichi | Ranged enemy that keeps distance and fires gravity-driven arrows. Each hit deals 3 HP / 1.5 hearts before protection; independent arrow hits stack. |
| RyoYamada | Wall-climbing melee enemy with velocity-based lunges and attack cooldowns. Melee hits deal 2 HP / one heart. |

The four named hostiles have separate identities and PNG sprites, sharing common infrastructure.
Their upright camera-facing sprites and hitboxes are **1.75 blocks tall**; sprites preserve the
original aspect ratio. Walking hostiles navigate ordinary one-block terrain; RyoYamada also
climbs walls. They target Survival players, not Creative or Spectator players.

Natural billboard spawning is night-only, uses equal initial weights, and shares a hostile cap
of **18**. Version 2.6.0 increases nighttime hostile candidate frequency by 1.5× without raising
the cap. Existing mobs can remain after sunrise. Pillager structure spawning remains separate.
Normally killing a billboard hostile plays `boowomp.ogg`; a successful HitoriGotoh detonation
does not also play the normal death sound. Full-game saves preserve each mob's identity.

**Wolves have been removed.** Old Wolf save entries are handled safely; Wolves cannot be summoned.
Use `/summon cow`, `/summon Pillager`, or any of the four billboard names above.

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

The installed game keeps separate worlds under `%LOCALAPPDATA%\VoxelFrontier\saves\`:

```text
saves/
    world-<safe-generated-id>/
        world.info
        world_seed.txt
        voxel_world.vxw
        voxel_inventory.vxi
        voxel_player.vps
        voxel_mobs.vxm
```

Visible names do not become folder paths, so duplicate names and filename characters are safe.
`world.info` stores the name and last-played time. Existing gameplay save formats remain unchanged.
World files store edits, player state, inventory, mobs, furnaces, chests, and other persistent data.
Each world restores its own day/night cycle; older saves without time metadata use 35 seconds.
Global settings stay in `%LOCALAPPDATA%\VoxelFrontier\voxel_settings.cfg`; screenshots and
logs also remain outside world folders.

On first launch, legacy files in the user-data root are copied and verified into **Imported World**.
The import is published only after copying succeeds, and the original files remain untouched.
Imported worlds retain their existing terrain-generator version. Back up the entire profile
before resetting a world; Reset World affects only the current world.

The installed game autosaves every **12 seconds** and when it exits normally. The standalone
demo never loads or writes these saves.

Back up the user-data folder before replacing saves or resetting a world. Use **Reset World**
from the pause menu to start over deliberately.

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
- Greedy meshing merges only faces with compatible texture, lighting, and AO
- Geometry-aware corner AO uses neighboring block bounds to handle partial geometry
- Low effect quality disables AO; Medium/High retain baked voxel AO (no SSAO pass)
- Brightness changes the low-light response through a uniform without remeshing chunks
- Fullbright bypasses normal lighting/AO and remains separate from Brightness

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

The F3 overlay includes FPS, coordinates, chunk, biome, facing, mode, seed, lighting, frame time,
render/simulation distances, entity/mob counts, spawn attempts, AI/navigation timings, queues,
vertices/triangles, and build/upload/edit timings. On Windows it also reports current process
RAM (working set) and peak usage, sampled periodically. F4 adds world-generation diagnostics.

## Settings

Settings are saved to:

```text
voxel_settings.cfg
```

Choose **VIDEO**, **AUDIO**, **CONTROLS**, or **WEATHER** from Settings, then use **BACK** to return.
Mouse sensitivity and existing keybindings are under Controls.

Available options include:

- Render distance
- Simulation distance
- Field of view
- Mouse sensitivity
- Brightness: 0–100%, with 50% as the normal midpoint; 100% is not Fullbright
- Low / Medium / High graphics presets, plus Custom
- Entity distance, particle density, effect quality, and frame limit
- Audio submenu: Master, Music, SFX, Passive Mobs, and Hostile Mobs volume
- Off / 2x / 4x MSAA
- Fullscreen
- VSync
- FPS display
- Coordinate display
- Custom keybindings
- Weather cycle, Off/Low/Medium/High quality, density, snow accumulation, lightning/flash intensity, fog and wind

Most settings apply immediately.

Background music uses every valid OGG track in `sounds/background/` and plays through a
shuffled list continuously. Music is streamed separately from sound effects and continues in
menus. The Music slider controls 0–100% of the intended music level; its real playback gain is
`Master × Music × 0.30`. The full installed game saves audio settings. The standalone demo keeps
them only for the current session. Release builds embed the background tracks in the executable.

## Weather

Each installed world stores weather, transition intensity, remaining duration and random
sequence in its existing seed metadata. Old metadata defaults to Clear; demo sessions remain
stateless. Hydrated farmland grows crops 25% faster while wet. Lightning deals 5 HP to nearby
exposed entities; Creative/Spectator players are immune. Fire is temporary and non-spreading.
A sky-exposed rod attracts strikes within 32 blocks. Close impacts reach 64 blocks and thunder
320 blocks; thunder arrives after `distance / 343` seconds. Weather uses Master/SFX controls.

Graphics Off skips precipitation generation, scans, uploads and draws. Low has a sparse
16-block radius; Medium/High use 24/30 blocks and add splashes. One batched draw handles
precipitation and lightning. Cached column roof heights avoid per-particle vertical scans.
Weather lighting/wet-ground shading use uniforms without chunk remeshing; actual snow/fire/
farmland edits use the normal block path. Storage, environment ticks and strike searches are bounded.
Graphics switches leave weather gameplay active; snow accumulation has its own explicit toggle.

Developer checks: `--weather-smoke --seed=123456789` covers biomes, roofs, ecology, commands,
audio, lightning, recipes and graphics paths. `--weather-benchmark=<folder>` also measures
RD8/16/32/64 at every weather quality. Use isolated user data for tests.
See [weather validation](docs/WEATHER_VALIDATION.md) for measurements and verification limits.
Original rod/fire artwork can be regenerated with `python tools/generate_weather_textures.py`
after the existing atlas generation tools.

## Source Structure

The project is split into focused systems:

- `main.cpp` - application entry point
- `Weather` - per-world weather, ecology, lightning and metadata
- `WeatherRenderer` - bounded local precipitation/lightning batch
- `WeatherValidation` - opt-in Release smoke checks and quality benchmarks
- `Game` - initialization, main loop, simulation, rendering, saves, and shutdown
- `World` - chunks, terrain, blocks, lighting, and world data
- `WorldLibrary` - saved-world listing, creation, metadata, and safe legacy import
- `Renderer` - OpenGL rendering and mesh handling
- `Player` - movement and first-person physics
- `SurvivalWorld` - survival mechanics and entities
- `FarmingSystem` - crops and farming
- `SoundSystem` - positional SFX, category gains, and streamed background music
- `CommandSystem` - registry, parsing, validation, and autocomplete
- `ChatUI` - session input, history, suggestions, and feedback
- `Explosion` - reusable bounded blast damage and terrain effects
- `AmbientOcclusion` - geometry-aware voxel corner AO
- `Persistence` / `UserData` - demo capability checks and Windows data/log paths
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
- **Audio:** miniaudio / OGG decoding
- **Windows Installer:** Inno Setup 7

---

**Voxel Frontier** is an experimental voxel sandbox focused on building Minecraft-style systems from scratch in modern C++ and OpenGL, with development driven heavily by ChatGPT Codex.
