#pragma once

#include "Block.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace AmbientOcclusion {

inline const BlockGeometryProperties& geometry(Block block) {
    static const auto properties = [] {
        std::array<BlockGeometryProperties, static_cast<std::size_t>(Block::Count)> result{};
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = blockGeometry(static_cast<Block>(i));
        return result;
    }();
    return properties[static_cast<std::size_t>(block)];
}

// The sample is the face corner expressed in the neighbor block's local space.
// A bounded distance to the block's occupied box softens partial-shape contacts.
inline float contribution(Block block, const glm::vec3& sample) {
    const BlockGeometryProperties& shape = geometry(block);
    if (shape.aoOcclusion <= 0.0f) return 0.0f;
    if (shape.aoOcclusion >= 1.0f &&
        sample.x >= shape.minX && sample.x <= shape.maxX &&
        sample.y >= shape.minY && sample.y <= shape.maxY &&
        sample.z >= shape.minZ && sample.z <= shape.maxZ) return 1.0f;
    const float dx = std::max({shape.minX - sample.x, sample.x - shape.maxX, 0.0f});
    const float dy = std::max({shape.minY - sample.y, sample.y - shape.maxY, 0.0f});
    const float dz = std::max({shape.minZ - sample.z, sample.z - shape.maxZ, 0.0f});
    const float distance = std::max({dx, dy, dz});
    return shape.aoOcclusion * std::clamp(1.0f - distance / 0.375f, 0.0f, 1.0f);
}

inline float cornerLevel(float sideA, float sideB, float diagonal) {
    // Two enclosing sides keep the classic strong corner shadow even when the
    // diagonal is empty. Quantization is exact in binary for greedy comparisons.
    const float blocked = std::clamp(sideA + sideB + diagonal +
                                      (1.0f - diagonal) * std::min(sideA, sideB),
                                      0.0f, 3.0f);
    return std::round((1.0f - blocked * 0.075f) * 64.0f) / 64.0f;
}

bool runSelfTest(std::string& report);

} // namespace AmbientOcclusion
