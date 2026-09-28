#include "Explosion.h"
#include "Definitions.h"
#include "World.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

float explosionExposure(const World& world, const glm::vec3& center,
                        const glm::vec3& target) {
    constexpr glm::vec3 samples[] = {{0, 0, 0}, {-.22f, 0, -.22f},
                                      {.22f, 0, -.22f}, {-.22f, 0, .22f},
                                      {.22f, 0, .22f}};
    int clear = 0;
    for (const glm::vec3& offset : samples) {
        const glm::vec3 delta = target + offset - center;
        const float distance = glm::length(delta);
        RayHit hit;
        if (distance < .01f || !world.raycast(center, delta / distance,
                                               distance - .05f, hit))
            ++clear;
    }
    return static_cast<float>(clear) / 5.0f;
}

float explosionDamage(const Explosion& blast, float distance, float exposure) {
    const float reach = std::clamp(1.0f - distance / blast.radius, 0.0f, 1.0f);
    return blast.entityDamage * reach * reach * exposure;
}

void damageExplosionTerrain(World& world, const Explosion& blast,
                            const std::function<void(const glm::ivec3&, Block)>& onBroken) {
    if (!blast.blockDamage) return;
    const int radius = static_cast<int>(std::ceil(blast.radius));
    const glm::ivec3 origin(glm::floor(blast.position));
    for (int y = origin.y - radius; y <= origin.y + radius; ++y)
        for (int z = origin.z - radius; z <= origin.z + radius; ++z)
            for (int x = origin.x - radius; x <= origin.x + radius; ++x) {
                const glm::ivec3 cell(x, y, z);
                const glm::vec3 center = glm::vec3(cell) + glm::vec3(.5f);
                const float distance = glm::length(center - blast.position);
                if (distance > blast.radius) continue;
                const Block block = world.getBlock(x, y, z);
                if (block == Block::Air || isWater(block) ||
                    block == Block::Chest || block == Block::Furnace) continue;
                const BlockDefinition& definition = blockDefinition(block);
                const bool reinforced = block == Block::StoneBricks ||
                    block == Block::MossyStoneBricks || block == Block::Bricks;
                const float resistance = definition.hardness *
                    (reinforced ? 1.8f : definition.requiredHarvestTier > 0 ? 1.5f : 1.0f);
                // Stable spatial variation makes a rough crater without frame-order RNG.
                std::uint32_t hash = static_cast<std::uint32_t>(x) * 73856093u ^
                                     static_cast<std::uint32_t>(y) * 19349663u ^
                                     static_cast<std::uint32_t>(z) * 83492791u;
                hash ^= hash >> 13;
                const float variation = .82f + static_cast<float>(hash & 255u) / 255.0f * .36f;
                const float force = blast.power * (1.0f - distance / blast.radius) * variation;
                if (force <= resistance * .62f) continue;
                onBroken(cell, block);
                world.setBlock(x, y, z, Block::Air);
            }
}
