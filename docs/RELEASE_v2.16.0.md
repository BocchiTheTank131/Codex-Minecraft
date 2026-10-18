# Voxel Frontier v2.16.0 — Building & Decoration

Released October 18, 2026.

## Building

- 142 additional block/item types, including six complete wood families.
- Oriented stairs with automatic inner/outer corners, upside-down stairs,
  top/bottom/double slabs, connected fences, walls and glass panes.
- Functional gates, paired doors, trapdoors, buttons and pressure plates.
- Eleven masonry shape families, decorative masonry and shingles.
- Axis-oriented logs, stripped logs, paths and climbable scaffolds.

## Homes and decoration

- Beds with sleeping, persistent personal respawn and safe fallback to world spawn.
- 27-slot barrels, tables, chairs, benches and shelves.
- Editable signs, four original paintings in two sizes, rotating item frames and
  flower pots. Displayed items retain durability and map identities.
- Lanterns, hanging lights, chains, lit/unlit candles and campfires.
- Four shared colors for beds, carpets and candles.
- Original 16×16 art, a paged Creative catalog and recipes for every new type.
- Creative uses inventory-sized slots across four pages. Selecting, shift-clicking
  and right-dragging catalog items now respects the displayed page.

## Polish fixes

- Carpets and other one-pixel collision surfaces can be walked onto without
  jumping, including while sneaking. Clearance checks prevent head clipping.
- Potted flowers use upright, double-sided crossed ground-texture planes and
  remain visible from the sides and back.
- New building icons are centered with a transparent margin; lower faces are
  no longer cut off. Older occupied icon cells are unchanged.

## Engine and compatibility

Stable 16-bit type/item IDs and validated block properties remove the old registry
limit. Ordinary terrain stays in one-byte cells; decorated chunks use local
palettes, with a wider fallback. Historical IDs, all generators V1–V22, the four
hostile billboard mobs and existing gameplay remain compatible.

New world/inventory/container/drop records are versioned and old formats remain
readable. Signs, bed respawns, containers and deferred recovery drops are saved
inside coordinated checkpoints. Keep backups before upgrading: older releases
cannot read saves written in the new formats.

## Deliberate limits

Furniture has collision but no sitting. Campfires provide decoration and light;
cooking/smoke, banners, full waterlogging, copy/paste and a separate stonecutter
are deferred. Doors and trapdoors use immediate toggles with sound. Sign text is
four ASCII lines of 24 characters, with up to eight nearby labels rendered.

## Downloads

- `VoxelFrontier-v2.16.0-Windows-Setup.exe`: full persistent game installer.
- `VoxelFrontier-v2.16.0-Windows-Standalone.exe`: stateless single-file demo.
- `SHA256SUMS.txt`: package checksums.

See [the validation report](BUILDING_DECORATION_VALIDATION.md) for measured results,
screenshots, migration evidence and remaining limitations.
