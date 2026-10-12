#pragma once
#include "Structure.h"

enum class LootRarity : std::uint8_t { Common, Uncommon, Rare };
struct WeightedLootEntry {
    Item item;
    int minimum, maximum, weight;
    LootRarity rarity;
};
// Separate tables leave historical surface-structure rolls unchanged.
std::array<ItemStack, 27> rollExplorationLoot(std::uint32_t seed,
                                            const StructureLootMarker& marker);
