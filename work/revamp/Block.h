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
    Water
};

inline bool isWater(Block block) {
    return block == Block::Water;
}

inline bool isSolid(Block block) {
    return block != Block::Air && block != Block::Water;
}

inline bool isRenderable(Block block) {
    return block != Block::Air;
}

