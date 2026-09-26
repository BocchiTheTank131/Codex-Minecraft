#pragma once

#include <cstdint>

enum class Block : std::uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Log,
    Leaves,
    Water,
    CoalOre,
    IronOre,
    GoldOre,
    CopperOre,
    DiamondOre,
    Planks,
    CraftingTable,
    Torch
};

inline bool isWater(Block block) { return block == Block::Water; }
inline bool isTorch(Block block) { return block == Block::Torch; }
inline bool isSolid(Block block) { return block != Block::Air && block != Block::Water && block != Block::Torch; }
inline bool isRenderable(Block block) { return block != Block::Air; }
inline bool occludesLight(Block block) {
    return isSolid(block) && block != Block::Leaves;
}

