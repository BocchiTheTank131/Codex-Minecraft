# Voxel Frontier

> A Minecraft-inspired voxel sandbox built entirely with **ChatGPT Codex**.

Everything in this project was created entirely with Codex, including the game code, README, GitHub updates, and development prompts.

### v2.19.0 - Exploration & Relics

Relics now power a Surveyor Compass, charged Echo Lantern with loaded-only cave
echoes, and a durable Ancient
Explorer Tool. A per-world Adventure Journal (`J`, configurable) tracks discovered
biomes, structures, relics, puzzles, finite challenges and one-time milestones.
Generator V24 retains V23 geography and adds Three Bells, Divided Current and
Pulse Hall chambers, accessible relic caches, locked finite vaults, a two-guard
ambush and a visible timed trap trial. Ancient Seals unlock vaults without being
consumed; Resonant Cores support a rare explorer upgrade and can also be crafted
in older worlds. Paid relic recipes also support pre-structure worlds.

All generators V1–V23, their existing loot, the 361 historical previews, original
world textures and the four hostile billboard mobs are preserved. New exploration
records share the immutable transactional checkpoint; the demo remains stateless.
See [review corrections](docs/V2190_REVIEW_FIXES.md), [validation](docs/EXPLORATION_RELICS_VALIDATION_v2.19.0.md) and
[release notes](docs/RELEASE_v2.19.0.md).

[Download v2.19.0](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.19.0).

### v2.18.1 - Engine Optimization

Exact V23 column reuse, bounded shared-buffer retirement, immutable background
checkpoint encoding, coalesced mesh requests, and affinity-aware streaming workers.
Terrain, visual settings, saves, automation and gameplay remain compatible.

See [measured validation](docs/ENGINE_OPTIMIZATION_VALIDATION_v2.18.1.md) and
[local release notes](docs/RELEASE_v2.18.1.md), including remaining driver and RD64
limits. Download the installer and standalone demo from the [v2.18.1 release](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.18.1).

### v2.18.0 - World Generation & Biome Expansion

Generator V23 adds connected mountain ranges and eroded uplands, broader compatible
habitats, expanded cave networks and underground lakes, supported cave approaches,
clustered mixed woodland, and five wilderness landmark designs. New worlds use V23;
saved worlds retain their original generator, including their unexplored chunks.

All 35 biomes, generators V1–V22, stable block/item IDs, automation, checkpoint
integrity, and the four hostile billboard mobs are preserved. No coordinate-precision
migration or new save format is included.

See [V23 validation](docs/GENERATOR_V23_VALIDATION.md) for native screenshots,
comparison maps, reproducible tests, generation costs, and remaining limitations.
[Release notes](docs/RELEASE_v2.18.0.md) describe the Windows installer and stateless
demo. Download both variants from the
[v2.18.0 release](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.18.0).

### v2.17.0 - Logic & Automation

Build deterministic power circuits, timed repeaters and logic gates; control lamps,
doors and machinery; transport container items with hoppers; and harvest crops into
storage with paid replanting. Original pixel textures and cached 64px previews extend
the existing building, crafting and quality-of-life systems.

See [Logic & Automation validation](docs/LOGIC_AUTOMATION_VALIDATION.md) for signal
rules, recipes, safety limits, test evidence and performance measurements. This update is available as a
[v2.17.0 release](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.17.0).

Stabilization changes are documented in
[the prerelease fix report](docs/V2170_PRERELEASE_FIX_VALIDATION.md).
Large-coordinate precision remains deferred; the report lists tested fixes and limitations.

### v2.16.2 - Quality of Life

Inventory and container sorting, Quick Stack, Deposit All, Take All, and protected
hotbar slots. Contextual hints, adjustable UI scaling, textured placement previews,
accurate checkpoint feedback, Creative categories/favorites/recent items, crafting
requirements, and conventional multiline sign editing extend the existing systems.
The v2.16.1 icons, all 22 generators, item/block IDs and billboard mobs are preserved.

See [QOL validation](docs/QOL_VALIDATION_v2.16.2.md) for controls, screenshots,
measurements, compatibility checks and limitations. Download both Windows variants from the
[v2.16.2 release](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.16.2).

### v2.16.1 - Inventory Icon & Visual Clarity

All 347 registered items now have cached 64×64 previews. Blocks use the actual
terrain mesh and material tiles in a consistent three-quarter view; stairs,
slabs, fences, furniture, beds and panes retain their real shapes. Boats use the
existing hull models. Tools, armor, food and other non-block items use shaded,
extruded pixel-art silhouettes. Four Creative pages and 44×44 slots are unchanged.

The atlas is generated offline, embedded in both Windows variants, and uses the
same 5.47 MiB of GPU texture storage as v2.16.0. There is no per-frame thumbnail
rendering. World textures, item/block IDs, saves and Generators V1–V22 are unchanged.
Gate rails/posts receive a matching visual model correction; collision is unchanged.
Bed pillow and blanket textures now follow all four placement directions.
Creative inventory now includes item-name search. Axe/shovel actions wear the
correct hand, and packaging requires the thumbnail manifest.

See the [visual validation report](docs/ICON_VISUAL_VALIDATION_v2.16.1.md) and
[local release notes](docs/RELEASE_v2.16.1.md). Release packages are available on GitHub.

### v2.16.0 - Building & Decoration

Build with 142 additional materials and pieces: six wood families, corner stairs,
slabs, connected fences/walls/panes, gates, trapdoors, doors, masonry, furniture,
lanterns, beds, barrels, signs, paintings, item frames, pots, carpets and scaffolds.
Beds provide sleeping and a safely validated personal respawn point. Signs and
displayed items participate in the existing coordinated save checkpoints.

