#pragma once

#include "Block.h"
#include <glm/vec3.hpp>
#include <functional>

class World;

struct Explosion {
    glm::vec3 position{0};
    float radius = 3.0f;
    float power = 3.0f;
    float entityDamage = 12.0f;
    bool blockDamage = true;
};

// World edits and drops are deliberately separate so future explosives can
// choose their own drop policy without duplicating blast propagation.
float explosionExposure(const World& world, const glm::vec3& center,
                        const glm::vec3& target);
float explosionDamage(const Explosion& blast, float distance, float exposure);
void damageExplosionTerrain(World& world, const Explosion& blast,
                            const std::function<void(const glm::ivec3&, Block)>& onBroken);
