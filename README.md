# Voxel Frontier

<div align="center">

![Voxel Frontier Banner](docs/images/banner.png)

### A modern C++17 / OpenGL 3.3 voxel sandbox crafted entirely with ChatGPT Codex.

[![Release](https://img.shields.io/badge/Release-v2.20.1-blue.svg?style=for-the-badge&logo=github)](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.20.1)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg?style=for-the-badge&logo=c%2B%2B)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![OpenGL 3.3](https://img.shields.io/badge/OpenGL-3.3%20Core-5586A4.svg?style=for-the-badge&logo=opengl)](https://www.khronos.org/opengl/)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg?style=for-the-badge&logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg?style=for-the-badge&logo=windows)](https://github.com/BocchiTheTank131/Codex-Minecraft)
[![Built with Codex](https://img.shields.io/badge/Developed%20With-ChatGPT%20Codex-brightgreen.svg?style=for-the-badge&logo=openai)](https://openai.com)

**[Latest Release (v2.20.1)](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.20.1)** • **[Features](#core-gameplay-systems)** • **[Controls](#complete-controls-reference)** • **[Commands](#in-game-console-commands)** • **[Crafting](#crafting-smelting--recipes)** • **[Architecture](#engine-architecture--technical-deep-dive)** • **[Building](#building-from-source)**

</div>

### v2.20.1 - Village Polish & Living World Improvements

Passive wildlife now uses a 10-second spawn cooldown (formerly 20) and a
52-mob cap (formerly 26), for twice the natural spawn rate and capacity.
Hostile spawning settings are unchanged.


Pigs and cows also receive distinct voxel models, movement-driven diagonal
strides, smooth turning, young-animal proportions and body collision/step jumps.
See [livestock validation](docs/LIVESTOCK_VISUALS_v2.20.1.md) for captures and tests.


Original upright humanoid villagers now have distinct profession clothing and
movement/work/conversation animation. The larger trading dialog shows usable
resources, stock, discounts and availability, with explicit mouse buttons.
Merchant exchanges, newly funded Community Stalls and project guidance are improved.
Existing saves, completed projects, 365 previews and Generators V1–V24 are retained.

See [local release notes](docs/RELEASE_v2.20.1.md) and
[validation and limitations](docs/VILLAGE_POLISH_VALIDATION_v2.20.1.md).
Download the full installer, stateless demo and checksums from the
[v2.20.1 release](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.20.1).
Validation used isolated profiles; no real saved worlds were modified.


---

## Table of Contents

- [About Voxel Frontier](#about-voxel-frontier)
- [Latest Release: v2.20.0 — Villages & Living World](#latest-release-v2200--villages--living-world)
- [Core Gameplay Systems](#core-gameplay-systems)
  - [1. Procedural Generation & World Architecture](#1-procedural-generation--world-architecture)
  - [2. The 35 Biomes Directory](#2-the-35-biomes-directory)
  - [3. Living Villages & Settlement Economy](#3-living-villages--settlement-economy)
  - [4. Exploration, Relics & Adventure](#4-exploration-relics--adventure)
  - [5. Logic, Automation & Mechanical Engineering](#5-logic-automation--mechanical-engineering)
  - [6. Architecture, Building & Decoration](#6-architecture-building--decoration)
  - [7. Survival, Farming, Food & Wildlife](#7-survival-farming-food--wildlife)
  - [8. Transportation & Cartography](#8-transportation--cartography)
  - [9. Dynamic Weather & Atmosphere](#9-dynamic-weather--atmosphere)
  - [10. Quality of Life & Inventory Management](#10-quality-of-life--inventory-management)
- [Mobs & Wildlife Encyclopedia](#mobs--wildlife-encyclopedia)
  - [Passive Wildlife](#passive-wildlife)
  - [Settlement NPCs & Raiders](#settlement-npcs--raiders)
  - [The Four Original Billboard Hostiles](#the-four-original-billboard-hostiles)
- [Complete Controls Reference](#complete-controls-reference)
- [In-Game Console Commands](#in-game-console-commands)
- [Crafting, Smelting & Recipes](#crafting-smelting--recipes)
  - [Crafting Grids & Principles](#crafting-grids--principles)
  - [Comprehensive Crafting Recipes Directory](#comprehensive-crafting-recipes-directory)
  - [Essential Tool & Weapon Progression](#essential-tool--weapon-progression)
  - [Armor Sets & Defense Ratings](#armor-sets--defense-ratings)
  - [Smelting & Fuel Efficiency](#smelting--fuel-efficiency)
- [Settings & Configuration Guide](#settings--configuration-guide)
- [World Seeds & Launch Flags](#world-seeds--launch-flags)
- [Engine Architecture & Technical Deep Dive](#engine-architecture--technical-deep-dive)
  - [Rendering Pipeline & Memory Optimization](#rendering-pipeline--memory-optimization)
  - [Multithreaded Chunk Streaming & Scheduling](#multithreaded-chunk-streaming--scheduling)
  - [Coordinated Checkpoint Persistence & Fail-Safe Recovery](#coordinated-checkpoint-persistence--fail-safe-recovery)
  - [Standalone Stateless Demo vs. Installed Full Game](#standalone-stateless-demo-vs-installed-full-game)
  - [Asset Pipeline & Offline Thumbnail Previews](#asset-pipeline--offline-thumbnail-previews)
- [Source Code Structure](#source-code-structure)
- [Building from Source](#building-from-source)
  - [Prerequisites](#prerequisites)
  - [Windows Build (MSVC / Visual Studio)](#windows-build-msvc--visual-studio)
  - [Linux Build](#linux-build)
  - [Official Packaging Script](#official-packaging-script)
- [Version History & Release Milestones](#version-history--release-milestones)
- [Testing & Validation Harness](#testing--validation-harness)
- [Credits & Acknowledgments](#credits--acknowledgments)

---

## About Voxel Frontier

**Voxel Frontier** is an expansive, high-performance voxel sandbox game written from scratch in modern **C++17** and **OpenGL 3.3 Core Profile**. Drawing inspiration from classic Minecraft mechanics, the entire project—including its custom rendering engine, procedural generation, audio mixer, physics engine, entity AI, documentation, and regression tests—was created cooperatively through **ChatGPT Codex**.

### Core Engineering Tenets
1. **Zero External Runtime Dependencies**: Standalone release builds embed all compiled GLSL shaders, 365 pre-rendered 64×64 item thumbnails, 16×16 texture atlases, billboard mob sprites, and streaming OGG background music directly into a single self-contained binary. No external runtime assets or VC++ redistributable packages are required.
2. **Byte-for-Byte Backward Compatibility**: The world generator maintains **Generators V1 through V24 byte-for-byte**. Older worlds retain their exact terrain algorithms, seeds, geographic boundaries, and ungenerated chunk allocations without seam artifacts or terrain resets.
3. **Transactional Immutable Persistence**: Saves use an atomic, coordinated checkpoint architecture across world blocks (`VXW`), player data (`VPS`), inventories (`VXI`), entities (`VXM`), automation circuits (`VXA`), exploration progress (`VXE`), and village settlements (`VXV`). In the event of system power loss, a fail-safe dual-key recovery mechanism restores the previous verified checkpoint into an isolated recovery profile.
4. **Hardware Efficiency**: Engineered with custom 28-byte packed vertex layouts, greedy quad meshing, geometry-aware baked ambient occlusion (AO), frustum culling, and decoupled worker threads for terrain synthesis and meshing. Voxel Frontier scales smoothly from low-end hardware up to extreme **64-chunk render distances**.

---

## Latest Release: v2.20.0 — Villages & Living World

The **v2.20.0: Villages & Living World** release transforms static procedural structures into vibrant, living communities with persistent identities, daily schedules, dynamic barter economies, and player-guided public works.

* **Four Persistent Professions**: Farmers, Blacksmiths, Librarians, and Merchants each maintain five dedicated barter offers. Four offers are accessible upon discovery, while the fifth is unlocked by completing community projects.
* **Finite Stock & Dynamic Replenishment**: Barter stock is finite (4/4/4/4/2 purchases). Village inventories automatically restock after 300 active seconds in loaded terrain, consuming abstract village supplies generated by player trade, request completion, and construction.
* **Living NPC Routines**: Villagers follow realistic schedules driven by the 20-minute day/night cycle:
  * *Morning to Afternoon (0–480s)*: Commute to assigned workplaces and labor.
  * *Evening (480–700s)*: Gather around the town well/meeting spot to socialize.
  * *Night (700–1200s)*: Return home to rest.
  * *Weather & Danger Shelter*: Seek indoor shelter during rain/thunderstorms and flee from nearby hostile threats.
  * *Door Interaction*: NPCs autonomously open and shut wooden doors with synchronized collision updates.
* **Village Request Board**: Complete three rotating procedural tasks per village (item delivery, seed supply, public works assistance).
* **Community Public Works Projects**: Directly fund and build permanent town upgrades:
  * **Paved Public Roads**: Smooth four-cell stone pathways connecting village thoroughfares.
  * **Community Market Stall**: A fenced market pavilion with a storage barrel that unlocks the final tier of barter offers.
  * *Player Protection*: Contributions can be freely refunded prior to construction; building requires explicit `Sneak + Enter` confirmation.
* **Settlement Reputation Scale**: Your standing with each village spans five distinct tiers: **Untrusted**, **Neutral**, **Friendly**, **Trusted**, and **Honored**. High standing provides barter discounts; hostility or property damage imposes strict, attributable penalties.
* **Unified Checkpoint Integration**: Village states (`VLG1`) and NPC bindings (`VIN1`) are serialized directly into the transactional checkpoint engine.

---

## Core Gameplay Systems

### 1. Procedural Generation & World Architecture

Voxel Frontier's procedural terrain engine uses layered multi-octave coherent noise to shape realistic continents, oceans, mountain peaks, and cave networks.

```
       Y=319 ── Maximum Player Construction Ceiling
                ▲
                │   [Mountain Summits & Snowy Peaks]
                │   • Windswept Hills, Stony Peaks, Jagged Cliffs
                ▼
       Y=256 ── Natural World Generation Cap (Smooth Peak Compression)
                ▲
                │   [Surface Landscapes & Canopy Forests]
                │   • Plains, Flower Forests, Taiga, Jungles, Badlands
                ▼
       Y=64  ── Sea Level (Beaches, Rivers, Oceans, Swamps)
                ▲
                │   [Subterranean Stone Layers]
                │   • Dirt, Gravel, Granite, Diorite, Andesite Veins
                │   • Underground Rivers & Abandoned Mineshafts
                ▼
       Y=0   ── Deepslate Transition Boundary
                ▲
                │   [Deep Underground & Aquifers]
                │   • Deepslate, Lava Reservoirs, Ancient Ruins
                │   • Puzzle Chambers & Relic Vaults (V24)
                ▼
       Y=-64 ── Bedrock Floor (Indestructible World Barrier)
```

#### World Dimension Boundaries
* **Vertical Span**: 384 blocks, spanning from **Y = -64 to Y = 319**.
* **Sea Level**: Fixed at **Y = 64**.
* **Natural Generation Ceiling**: Natural surface terrain, vegetation, and structures are capped at **Y = 256** using smooth peak compression. Player building continues freely up to **Y = 319**.
* **Generator Versions**: 24 distinct generator iterations are fully preserved in engine code. Existing worlds retain their original generator (V1–V24); newly created worlds utilize **Generator V24** (Exploration Chambers & Relics) or **V23** (Connected Uplands & Lakes).

---

### 2. The 35 Biomes Directory

Surface biomes are categorized into five macro-climates based on normalized temperature and humidity fields, augmented by altitude cooling:

| Climate Zone | Normalized Temp | Biomes Included | Flora, Geology & Environmental Features |
|---|---|---|---|
| **Frozen** | 0.00 – 0.17 | Snowy Plains, Ice Spikes, Snowy Taiga, Frozen Ocean, Frozen River, Snowy Beach | Packed Ice pillars, Blue Ice sheets, falling snow, freezing surface water, accumulated snow layers. |
| **Cold** | 0.17 – 0.34 | Taiga, Old Growth Taiga, Cold Ocean, Stony Shore | Giant Spruce trees, Podzol ground cover, fern clusters, steep gravel and stone shorelines. |
| **Temperate** | 0.34 – 0.60 | Plains, Forest, Birch Forest, Dark Forest, Flower Forest, Meadow, Mountains, Windswept Hills | Oak and Birch woods, dense Dark Oak canopies with Giant Mushrooms, wild roses, dandelions, rolling foothills. |
| **Warm** | 0.60 – 0.80 | Jungle, Sparse Jungle, Swamp, Savanna, Savanna Plateau | Giant Jungle trees with climbable Vines, Bamboo groves, Acacia umbrella canopies, shallow Swamp pools with Lily Pads. |
| **Hot** | 0.80 – 1.00 | Desert, Desert Hills, Badlands, Wooded Badlands | Rolling sand dunes, Sandstone formations, multi-colored Terracotta strata bands, Cacti, Dead Bushes. |
| **Special & Coastal** | Variable | Mushroom Fields, Ocean, Deep Ocean, Warm Ocean, Beach, River, Stony Peaks, Snowy Slopes | Rare offshore Mycelium islands immune to hostile mob spawns, coral reefs, downhill river drainage systems. |

---

### 3. Living Villages & Settlement Economy

Villages generate naturally in Plains and Desert biomes, featuring houses, libraries, blacksmith shops, farms, and central wells.

#### Villager Professions & Barter Catalog
Right-click any adult villager to open the interaction dialog. Sneaking (`Left Shift`) bypasses the interface to place or interact with blocks behind them.

```
┌────────────────────────────────────────────────────────┐
│  VILLAGE: Oakridge (Friendly)         SUPPLIES: 142/256│
├──────────────┬──────────────┬──────────────┬───────────┤
│ [1] OFFERS   │ [2] REQUESTS │ [3] PROJECTS │ [4] INFO  │
├──────────────┴──────────────┴──────────────┴───────────┤
│  PROFESSION: Blacksmith (Tier 1-4 Available, Stall Lck) │
│                                                        │
│  [Trade 1]  16 Coal        ───►  2 Iron Ingots  [4/4]  │
│  [Trade 2]  12 Copper      ───►  2 Iron Ingots  [4/4]  │
│  [Trade 3]   4 Iron Ingots ───►  1 Iron Shovel  [4/4]  │
│  [Trade 4]   6 Iron Ingots ───►  1 Iron Pickaxe [4/4]  │
│  [Trade 5]   6 Gold Ingots ───►  1 Diamond      [LOCKED]│
└────────────────────────────────────────────────────────┘
```

* **Farmer**:
  * `20 Wheat` → `4 Bread`
  * `16 Carrots` → `8 Potatoes` *(or 6 Beetroot)*
  * `8 Eggs` → `4 Seeds`
  * `2 Iron Ingots` → `5 Cooked Beef`
  * *Market Stall Unlock*: `1 Gold Ingot` → `4 Pumpkin Seeds`
* **Blacksmith**:
  * `16 Coal` → `2 Iron Ingots`
  * `12 Copper Ingots` → `2 Iron Ingots`
  * `4 Iron Ingots` → `1 Iron Shovel`
  * `6 Iron Ingots` → `1 Iron Pickaxe`
  * *Market Stall Unlock*: `6 Gold Ingots` → `1 Diamond`
* **Librarian**:
  * `24 Paper` → `2 Books`
  * `32 Paper` → `8 Torches`
  * `4 Books` → `1 Empty Map`
  * `3 Gold Ingots` → `1 Surveyor Token`
  * *Market Stall Unlock*: `4 Gold Ingots` → `1 Cavern Shard`
* **Merchant**:
  * `48 Cobblestone` → `8 Oak Logs`
  * `16 Logs` → `4 Cooked Beef`
  * `2 Iron Ingots` → `4 Coal`
  * `3 Iron Ingots` → `1 Oak Boat`
  * *Market Stall Unlock*: `2 Gold Ingots` → `1 Empty Map`

#### Settlement Reputation Tiers
* **Untrusted** (< -100): Villagers distrust you. Barter prices of 8 or more items increase by +2.
* **Neutral** (-100 to +100): Standard barter prices.
* **Friendly** (+100 to +300): Friendly discounts. Eligible prices decrease by 1 item.
* **Trusted** (+300 to +600): Respected patron status.
* **Honored** (> +600): Hero of the village. Eligible barter prices decrease by 2 items (maximum discount).

---

### 4. Exploration, Relics & Adventure

Version 2.19.0 introduces rich dungeon exploration, ancient puzzle chambers, and collectible relic artifacts.

#### The Adventure Journal (`J`)
Press **`J`** (configurable in Controls) at any time to open the personal per-world Adventure Journal:
* **Discoveries**: Tracks all visited biomes (out of 35), underground mineshafts, monster dungeons, ancient ruins, and puzzle chambers.
* **Relic Progress**: Shows unlocked ancient vaults, discovered relic caches, and active chamber puzzle states.
* **Challenges & Milestones**: Records completed village requests, funded public works, survived ambushes, and exploration achievements with one-time experience rewards.

#### Generator V24 Underground Chambers
Subterranean cave networks conceal three ancient puzzle chamber archetypes:
1. **Three Bells Chamber**: A musical sequence memory puzzle. Players must ring the bells in the correct order indicated by golden floor glyphs.
2. **Divided Current Chamber**: An electrical logic routing challenge requiring the player to complete an AND-gate circuit using redstone power wires and switches.
3. **Pulse Hall**: A high-stakes timing gauntlet. Pressure plates trigger timed pulse waves; players must navigate the chamber across safe stone tiles while avoiding charged red floor sections.
4. **Vault Encounters**: Guarded by finite Pillager ambushes or pulse traps. Solving chambers grants access to **Reinforced Ancient Vaults** containing diamonds, rare ores, enchanted relics, and **Resonant Cores**.

#### Powerful Ancient Relic Tools
* **Surveyor Compass**: Crafted with a Surveyor Token, copper, and iron. Its needle detects and directs the player toward nearby undiscovered underground chambers and structures without forcing distant chunk generation.
* **Echo Lantern**: Crafted with a Cavern Shard. Places persistent subterranean illumination. Sneak + Right-Click fires a loaded-only sonic echo pulse that reveals unseen adjacent caverns.
* **Ancient Explorer Tool**: An elite universal multi-tool crafted by combining an **Ancient Seal**, **Resonant Core**, **Iron Pickaxe**, and **Diamonds**. Features **1,024 durability**, tier-3 iron mining capability, and 5.0 attack damage.
* **Reusable Ancient Seals**: Found in deep ruins or crafted through expensive late-game recipes. Serves as a permanent key that unlocks ancient vaults without consuming the seal.

---

### 5. Logic, Automation & Mechanical Engineering

Voxel Frontier includes a fully deterministic, bounded digital logic and industrial automation suite (v2.17.0). Circuits operate with 0–15 signal strength levels and run synchronously with the world clock.

```
[Daylight Sensor] ──► [Power Wire (Signal: 15)] ──► [Repeater (Delay: 2 Ticks)] ──► [Piston]
                                                                                       │
[Chest (Storage)] ◄── [Directional Hopper] ◄── [Auto-Harvester] ◄──────────────────────┘
```

#### Automation Components
* **Power Wire**: Low-profile surface conduit that transmits signals up to 15 blocks. Signal attenuates by 1 per block travelled.
* **Power Sources**:
  * **Lever**: Toggleable switch providing constant signal 15.
  * **Stone & Wood Buttons**: Momentary switches providing pulses (10 ticks for stone, 15 ticks for wood).
  * **Pressure Plates**: Weighted floor plates activated by player, mob, or dropped item presence.
  * **Daylight Sensor**: Outputs signal proportional to sun angle and sky exposure.
  * **Crop Sensor**: Emits a redstone pulse when an adjacent crop reaches its final mature growth stage.
  * **Observer**: High-speed block update detector; fires a 1-tick pulse upon observing state changes in the block it faces.
* **Logic Gates**:
  * **Repeater**: Boosts signal back to 15 and introduces adjustable delays (1 to 4 ticks).
  * **NOT Gate (Inverter)**: Reverses input signal state.
  * **AND Gate**: Outputs signal 15 only when both designated inputs are active.
  * **OR Gate**: Outputs signal 15 when either input is active.
* **Actuators & Mechanics**:
  * **Power Lamp**: Controllable light source (emits light level 15 when powered).
  * **Piston & Sticky Piston**: Mechanically pushes up to 12 contiguous blocks. Sticky pistons pull the adjacent block back when retracted.
  * **Directional Hopper**: 27-slot item conduit that pulls items from containers above and injects them into targeted containers (chests, furnaces) along its nozzle. Powered hoppers freeze item transfer.
  * **Auto-Harvester**: When powered, automatically harvests ripe crops in front of it and replants matching seeds from its internal hopper buffer.

---

### 6. Architecture, Building & Decoration

Over 142 placeable architectural pieces and decorative materials provide unprecedented building flexibility (v2.16.0).

#### Architectural Blocks
* **Six Wood Families**: Oak, Birch, Spruce, Jungle, Acacia, and Dark Oak. Each includes logs, stripped logs, planks, leaves, doors, trapdoors, fences, gates, slabs, stairs, buttons, pressure plates, and signs.
* **Corner Stairs & Slabs**: Stairs dynamically form exterior and interior corner joints based on neighboring stair orientations. Slabs placed in the upper half of a block occupy top alignment; placing two matching slabs in one block space merges them into a solid block.
* **Connected Fences, Walls & Glass Panes**: Procedurally connect to adjacent solid blocks and identical elements with seamless corner posts and rail connections.
* **Masonry & Stonecraft**: Stone Bricks, Mossy Stone Bricks, Cracked Stone Bricks, Chiseled Stone Bricks, Deepslate Bricks, Smooth Stone, Cut Sandstone, and Roof Shingles.

#### Furniture & Interior Decoration
* **Furniture**: Functional wooden Tables, Chairs, Benches, and Shelves.
* **Beds**: Sleep to skip the night and set an authoritative, safely validated player respawn coordinate. Colored with wool dyes.
* **Storage Barrels**: 27-slot wooden storage alternative that opens even when a solid block is placed directly above it.
* **Lighting**: Wall-mounted and hanging Lanterns, Chains, decorative Candles, and Campfires.
* **Wall Displays**: Framed Paintings (featuring original pixel art in multiple dimensions) and Item Frames (displays 3D models of placed items).
* **Flower Pots & Carpets**: Display potted saplings and flowers; place 1/16-height wool carpets in multiple color variants.
* **Scaffolding**: Fast climbing structure. Press `Space` inside scaffolding to ascend; hold `Left Shift` to descend safely.
* **Multiline Signs**: Placeable wall and standing signs with real-time in-game text editing (4 lines, caret navigation, clipboard copy-paste with `Ctrl+V`, newline with `Enter`, save with `Ctrl+Enter`).

---

### 7. Survival, Farming, Food & Wildlife

Survival mode challenges players to manage health, hunger, stamina, and combat threats.

```
Health: [ ♥ ♥ ♥ ♥ ♥ ♥ ♥ ♥ ♥ ♥ ] (20 HP)
Hunger: [ 🍗 🍗 🍗 🍗 🍗 🍗 🍗 🍗 🍗 🍗 ] (20 Points) + Saturation Buffer
Armor:  [ 🛡 🛡 🛡 🛡 🛡 🛡 🛡 🛡 🛡 🛡 ] (Up to 20 Armor Points)
```

#### Nutrition & Saturation System
The hunger bar drains through sprinting, jumping, mining, and taking damage. When hunger is at 18 or higher, health naturally regenerates. When hunger reaches 0, starvation inflicts damage. Foods provide both base hunger restoration and invisible **saturation buffers**:

| Food Item | Hunger Restored | Saturation Value | Source / Preparation |
|---|---|---|---|
| **Cooked Beef** | 8.0 HP (4 drumsticks) | 12.8 | Smelt Raw Beef in Furnace |
| **Cooked Porkchop** | 8.0 HP (4 drumsticks) | 12.8 | Smelt Raw Pork in Furnace |
| **Cooked Mutton** | 6.0 HP (3 drumsticks) | 9.6 | Smelt Raw Mutton in Furnace |
| **Cooked Chicken** | 6.0 HP (3 drumsticks) | 7.2 | Smelt Raw Chicken in Furnace |
| **Baked Potato** | 5.0 HP (2.5 drumsticks) | 6.0 | Smelt Potato in Furnace |
| **Bread** | 5.0 HP (2.5 drumsticks) | 6.0 | Craft 3 Wheat in row |
| **Pumpkin Pie** | 8.0 HP (4 drumsticks) | 4.8 | Craft Pumpkin + Sugar + Egg |
| **Mushroom Stew** | 6.0 HP (3 drumsticks) | 7.2 | Craft Red + Brown Mushroom + Bowl |
| **Beetroot Soup** | 6.0 HP (3 drumsticks) | 7.2 | Craft 5 Beetroots + Bowl |
| **Vegetable Stew** | 6.0 HP (3 drumsticks) | 7.2 | Craft Carrot + Potato + Beetroot + Bowl |
| **Apple** | 4.0 HP (2 drumsticks) | 2.4 | Oak leaf drops |
| **Carrot** | 3.0 HP (1.5 drumsticks) | 3.6 | Harvested from Farmland |
| **Melon Slice** | 2.0 HP (1 drumstick) | 1.2 | Broken from Melon block |
| **Sweet Berries** | 2.0 HP (1 drumstick) | 1.0 | Harvested from Sweet Berry Bush |

#### Agriculture & Husbandry
* **Tilling & Hydration**: Right-click dirt or grass with any Hoe to create Farmland. Farmland stays hydrated within 4 blocks of water. Rain hydrates all exposed farmland automatically, boosting crop growth by 25%.
* **Seven Crop Varieties**: Wheat, Carrots, Potatoes, Beetroot, Pumpkins, Melons, and Sweet Berries. Immature crops grow through 4 visual stages.
* **Composting**: Fill a wooden Composter with excess seeds, leaves, or plants to produce **Bone Meal**. Bone Meal instantly accelerates crop growth.
* **Animal Husbandry**:
  * Breed Cows and Sheep with Wheat.
  * Breed Pigs with Carrots or Potatoes.
  * Breed Chickens with Seeds.
  * Shear adult sheep using craftable Shears to gather Wool without killing them.
  * Milk cows using an Iron Bucket to obtain Milk.

---

### 8. Transportation & Cartography

Traverse vast continental landmasses and map your discoveries with dedicated navigation tools (v2.14.0).

#### Craftable Wooden Boats
Crafted from 5 matching planks in an open U-shape. Available in 6 wood varieties (Oak, Birch, Spruce, Jungle, Acacia, Dark Oak).
* **Physics & Buoyancy**: Real-time water buoyancy, hydrodynamic drag, steering momentum, and collision impact physics.
* **Controls**: Right-click to board. Use `W`/`S` for acceleration/reverse and `A`/`D` to steer. Press `Left Shift` to disembark safely on solid ground. Attack an empty boat to recover it as an inventory item.

#### Cartography & Survey Maps (`M`)
* **Crafting Maps**: Craft a Compass with 4 Iron Ingots around a Copper Ingot. Surround the Compass with 8 sheets of Paper to produce an **Empty Map**.
* **Progressive Surveying**: Right-click an Empty Map to initialize a 128×128 pixel local survey. Holding the map in your main hand or offhand surveys surrounding terrain in real time.
* **Interactive Map Viewer (`M`)**: Press **`M`** to open the full-screen map interface. Press **`F6`** to refresh surveyed pixels from loaded terrain.
* **Zooming**: Sneak + Right-Click with 8 sheets of Paper in your inventory expands the map scale by one tier (1:1, 1:2, 1:4, 1:8, up to 1:16 coverage).

#### Waypoint Beacon System (`N`)
Right-click a Compass or press **`N`** to open the per-world Waypoint Manager:
* Create up to 64 custom named waypoints with coordinate coordinates.
* Toggle visual screen-edge directional indicator arrows.
* Press `Home` to quickly target the original world spawn point.

---

### 9. Dynamic Weather & Atmosphere

Voxel Frontier features an atmospheric weather engine (v2.10.0) synchronized across world time and climate zones.

* **Weather Cycles**: Weather transitions naturally between Clear (lasting 4–12 minutes), Rain (2–5 minutes), and Thunderstorms (1–3 minutes).
* **Biome-Specific Precipitation**: Hot biomes (Deserts, Savannas, Badlands) remain dry with cloud cover; cold biomes experience snowfall with real-time snow layer accumulation (up to 8 layers).
* **Acoustics & Sound Delay**: Thunder audio propagates at real-world acoustic speed (`delay = distance / 343 meters per second`), with near strikes producing crisp cracks and distant strikes producing rumbling roars.
* **Lightning & Protection**: Lightning strikes set temporary fires and inflict 5 HP to unprotected entities. Craft and place a vertical **Lightning Rod** (3 copper ingots) to divert strikes within a 32-block radius.

---

### 10. Quality of Life & Inventory Management

Engineered with modern conveniences that eliminate tedious inventory management (v2.16.2).

* **Inventory & Container Sorting**: Click the **Sort** button in player inventory or chest interfaces to organize items by category and ID.
* **Bulk Transfer Shortcuts**:
  * **Quick Stack**: Instantly transfers items from player inventory that already exist in the open container.
  * **Deposit All**: Deposits all inventory items (excluding hotbar) into the container.
  * **Take All**: Empties container contents into the player inventory.
* **Hotbar Lock Protection (`Alt + Click`)**: `Alt + Click` any hotbar slot to place a golden lock on it. Locked slots are completely immune to sorting, quick-stacking, and bulk deposits.
* **Reverse Recipe Lookup (`U`)**: Hover your mouse over any item in your inventory or a container and press **`U`** to instantly open the Recipe Book filtered to every recipe that consumes that item.
* **Interactive Recipe Book**: Built-in search bar, 5 category tabs, missing ingredient indicators, and `Shift + Click` batch crafting.
* **Creative Mode Catalog**: 6 categorized tabs (Building, Decoration, Redstone/Logic, Transportation, Miscellaneous, All), text search, item favorites (`Alt + RMB`), and recent history.
* **Textured Placement Ghosts**: Placing stairs, slabs, doors, or furniture displays a translucent preview showing orientation and connection geometry before confirming placement.
* **Adjustable UI Scaling**: Select between **Auto**, **Small**, **Normal**, and **Large** UI scales under Video Settings.

---

## Mobs & Wildlife Encyclopedia

Voxel Frontier features 10 distinct entity types, each with custom AI behaviors, physics hitboxes, and audio profiles.

```
Passive / Wildlife:  [Cow]  [Pig]  [Sheep]  [Chicken]
Village & Outpost:   [Villager]  [Pillager]
Billboard Hostiles:  [Hitori Gotoh]  [Kita Ikuyo]  [Nijika Ijichi]  [Ryo Yamada]
```

### Passive Wildlife
* **Cow**: 10 HP. Spawns in grassy biomes. Drops Raw Beef and Leather. Breeds with Wheat. Milked with Bucket.
* **Pig**: 10 HP. Spawns in plains and forests. Drops Raw Porkchop. Breeds with Carrots and Potatoes.
* **Sheep**: 8 HP. Spawns in various wool colors. Drops Raw Mutton and Wool. Sheared with Shears. Breeds with Wheat.
* **Chicken**: 4 HP. Lays Eggs periodically. Drops Raw Chicken and Feathers. Breeds with Seeds. Slow-falling flight prevents fall damage.

### Settlement NPCs & Raiders
* **Villager**: 20 HP. Peaceful inhabitants of villages. Participates in barter trade, public works, and daily routines. Flee from hostiles.
* **Pillager**: 24 HP. Hostile raiders spawning in Pillager Outposts and subterranean Monster Dungeons. Wields ranged crossbows and patrols territory.

### The Four Original Billboard Hostiles
Four unique hostile mobs inspired by *Bocchi the Rock!*, rendered as camera-facing 1.75-block pixel-art billboard sprites with custom behaviors:

| Hostile Mob | Health | Attack Damage | Movement Speed | Behavior & Combat Tactics | Notable Drops |
|---|---|---|---|---|---|
| **Hitori Gotoh** | 20 HP | Variable (Explosive: up to 25 HP) | Walk: 1.5 b/s<br>Chase: 4.5 b/s | **Silent Explosive Stalker (Creeper analogue)**. Creeps silently toward visible Survival players. When within 3 blocks, begins a 1.5-second swelling fuse. Moving outside fuse radius cancels detonation. Explosion destroys blocks and deals heavy damage. Normal death plays `boowomp.ogg`. | Gunpowder, Coal |
| **Kita Ikuyo** | 20 HP | 2.0 HP (1 heart) | Walk: 2.0 b/s<br>Chase: 6.0 b/s | **Aggressive Melee Hunter**. Possesses exceptional 35-block detection range and retains memory of player's last known position. Charges in direct pursuit and delivers rapid melee strikes. | Red Flower, Gold Nuggets |
| **Nijika Ijichi** | 20 HP | 3.0 HP (1.5 hearts) | Walk: 1.5 b/s<br>Chase: 4.5 b/s | **Tactical Ranged Sniper**. Maintains a standoff distance of 8–16 blocks from the player. Fires gravity-affected arrows with ballistic trajectories. Repositions if the player closes distance. | Sticks, Arrows, Bones |
| **Ryo Yamada** | 20 HP | 2.0 HP (1 heart) | Walk: 2.0 b/s<br>Chase: 6.0 b/s | **Wall-Climbing Predator**. Navigates vertical cliffs and building walls effortlessly. Performs high-velocity lunges from ledges onto unsuspecting targets. | Weeds, Emeralds, Dirt |

* **Hostile Spawning Rules**: Billboard hostiles spawn naturally at light level 6 or below, with an active hostile cap of **18 mobs**. Mobs do not despawn upon sunrise unless slain.

---

## Complete Controls Reference

All keybindings can be customized from **Pause Menu → Settings → Controls**.

### Movement & Actions
| Input | Action | Context / Special Function |
|---|---|---|
| `W` `A` `S` `D` | Movement | Walk forward, left, backward, right |
| `Mouse` | Look / Aim | Pitch and yaw camera rotation |
| `Space` | Jump / Ascend | Swim upward in water; ascend ladders/scaffolding; double-tap in Creative to toggle Flight |
| `Left Ctrl` | Sprint | Increases movement speed (6.2 → 8.6 blocks/sec); boosts Creative flight speed |
| `Left Shift` | Sneak / Descend | Prevents falling off block ledges; swim downward; descend ladders/scaffolding; bypasses villager/block interaction |
| `Left Mouse` | Attack / Mine | Mine targeted voxel or strike mob with held weapon |
| `Right Mouse` | Use / Place / Interact | Place held block; open containers/furnaces; interact with villagers; hold with Shield to block |
| `F` | Swap Offhand | Instantly swaps currently selected hotbar stack with offhand slot |
| `Middle Mouse` / `R` | Pick Block | Samples targeted block from inventory into active hotbar slot |
| `1` – `9` / `Scroll Wheel` | Select Hotbar | Switches active hotbar slot (1 through 9) |
| `Q` | Drop Item | Drops single item from active slot |
| `Ctrl + Q` | Drop Full Stack | Drops entire stack from active slot |

### Interfaces & Tools
| Input | Action | Context / Special Function |
|---|---|---|
| `E` | Inventory | Toggle player inventory / Recipe Book |
| `U` | Recipes Using Item | Hover any item in inventory/chest and press `U` to view recipes consuming it |
| `J` | Adventure Journal | Opens exploration discovery log, relic progress, and world achievements |
| `M` | Map Interface | Opens full-screen cartography map viewer (when holding a filled map) |
| `N` | Waypoint Manager | Opens custom waypoint and beacon navigation overlay |
| `/` | Open Command Console | Opens chat input with `/` prefix pre-populated |
| `Esc` | Pause / Back | Pauses gameplay, closes active UI menus |
| `G` | Toggle Fullbright | Toggles full ambient light override (independent of brightness slider) |
| Hold `C` + `Scroll` | Smooth Zoom | Cinematic FOV zoom lens (mouse wheel adjusts magnification) |
| `F2` | Screenshot | Captures uncompressed BMP screenshot to user data directory |
| `F3` | Debug Overlay | Displays coordinates, biome, chunk, FPS, draw calls, VRAM, and automation diagnostics |
| `F4` | Worldgen Diagnostics | Shows detailed multi-noise climate, temperature, and terrain elevation values |
| `F6` | Refresh Map | Force-refreshes survey map pixels from loaded terrain columns |

### Inventory & UI Shortcuts
| Input | Action |
|---|---|
| `Shift + Left Click` | Instantly moves clicked stack between inventory and container / armor slot |
| `Left Click + Drag` | Distributes held stack evenly across dragged inventory slots |
| `Right Click + Drag` | Deposits exactly one item from held stack into each dragged slot |
| `Alt + Left Click` | Toggles Hotbar Lock on clicked slot (protects from auto-sorting and bulk deposits) |
| `Alt + Right Click` | Toggles item Favorite status in Creative mode catalog |

---

## In-Game Console Commands

Press **`/`** to open the in-game command terminal. Command autocompletion (`Tab`), history navigation (`Up`/`Down`), and coordinate relative offsets (`~`) are supported.

> **Note on Cheats**: Gameplay-modifying commands require **Cheats: Enabled** in the world settings. The standalone demo disallows cheat commands.

| Command Syntax | Description | Example Usage |
|---|---|---|
| `/help [command]` | Lists all available commands or displays syntax for a specific command. | `/help give` |
| `/gamemode <survival\|creative\|spectator>` | Changes player game mode (`s` and `c` are accepted aliases). | `/gamemode creative` |
| `/time set <day\|noon\|night\|midnight\|seconds>` | Sets current position in the 1,200-second day/night cycle. | `/time set noon` |
| `/time add <seconds>` | Advances world time by specified seconds. | `/time add 120` |
| `/weather <clear\|rain\|thunder> [seconds]` | Changes weather pattern for up to 86,400 seconds. | `/weather thunder 300` |
| `/weather query` | Displays active weather state and remaining duration. | `/weather query` |
| `/give [@s] <item> [count]` | Grants specified item (1 to 2,304 count). Respects stack limits. | `/give @s diamond_sword 1` |
| `/clear [item] [count]` | Clears all items or specified quantity from inventory. | `/clear cobblestone 64` |
| `/tp <x> <y> <z>` | Teleports player to absolute or relative (`~`) coordinates. | `/tp ~ 10 ~` |
| `/kill [@s]` | Slays current player entity (bypasses Creative immunity). | `/kill` |
| `/summon <mob> [x y z]` | Spawns mob entity at coordinates or 3 blocks along crosshair. | `/summon HitoriGotoh` |
| `/summon lightning_bolt [x y z]` | Strikes lightning bolt at target coordinates. | `/summon lightning_bolt ~ ~ ~` |
| `/seed` | Displays current world seed (allowed with Cheats disabled). | `/seed` |
| `/setblock <x> <y> <z> <block>` | Modifies a single block in a loaded chunk. | `/setblock ~ ~-1 ~ stone_bricks` |
| `/fill <x1> <y1> <z1> <x2> <y2> <z2> <block>` | Fills an inclusive bounding box (up to 4,096 blocks). | `/fill ~-2 ~-1 ~-2 ~2 ~-1 ~2 glass` |

---

## Crafting, Smelting & Recipes

### Crafting Grids & Principles
* **2×2 Player Grid**: Available anywhere via `E`. Produces planks, sticks, crafting tables, torches, slabs, and compressed blocks.
* **3×3 Crafting Table**: Placed workstation required for advanced tools, armor, machinery, furniture, and complex items.
* **Plank Equivalence**: Generic recipes accept mixed plank varieties. Specific wood architectural pieces require 6 matching wood planks.

### Comprehensive Crafting Recipes Directory

#### Basic Resources & Utility
| Ingredients / Grid Layout | Output Item | Grid Size |
|---|---|---|
| 1 Log (Any wood variety) | 4 Planks (Matching variety) | 2×2 / 3×3 |
| 2 Planks (vertically aligned) | 4 Sticks | 2×2 / 3×3 |
| 4 Planks (2×2 square) | 1 Crafting Table | 2×2 / 3×3 |
| 8 Cobblestone (ring around empty center) | 1 Furnace | 3×3 Table |
| 8 Planks (ring around empty center) | 1 Storage Chest | 3×3 Table |
| 7 Sticks (H-pattern) | 3 Ladders | 3×3 Table |
| 1 Coal or Charcoal + 1 Stick | 4 Torches | 2×2 / 3×3 |
| 3 Sugar Cane (horizontally aligned) | 3 Paper | 3×3 Table |
| 3 Paper + 1 Leather (shapeless) | 1 Book | 2×2 / 3×3 |
| 3 Books + 6 Planks (top/bottom rows) | 1 Bookshelf | 3×3 Table |
| 3 Copper Ingots (vertically aligned) | 1 Lightning Rod | 3×3 Table |
| 4 Clay Balls (2×2 square) | 1 Clay Block | 2×2 / 3×3 |
| 4 Snowballs (2×2 square) | 1 Snow Block | 2×2 / 3×3 |
| 9 Ingots / Gems / Wheat (3×3 square) | 1 Storage Block (Coal/Iron/Gold/Diamond/Hay) | 3×3 Table |
| 1 Storage Block | 9 Raw Ingots / Gems / Wheat | 2×2 / 3×3 |

#### Tools, Weapons & Combat
| Ingredients / Grid Layout | Output Item | Grid Size |
|---|---|---|
| 3 Material + 2 Sticks (T-shape) | 1 Pickaxe (Wood/Stone/Iron/Gold/Diamond) | 3×3 Table |
| 3 Material + 2 Sticks (corner axe shape) | 1 Axe (Wood/Stone/Iron/Gold/Diamond) | 3×3 Table |
| 1 Material + 2 Sticks (vertical column) | 1 Shovel (Wood/Stone/Iron/Gold/Diamond) | 3×3 Table |
| 2 Material + 1 Stick (vertical column) | 1 Sword (Wood/Stone/Iron/Gold/Diamond) | 3×3 Table |
| 2 Material + 2 Sticks (L-shape) | 1 Hoe (Wood/Stone/Iron/Gold/Diamond) | 3×3 Table |
| 2 Iron Ingots (diagonal) | 1 Shears | 2×2 / 3×3 |
| 3 Iron Ingots (V-shape) | 1 Bucket | 3×3 Table |
| 6 Planks + 1 Iron Ingot (Y-shape around center) | 1 Shield | 3×3 Table |

#### Armor Sets
| Ingredients / Grid Layout | Output Item | Grid Size |
|---|---|---|
| 5 Material (inverted U-pattern) | 1 Helmet (Leather/Gold/Iron/Diamond) | 3×3 Table |
| 8 Material (all cells except top-center) | 1 Chestplate (Leather/Gold/Iron/Diamond) | 3×3 Table |
| 7 Material (pants arch pattern) | 1 Leggings (Leather/Gold/Iron/Diamond) | 3×3 Table |
| 4 Material (two 2-high columns) | 1 Boots (Leather/Gold/Iron/Diamond) | 3×3 Table |

#### Transportation & Navigation
| Ingredients / Grid Layout | Output Item | Grid Size |
|---|---|---|
| 5 Matching Planks (open U-shape) | 1 Boat (Oak/Birch/Spruce/Jungle/Acacia/Dark Oak) | 3×3 Table |
| 4 Iron Ingots + 1 Copper Ingot (cross around center) | 1 Compass | 3×3 Table |
| 8 Paper + 1 Compass (ring around center) | 1 Empty Map | 3×3 Table |

#### Logic & Automation
| Ingredients / Grid Layout | Output Item | Grid Size |
|---|---|---|
| 1 Copper Ingot + 1 Coal | 8 Power Wires | 2×2 / 3×3 |
| 1 Stick + 1 Cobblestone | 1 Lever | 2×2 / 3×3 |
| 1 Wood Plank / 1 Stone | 1 Button (Wood / Stone) | 2×2 / 3×3 |
| 2 Wood Planks / 2 Stone (horizontal) | 1 Pressure Plate (Wood / Stone) | 2×2 / 3×3 |
| 2 Torches + 1 Power Wire + 3 Stone | 1 Repeater | 3×3 Table |
| 1 Repeater + 2 Power Wires + 1 Stone | 1 AND Gate | 2×2 / 3×3 |
| 2 Power Wires + 1 Repeater + 1 Stone | 1 OR Gate | 2×2 / 3×3 |
| 1 Torch + 1 Power Wire + 1 Stone | 1 NOT Gate | 3×3 Table |
| 4 Glass + 2 Torches + 2 Power Wires + 1 Copper | 1 Power Lamp | 3×3 Table |
| 5 Iron Ingots + 1 Storage Chest (V-shape around chest) | 1 Directional Hopper | 3×3 Table |
| 3 Planks + 4 Cobble + 1 Iron + 1 Power Wire | 1 Piston | 3×3 Table |
| 1 Sweet Berries + 1 Piston | 1 Sticky Piston | 2×2 / 3×3 |
| 3 Glass + 1 Power Wire + 1 Copper + 1 Wood Slab | 1 Daylight Sensor | 3×3 Table |
| 2 Cobble + 1 Glass + 2 Power Wires + 1 Iron | 1 Observer | 3×3 Table |
| 1 Wheat + 1 Carrot + 1 Power Wire + 1 Glass | 1 Crop Sensor | 2×2 / 3×3 |
| 5 Iron + 2 Power Wires + 1 Hopper + 2 Cobblestone | 1 Auto-Harvester | 3×3 Table |

#### Ancient Relics & Chambers
| Ingredients / Grid Layout | Output Item | Grid Size |
|---|---|---|
| 4 Gold Ingots + 4 Paper + 1 Compass | 1 Surveyor Token (Paid Acquisition) | 3×3 Table |
| 4 Copper + 4 Glass + 1 Diamond | 1 Cavern Shard (Paid Acquisition) | 3×3 Table |
| 4 Gold + 3 Books + 2 Diamonds | 1 Ancient Seal (Paid Acquisition) | 3×3 Table |
| 2 Copper + 1 Surveyor Token + 2 Iron + 1 Compass + 1 Gold | 1 Surveyor Compass | 3×3 Table |
| 2 Iron + 1 Cavern Shard + 2 Glass + 1 Torch + 1 Gold | 1 Echo Lantern | 3×3 Table |
| 2 Copper + 2 Cavern Shards + 2 Gold + 1 Iron Ingot | 1 Resonant Core | 3×3 Table |
| 2 Diamonds + 1 Ancient Seal + 1 Iron Pickaxe + 1 Resonant Core | 1 Ancient Explorer Tool | 3×3 Table |

---

### Essential Tool & Weapon Progression

| Tool / Weapon Tier | Harvest Level | Durability | Attack Damage | Attack Cooldown | Mining Multiplier |
|---|---|---|---|---|---|
| **Wood** | Tier 1 (Coal, Stone) | 60 uses | Sword: 4.0 HP / Axe: 6.0 HP | 0.625s / 0.85s | 2.0× |
| **Stone** | Tier 2 (Iron, Copper, Lapis) | 132 uses | Sword: 5.0 HP / Axe: 8.0 HP | 0.625s / 0.85s | 4.0× |
| **Iron** | Tier 3 (Gold, Redstone, Diamond) | 251 uses | Sword: 6.0 HP / Axe: 10.0 HP | 0.625s / 0.85s | 6.0× |
| **Gold** | Tier 1 (Fast, Fragile) | 33 uses | Sword: 4.0 HP / Axe: 6.0 HP | 0.625s / 0.85s | 12.0× |
| **Diamond** | Tier 4 (Obsidian, Ancient Debris) | 1,561 uses | Sword: 7.0 HP / Axe: 12.0 HP | 0.625s / 0.85s | 8.0× |
| **Ancient Explorer Tool** | Tier 3 (Universal Multi-Tool) | 1,024 uses | 5.0 HP | 0.58s | 7.0× |

---

### Armor Sets & Defense Ratings

Armor points mitigate incoming physical damage linearly: each armor point grants 4% damage reduction (up to 80% maximum reduction with a full Diamond set).

| Armor Material | Helmet Durability | Chestplate Durability | Leggings Durability | Boots Durability | Full Set Protection |
|---|---|---|---|---|---|
| **Leather** | 55 | 80 | 75 | 65 | 7 Armor Points (28% Reduction) |
| **Gold** | 77 | 112 | 105 | 91 | 11 Armor Points (44% Reduction) |
| **Iron** | 165 | 240 | 225 | 195 | 15 Armor Points (60% Reduction) |
| **Diamond** | 363 | 528 | 495 | 429 | 20 Armor Points (80% Reduction) |

---

### Smelting & Fuel Efficiency

Furnaces smelt raw ores, cook food, and produce refined materials. Smelting takes **5.0 seconds** per item.

| Fuel Material | Burn Time (Seconds) | Items Smelted per Unit |
|---|---|---|
| **Coal / Charcoal** | 40.0 seconds | 8 items |
| **Coal Block** | 400.0 seconds | 80 items |
| **Wood Planks / Logs** | 15.0 seconds | 3 items |
| **Wooden Tools / Sticks** | 5.0 seconds | 1 item |

#### Key Smelting Conversions
* `Cobblestone` → `Stone`
* `Sand` → `Glass`
* `Clay Ball` → `Brick`
* `Iron / Gold / Copper Ore` → `Iron / Gold / Copper Ingot`
* `Log (Any wood)` → `Charcoal`
* `Raw Beef / Pork / Mutton / Chicken` → `Cooked Variant`

---

## Settings & Configuration Guide

Global settings are stored in `%LOCALAPPDATA%\VoxelFrontier\voxel_settings.cfg` and persist across world sessions.

```
PAUSE MENU ──► SETTINGS ──┬──► [VIDEO]    (Render Distance, LOD, MSAA, UI Scale)
                          ├──► [AUDIO]    (Master, Music, SFX, Mobs, Weather)
                          ├──► [CONTROLS] (Key Remapping, Inverted Mouse, Sensitivity)
                          └──► [WEATHER]  (Quality, Snow Accumulation, Lightning)
```

### Video Settings
* **Render Distance**: 2 to 64 Chunks. (Default: 12 chunks).
* **Simulation Distance**: 2 to 32 Chunks. Governs mob AI, crop growth, and automation tick range.
* **Field of View (FOV)**: 30° to 110° (Default: 70°).
* **Brightness**: 0% to 100% (50% is standard; 100% is bright; Fullbright mode is toggled via `G`).
* **Graphics Presets**: Low / Medium / High / Custom.
* **Chunk Streaming**: Auto / Low / Balanced / High worker thread allocations.
* **Antialiasing (MSAA)**: Off / 2× / 4× multisample antialiasing.
* **UI Scale**: Auto / Small / Normal / Large.

### Audio Settings
* **Master Volume**: Master audio attenuation.
* **Music Volume**: Controls background piano and ambient soundtrack playback (Gain: `Master × Music × 0.30`).
* **Sound Effects (SFX)**: Block mining, placing, footsteps, tool breaking, door mechanics.
* **Passive Mobs**: Footsteps, cows, sheep, pigs, chickens.
* **Hostile Mobs**: Creeper fuses, bow twangs, mob pain/death sounds.
* **Weather Audio**: Positional thunderclaps, outdoor rainfall, roof dripping.

---

## World Seeds & Launch Flags

### Seed Management
* World seeds can be specified during world creation. Blank seed fields generate an automatic pseudo-random integer.
* Default baseline seed: **`20260917`**.
* The current world seed is saved in `world_seed.txt` within each world profile.

### Command-Line Arguments
Launch `VoxelFrontier.exe` with command-line flags to automate testing, benchmarks, or session modes:

```powershell
# Launch world with specific seed
.\VoxelFrontier.exe --seed=123456789

# Force initial game mode
.\VoxelFrontier.exe --survival
.\VoxelFrontier.exe --creative
.\VoxelFrontier.exe --spectator

# Automated world generation preview capture (renders overhead BMP and exits)
.\VoxelFrontier.exe --worldgen-test --seed=20260917 --preview-x=0 --preview-z=0

# Run automated stability regressions
.\VoxelFrontier.exe --survival-smoke --seed=20260917
.\VoxelFrontier.exe --command-smoke --seed=20260917
```

---

## Engine Architecture & Technical Deep Dive

```
┌─────────────────────────────────────────────────────────────────┐
│                    VOXEL FRONTIER ENGINE                        │
├───────────────────────────────┬─────────────────────────────────┤
│       MAIN THREAD             │      BACKGROUND WORKER POOL     │
│  • GLFW Input & Windowing     │  • Noise & Terrain Gen (V1-V24) │
│  • Player Simulation (120 Hz) │  • Underground Structure Plans  │
│  • Automation Logic & Ticks   │  • Asynchronous Meshing         │
│  • Coordinated Checkpoint Save│  • Biome & Climate Synthesis    │
│  • OpenGL 3.3 Core Renderer   │                                 │
└───────────────┬───────────────┴─────────────────┬───────────────┘
                │ Frame-Budgeted VBO/EBO Uploads  │
                ▼                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                     HARDWARE / GPU PIPELINE                     │
│  • Packed 28-Byte Vertices   • Greedy Quad Merging              │
│  • Baked Voxel AO (No SSAO)  • Dual-Pass Alpha Sorting          │
│  • Frustum & Occlusion Cull  • Embedded 64×64 Preview Atlases   │
└─────────────────────────────────────────────────────────────────┘
```

### Rendering Pipeline & Memory Optimization
* **Lossless 28-Byte Terrain Vertices**: Terrain vertices pack 3D position coordinates, normals, UV texture coordinates, packed sunlight/blocklight channels, and 2-bit baked ambient occlusion into exactly 28 bytes per vertex.
* **Greedy Quad Meshing**: Coplanar adjacent voxel faces with identical texture IDs, light levels, and AO parameters are merged into large single quads, reducing draw calls and vertex counts by up to 60%.
* **Geometry-Aware Voxel AO**: Ambient occlusion is precomputed per-vertex during chunk meshing by sampling adjacent solid block corners and partial block bounding boxes (stairs, slabs). No expensive SSAO shader pass is required.
* **Render Distance Scaling**: Configurable from **2 up to 64 chunks**. Dual-pass rendering cleanly separates opaque voxel geometry from alpha-blended transparent surfaces (water, tinted glass, ice).

### Multithreaded Chunk Streaming & Scheduling
* **Decoupled Worker Pools**: Terrain generation (noise, caves, structures) and chunk meshing (vertex generation) execute across separate thread pools scaled to CPU core count.
* **Frustum-Prioritized Streaming**: Chunks directly in front of the player's view vector receive top generation priority, minimizing visual pop-in during sprint or flight navigation.
* **Frame-Budgeted Uploads**: OpenGL buffer uploads occur exclusively on the main thread and adhere to strict millisecond frame budgets, preventing stutter during high-speed travel.

### Coordinated Checkpoint Persistence & Fail-Safe Recovery
* **Atomic Save Commits**: Saves are encoded from immutable background snapshots and committed atomically using staged directory renames. Worlds are never left in a corrupted half-written state.
* **Checksum Verification**: Every saved record incorporates CRC checksums. Checkpoint records include:
  * `AUT1`: Automation devices, signal states, and logic nodes.
  * `BDP1`: Placed structural entities, signs, and beds.
  * `ERL1`: Exploration discoveries, relic unlocks, and puzzle states.
  * `VLG1` & `VIN1`: Village settlement records, reputation, funding, and NPC associations.
* **Fail-Safe Checkpoint Recovery**: If a world save fails validation due to unexpected system shutdown, navigating to **Select World** and pressing **`R` twice within 5 seconds** restores the previous verified checkpoint into a separate recovery world (`world-<id>-recovered`).

### Standalone Stateless Demo vs. Installed Full Game
The codebase compiles into two distinct release configurations:
1. **Full Persistent Game (`Windows-Setup.exe`)**: Stores all worlds, settings, and logs under `%LOCALAPPDATA%\VoxelFrontier\`. Autosaves every 12 seconds and upon exit. Supports full command suites.
2. **Stateless Standalone Demo (`Windows-Standalone.exe`)**: A single standalone executable. Runs strictly in-memory without creating or reading saved world files. Cheat commands are disabled. Perfect for immediate zero-footprint gameplay previews.

### Asset Pipeline & Offline Thumbnail Previews
All 365 registered inventory items feature offline-rendered **64×64 pixel previews** packed into an embedded texture atlas. Blocks are rendered in an isometric orthographic projection sampling true material textures; tools and items feature shaded pixel silhouettes. The atlas is generated offline via `tools/generate_item_thumbnails.py`, eliminating runtime preview generation overhead.

---

## Source Code Structure

```
Codex-Minecraft/
├── assets/                     # Embedded 16x16 textures, audio, and icon manifests
│   ├── inventory_atlas_16.png  # Base pixel art item sprite atlas
│   ├── inventory_previews.json # 365 offline thumbnail geometry definitions
│   └── sounds/                 # OGG sound effects and background music tracks
├── docs/                       # Validation reports, audit logs, and release notes
├── include/                    # Public C++ header declarations
│   ├── Adventure.h             # Relic chambers, puzzles, and adventure journal
│   ├── Automation.h            # Redstone logic, wires, repeaters, pistons, hoppers
│   ├── Biome.h                 # 35 biome definitions, climate curves, foliage colors
│   ├── Block.h                 # Block enum, states, and 16-bit property bitmasks
│   ├── Building.h              # Slabs, stairs, furniture, signs, and decorations
│   ├── CommandSystem.h         # Console parser, tokenization, autocomplete
│   ├── Definitions.h           # Item and block registries, tools, fuel values
│   ├── InputManager.h          # GLFW input routing and custom keybindings
│   ├── Player.h                # First-person physics, collision, camera
│   ├── Renderer.h              # OpenGL 3.3 Core shader passes and buffer management
│   ├── SaveFile.h              # Atomic checkpoint serialization and recovery
│   ├── Survival.h              # Inventory, crafting recipes, health, hunger
│   ├── Villages.h              # Settlement state, barter trades, NPC routines
│   └── World.h                 # Chunk maps, voxel lighting, multi-noise generators
├── src/                        # C++ Implementation translation units
│   ├── main.cpp                # Program entry point and runtime flags
│   ├── Game.cpp                # Core application loop, frame timing, state machine
│   ├── World.cpp               # Terrain generation algorithms (V1-V24)
│   ├── Renderer.cpp            # Chunk mesh generation and draw dispatch
│   └── ...                     # Subsystem implementations
├── tests/                      # Automated stability, regression, and benchmark suites
│   ├── fixtures/               # Frozen historical generator hash tables
│   └── stability/              # Catch2-style native C++ regression tests
├── tools/                      # Offline asset generators and audit scripts
│   ├── generate_item_thumbnails.py
│   └── run_stability_regressions.ps1
├── CMakeLists.txt              # Primary CMake build script
├── package_release.ps1         # Automated release packaging pipeline
└── README.md                   # Project documentation
```

---

## Building from Source

### Prerequisites
* **C++17 Compiler**: MSVC (Visual Studio 2022 / 2026), GCC 9+, or Clang 10+
* **CMake**: Version 3.20 or newer
* **Graphics Driver**: OpenGL 3.3 Core Profile compatible GPU
* *Dependencies fetched automatically via CMake FetchContent*:
  * [GLFW 3.4](https://www.glfw.org/) (Windowing & Input)
  * [GLAD 0.1.36](https://glad.dav1d.de/) (OpenGL Loader)
  * [GLM 1.0.1](https://github.com/g-truc/glm) (Mathematics)
  * [miniaudio 0.11.25](https://miniaud.io/) (Audio Engine)

### Windows Build (MSVC / Visual Studio)
Open PowerShell from the repository root:

```powershell
# Configure standard build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Compile Release binary
cmake --build build --config Release --parallel 4

# Run executable
.\build\Release\VoxelFrontier.exe
```

### Linux Build
```bash
# Install system development headers
sudo apt-get update
sudo apt-get install -y build-essential cmake git xorg-dev libglu1-mesa-dev

# Configure and compile
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)

# Run executable
./build/VoxelFrontier
```

### Official Packaging Script
To build both the **Full Persistent Game Installer** and the **Stateless Standalone Demo** alongside verification checksums (`SHA256SUMS.txt`), install Inno Setup 7 and run:

```powershell
# Install Inno Setup 7 via winget (one-time setup)
winget install --id JRSoftware.InnoSetup.7 -e --scope user --silent

# Execute automated packaging pipeline
.\package_release.ps1 -BuildJobs 4
```
Compiled installers and standalone executables are output directly to `dist/`.

---

## Version History & Release Milestones

* **v2.20.0 — Villages & Living World**: Villager professions (Farmer, Smith, Librarian, Merchant), finite barter stock, 300s replenishment, daily NPC work/rest schedules, community projects (paved roads, market stall), village reputation tiers, and `VLG1`/`VIN1` save records.
* **v2.19.0 — Exploration & Relics**: Generator V24, Three Bells / Divided Current / Pulse Hall puzzle chambers, Surveyor Compass, Echo Lantern, Ancient Explorer Tool, reusable Ancient Seals, locked vaults, and in-game Adventure Journal (`J`).
* **v2.18.1 — Engine Optimization**: Exact V23 column reuse, bounded shared-buffer retirement, immutable background checkpoint encoding, coalesced mesh requests, and affinity-aware streaming worker pools.
* **v2.18.0 — World Generation & Biome Expansion**: Generator V23, connected mountain ranges, eroded uplands, expanded cave networks, underground lakes, mixed woodland, and 5 wilderness landmark structures.
* **v2.17.0 — Logic & Automation**: Digital redstone-equivalent circuits, directional power wires, logic gates (AND, OR, NOT), repeaters, levers, buttons, pressure plates, pistons, sticky pistons, hoppers, daylight/crop/observer sensors, and auto-harvesters.
* **v2.16.2 — Quality of Life**: Inventory and container sorting, Quick Stack, Deposit All, Take All, protected hotbar slots (`Alt + Click`), reverse recipe lookup (`U`), placement ghost previews, multiline signs, and UI scaling.
* **v2.16.1 — Inventory Icon & Visual Clarity**: Cached 64×64 thumbnail previews for all 347 items, 3D orthographic mesh rendering, and creative catalog search.
* **v2.16.0 — Building & Decoration**: 142 architectural items: 6 wood varieties, corner stairs, slabs, connected fences/walls/panes, furniture, beds with personal spawn points, barrels, lanterns, scaffolds, and carpets.
* **v2.15.0 — Farming, Food & Survival**: 7 crop varieties (wheat, carrot, potato, beetroot, pumpkin, melon, berries), hoe tilling, soil irrigation, composting, Bone Meal, animal breeding, shearing, milking, and saturation mechanics.
* **v2.14.0 — Transportation & Navigation**: 6 craftable wooden boats with authentic buoyancy and steering, compass pointing to world spawn, custom waypoints (`N`), and cartography survey maps (`M`) with zoom levels.
* **v2.13.0 — Equipment & Combat**: 4 armor tiers (Leather, Gold, Iron, Diamond), dedicated equipment slots, offhand slot (`F`), craftable shields with active RMB blocking, weapon attack cooldowns, and critical hits.
* **v2.11.0 — Biomes & Climate Overhaul**: 35 distinct biomes across 5 normalized climate zones, altitude cooling, climate-blended foliage, and 384-block height range (-64 to 319).
* **v2.10.0 — Atmospheric Weather**: Clear, rain, and thunderstorm weather cycles, climate-aware precipitation, realistic sound speed delays (`distance / 343`), snow accumulation, and lightning rods.

---

## Testing & Validation Harness

Voxel Frontier includes an exhaustive automated regression suite covering world generation determinism, save checkpoint integrity, item conservation, and performance benchmarking.

```powershell
# Run stability regression suite (includes both full persistent and demo builds)
.\tools\run_stability_regressions.ps1 -IncludeDemo

# Run focused validation suites for village and exploration systems
python tools/run_village_validation.py --phase focused --output build/agent-village-focused
python tools/validate_villages_v2200.py --phase functional --output build/agent-functional
```

* **Historical Baselines**: The test harness verifies current candidate builds against 1,326 historical hash records and 738 frozen generator fixtures. Generator output across all versions V1 through V24 remains guaranteed identical down to the individual block.

---

## Credits & Acknowledgments

* **Codebase & Engineering**: Created entirely with **ChatGPT Codex** in collaboration with project developers.
* **Windowing & Context**: [GLFW 3.4](https://www.glfw.org/)
* **OpenGL Initialization**: [GLAD 0.1.36](https://glad.dav1d.de/)
* **Mathematics**: [GLM 1.0.1](https://github.com/g-truc/glm)
* **Audio Library**: [miniaudio 0.11.25](https://miniaud.io/)
* **Packaging**: [Inno Setup 7](https://jrsoftware.org/isinfo.php)
* **Inspiration**: Built with admiration for Mojang's classic sandbox Minecraft and the anime *Bocchi the Rock!*.

---

<div align="center">
  <sub>Voxel Frontier is an independent, non-commercial educational project. All rights to third-party inspirations belong to their respective creators.</sub>
</div>