Block types and items now have stable 16-bit identifiers. Block properties are
separate from type IDs; ordinary terrain retains compact one-byte storage.
Historical IDs and Generators V1–V22 remain unchanged. Older saves are readable;
the new save formats cannot be opened by older game versions, so retain backups
before upgrading.

Controls: right-click to interact; Sneak to place beside interactive blocks;
Sneak + right-click to remove a displayed item or plant. Sneak + shovel makes a
dirt path; axes strip logs. Sneak places a medium painting. Signs use Tab for a
new line, Enter to save and Escape to cancel. Scaffold uses jump/sneak to climb.
Creative's four-page catalog uses the same slot size as inventory and separates
unlimited catalog entries from owned stacks. Clicking or right-dragging an item
selects the item displayed on the current page.
Recolor beds, carpets and candles with red/yellow flowers or Bone Meal; Creative
Sneak + empty hand cycles all four colors.

See [architecture, validation, screenshots and limitations](docs/BUILDING_DECORATION_VALIDATION.md)
and the [release notes](docs/RELEASE_v2.16.0.md).
Download the installer or standalone demo from the [v2.16.0 release](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.16.0).

### v2.15.1 - Survival Statistics & Wheat XP

Creative and Spectator simulation retain Survival health, hunger and saturation.
Harvesting ripe wheat in Survival again awards 2 XP; immature crops and Creative
harvesting award none. See [fix validation](docs/VALIDATION_v2.15.1.md).
Download the installer or standalone demo from the [v2.15.1 release](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.15.1).

### v2.15.0 - Farming, Food & Survival

Grow carrots, potatoes, beetroot, pumpkins, melons and berries alongside wheat.
Irrigate fields, craft hoes, compost spare produce, and use Bone Meal. Raise
chickens for eggs and meat, shear sheep, milk cows, and prepare new meals.
Saturation supports longer journeys while Survival statistics persist across
game-mode changes. New seeds can be collected in existing worlds.

This update retains Generator V22 and the coordinated checkpoint system.
See [farming rules, validation and limitations](docs/FARMING_SURVIVAL_VALIDATION.md).
Download the installer or standalone demo from the [v2.15.0 release](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.15.0).

### v2.14.3 - Checkpoint Integrity, Audio & Settings Polish

Game-mode, time and weather changes are saved together in complete checkpoints.
The save layer rejects standalone changes to checkpoint-managed files, and permanent
checksum failures stop automatic retries. For worlds affected by v2.14.2, retain all
original files and use **R twice** in Select World to recover a verified previous
checkpoint into a separate world. Recovery refuses missing or invalid backups.

See [checkpoint correction and recovery instructions](docs/CHECKPOINT_v2.14.3_VALIDATION.md).
Includes updated audio, a now-playing label, fresh-profile defaults and crisp UI
rendering separate from scene antialiasing.
Download [v2.14.3](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.14.3).

### v2.14.2 - Stability, Reliability & Performance

This build adds verified checkpoints, fail-safe handling of corrupt world sidecars,
per-world session protection, unloaded-terrain collision barriers and corrected
low-FPS simulation timing. In Select World, an unavailable world can be recovered
into a **separate world** with **R twice within five seconds**, when a verified
previous checkpoint exists. Keep external backups; recovery never repairs the
original in place. The standalone demo remains stateless.

See [Stability validation](docs/STABILITY_VALIDATION.md) for all 25 audit findings,
measured results, reproducible tests and remaining limitations.
Download [v2.14.2](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.14.2).

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
New worlds have a name, an optional numeric seed, Survival or Creative mode, and per-world Cheats (Off by default). Select a saved world to change its Cheats permission.
The standalone demo skips world selection and starts a fresh temporary world.
The world starts only after PLAY. From the pause menu, **SAVE & RETURN TO MENU** saves the
installed game; the standalone demo's **RETURN TO MENU** discards its temporary world.
Pressing PLAY again in the demo starts a fresh gameplay session.

## Features

- Versioned procedural worlds: new Generator V21 worlds span Y=-64 through 319, with natural generation capped at Y=256; V11–V20 worlds retain their original terrain and generators 1–10 keep Y=0 through 255
- Multi-noise terrain with continents, mountains, rivers, beaches, oceans, caves, ravines, and aquifers
- Plains, deserts, snowy mountains, forests, and other terrain variations
- Villages, pillager outposts, desert pyramids, ruins, wells, and campsites
- Rare abandoned mineshafts, monster dungeons and ancient underground ruins, with persistent treasure and finite encounters
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
- Leather, Gold, Iron and Diamond armor, directional shields, and dedicated equipment slots
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
birch, spruce, jungle, acacia and dark oak planks, including mixed planks where the output has no wood variant. Breaking Stone
provides Cobblestone for stone tools and the eight Cobblestone Furnace recipe; smelting
Cobblestone produces Stone for Stone Bricks and Stone Slabs.
Andesite crafts from Diorite and Cobblestone.

Version **2.13.0** has **164 inventory items**, **98 placeable block families**, **167 block
IDs/states including Air**, **90 crafting recipes**, and **23 furnace recipes**. Existing item
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
| Five Leather / Gold Ingots / Iron Ingots / Diamonds in a helmet pattern | Matching Helmet | 3×3 |
| Eight matching armor materials, empty top center | Matching Chestplate | 3×3 |
| Seven matching armor materials, empty lower centers | Matching Leggings | 3×3 |
| Four matching armor materials in two separated columns | Matching Boots | 3×3 |
| Six generic Planks around an Iron Ingot | Shield | 3×3 |
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

Generic plank ingredients accept Oak, Birch, Spruce, Jungle, Acacia and Dark Oak, including
mixed planks. Wooden Doors require six matching planks; all families produce the existing
Wooden Door item.
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

## v2.13.2 - The Geographic Diversity Update

