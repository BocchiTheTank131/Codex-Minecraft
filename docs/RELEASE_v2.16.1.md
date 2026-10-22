# Voxel Frontier v2.16.1 — Inventory Icon & Visual Clarity

Release v2.16.1, approved for publication on October 22, 2026.

## Clearer inventory

- 347 cached 64×64 item previews, replacing the enlarged 16×16 runtime artwork.
- Textured three-quarter block previews using the actual terrain mesher, with
  correct tops/sides, stairs, slabs, connected fences/walls/panes, doors, beds,
  furniture and decorations.
- Miniature boats use the existing hull models and wood textures. Other items
  use shaded, extruded pixel-art silhouettes instead of generic cubes.
- Consistent framing, transparent gutters, alpha-correct edge filtering and
  reliable stable-ID mapping across the entire UI.
- The shared gate visual now has rails and posts in both the world and preview;
  its collision barrier and placement behavior are unchanged.

## Bed orientation fix

- Pillow and blanket textures now rotate with the placed bed in all four directions.
- Existing beds receive the correction when meshed; no save conversion is needed.
- Added 64 mesh checks covering all directions, colors, head/foot halves and
  negative chunks. Verified all four directions in an actual in-game view.

## Interaction and packaging fixes

- Axe stripping and shovel paths choose the eligible hand from the targeted
  action and wear only that hand's tool. Main-hand axes can strip with an
  offhand shield; unrelated targets retain normal shield defense.
- Creative inventory has case-insensitive item-name search with cached results,
  accurate filtered clicks/tooltips, paging, and the same 44-pixel slots.
- Packaging requires the preview manifest and rejects missing or stale assets.

## Efficient and compatible

- Offline generation; no thumbnail models, readbacks or new framebuffers during
  ordinary startup, inventory opening or page switching.
- Atlas dimensions and GPU storage remain 640×2240 / 5.47 MiB.
- Four Creative pages, 44×44 slots, counts, durability, equipment, crafting,
  Recipe Book, containers, hotbar, cursor and item-frame mappings are retained.
- Historical textures, item/block IDs, generators 1–22 and the four billboard
  mobs are unchanged. No new save format or world migration is introduced.
- Packaging verifies asset fingerprints and rejects stale previews.

## Validation

All 347 previews passed coverage, alpha, bounds and atlas checks. Native building,
farming and Creative interaction tests passed; all 20 full/demo regression suites
passed; 1,326 historical chunk hashes match. Both Windows variants embed the exact
baked atlas. An isolated installer upgrade passed without changing the real install.

Repeated controlled UI timings stayed within normal run variation; no overall
FPS or startup speedup is claimed. Larger-resolution UI captures use exact offscreen
native targets because this desktop restricts window height. Some similar stone
materials still benefit from tooltips. Non-block art retains its original authored
pixel detail, enhanced by geometry, framing and shading.

## Windows downloads

- `VoxelFrontier-v2.16.1-Windows-Setup.exe`: full installed game.
- `VoxelFrontier-v2.16.1-Windows-Standalone.exe`: stateless demo.
- `SHA256SUMS.txt`: checksums.

See [the visual validation report](ICON_VISUAL_VALIDATION_v2.16.1.md) for comparisons,
measurements, repeatable commands and limitations.
