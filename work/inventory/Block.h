#pragma once
#include <cstdint>

enum class Block : std::uint8_t {
    Air=0, Grass, Dirt, Stone, Sand, Log, Leaves, Water,
    CoalOre, IronOre, GoldOre, CopperOre, DiamondOre,
    Planks, CraftingTable, Torch, Farmland, Crop0, Crop1, Crop2, Crop3
};

inline bool isWater(Block b){return b==Block::Water;}
inline bool isTorch(Block b){return b==Block::Torch;}
inline bool isCrop(Block b){return b>=Block::Crop0&&b<=Block::Crop3;}
inline bool isSolid(Block b){return b!=Block::Air&&b!=Block::Water&&b!=Block::Torch&&!isCrop(b);}
inline bool isRenderable(Block b){return b!=Block::Air;}
inline bool occludesLight(Block b){return isSolid(b)&&b!=Block::Leaves;}