- New worlds use V22: shorter continental/erosion wavelengths, natural coasts without the radial starter island, compatible regional habitats, denser winding drainage and occasional connected inland lakes.
- More accessible cave approaches select supported native floors, retain their landing beds, avoid rivers/surface structures and prevent nearby vegetation from obstructing the bore.
- Survival selects natural land using bounded resource/variety/cave checks. Ocean-heavy seeds use a bounded second search; respawn rechecks collision safety. Existing saves retain generators 1-21 and their geography.
- Temperature/humidity, vegetation placement rules, mountain/density profiles, height limits, weather, mobs and the optimized renderer remain intact.
- Release validation: `--diversity-audit=<directory> --generator=22` exports three-seed V9/V18/V21/V22 maps at 4,096/15,360/46,080 blocks and validates generation, rivers, entrances, 50 paired spawns and real on-foot cave walks. `--diversity-spawns=<directory>` runs the spawn/walking subset. `tools/build_diversity_previews.py` renders the maps; `tools/run_geographic_diversity_benchmarks.ps1` compares fixed streaming paths.
- [Measurements, captures, compatibility checks and limitations](docs/GEOGRAPHIC_DIVERSITY_VALIDATION.md).

## v2.13.1 - The Optimization Update

- Lossless 28-byte terrain vertices and exact quad indexing: approximately 57% less live terrain/upload data, without changing full-detail geometry, UVs, lighting or AO. Shared opaque/water VBO+EBO pages retain the original transparent draw sequence.
- Reused stationary visibility lists, touched-page batching, smaller allocation slack, bounded idle-page reserves and paired GPU retirement.
- Video options: Terrain LOD Off/Low/Medium/High, Occlusion Off/Auto/On, Terrain Memory Target Auto/Low/Balanced/High. LOD and occlusion default Off. Current LOD reduces distant tiny decorations only; full terrain/canopies/structures/transparent surfaces remain exact. Memory targets are soft and never discard visible geometry.
- F3 adds LOD/culling counts, drawn geometry/calls, terrain memory, worker/queue/throughput and generation/mesh/upload/render diagnostics.
- No generator, ID, save, gameplay, physics, weather or billboard mob changes. Full/demo Release builds and exact historical/pixel checks passed.
- [Measured results, reproduction, limitations and deferred techniques](docs/OPTIMIZATION_VALIDATION.md). RD8–32 forest frame times improved materially; occlusion showed no reliable general gain and worst teleport spikes remain. Full surface/horizon LOD and hard VRAM residency limits are deferred.

## v2.13.0 - The Equipment & Combat Update

- Leather, Gold, Iron and Diamond armor: helmet, chestplate, leggings and boots, with 7/11/15/20 armor points for complete sets. Four dedicated armor slots, material durability, breakage, 16×16 icons, crafting and Recipe Book support.
- One offhand slot and a craftable shield. Hold RMB with a sword/empty hand to block frontal melee, arrows and explosions. Rear and environmental damage bypass shields. Blocking slows movement by 35%. Primary food, placement and container interactions retain priority.
- **F** swaps the selected stack and offhand (configurable in Controls). Equipment supports clicks, drag release, shift-click, Q/Ctrl+Q drops, and normal pickup. Creative catalog copies remain unlimited; owned stacks are transferred, never copied.
- The HUD shows armor protection, offhand items and durability. First-person shields use the existing item batch, with brief block feedback and equipment-break audio.
- Swords deal 4/5/6/4/7 HP for Wood/Stone/Iron/Gold/Diamond, with a 0.625-second shared attack cooldown. Existing critical hits, knockback, mob behavior and animations are retained.
- New-world underground loot occasionally includes worn armor or shields; deep Diamond helmets are very rare. Existing chest contents and legacy world loot rules remain unchanged. All gear can be crafted in existing worlds.
- Inventory V5 stores five equipment stacks; V4 inventories load with empty equipment. World saves append an independent loot-rule marker. No terrain generator change: generators 1–21 remain intact. Demo persistence and cheats restrictions remain intact.
- Release builds and validation results: [Equipment validation](docs/EQUIPMENT_VALIDATION.md).

## v2.12.1 - Gameplay, Structures & Bug Fixes

- Generator V21 increases underground structure frequency by about 25% and adds sparse cobwebs to every accepted structure, while preserving generators 1–20. Generator V20 expands underground room networks and chambers, adds storage/library details and more loops, and keeps generators 1–19 unchanged.
- Water and lava use per-job lava deadlines to avoid bounded-queue tick starvation, and source activation restores missing downstream notifications. They share a bounded queued flow solver: water reaches seven horizontal blocks, lava four, with downward flow and slower lava updates. Flow is reconstructed from persistent sources after reload.
- Lava applies one refreshable five-second burn, dealing one HP per second. Water extinguishes it. Burning uses existing batched particles; cobweb intersection slows player movement by 75%.
- Installed worlds store Cheats individually. New worlds default Off; legacy worlds retain their previous command access. The standalone demo denies privileged commands. Pause-menu gamemode selection is removed.
- Natural billboard hostiles use actual sky/block light, supporting dark caves in daytime while rejecting bright rooms.
- New/reset inventories start empty. Creative gameplay and GUI dropping remove the actual selected quantity. Q/Ctrl+Q support cursor or hovered owned stacks; dragging a held stack outside drops it once. Catalogue entries remain unlimited. Cracked Stone Bricks use a shaded cube inventory icon.
- Release regression evidence and performance limits: [Gameplay validation](docs/GAMEPLAY_VALIDATION.md).

## v2.12.0 - The Exploration Update

New worlds use **Generator V19**, adding underground structures to V18 geography.
Saved worlds keep generators 1–18, including their unexplored chunks.

- **Abandoned Mineshafts:** branching tunnels, supported wooden platforms, beams,
  storage rooms, cobwebs, stairs, occasional collapsed dead ends and loot.
