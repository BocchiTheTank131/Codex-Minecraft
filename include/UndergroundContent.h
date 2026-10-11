#pragma once
#include "Definitions.h"
#include <array>
#include <limits>

struct UndergroundContentEntry {
    Block block;
    Item item;
    const char* name;
    int tile;
    Item drop;
    Item smelt;
    float hardness;
    int tier;
};
inline const std::array<UndergroundContentEntry, 8>& undergroundContent() {
    static const std::array<UndergroundContentEntry, 8> entries{{
        {Block::Deepslate, Item::Deepslate, "Deepslate", 96, Item::Deepslate, Item::None, 3.f, 1},
        {Block::DeepslateCoalOre, Item::DeepslateCoalOre, "DeepslateCoalOre", 97, Item::Coal, Item::Coal, 4.5f, 1},
        {Block::DeepslateIronOre, Item::DeepslateIronOre, "DeepslateIronOre", 98, Item::DeepslateIronOre, Item::IronIngot, 4.5f, 2},
        {Block::DeepslateCopperOre, Item::DeepslateCopperOre, "DeepslateCopperOre", 99, Item::DeepslateCopperOre, Item::CopperIngot, 4.5f, 2},
        {Block::DeepslateGoldOre, Item::DeepslateGoldOre, "DeepslateGoldOre", 100, Item::DeepslateGoldOre, Item::GoldIngot, 4.5f, 3},
        {Block::DeepslateDiamondOre, Item::DeepslateDiamondOre, "DeepslateDiamondOre", 101, Item::Diamond, Item::Diamond, 4.5f, 3},
        {Block::Bedrock, Item::Bedrock, "Bedrock", 102, Item::None, Item::None, std::numeric_limits<float>::infinity(), 255},
        {Block::Lava, Item::Lava, "Lava", 103, Item::None, Item::None, 0.f, 0},
    }};
    return entries;
}
