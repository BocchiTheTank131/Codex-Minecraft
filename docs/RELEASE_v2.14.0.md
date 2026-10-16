## The Transportation & Navigation Update

Explore V22 rivers, oceans and islands with six craftable wooden boats, progressive exploration maps, a spawn compass and per-world waypoints.

### Features
- Oak, Birch, Spruce, Jungle, Acacia and Dark Oak boats with acceleration, momentum, buoyancy, flowing-water support, collisions, mounting, safe dismounting and item recovery.
- Boats persist in installed worlds, with bounded simulation around loaded terrain.
- Empty and Filled Maps with unique identities, progressive exploration, five scales, player/facing markers and persistent survey data. Maps transfer through inventories, chests and dropped items.
- A craftable Compass points toward the original world spawn. Named waypoints have per-world selection, direction and distance displays.
- Original pixel-art icons and boat textures, crafting recipes and Creative entries.
- Generator V22 and all existing generators, terrain, billboard mobs, equipment and gameplay remain preserved. The standalone demo remains stateless.

### Controls
- Right-click a boat to mount; use movement controls to steer and Sneak to dismount. Break an unoccupied boat to recover it.
- Right-click an Empty Map to initialize it; M/right-click opens the held map. Sneak-right-click zooms out using eight Paper.
- N or right-click with a Compass opens waypoint management. The navigation editor lists its keyboard controls.

### Downloads
- **Windows-Setup.exe**: full game installer with persistent worlds.
- **Windows-Standalone.exe**: standalone demo with existing restrictions and stateless behavior.
- **SHA256SUMS.txt**: SHA-256 checksums for both downloads.

### Validation and limitations
Both Release variants built; all 13 regression suites passed in a fresh isolated run. All 663 checked historical terrain hashes matched. The packaged standalone demo passed transportation checks without creating persistent world or navigation data.

RD8/16/32 measurements showed no established general FPS speedup. A held map plus five or six boats added approximately 0.014–0.069 ms mean frame time in measured ocean views. Existing teleport stalls around 48 ms remain; physical low-end hardware was not available.

Optional stairs, fences, gates and trapdoors are deferred. Boats have one player seat. Map pixels retain surveyed observations rather than continuously refreshing terrain edits. One earlier rapid-save stress run failed to replace an inventory checkpoint; the prior save and complete temporary file were retained. Its cause remains unresolved, although the fresh isolated regression run passed.

See the [validation report](https://github.com/BocchiTheTank131/Codex-Minecraft/blob/v2.14.0/docs/TRANSPORTATION_VALIDATION.md) for screenshots, measurements, storage details and remaining limitations.