- **Monster Dungeons:** varied cobblestone/mossy/cracked rooms, treasure and small,
  finite groups of existing Pillagers. Deeper encounters are harder.
- **Ancient Underground Ruins:** rare multi-room layouts, corridors, stairs,
  collapsed corners and secluded library treasure alcoves. Deep ruins are rarer.
- Every accepted layout connects to a verified dry cave opening. World-coordinate
  plans cross chunk boundaries consistently and avoid existing structure footprints.
- Weighted Common/Uncommon/Rare rewards include food, torches, materials, ores,
  ingots, tools and occasional diamonds. Depth improves reward chances; no chest
  receives more than one rare roll.
- Three collectible relics: **Surveyor Token**, **Cavern Shard** and **Ancient Seal**,
  with original 16×16 icons. They are collectibles, with no new combat powers.
- Cobweb and Cracked Stone Bricks are appended block/item IDs. Cobwebs are
  decorative, non-solid crossed geometry; this update adds no slowing mechanic.
- Existing 27-slot chests preserve emptied/modified contents. Encounter markers
  persist so saved encounters do not respawn on chunk reload. Enemies use existing
  simulation distance rules; there is no continuous monster spawner.
- Full builds persist worlds normally; standalone demos remain stateless.
  Weather, terrain shapes, climate, vegetation and the 20-minute cycle are retained.

See [Exploration validation and performance](docs/EXPLORATION_VALIDATION.md).
The opt-in `--underground-audit=<absolute-directory>` fixture exports structure
locations, depth/biome counts, deterministic generation checks, loot samples,
legacy hashes and save tests. Supply `baseline-historical.csv` with
`seed,version,x,z,hash` columns from the validated historical fixtures. Run
`python tools/build_exploration_previews.py <directory>` for labeled maps.
Use an isolated `VOXEL_FRONTIER_DATA_DIR` for all developer tests.
Original pixel assets can be regenerated with `tools/generate_exploration_assets.py`.

## v2.11.3 - Generator V18: Geography, Rivers & Biome Balance

New worlds use **Generator V18**. Saved worlds retain generators 1–17,
including unexplored chunks.

- Smaller regional landmasses, more varied bays and straits, and connected oceans.
- Deterministic downhill river networks with tributaries, shared junctions,
  narrow headwaters, wider main channels, local water elevations and sloping banks.
- Climate-compatible habitat balance favors ordinary Forest, Plains and Taiga;
  Flower Forest, Old Growth Taiga and ocean Mushroom Fields are less common.
- V17 temperature/humidity, vegetation density, mountain shaping, caves, weather,
  the 20-minute cycle, natural Y256 ceiling and player building to Y319 remain.
- Bounded caches belong to each generation worker; no renderer or save-format rewrite.

See [V18 validation, measurements and limitations](docs/GENERATOR_V18_GEOGRAPHY.md).
`--aesthetics-audit=<absolute-directory> --generator=18` exports identical-seed
V17/V18 overview maps, native river windows, river gradients/widths and regression data.

## v2.11.2 - Generator V17: Terrain & Vegetation Refinement

New worlds use **Generator V17**. Existing worlds continue using their
stored generator, including unexplored chunks from generators 1–16.

- Smaller continental features and stronger regional coastal/erosion variation
  produce more bays, peninsulas, islands and inland basins without V9 fragmentation.
- Moderate extra inland relief reuses existing samples; mountain density,
  connected overhangs, caves and the natural Y=256 ceiling remain intact.
- V16's temperature, humidity and macroclimate samples are preserved exactly.
- Valid woody tree roots are thinned by approximately 20%, independent of species.
  Jungle bamboo uses deterministic groves with occasional isolated stalks and
  approximately 50% fewer stalks. Removed candidates do not become extra plants.
- The existing worker, mesh, lighting and rendering pipelines are unchanged.

See [V17 validation and performance](docs/GENERATOR_V17_REFINEMENT.md).
`--aesthetics-audit=<absolute-directory> --generator=17` exports V9/V16/V17 maps,
paired terrain routes, vegetation counts and compatibility regressions.

## v2.11.2 foundation - Generator V16: Climate Distribution Refinement

Generator V16 introduced regional climate refinement. Existing worlds retain generators
**1–15**, including their climate, weather classification and unexplored chunks.
These changes are included in stable v2.11.2.

- An unchanged broad macroclimate now contains stronger, smooth regional temperature
  pockets, with restrained fine detail. Altitude cooling remains separate.
- Biomes, vegetation, precipitation, freezing and climate colors use the same
  versioned temperature field. All 35 biomes remain available.
- V15 height shaping, density fields, mountains, underground rules, humidity,
  continents, rivers, height limits and the 20-minute cycle remain unchanged.
- One additional noise sample per cached terrain column; no new per-block climate
  evaluation, rendering pass or save layout.

See [V16 validation and performance](docs/GENERATOR_V16_CLIMATE_VALIDATION.md).
`--aesthetics-audit=<absolute-directory> --generator=16` exports paired V9/V15/V16
maps at 4,096 and 30,720 blocks plus route, terrain and generation measurements.
`tools/analyze_climate_distribution.py` measures climate boundaries, region sizes,
multi-scale temperature differences and exploration diversity from those fixtures.

## v2.11.2 foundation - Generator V15: Controlled Chaos

Generator V15 introduced the following terrain changes. Saved V15 worlds retain
their geography, as do worlds using generators **1–14**, including their
unexplored chunks. Release metadata remains
`2.11.2-beta2` until the update is approved for publication.

- Narrow contour ridges, compact mountain groups, stronger medium-scale hills and
  valley shoulders replace V14's broad uplifted mountain masses.
