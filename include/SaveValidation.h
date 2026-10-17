#pragma once
#include "Survival.h"
#include <cmath>

// Shared admission/codec invariants. Never normalize corrupted records into items.
namespace SaveValidation {
constexpr unsigned MaxMobs = 100000, MaxDrops = 100000, MaxMarkers = 100000;
inline bool position(glm::vec3 p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
        std::abs(double(p.x)) <= 30000000 && std::abs(double(p.z)) <= 30000000 &&
        std::abs(double(p.y)) <= 30000000;
}
inline bool velocity(glm::vec3 v) {
    // Includes 180 seconds of free-falling drops; rejects corrupt, finite impulses.
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
        std::abs(double(v.x)) <= 10000 && std::abs(double(v.y)) <= 10000 &&
        std::abs(double(v.z)) <= 10000;
}
inline bool stack(const ItemStack& s) {
    if (int(s.item) < 0 || s.item >= Item::Count) return false;
    if (s.item == Item::None) return s.count == 0 && s.durability == 0 && s.identity == 0;
    return s.count >= 1 && s.count <= Inventory::maxStack(s.item) &&
        s.durability >= 0 && s.durability <= Inventory::maxDurability(s.item) &&
        (s.item == Item::FilledMap || s.identity == 0);
}
}
