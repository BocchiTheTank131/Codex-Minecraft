# Voxel Frontier v2.16.2 — Quality of Life

Windows installer and stateless standalone demo.

- Sort player inventories and container contents; Quick Stack, Deposit All and
  Take All retain stack metadata and respect locked hotbar slots.
- Alt+click a hotbar slot to protect it from sorting and bulk transfers.
- Context-sensitive hints use the configured Sneak key and available actions.
- Auto / Small / Normal / Large UI scaling keeps panels and mouse input aligned.
- Textured placement ghosts show orientation, connected stairs, slab merging,
  multi-block structures and collision/support failures before confirmation.
- Saving / Saved / failure feedback follows the actual checkpoint commit. The
  pause menu shows the last successful save time, including verified loaded saves.
- Six Creative categories, favorites and recent selections share the existing
  search and paging, including mouse-wheel paging. Alt+RMB toggles favorites without picking up an item.
- Recipe feedback shows missing quantities, table requirements and blocked-grid
  conditions. Hover an inventory item and press the configurable Recipes Using Item key
  (default U) to find recipes using it.
- Multiline sign editing supports a caret, selection, arrows, Home/End, Delete,
  Backspace, clipboard copy/paste, Enter for new lines and Ctrl+Enter to save.

Hotbar locks use a versioned two-byte inventory extension. Existing V4–V7
inventories remain readable with empty locks. Item/block IDs, all 22 generators,
world textures, the 347 cached inventory icons and the four billboard mobs are
unchanged. The standalone demo remains stateless.

See
[QOL validation](QOL_VALIDATION_v2.16.2.md) for measurements, screenshots, controls
and limitations.

Validation: 1,375 QOL assertions, 20 full/demo regression suites, and 1,326 unchanged historical chunk hashes.

Assets: Windows Setup installer, Windows Standalone demo, and SHA256SUMS.txt.