- Restrained small-scale rock detail supplements the existing large-scale continents,
  drainage and climate. Compatible local habitats vary within the broad climate regions.
- Existing attached overhangs, caves, ravines, aquifers, ores and all 35 biomes remain.
- Natural terrain, vegetation and structures stay at or below **Y=256**. Smooth peak
  compression avoids flat clipped summits; high-altitude trees require room for their
  complete models. Player construction still reaches **Y=319**.
- V14's narrower coastal selection, woodland clearings, reduced Flower Forest density,
  shared climate/weather data and **20-minute day/night cycle** remain in place.

See [V15 validation and performance](docs/GENERATOR_V15_VALIDATION.md) for paired
maps, actual game captures, exact historical hashes, benchmarks and limitations.
`--aesthetics-audit=<absolute-directory> --generator=15` exports V9/V14/V15 maps
and V14/V15 terrain, route, connectivity and generation measurements.

## v2.11.2 foundation - Terrain refinement and 20-minute day/night cycle

The published v2.11.2-beta2 includes Generator V13. The V14 refinement introduced
a separate generator so existing V13 worlds remain geographically unchanged.
V15 builds include these changes while preserving V14 geography for V14 saves.

- Narrower mountain ridges retain similar peak elevations, with more inland rolling relief.
- Smooth medium-scale temperature variation supplements the unchanged macroclimate.
- Narrower beach bands and relief-aware Stony Shores retain continuous coastlines.
- Flower Forest decoration produces approximately half as many flowers, preserving both
  flower colors. Deterministic woodland clearings vary canopy coverage.
- The full day/night cycle now lasts **1,200 seconds (20 minutes)**: 600 seconds of
  daylight and 600 seconds of night, with the existing smooth sunrise/sunset lighting.
- Legacy 420-second saved times are converted by cycle phase. An optional `TIME_CYCLE1`
  metadata tail prevents repeated conversion; weather timers and terrain versions are independent.

See [terrain refinement validation](docs/GENERATOR_V14_REFINEMENT.md) for measured
results, comparisons and limitations. These changes are included in stable v2.11.2.

## v2.11.2-beta2 - Generator V13: Terrain Aesthetics

This prerelease creates **Generator V13** worlds. Existing saved worlds keep
**generators 1-12**, including unexplored chunks; no item IDs or save layouts change.
The v2.11.2-beta2 prerelease includes V13 for new worlds; existing worlds retain their generator.

- Connected mountain envelopes replace the above-surface 3D noise lobes that produced
  floating stone pancakes. Slope-aware protected roofs attach localized undercuts.
- Regional ridges, rocky faces, foothills, rolling terrain and dry plateaus retain
  the broad continents, oceans and macroclimate; inland relief cannot create tiny oceans.
- Medium-scale compatible habitats add variety within the existing 35-biome climate system.
- Suitable warm/wet mountains retain woodland instead of becoming stone solely because
  of elevation. Hot dry woodland buffers sand/terracotta against wetter forests.
- Tree roots respect neighboring terrain and cave openings; canopy heights vary,
  while generation remains deterministic across chunks and worker order.
- The existing 384-block world, sea level 64, underground cave fields, aquifer and ore
  rules, weather, structures, asynchronous streaming and render-distance limits remain.

See [V13 validation and limitations](docs/GENERATOR_V13_VALIDATION.md). Opt-in
`--aesthetics-audit=<absolute-directory> --generator=13` exports paired V12/V13 maps,
short-route statistics, actual terrain volumes and connectivity measurements, and runs
cave, structure, parallel generation, mesh, GPU-upload and save compatibility checks.
`--worldgen-test --generator=13 --preview-clean` captures terrain in the real renderer;
`--preview-ground` selects a surface camera. These switches are development fixtures.

## v2.11.2-beta - World Generation, Vegetation & Chunk Streaming

New worlds use **Generator V12**. Worlds saved with generators **1–11** retain their
original geography, including chunks explored after updating. No block/item IDs or save
record layouts change. Generator V12 preserves the 384-block height, continents,
macroclimate, 35 biomes, density overhangs, underground caves, ores and aquifers.

- Medium-scale climate-compatible habitats replace rapidly alternating local variants.
  Inland rolling relief adds foothills and valleys without shifting the continental field.
- Swamps have rare mushroom clusters; dark forests have spaced clusters; Mushroom Fields
  retain plentiful mushrooms. Ground decoration between clusters is sparser, and tree
  candidates use deterministic spacing.
- Occasional hillside passages and sinkholes connect the surface to existing dry cave
  cavities. Structure reservations and low/wet shore terrain are protected.
- Generation and meshing use separate adaptive worker pools. **Chunk Streaming** in Video
  offers Auto, Low, Balanced and High; dual-core systems use one worker for each stage.
- Nearby and forward-facing work is prioritized. Mesh revisions and world epochs discard
  obsolete results; required neighbor arrivals are coalesced before background meshing.
- OpenGL uploads and resource cleanup stay on the main thread under soft time budgets.
  Teleport cleanup spreads CPU payload and GPU resource releases across frames.
- F3 distinguishes **surface temperature** from **camera-altitude temperature**, and shows
  worker limits/activity, generation/mesh throughput, stage queues and upload timings.
  Weather continues using surface climate rather than Spectator altitude.

Validation details and measured limitations are in
[Generator V12 validation](docs/GENERATOR_V12_VALIDATION.md). Developer fixtures:
`--exploration-audit=<absolute-output-directory>` exports paired V11/V12 maps, route
diversity, vegetation counts and connected-entrance tests; `--streaming-benchmark=<directory>`
measures RD8/16/32/64 warmup, 8× flight, recovery and teleport phases. Optional controls:
`--generator=11|12`, `--streaming-rd=64`, `--streaming-quality=0..3`,
`--streaming-flight-seconds=90`, `--streaming-recovery-seconds=120`. Five-second queue and
memory samples support checking longer-flight convergence.

