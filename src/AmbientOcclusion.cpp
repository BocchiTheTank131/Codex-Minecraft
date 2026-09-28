#include "AmbientOcclusion.h"

namespace AmbientOcclusion {
bool runSelfTest(std::string& report) {
    const glm::vec3 lower(0.0f), upper(0.0f, 1.0f, 0.0f);
    const auto near = [](float actual, float expected) { return std::abs(actual - expected) < 0.02f; };
    const auto check = [&](bool good, const char* name) {
        if (!good) report = name;
        return good;
    };
    if (!check(near(contribution(Block::Stone, lower), 1.0f), "full cube")) return false;
    if (!check(near(contribution(Block::WoodenSlab, lower), 0.85f) &&
               near(contribution(Block::WoodenSlab, upper), 0.0f), "bottom slab")) return false;
    if (!check(near(contribution(Block::WoodenSlabTop, lower), 0.0f) &&
               near(contribution(Block::WoodenSlabTop, upper), 0.85f), "top slab")) return false;
    if (!check(near(contribution(Block::Snow, lower), 0.25f) &&
               near(contribution(Block::Snow, upper), 0.0f), "snow layer")) return false;
    if (!check(near(contribution(Block::Leaves, lower), 0.35f) &&
               contribution(Block::Glass, lower) == 0.0f &&
               contribution(Block::Ice, lower) < 0.1f, "transparent blocks")) return false;
    const Block closed = doorBlock(0, false, false, false);
    const Block opened = doorBlock(0, false, false, true);
    if (!check(contribution(closed, lower) > 0.6f &&
               contribution(closed, {0, 0, 1}) == 0.0f &&
               contribution(opened, lower) > 0.6f &&
               contribution(opened, {1, 0, 0}) == 0.0f, "door panels")) return false;
    for (Block block : {Block::Torch, Block::LadderNorth, Block::Water,
                        Block::Crop3, Block::TallGrass, Block::RedFlower}) {
        if (!check(contribution(block, lower) == 0.0f, "nonoccluding detail")) return false;
    }
    if (!check(contribution(Block::Cactus, lower) < 0.5f &&
               contribution(Block::Farmland, lower) < 1.0f, "cactus/farmland")) return false;
    if (!check(near(cornerLevel(0, 0, 0), 1.0f) &&
               cornerLevel(1, 1, 0) < 0.8f &&
               cornerLevel(1, 0, 0) > cornerLevel(1, 1, 0) &&
               cornerLevel(1, 1, 1) == cornerLevel(1, 1, 0), "corner curve")) return false;
    report = "geometry-aware corner AO passed";
    return true;
}
} // namespace AmbientOcclusion