This is a prerelease. Dense RD64 scenes still have substantial memory/rendering costs,
and occasional teleport stalls remain; see the validation report before selecting a
high render distance. The latest stable release remains v2.11.1.

## v2.11.1 - Generator V11: Terrain & Underground Overhaul

This release introduced **terrain generator 11**, with **384 vertical blocks (Y=-64..319)** and
**sea level 64**. Existing worlds keep their saved generator version **1–10**, original
vertical coordinates, sea level, and unexplored terrain. The application release version is
**v2.11.1**.

V11 retains V10's continents, connected oceans and macroclimate. Compatible local habitat
variation creates more medium-sized biome regions while preserving all 35 biomes and
altitude cooling. A cached 3D density lattice adds mountainous overhangs and occasional
floating rock formations to the heightmap. Broad chambers, connecting tunnels, pillars
and winding surface ravines replace the denser underground carving. Regional aquifers
leave most sampled cave space dry, with occasional deep water and lava pools.

New materials are **Deepslate, Deepslate Coal/Iron/Copper/Gold/Diamond Ore, Bedrock, and
Lava**. Eight item/block IDs are appended; existing IDs are unchanged. Deepslate ores use
the corresponding existing drops and furnace outputs. This build has **142
inventory items**, **96 placeable block families**, and **160 block IDs/states including Air**.
Bedrock resists Survival mining.
Lava pools emit light and damage Survival players; this update does not add flowing-lava
or water/lava reaction simulation. Coal favors higher elevations, iron has low and mountain
distributions, copper favors middle depths, gold favors deep layers, and diamond increases
with depth with reduced air exposure. Rare regional iron/copper lodes supplement small veins.

The eight original 16×16 icons extend the native atlas to **160×240**, and the runtime atlas
to **640×960**. Existing atlas cells and world textures are preserved. Creative inventory,
commands, targeting, mining, lighting, weather, structures and persistence understand the
new world bounds. Full-game persistence and the stateless standalone demo remain separate.

Generated V11 chunks omit unused upper air in 16-block bands. Density fields and ore rolls
are cached per chunk; generation remains asynchronous. See
[Generator V11 validation](docs/GENERATOR_V11_VALIDATION.md) for measured performance,
compatibility coverage and limitations, including the additional rendering cost of cave geometry.

Developer tools: `--density-audit=<absolute-output-directory>` exports paired V10/V11
biome/climate/terrain maps at 4,096 and 30,720 blocks, depth ore counts, underground
cross-sections, 3D cutaway volumes, timings and compatibility checks for three seeds.
`python tools/build_density_previews.py <directory>` creates labeled previews and statistics
(development dependencies: Pillow, NumPy, Matplotlib). Runtime previews accept `--generator=11`,
`--preview-y`, `--preview-yaw`, `--preview-pitch`, and `--preview-torch` with `--worldgen-test`.

## v2.11.1-beta - Geographical Generator 10

The published beta uses **terrain generator 10**. Broad continental fields form connected landmasses
and ocean basins, with a smaller coastal-detail field for islands, bays and peninsulas. Climate
is split into regional temperature/humidity and restrained local variation; altitude still cools
mountains. The five climate regions and all 35 biomes remain available, with existing vegetation,
materials, climate colors and weather behavior.

Regional erosion and mountain belts create connected ranges, foothills and valleys. Rivers use
a broad warped contour and a graded valley/channel profile. A rotated noise slice avoids grid-like
river patterns. Mushroom Fields are restricted to rare offshore islands; swamp basins include
shallow water for lily pads. The starter-land fallback raises ocean near spawn gently instead of
adding an artificial mountain to every new world.

**Compatibility:** worlds saved with generators 1–9 keep those algorithms, including unexplored
chunks. No block/item IDs or save-record formats changed. The full game remains persistent and
the standalone demo remains stateless. Column caches reuse exact cave/ore noise calculations
within each generation worker; they do not interpolate or approximate noise values.

Developer validation: `--geography-audit=<absolute-output-directory>` compares generators 9/10
over **30,720 × 30,720** and **4,096 × 4,096-block** regions for seeds `20260917`, `123456789`
and `42`. Each map samples 512 × 512 columns (60-block and 8-block spacing respectively).
It exports biome, climate, continentalness and elevation data, regional generation timings,
natural-content checks, structures, chunk hashes, threaded determinism and generator save/load
tests. `python tools/build_geography_previews.py <directory>` makes labeled comparison maps
and connected-region statistics. These are development tools; no Python dependency is required
to run the game. The historical `--biome-audit` remains pinned to generator 9.

See [geography validation](docs/GEOGRAPHY_VALIDATION.md) for measured distributions,
compatibility checks, performance comparisons and remaining limitations.

This beta introduces generator 10 for new worlds. Existing worlds retain their stored
generator. See the validation report for performance measurements and limitations.

## v2.11.0 - Biome & Climate Update

The published v2.11.0 release introduced **terrain generator 9**, with 35 biomes selected from temperature,
humidity, continentalness, erosion, ridges, river masks and elevation. Temperature and humidity
are normalized to 0–1. Elevation cools mountain slopes and summits without changing the
regional base climate. Frozen/Cold/Temperate/Warm/Hot boundaries are 0.17/0.34/0.60/0.80.

| Region | Biomes |
|---|---|
| Frozen | Snowy Plains, Ice Spikes, Snowy Taiga, Frozen Ocean, Frozen River, Snowy Beach |
| Cold | Taiga, Old Growth Taiga, Cold Ocean, Stony Shore |
| Temperate | Plains, Forest, Birch Forest, Dark Forest, Flower Forest, Meadow, Mountains, Windswept Hills |
| Warm | Jungle, Sparse Jungle, Swamp, Savanna, Savanna Plateau |
| Hot | Desert, Desert Hills, Badlands, Wooded Badlands |
| Other landscapes | Mushroom Fields, Ocean, Warm Ocean, Beach, River, Snowy Slopes, Stony Peaks, Snowy Peaks |

These regions have distinct surfaces and vegetation: giant spruce/jungle trees, spreading
acacia crowns, dense dark oak trees, fern-covered taiga, flower forests, swamp lily pads,
layered badlands, packed-ice spikes/icebergs, and rare mycelium islands with giant mushrooms.
Mushroom Fields exclude natural billboard-hostile spawning; summoned/structure mobs retain
normal behavior. Grass, foliage and water use climate colors with smooth chunk-edge blending.

**32 new inventory/block families:** Spruce/Jungle/Acacia/Dark Oak Logs, Planks and Leaves;
Podzol, Mycelium, Coarse Dirt, Packed Ice, Blue Ice; Terracotta and Red/Orange/Yellow/White/Brown
Terracotta; Bamboo, Fern, Dead Bush, Lily Pad, Red/Brown Mushroom, Mushroom Stem and
Red/Brown Mushroom Blocks. Original 16×16 artwork adds 36 world tiles and 32 inventory icons.
Existing occupied inventory cells remain unchanged; the runtime inventory atlas is now 640×896.
New content is available in Creative and through `/give` and `/setblock` name lookup.

Each new Log crafts into four matching Planks and smelts into Charcoal. Generic plank recipes
accept all six wood families. Four Dirt/Gravel in alternating corners craft four Coarse Dirt;
nine Ice craft Packed Ice, and nine Packed Ice craft Blue Ice. Clay blocks smelt into Terracotta.
Colored Terracotta is obtained from badlands/Creative; no dye system is added. Natural ground
and vegetation blocks can be mined and placed with their own inventory items.

Weather reads shared climate metadata: normal Taiga gets rain, freezing surfaces get snow,
and Deserts/Savannas/Badlands remain dry. Exposed ordinary water freezes in cold climates;
ordinary Ice melts in warmer climates or strong block light. Packed/Blue Ice remain permanent.
The existing snow-accumulation option also controls this environmental freezing/melting.
Weather audio, lightning, farmland hydration and settings remain supported.

**Compatibility:** existing worlds retain generator versions 1–8, including unexplored chunks.
New IDs are appended; the world/inventory save formats are unchanged. Full installations save
normally, while the standalone demo remains stateless. F3/F4 show normalized temperature,
humidity, effective temperature, climate region and generator version in v9 worlds.

Developer validation: `--biome-audit=<absolute-output-directory>` exports chunk hashes,
CPU generation timings, three-seed biome/climate/height maps, distributions, content counts,
and persistence/recipe tests. `python tools/build_biome_previews.py <directory>` labels the maps.
`python tools/generate_biome_textures.py` appends biome artwork; Pillow is development-only.

The Windows release provides the **2.11.1** standalone demo and full-game installer.

## Windows Release

### v2.11.1 - Terrain & Underground Overhaul

- Generator 11: 384-block worlds (Y=-64..319), sea level 64, 3D terrain and expanded caves.
- Surface ravines, regional aquifers, deep lava pools and depth-based ore distributions.
- Deepslate, five Deepslate ores and Bedrock; eight original new block/item textures.
- All 35 biomes retained; generators 1–10 and existing numeric IDs remain compatible.
- Reduced upper-air storage; additional cave geometry still increases rendering cost.
- Full installer remains persistent; standalone remains a stateless demo.

### v2.11.1-beta - Geography Update

- Larger continents, connected ocean basins and broad regional climates.
- Regional mountain belts, foothills and longer river valleys; all 35 biomes retained.
- Generator 10 for new worlds; generators 1–9 remain unchanged for existing worlds.
- Exact worker-local noise caching and expanded geography/compatibility validation.
- Standalone remains a stateless demo; installed worlds and settings remain persistent.

### v2.11.0 - The Biome & Climate Update

- 35 biomes with normalized climate, altitude cooling and biome-specific vegetation.
- 32 new item/block families, expanded wood recipes and climate-colored grass, foliage and water.
- Generator 9 for new worlds; existing worlds retain generators 1–8.
- Climate-aware weather, freezing/melting and new F3/F4 diagnostics.

### v2.10.3 - Billboard Mob Speed Adjustment

- All four billboard mobs wander at 1.5 blocks/second and chase at 4.5 blocks/second.
- Player and other mob speeds are unchanged. Existing combat roles and movement mechanics are preserved.

### v2.10.2 - Billboard Mob Speed Update

- All four billboard mobs wander at 2 blocks/second and chase at 6 blocks/second.
- Player walking (6.2 blocks/second) and sprinting (8.6 blocks/second) are unchanged.
- Existing combat roles, fuse slowdown, climbing, knockback, and persistence remain supported.

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

Prerelease: [Voxel Frontier v2.11.2-beta2](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.11.2-beta2).
Latest stable: [Voxel Frontier v2.14.3](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.14.3).

| v2.14.3 Windows build | Purpose | Persistence |
|---|---|---|
| `VoxelFrontier-v2.14.3-Windows-Standalone.exe` | Run the single-file demo directly | Fresh session every launch |
| `VoxelFrontier-v2.14.3-Windows-Setup.exe` | Install the full game | Saves worlds and settings |
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
| Right Mouse | Place/use item, interact, or hold to raise offhand shield |
| `F` | Swap selected hotbar stack and offhand |
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
and gameplay shortcuts are suppressed. Commands work in Survival, Creative, and Spectator. Gameplay-changing commands require Cheats On in the installed world; `/help` and `/seed` remain available. The demo denies privileged commands regardless of command context. Use `/gamemode` with Cheats On to switch modes during gameplay.

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
- Time uses **seconds**, not Minecraft ticks. Current source builds wrap the cycle every **1,200 seconds**:
  Day = 100, Noon ≈ 233.155, Night = 700, Midnight ≈ 833.155.
  Noon/midnight match the retained solar phase offset. Published beta2 uses the prior
  420-second cycle; old saved times retain their phase when loaded by current source builds.
- Gameplay-changing commands have an internal cheat permission check. Cheats currently default
  to enabled; no world-creation permission toggle is exposed yet. `/help` and `/seed` remain
  available if permission is disabled.
- `/locate`, `/difficulty`, `/gamerule`, multiplayer selectors, and Minecraft's full command
  catalog are not implemented.

Installed-world changes use the normal save system. Demo commands affect only the current
session. Command-summoned hostiles may appear in daytime; natural spawning requires sufficiently dark sky/block light, including caves and dark thunderstorms.

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

Natural billboard spawning is light-gated (maximum effective light 6), uses equal initial weights, and shares a hostile cap
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
- Adaptive generation and mesh pools respect hardware limits and the Chunk Streaming setting
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
Streaming diagnostics include queued/ready work, active generation/mesh workers, chunks per
second, light solves and upload batches. Surface and camera temperatures are shown separately.

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
- Chunk Streaming: Auto / Low / Balanced / High worker and scheduling limits
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
- `TerrainDensity` / `WorldDensity` - versioned V11 density lattice, caves, aquifers and depth-based ores
- `TerrainExploration` / `WorldExploration` - versioned habitat variants, spaced vegetation and connected surface cave passages
- `TerrainAesthetics` / `WorldAesthetics` - V13 connected density constraints, mountain profiles and climate-compatible woodland
- `TerrainChaos` - V15 cached-column ridge shaping, medium-scale relief and smooth natural-height ceiling
- `ChunkStreaming` / `WorldStreaming` - hardware-aware worker plans, priorities, cancellation and bounded main-thread integration
- `WorldExplorationValidation` / `StreamingValidation` - route, compatibility, concurrency and flight fixtures
- `WorldDensityValidation` - V11 maps, cutaways, deterministic hashes and vertical-range regressions
- `UndergroundContent` - appended underground block/item metadata
- `UndergroundStructures` - versioned Generator V19/V20/V21 regional layouts, cave connections and bounded plan cache
- `UndergroundWorld` - integration with existing terrain density and surface-structure reservations
- `ExplorationLoot` - deterministic weighted rewards and collectible relics
- `UndergroundValidation` - opt-in structure access, compatibility, loot and persistence checks
- `Biome` - shared climate thresholds, biome definitions and vegetation/water colors
- `BiomeContent` / `BiomeVegetation` - biome materials and deterministic chunk decoration
- `WorldBiomeValidation` - compatibility hashes, climate maps and content validation
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

## v2.14.0 - Transportation & Navigation (local review)

- Six craftable wooden boats use original pixel textures, oriented hull collisions, fixed-step buoyancy, momentum, steering, safe exits and per-world persistence. Aim at open water and right-click to place; right-click the hull to mount; use movement controls to drive/turn, Sneak to exit and attack an unoccupied boat to recover it.
- Craft a Compass with four Iron Ingots around a Copper Ingot. Its smooth needle points to the original world spawn. Right-click it, or press **N**, for named per-world waypoints. Tab switches name/coordinates, Enter adds, Ctrl+Enter renames, arrows select, Delete removes and Home selects spawn. Escape closes; Ctrl+A clears the active field.
- Craft an Empty Map with eight Paper around a Compass. Right-click to initialize it. Held/offhand maps progressively survey nearby loaded surface columns; they never generate distant chunks. **M** or right-click toggles the large view. Sneak + right-click spends eight Paper to zoom out, up to 1:16. Unique map IDs survive inventory, offhand, chest and dropped-item transfers. M/N shortcuts yield to existing custom control bindings.
- World navigation data is bounded to 128 maps, 64 waypoints and 1,024 boats. Maps use 128×128 pixels and cover 128–2,048 blocks. The standalone demo keeps this data in memory only. Older inventory/container formats load with empty identities; generators 1–22 remain unchanged.
- Optional stairs/fences/gates/trapdoors are deferred to keep this pass focused on validated transportation/navigation.
- Reproduce regressions with `tools/run_transport_validation.ps1`; see [transport validation](docs/TRANSPORTATION_VALIDATION.md) for measurements, screenshots, package locations and limitations.


## v2.14.1 - Transportation & Navigation Polish

- Save diagnostics include the operation, path, error domain/code, thread and available Windows handle owners. Serialized writers and a nonblocking same-profile process guard protect temporary files. World/inventory/navigation saves use recoverable staged checkpoints with bounded frame-scheduled retries; failed commits are never reported as saved.
- Boats use spatial collision candidates, swept contacts, partial movement to contact and bounded spike catch-up. Normal simulation remains 120 Hz; source/flowing water support uses a median surface sample.
- Large maps show clearer markers, scale/coverage and controls. **F6** refreshes surveyed pixels from loaded terrain only while the large map is open; it yields to a custom control binding. **Sneak + RMB** zoom spends eight Paper once, with feedback and a short debounce. Ctrl retains its existing Sprint behavior.
- Waypoints validate coordinates and require a second Delete within four seconds to remove the selected entry.
- Existing save formats, all generators including V22, billboard mobs and demo restrictions are preserved. The original intermittent replacement failure's cause remains unresolved; tested transient failures recover without retry sleeps on the rendering thread.

See [polish validation](docs/TRANSPORTATION_POLISH_VALIDATION.md) for reproductions, tests, performance, screenshots and limitations. Publication approved by the user for v2.14.1.
