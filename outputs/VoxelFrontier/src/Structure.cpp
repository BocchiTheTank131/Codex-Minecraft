#include "Structure.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <future>
#include <limits>

namespace {
constexpr int ChunkSize = 16;
constexpr int WorldHeight = 256;
constexpr int SeaLevel = 48;
constexpr int MajorRegion = 320;
constexpr int SmallRegion = 128;

int floorDiv(int value, int divisor) {
    int quotient = value / divisor;
    if (value % divisor < 0) --quotient;
    return quotient;
}

std::uint32_t mix(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    return value ^ (value >> 16U);
}

std::uint32_t regionHash(std::uint32_t seed, int regionX, int regionZ,
                         std::uint32_t salt) {
    return mix(seed ^ mix(static_cast<std::uint32_t>(regionX) * 0x9e3779b9U) ^
               mix(static_cast<std::uint32_t>(regionZ) * 0x85ebca6bU) ^ salt);
}

struct Candidate {
    int x = 0;
    int z = 0;
    std::uint32_t hash = 0;
};

Candidate candidateFor(std::uint32_t seed, int regionX, int regionZ, int size,
                       std::uint32_t salt) {
    const std::uint32_t hash = regionHash(seed, regionX, regionZ, salt);
    const int jitter = size == MajorRegion ? 70 : 26;
    return {regionX * size + size / 2 +
                static_cast<int>(hash % static_cast<std::uint32_t>(jitter * 2 + 1)) - jitter,
            regionZ * size + size / 2 +
                static_cast<int>((hash >> 10U) % static_cast<std::uint32_t>(jitter * 2 + 1)) - jitter,
            hash};
}

void add(StructureTemplate& result, int x, int y, int z, Block block) {
    result.cells.push_back({x, y, z, block, TemplateOperation::Place});
}

void clear(StructureTemplate& result, int x, int y, int z) {
    result.cells.push_back({x, y, z, Block::Air, TemplateOperation::Clear});
}

void floor(StructureTemplate& result, int halfX, int halfZ, int y, Block block) {
    for (int z = -halfZ; z <= halfZ; ++z)
        for (int x = -halfX; x <= halfX; ++x)
            add(result, x, y, z, block);
}

void finishBounds(StructureTemplate& result) {
    result.bounds = {10000, 10000, 10000, -10000, -10000, -10000};
    for (const StructureCell& cell : result.cells) {
        result.bounds.minX = std::min(result.bounds.minX, cell.x);
        result.bounds.minY = std::min(result.bounds.minY, cell.y);
        result.bounds.minZ = std::min(result.bounds.minZ, cell.z);
        result.bounds.maxX = std::max(result.bounds.maxX, cell.x);
        result.bounds.maxY = std::max(result.bounds.maxY, cell.y);
        result.bounds.maxZ = std::max(result.bounds.maxZ, cell.z);
    }
    for (const StructureMobMarker& marker : result.mobs) {
        result.bounds.minX = std::min(result.bounds.minX, marker.x);
        result.bounds.minY = std::min(result.bounds.minY, marker.y);
        result.bounds.minZ = std::min(result.bounds.minZ, marker.z);
        result.bounds.maxX = std::max(result.bounds.maxX, marker.x);
        result.bounds.maxY = std::max(result.bounds.maxY, marker.y);
        result.bounds.maxZ = std::max(result.bounds.maxZ, marker.z);
    }
}

StructureTemplate buildHouse(int halfX, int halfZ, int wallHeight,
                             StructurePieceKind kind) {
    StructureTemplate result;
    floor(result, halfX, halfZ, 0, Block::Cobblestone);
    for (int y = 1; y <= wallHeight; ++y) {
        for (int z = -halfZ; z <= halfZ; ++z) {
            for (int x = -halfX; x <= halfX; ++x) {
                const bool edge = std::abs(x) == halfX || std::abs(z) == halfZ;
                if (!edge) {
                    clear(result, x, y, z);
                    continue;
                }
                const bool corner = std::abs(x) == halfX && std::abs(z) == halfZ;
                const bool window = y == 2 && !corner &&
                    ((std::abs(x) == halfX && z == 0) ||
                     (std::abs(z) == halfZ && std::abs(x) == halfX - 1));
                add(result, x, y, z, corner ? Block::Log :
                    window ? Block::Glass : Block::Planks);
            }
        }
    }
    for (int z = -halfZ - 1; z <= halfZ + 1; ++z) {
        for (int x = -halfX - 1; x <= halfX + 1; ++x) {
            const int roofRise = std::max(0, halfX + 1 - std::abs(x)) / 2;
            add(result, x, wallHeight + 1 + roofRise, z, Block::Planks);
        }
    }
    add(result, 0, 1, -halfZ, doorBlock(0, false, false));
    add(result, 0, 2, -halfZ, doorBlock(0, true, false));
    result.connectors.push_back({0, 0, -halfZ - 1});
    result.mobs.push_back({0, 1, 0, false});
    if (kind == StructurePieceKind::Storage || kind == StructurePieceKind::Smith ||
        kind == StructurePieceKind::LargeHouse) {
        const int chestX = halfX - 1;
        add(result, chestX, 1, halfZ - 1, Block::Chest);
        result.loot.push_back({chestX, 1, halfZ - 1, StructureLoot::Village, 0});
    }
    if (kind == StructurePieceKind::Smith) {
        add(result, -halfX + 1, 1, halfZ - 1, Block::Furnace);
        add(result, -halfX + 1, 1, -halfZ + 1, Block::Torch);
    }
    finishBounds(result);
    return result;
}

StructureTemplate buildTemplate(StructurePieceKind kind) {
    if (kind == StructurePieceKind::House) return buildHouse(3, 3, 3, kind);
    if (kind == StructurePieceKind::LargeHouse) return buildHouse(4, 4, 4, kind);
    if (kind == StructurePieceKind::Farmhouse) return buildHouse(4, 3, 3, kind);
    if (kind == StructurePieceKind::Smith) return buildHouse(4, 3, 3, kind);
    if (kind == StructurePieceKind::Storage) return buildHouse(3, 4, 3, kind);
    if (kind == StructurePieceKind::Tower) return buildHouse(3, 3, 7, kind);

    StructureTemplate result;
    switch (kind) {
    case StructurePieceKind::Well:
    case StructurePieceKind::DesertWell:
        floor(result, 3, 3, 0, Block::Cobblestone);
        for (int z = -1; z <= 1; ++z)
            for (int x = -1; x <= 1; ++x)
                add(result, x, 0, z, Block::Water);
        for (int x : {-2, 2})
            for (int z : {-2, 2})
                for (int y = 1; y <= 4; ++y)
                    add(result, x, y, z, Block::Log);
        floor(result, 2, 2, 5, Block::Planks);
        result.connectors = {{0, 0, -4}, {4, 0, 0}, {0, 0, 4}, {-4, 0, 0}};
        break;
    case StructurePieceKind::Farm:
        floor(result, 4, 3, 0, Block::Dirt);
        for (int z = -2; z <= 2; ++z)
            for (int x = -3; x <= 3; ++x) {
                add(result, x, 0, z, x == 0 ? Block::Water : Block::Farmland);
                if (x != 0) add(result, x, 1, z, Block::Crop3);
            }
        for (int z = -3; z <= 3; ++z)
            for (int x : {-4, 4}) add(result, x, 1, z, Block::WoodenSlab);
        result.connectors.push_back({0, 0, -4});
        break;
    case StructurePieceKind::Pen:
        floor(result, 4, 3, 0, Block::Grass);
        for (int z = -3; z <= 3; ++z)
            for (int x = -4; x <= 4; ++x)
                if (std::abs(x) == 4 || std::abs(z) == 3)
                    add(result, x, 1, z, (x + z) % 3 == 0 ? Block::Log : Block::WoodenSlab);
        clear(result, 0, 1, -3);
        result.connectors.push_back({0, 0, -4});
        result.mobs.push_back({0, 1, 0, false});
        break;
    case StructurePieceKind::Lamp:
        for (int y = 0; y <= 3; ++y) add(result, 0, y, 0, Block::Log);
        add(result, 0, 4, 0, Block::Torch);
        break;
    case StructurePieceKind::OutpostTower:
        floor(result, 4, 4, 0, Block::Cobblestone);
        for (int y = 1; y <= 15; ++y) {
            for (int x : {-4, 4})
                for (int z : {-4, 4}) add(result, x, y, z, Block::Log);
            if (y == 5 || y == 10 || y == 14) floor(result, 4, 4, y, Block::Planks);
            if (y > 1 && y < 14) {
                add(result, -4, y, 0, Block::Log);
                add(result, -3, y, 0, Block::LadderEast);
            }
        }
        for (int x = -5; x <= 5; ++x)
            for (int z = -5; z <= 5; ++z) {
                if (std::abs(x) == 5 || std::abs(z) == 5)
                    add(result, x, 15, z, Block::WoodenSlab);
                add(result, x, 17, z, Block::Planks);
            }
        add(result, 2, 15, 2, Block::Chest);
        add(result, -2, 15, -2, Block::Torch);
        result.loot.push_back({2, 15, 2, StructureLoot::Outpost, 0});
        result.mobs.push_back({0, 15, 0, true});
        result.mobs.push_back({6, 1, 2, true});
        result.connectors.push_back({0, 0, -5});
        break;
    case StructurePieceKind::Tent:
        floor(result, 3, 3, 0, Block::Dirt);
        for (int z = -3; z <= 3; ++z)
            for (int x = -3; x <= 3; ++x) {
                if (std::abs(x) == 3) add(result, x, 1, z, Block::Log);
                if (std::abs(x) <= 3) add(result, x, 4 - std::abs(x), z, Block::Planks);
            }
        for (int y = 1; y <= 2; ++y)
            for (int x = -1; x <= 1; ++x) clear(result, x, y, -3);
        result.connectors.push_back({0, 0, -4});
        break;
    case StructurePieceKind::LogPile:
        for (int z = -2; z <= 2; ++z)
            for (int x = -2; x <= 2; ++x)
                if ((x + z) % 2 == 0) add(result, x, 0, z, Block::Log);
        break;
    case StructurePieceKind::Pyramid:
        for (int layer = 0; layer <= 9; ++layer) {
            const int radius = 12 - layer;
            for (int z = -radius; z <= radius; ++z)
                for (int x = -radius; x <= radius; ++x)
                    if (std::abs(x) == radius || std::abs(z) == radius || layer == 0)
                        add(result, x, layer, z,
                            (layer % 3 == 0 && (x + z) % 7 == 0) ? Block::Clay : Block::Sand);
        }
        floor(result, 3, 3, 10, Block::Clay);
        for (int y = 1; y <= 6; ++y)
            for (int z = -5; z <= 5; ++z)
                for (int x = -5; x <= 5; ++x) clear(result, x, y, z);
        for (int z = -12; z <= -5; ++z)
            for (int x = -1; x <= 1; ++x)
                for (int y = 1; y <= 3; ++y) clear(result, x, y, z);
        for (int y = -5; y <= -2; ++y)
            for (int z = -5; z <= 5; ++z)
                for (int x = -5; x <= 5; ++x)
                    add(result, x, y, z,
                        (std::abs(x) == 5 || std::abs(z) == 5 || y == -5)
                            ? Block::Sand : Block::Air);
        for (int step = 0; step <= 5; ++step) {
            const int z = -5 + step;
            for (int x = -1; x <= 1; ++x) {
                add(result, x, -step, z, Block::StoneBricks);
                for (int y = 1 - step; y <= 3 - step; ++y) clear(result, x, y, z);
            }
        }
        for (int x : {-3, 3})
            for (int z : {-3, 3}) {
                add(result, x, -4, z, Block::Chest);
                result.loot.push_back({x, -4, z, StructureLoot::Pyramid, 0});
            }
        add(result, 0, -4, 2, Block::Sand);
        add(result, 0, -3, 2, Block::Cactus);
        result.connectors.push_back({0, 0, -13});
        break;
    case StructurePieceKind::Ruin:
        floor(result, 4, 4, 0, Block::MossyCobblestone);
        for (int z = -4; z <= 4; ++z)
            for (int x = -4; x <= 4; ++x)
                if (std::abs(x) == 4 || std::abs(z) == 4) {
                    const int wallHeight = 1 + static_cast<int>(mix(static_cast<std::uint32_t>(
                        (x + 11) * 31 + (z + 11) * 97)) % 4U);
                    for (int y = 1; y <= wallHeight; ++y)
                        add(result, x, y, z, Block::MossyStoneBricks);
                }
        clear(result, 0, 1, -4);
        clear(result, 0, 2, -4);
        add(result, 2, 1, 2, Block::Chest);
        result.loot.push_back({2, 1, 2, StructureLoot::Ruin, 0});
        break;
    case StructurePieceKind::Campsite:
        floor(result, 3, 3, 0, Block::Dirt);
        for (int x : {-3, 3})
            for (int z : {-3, 3})
                for (int y = 1; y <= 3; ++y) add(result, x, y, z, Block::Log);
        for (int x = -3; x <= 3; ++x)
            for (int z = -3; z <= 3; ++z)
                if (std::abs(x) == 3 || std::abs(z) == 3)
                    add(result, x, 4, z, Block::Planks);
        add(result, 0, 1, 0, Block::Torch);
        break;
    default:
        break;
    }
    finishBounds(result);
    return result;
}

Block desertMaterial(Block block) {
    switch (block) {
    case Block::Log: return Block::StoneBricks;
    case Block::Planks: return Block::Sand;
    case Block::Cobblestone: return Block::Sand;
    case Block::MossyCobblestone: return Block::StoneBricks;
    case Block::MossyStoneBricks: return Block::Clay;
    default: return block;
    }
}

StructurePiece makePiece(StructurePieceKind kind, int x, int y, int z,
                         int rotation, bool mirror, bool desert) {
    StructurePiece piece{kind, x, y, z, rotation & 3, mirror, desert, {}};
    piece.bounds = structureBoundingBox(piece);
    return piece;
}

void includeBox(StructureBox& total, const StructureBox& box) {
    total.minX = std::min(total.minX, box.minX);
    total.minY = std::min(total.minY, box.minY);
    total.minZ = std::min(total.minZ, box.minZ);
    total.maxX = std::max(total.maxX, box.maxX);
    total.maxY = std::max(total.maxY, box.maxY);
    total.maxZ = std::max(total.maxZ, box.maxZ);
}

bool addPiece(StructurePlan& plan, StructurePiece piece, int maximumSlope,
              const std::function<StructureTerrain(int, int)>& terrain) {
    if (!canPlaceStructure(piece.bounds, maximumSlope, terrain)) return false;
    for (const StructurePiece& existing : plan.pieces)
        if (piece.bounds.overlapsXZ(existing.bounds, 1)) return false;
    if (plan.pieces.empty()) plan.bounds = piece.bounds;
    else includeBox(plan.bounds, piece.bounds);
    plan.pieces.push_back(piece);
    return true;
}

} // namespace

bool StructureBox::intersectsXZ(int minimumX, int minimumZ,
                                int maximumX, int maximumZ) const {
    return minX <= maximumX && maxX >= minimumX &&
           minZ <= maximumZ && maxZ >= minimumZ;
}

std::uint64_t structureMobMarkerId(int x, int y, int z, bool hostile) {
    const std::uint64_t horizontal =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) |
        static_cast<std::uint32_t>(z);
    const std::uint64_t vertical = static_cast<std::uint64_t>(
        static_cast<std::uint32_t>(y)) * 0x9e3779b97f4a7c15ULL;
    std::uint64_t value = horizontal ^ vertical ^
        (hostile ? 0xd1b54a32d192ed03ULL : 0xabc98388fb8fac03ULL);
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

bool StructureBox::overlapsXZ(const StructureBox& other, int padding) const {
    return minX <= other.maxX + padding && maxX + padding >= other.minX &&
           minZ <= other.maxZ + padding && maxZ + padding >= other.minZ;
}

const StructureTemplate& structureTemplate(StructurePieceKind kind) {
    static const std::array<StructureTemplate, 17> templates = [] {
        std::array<StructureTemplate, 17> result;
        for (int index = 0; index < static_cast<int>(result.size()); ++index)
            result[static_cast<std::size_t>(index)] =
                buildTemplate(static_cast<StructurePieceKind>(index));
        return result;
    }();
    return templates[static_cast<std::size_t>(kind)];
}

std::array<int, 2> rotateLocalPosition(int x, int z, int rotation, bool mirror) {
    if (mirror) x = -x;
    switch (rotation & 3) {
    case 1: return {-z, x};
    case 2: return {-x, -z};
    case 3: return {z, -x};
    default: return {x, z};
    }
}

StructureBox structureBoundingBox(const StructurePiece& piece) {
    const StructureBox local = structureTemplate(piece.kind).bounds;
    StructureBox result{std::numeric_limits<int>::max(), piece.y + local.minY,
                        std::numeric_limits<int>::max(), std::numeric_limits<int>::min(),
                        piece.y + local.maxY, std::numeric_limits<int>::min()};
    for (int x : {local.minX, local.maxX})
        for (int z : {local.minZ, local.maxZ}) {
            const auto transformed = rotateLocalPosition(x, z, piece.rotation, piece.mirror);
            result.minX = std::min(result.minX, piece.x + transformed[0]);
            result.maxX = std::max(result.maxX, piece.x + transformed[0]);
            result.minZ = std::min(result.minZ, piece.z + transformed[1]);
            result.maxZ = std::max(result.maxZ, piece.z + transformed[1]);
        }
    return result;
}

std::array<int, 3> terrainHeightForFootprint(
    const StructureBox& footprint,
    const std::function<StructureTerrain(int, int)>& terrain) {
    int lowest = WorldHeight, highest = 0, sum = 0, count = 0;
    for (int z = footprint.minZ; z <= footprint.maxZ; ++z)
        for (int x = footprint.minX; x <= footprint.maxX; ++x) {
            const int height = terrain(x, z).height;
            lowest = std::min(lowest, height);
            highest = std::max(highest, height);
            sum += height;
            ++count;
        }
    return {lowest, highest, count > 0 ? (sum + count / 2) / count : 0};
}

bool canPlaceStructure(const StructureBox& footprint, int maximumSlope,
                       const std::function<StructureTerrain(int, int)>& terrain) {
    const auto heights = terrainHeightForFootprint(footprint, terrain);
    if (heights[0] <= SeaLevel + 2 || heights[1] - heights[0] > maximumSlope ||
        heights[2] + footprint.maxY - footprint.minY >= WorldHeight - 3)
        return false;
    const int centerX = (footprint.minX + footprint.maxX) / 2;
    const int centerZ = (footprint.minZ + footprint.maxZ) / 2;
    return terrain(centerX, centerZ).river <= 0.09f;
}

StructurePlan StructureGenerator::planMajor(
    int regionX, int regionZ,
    const std::function<StructureTerrain(int, int)>& terrain) const {
    StructurePlan plan;
    plan.regionX = regionX;
    plan.regionZ = regionZ;
    const Candidate candidate = candidateFor(seed_, regionX, regionZ,
                                             MajorRegion, 0x72a15bc3U);
    const StructureTerrain center = terrain(candidate.x, candidate.z);
    const int roll = static_cast<int>((candidate.hash >> 20U) % 100U);
    if (center.biome == StructureBiome::Desert) {
        if (roll < 36) plan.kind = StructureKind::DesertVillage;
        else if (roll < 56) plan.kind = StructureKind::DesertPyramid;
        else if (roll < 67) plan.kind = StructureKind::Outpost;
        else return plan;
    } else if (center.biome == StructureBiome::Plains) {
        if (roll < 34) plan.kind = StructureKind::PlainsVillage;
        else if (roll < 48) plan.kind = StructureKind::Outpost;
        else return plan;
    } else if (center.biome == StructureBiome::Forest ||
               center.biome == StructureBiome::Mountains) {
        if (roll < 15) plan.kind = StructureKind::Outpost;
        else return plan;
    } else {
        return plan;
    }
    if (center.height < SeaLevel + 4 || center.height > WorldHeight - 28 ||
        center.river > 0.08f)
        return plan;

    const bool desert = plan.kind == StructureKind::DesertVillage ||
                        center.biome == StructureBiome::Desert;
    if (plan.kind == StructureKind::DesertPyramid) {
        const StructurePiece pyramid = makePiece(StructurePieceKind::Pyramid,
            candidate.x, center.height, candidate.z,
            static_cast<int>((candidate.hash >> 8U) & 3U), false, true);
        addPiece(plan, pyramid, 3, terrain);
        return plan;
    }
    if (plan.kind == StructureKind::Outpost) {
        const StructurePiece tower = makePiece(StructurePieceKind::OutpostTower,
            candidate.x, center.height, candidate.z,
            static_cast<int>((candidate.hash >> 8U) & 3U), false, desert);
        if (!addPiece(plan, tower, 4, terrain)) return plan;
        const std::array<std::array<int, 2>, 4> offsets{{{{-15, -12}}, {{15, -11}},
                                                          {{-13, 14}}, {{14, 13}}}};
        for (int index = 0; index < 4; ++index) {
            if (mix(candidate.hash + static_cast<std::uint32_t>(index) * 331U) % 3U == 0U)
                continue;
            const int x = candidate.x + offsets[static_cast<std::size_t>(index)][0];
            const int z = candidate.z + offsets[static_cast<std::size_t>(index)][1];
            const StructurePieceKind kind = index % 2 == 0
                ? StructurePieceKind::Tent : StructurePieceKind::LogPile;
            addPiece(plan, makePiece(kind, x, terrain(x, z).height, z,
                     index & 3, index == 2, desert), 3, terrain);
        }
        return plan;
    }

    // A village starts at one meeting point. Roads grow from its four
    // connectors; houses/farms are placed along those roads with a hard cap.
    const StructurePiece meeting = makePiece(StructurePieceKind::Well,
        candidate.x, center.height, candidate.z, 0, false, desert);
    if (!addPiece(plan, meeting, 3, terrain)) return plan;
    const int armCount = 2 + static_cast<int>((candidate.hash >> 5U) % 3U);
    const int firstDirection = static_cast<int>((candidate.hash >> 12U) & 3U);
    constexpr std::array<std::array<int, 2>, 4> directions{{{{0, -1}}, {{1, 0}},
                                                              {{0, 1}}, {{-1, 0}}}};
    const auto& meetingConnectors = structureTemplate(StructurePieceKind::Well).connectors;
    constexpr std::array<StructurePieceKind, 9> buildings{
        StructurePieceKind::House, StructurePieceKind::House,
        StructurePieceKind::LargeHouse, StructurePieceKind::Farmhouse,
        StructurePieceKind::Smith, StructurePieceKind::Storage,
        StructurePieceKind::Tower, StructurePieceKind::Farm,
        StructurePieceKind::Pen};
    for (int arm = 0; arm < armCount && plan.pieces.size() < 16; ++arm) {
        const int direction = (firstDirection + arm) & 3;
        const int dx = directions[static_cast<std::size_t>(direction)][0];
        const int dz = directions[static_cast<std::size_t>(direction)][1];
        const StructureConnector& connector =
            meetingConnectors[static_cast<std::size_t>(direction)];
        const int firstStep = std::max(std::abs(connector.x), std::abs(connector.z));
        const int armLength = 19 + static_cast<int>(
            mix(candidate.hash + static_cast<std::uint32_t>(arm) * 919U) % 18U);
        int previousHeight = center.height;
        for (int step = firstStep; step <= armLength; ++step) {
            const int x = candidate.x + dx * step;
            const int z = candidate.z + dz * step;
            const StructureTerrain ground = terrain(x, z);
            if (ground.height <= SeaLevel + 2 || ground.river > 0.09f ||
                std::abs(ground.height - previousHeight) > 2)
                break;
            previousHeight = ground.height;
            for (int side = -1; side <= 1; ++side)
            {
                const int roadX = x - dz * side;
                const int roadZ = z + dx * side;
                const int roadY = terrain(roadX, roadZ).height;
                if (std::abs(roadY - ground.height) > 2) continue;
                plan.roads.push_back({roadX, roadY, roadZ,
                                      desert ? Block::Sand : Block::Gravel});
                includeBox(plan.bounds, {roadX, roadY, roadZ,
                                         roadX, roadY + 2, roadZ});
            }
            if (step != 12 && step != 24) continue;
            for (int side : {-1, 1}) {
                const std::uint32_t choice = mix(candidate.hash +
                    static_cast<std::uint32_t>(arm * 100 + step * 7 + side + 3));
                if (choice % 5U == 0U || plan.pieces.size() >= 16) continue;
                const int buildingX = x - dz * side * 10;
                const int buildingZ = z + dx * side * 10;
                const int rotation = (direction + (side > 0 ? 1 : 3) + 2) & 3;
                const StructurePieceKind kind = buildings[choice % buildings.size()];
                const StructurePiece building = makePiece(kind, buildingX,
                    terrain(buildingX, buildingZ).height, buildingZ,
                    rotation, (choice & 0x100U) != 0U, desert);
                if (!addPiece(plan, building, kind == StructurePieceKind::Farm ? 2 : 3,
                              terrain)) continue;
                const StructureConnector& door =
                    structureTemplate(kind).connectors.front();
                const auto entranceOffset = rotateLocalPosition(
                    door.x, door.z, building.rotation, building.mirror);
                const int entranceX = building.x + entranceOffset[0];
                const int entranceZ = building.z + entranceOffset[1];
                int pathX = x;
                int pathZ = z;
                for (int pathStep = 0; pathStep < 12 &&
                     (pathX != entranceX || pathZ != entranceZ); ++pathStep) {
                    if (pathX != entranceX)
                        pathX += entranceX > pathX ? 1 : -1;
                    else
                        pathZ += entranceZ > pathZ ? 1 : -1;
                    const int pathY = terrain(pathX, pathZ).height;
                    if (std::abs(pathY - ground.height) > 2) break;
                    plan.roads.push_back({pathX, pathY,
                                          pathZ, desert ? Block::Sand : Block::Gravel});
                    includeBox(plan.bounds, {pathX, pathY, pathZ,
                                             pathX, pathY + 2, pathZ});
                }
            }
        }
    }
    return plan;
}

StructurePlan StructureGenerator::planSmall(
    int regionX, int regionZ,
    const std::function<StructureTerrain(int, int)>& terrain) const {
    StructurePlan plan;
    plan.regionX = regionX;
    plan.regionZ = regionZ;
    const Candidate candidate = candidateFor(seed_, regionX, regionZ,
                                             SmallRegion, 0xc561ab27U);
    if ((candidate.hash >> 18U) % 100U >= 22U) return plan;
    const StructureTerrain ground = terrain(candidate.x, candidate.z);
    if (ground.height <= SeaLevel + 3 || ground.river > 0.08f ||
        ground.biome == StructureBiome::Other)
        return plan;
    // Keep inexpensive ruins away from major centers without querying plans.
    for (int majorZ = floorDiv(candidate.z, MajorRegion) - 1;
         majorZ <= floorDiv(candidate.z, MajorRegion) + 1; ++majorZ)
        for (int majorX = floorDiv(candidate.x, MajorRegion) - 1;
             majorX <= floorDiv(candidate.x, MajorRegion) + 1; ++majorX) {
            const Candidate major = candidateFor(seed_, majorX, majorZ,
                                                  MajorRegion, 0x72a15bc3U);
            if (std::abs(candidate.x - major.x) < 82 &&
                std::abs(candidate.z - major.z) < 82)
                return plan;
        }
    const bool desert = ground.biome == StructureBiome::Desert;
    const int choice = static_cast<int>((candidate.hash >> 9U) % 3U);
    const StructurePieceKind kind = desert && choice == 1
        ? StructurePieceKind::DesertWell
        : choice == 0 ? StructurePieceKind::Ruin
        : StructurePieceKind::Campsite;
    plan.kind = kind == StructurePieceKind::DesertWell ? StructureKind::Well
              : kind == StructurePieceKind::Ruin ? StructureKind::RuinedHouse
              : StructureKind::Campsite;
    addPiece(plan, makePiece(kind, candidate.x, ground.height, candidate.z,
             static_cast<int>((candidate.hash >> 13U) & 3U),
             (candidate.hash & 0x10000U) != 0U, desert), 3, terrain);
    return plan;
}

std::shared_ptr<const StructurePlan> StructureGenerator::cachedMajorPlan(
    int regionX, int regionZ,
    const std::function<StructureTerrain(int, int)>& terrain) const {
    const std::int64_t key = static_cast<std::int64_t>(
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(regionX)) << 32U) |
        static_cast<std::uint32_t>(regionZ));
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        const auto found = majorCache_.find(key);
        if (found != majorCache_.end()) return found->second;
    }
    // The expensive terrain sampling runs without holding the cache lock.
    auto computed = std::make_shared<const StructurePlan>(
        planMajor(regionX, regionZ, terrain));
    std::lock_guard<std::mutex> lock(cacheMutex_);
    const auto found = majorCache_.find(key);
    if (found != majorCache_.end()) return found->second;
    majorCache_.emplace(key, computed);
    majorCacheOrder_.push_back(key);
    if (majorCacheOrder_.size() > 128) {
        majorCache_.erase(majorCacheOrder_.front());
        majorCacheOrder_.pop_front();
    }
    return computed;
}

StructurePlan StructureGenerator::majorPlanForRegion(
    int regionX, int regionZ,
    const std::function<StructureTerrain(int, int)>& terrain) const {
    return *cachedMajorPlan(regionX, regionZ, terrain);
}

std::vector<StructurePlan> StructureGenerator::plansForChunk(
    int chunkX, int chunkZ,
    const std::function<StructureTerrain(int, int)>& terrain) const {
    const int minimumX = chunkX * ChunkSize;
    const int minimumZ = chunkZ * ChunkSize;
    const int maximumX = minimumX + ChunkSize - 1;
    const int maximumZ = minimumZ + ChunkSize - 1;
    std::vector<StructurePlan> plans;
    for (int regionZ = floorDiv(minimumZ, MajorRegion) - 1;
         regionZ <= floorDiv(maximumZ, MajorRegion) + 1; ++regionZ) {
        for (int regionX = floorDiv(minimumX, MajorRegion) - 1;
             regionX <= floorDiv(maximumX, MajorRegion) + 1; ++regionX) {
            const Candidate candidate = candidateFor(seed_, regionX, regionZ,
                                                     MajorRegion, 0x72a15bc3U);
            if (candidate.x + 82 < minimumX || candidate.x - 82 > maximumX ||
                candidate.z + 82 < minimumZ || candidate.z - 82 > maximumZ)
                continue;
            const StructurePlan& plan = *cachedMajorPlan(regionX, regionZ, terrain);
            if (!plan.pieces.empty() &&
                plan.bounds.intersectsXZ(minimumX, minimumZ, maximumX, maximumZ))
                plans.push_back(plan);
        }
    }
    for (int regionZ = floorDiv(minimumZ, SmallRegion) - 1;
         regionZ <= floorDiv(maximumZ, SmallRegion) + 1; ++regionZ) {
        for (int regionX = floorDiv(minimumX, SmallRegion) - 1;
             regionX <= floorDiv(maximumX, SmallRegion) + 1; ++regionX) {
            const Candidate candidate = candidateFor(seed_, regionX, regionZ,
                                                     SmallRegion, 0xc561ab27U);
            if (candidate.x + 12 < minimumX || candidate.x - 12 > maximumX ||
                candidate.z + 12 < minimumZ || candidate.z - 12 > maximumZ)
                continue;
            StructurePlan plan = planSmall(regionX, regionZ, terrain);
            if (!plan.pieces.empty() &&
                plan.bounds.intersectsXZ(minimumX, minimumZ, maximumX, maximumZ))
                plans.push_back(std::move(plan));
        }
    }
    return plans;
}

void StructureGenerator::placeTemplate(
    const StructurePiece& piece, int chunkX, int chunkZ,
    std::vector<Block>& blocks, std::vector<StructureLootMarker>& loot,
    std::vector<StructureMobMarker>& mobs) const {
    const StructureTemplate& source = structureTemplate(piece.kind);
    const int minimumX = chunkX * ChunkSize;
    const int minimumZ = chunkZ * ChunkSize;
    auto index = [](int x, int y, int z) {
        return static_cast<std::size_t>((y * ChunkSize + z) * ChunkSize + x);
    };
    for (const StructureCell& cell : source.cells) {
        const auto rotated = rotateLocalPosition(cell.x, cell.z,
                                                 piece.rotation, piece.mirror);
        const int localX = piece.x + rotated[0] - minimumX;
        const int localZ = piece.z + rotated[1] - minimumZ;
        const int y = piece.y + cell.y;
        if (localX < 0 || localX >= ChunkSize || localZ < 0 ||
            localZ >= ChunkSize || y < 0 || y >= WorldHeight)
            continue;
        Block block = cell.operation == TemplateOperation::Clear
            ? Block::Air : (piece.desert ? desertMaterial(cell.block) : cell.block);
        if (isDoor(block)) {
            constexpr int directionX[4] = {0, 0, 1, -1};
            constexpr int directionZ[4] = {-1, 1, 0, 0};
            const auto direction = rotateLocalPosition(directionX[doorFacing(block)],
                                                       directionZ[doorFacing(block)],
                                                       piece.rotation, piece.mirror);
            const int facing = direction[0] == 1 ? 2 : direction[0] == -1 ? 3 :
                               direction[1] == 1 ? 1 : 0;
            block = doorBlock(facing, isDoorUpper(block),
                              doorHingeRight(block) != piece.mirror, isDoorOpen(block));
        }
        if (isLadder(block)) {
            const int facing = (static_cast<int>(block) -
                                static_cast<int>(Block::LadderNorth) + piece.rotation) & 3;
            block = static_cast<Block>(static_cast<int>(Block::LadderNorth) + facing);
        }
        blocks[index(localX, y, localZ)] = block;
    }
    for (const StructureLootMarker& marker : source.loot) {
        const auto rotated = rotateLocalPosition(marker.x, marker.z,
                                                 piece.rotation, piece.mirror);
        const int x = piece.x + rotated[0];
        const int z = piece.z + rotated[1];
        const int y = piece.y + marker.y;
        if (x < minimumX || x >= minimumX + ChunkSize ||
            z < minimumZ || z >= minimumZ + ChunkSize ||
            y < 0 || y >= WorldHeight)
            continue;
        loot.push_back({x, y, z, marker.table,
                        regionHash(seed_, x, z, static_cast<std::uint32_t>(y))});
    }
    for (const StructureMobMarker& marker : source.mobs) {
        const auto rotated = rotateLocalPosition(marker.x, marker.z,
                                                 piece.rotation, piece.mirror);
        const int x = piece.x + rotated[0];
        const int z = piece.z + rotated[1];
        if (x >= minimumX && x < minimumX + ChunkSize &&
            z >= minimumZ && z < minimumZ + ChunkSize)
            mobs.push_back({x, piece.y + marker.y, z, marker.hostile});
    }
}

bool StructureGenerator::applyToChunk(
    int chunkX, int chunkZ, std::vector<Block>& blocks,
    std::vector<StructureLootMarker>& loot,
    std::vector<StructureMobMarker>& mobs,
    const std::function<StructureTerrain(int, int)>& terrain) const {
    const std::vector<StructurePlan> plans = plansForChunk(chunkX, chunkZ, terrain);
    const int minimumX = chunkX * ChunkSize;
    const int minimumZ = chunkZ * ChunkSize;
    auto index = [](int x, int y, int z) {
        return static_cast<std::size_t>((y * ChunkSize + z) * ChunkSize + x);
    };
    for (const StructurePlan& plan : plans) {
        for (const StructureRoadCell& road : plan.roads) {
            const int x = road.x - minimumX;
            const int z = road.z - minimumZ;
            if (x < 0 || x >= ChunkSize || z < 0 || z >= ChunkSize ||
                road.y < 1 || road.y + 3 >= WorldHeight)
                continue;
            blocks[index(x, road.y, z)] = road.block;
            for (int y = road.y + 1; y <= road.y + 3; ++y)
                blocks[index(x, y, z)] = Block::Air;
        }
        for (const StructurePiece& piece : plan.pieces) {
            if (!piece.bounds.intersectsXZ(minimumX, minimumZ,
                                           minimumX + ChunkSize - 1,
                                           minimumZ + ChunkSize - 1))
                continue;
            const Block foundation = piece.desert ? Block::Clay : Block::Cobblestone;
            for (int worldZ = std::max(minimumZ, piece.bounds.minZ);
                 worldZ <= std::min(minimumZ + ChunkSize - 1, piece.bounds.maxZ); ++worldZ) {
                for (int worldX = std::max(minimumX, piece.bounds.minX);
                     worldX <= std::min(minimumX + ChunkSize - 1, piece.bounds.maxX); ++worldX) {
                    const int x = worldX - minimumX;
                    const int z = worldZ - minimumZ;
                    const int ground = terrain(worldX, worldZ).height;
                    // A short foundation follows natural ground; placements with
                    // larger relief were already rejected during planning.
                    for (int y = ground + 1; y < piece.y && y < WorldHeight; ++y)
                        blocks[index(x, y, z)] = foundation;
                    for (int y = piece.y + 1;
                         y <= std::max(ground + 2, piece.bounds.maxY + 2) &&
                         y < WorldHeight; ++y)
                        blocks[index(x, y, z)] = Block::Air;
                }
            }
            placeTemplate(piece, chunkX, chunkZ, blocks, loot, mobs);
        }
    }
    return !plans.empty();
}

std::array<ItemStack, 27> StructureGenerator::rollLoot(
    const StructureLootMarker& marker) const {
    std::array<ItemStack, 27> slots{};
    struct Entry { Item item; int minimum; int maximum; int weight; };
    static constexpr std::array<Entry, 7> village{{
        {Item::Bread, 1, 4, 22}, {Item::Apple, 1, 3, 16},
        {Item::Wheat, 2, 7, 22}, {Item::Seeds, 2, 6, 16},
        {Item::Coal, 1, 4, 12}, {Item::IronIngot, 1, 2, 7},
        {Item::StonePickaxe, 1, 1, 5}}};
    static constexpr std::array<Entry, 7> outpost{{
        {Item::Bread, 1, 4, 18}, {Item::Coal, 2, 5, 16},
        {Item::IronIngot, 1, 4, 21}, {Item::Leather, 1, 3, 14},
        {Item::IronSword, 1, 1, 10}, {Item::GoldIngot, 1, 2, 14},
        {Item::Diamond, 1, 1, 7}}};
    static constexpr std::array<Entry, 7> pyramid{{
        {Item::GoldIngot, 2, 6, 22}, {Item::IronIngot, 2, 5, 20},
        {Item::Diamond, 1, 2, 5}, {Item::Apple, 1, 3, 12},
        {Item::Bread, 2, 5, 13}, {Item::GoldSword, 1, 1, 8},
        {Item::Coal, 2, 6, 20}}};
    static constexpr std::array<Entry, 5> ruin{{
        {Item::Bread, 1, 2, 24}, {Item::Coal, 1, 3, 22},
        {Item::Wheat, 1, 4, 24}, {Item::IronIngot, 1, 2, 18},
        {Item::GoldIngot, 1, 1, 12}}};
    const Entry* entries = nullptr;
    int entryCount = 0;
    switch (marker.table) {
    case StructureLoot::Village: entries = village.data(); entryCount = static_cast<int>(village.size()); break;
    case StructureLoot::Outpost: entries = outpost.data(); entryCount = static_cast<int>(outpost.size()); break;
    case StructureLoot::Pyramid: entries = pyramid.data(); entryCount = static_cast<int>(pyramid.size()); break;
    case StructureLoot::Ruin: entries = ruin.data(); entryCount = static_cast<int>(ruin.size()); break;
    }
    int totalWeight = 0;
    for (int index = 0; index < entryCount; ++index)
        totalWeight += entries[index].weight;
    std::uint32_t state = mix(seed_ ^ marker.salt);
    const int rolls = marker.table == StructureLoot::Pyramid ? 5 :
                      marker.table == StructureLoot::Outpost ? 4 : 3;
    for (int roll = 0; roll < rolls; ++roll) {
        state = mix(state + 0x9e3779b9U);
        int choice = static_cast<int>(state % static_cast<std::uint32_t>(totalWeight));
        const Entry* picked = &entries[0];
        for (int index = 0; index < entryCount; ++index) {
            choice -= entries[index].weight;
            if (choice < 0) { picked = &entries[index]; break; }
        }
        state = mix(state + 0x85ebca6bU);
        const int count = picked->minimum + static_cast<int>(
            state % static_cast<std::uint32_t>(picked->maximum - picked->minimum + 1));
        const int slot = static_cast<int>((state >> 9U) % 27U);
        slots[static_cast<std::size_t>(slot)] = {
            picked->item, count, Inventory::maxDurability(picked->item)};
    }
    return slots;
}

bool StructureGenerator::runSelfTest(std::string& report) const {
    const auto terrain = [](int x, int) {
        return StructureTerrain{72, x >= 0 ? StructureBiome::Desert
                                            : StructureBiome::Plains, 0.0f};
    };
    std::array<int, 4> majorCounts{};
    bool valid = true;
    StructurePlan borderPlan;
    StructurePiece borderPiece;
    bool foundCrossingPiece = false;
    bool foundLoot = false;
    for (int regionZ = -4; regionZ <= 4; ++regionZ) {
        for (int regionX = -4; regionX <= 4; ++regionX) {
            const StructurePlan first = planMajor(regionX, regionZ, terrain);
            const StructurePlan second = planMajor(regionX, regionZ, terrain);
            valid = valid && first.pieces.size() == second.pieces.size() &&
                    first.roads.size() == second.roads.size();
            if (first.pieces.empty()) continue;
            ++majorCounts[static_cast<std::size_t>(first.kind)];
            for (std::size_t left = 0; left < first.pieces.size(); ++left)
                for (std::size_t right = left + 1; right < first.pieces.size(); ++right)
                    valid = valid && !first.pieces[left].bounds.overlapsXZ(
                        first.pieces[right].bounds);
            for (const StructurePiece& piece : first.pieces) {
                if (!foundCrossingPiece &&
                    floorDiv(piece.bounds.minX, ChunkSize) !=
                        floorDiv(piece.bounds.maxX, ChunkSize)) {
                    borderPlan = first;
                    borderPiece = piece;
                    foundCrossingPiece = true;
                }
                const auto& markers = structureTemplate(piece.kind).loot;
                if (!markers.empty()) {
                    foundLoot = true;
                    const auto firstLoot = rollLoot(markers.front());
                    const auto secondLoot = rollLoot(markers.front());
                    for (std::size_t slot = 0; slot < firstLoot.size(); ++slot)
                        valid = valid && firstLoot[slot].item == secondLoot[slot].item &&
                                firstLoot[slot].count == secondLoot[slot].count;
                }
            }
        }
    }
    valid = valid && majorCounts[0] > 0 && majorCounts[1] > 0 &&
            majorCounts[2] > 0 && majorCounts[3] > 0 && foundCrossingPiece && foundLoot;
    if (foundCrossingPiece) {
        const int leftChunkX = floorDiv(borderPiece.bounds.minX, ChunkSize);
        const int rightChunkX = leftChunkX + 1;
        const int chunkZ = floorDiv(borderPiece.z, ChunkSize);
        std::vector<Block> left(ChunkSize * WorldHeight * ChunkSize, Block::Air);
        std::vector<Block> right = left;
        std::vector<Block> leftAgain = left;
        std::vector<Block> rightAgain = right;
        std::vector<StructureLootMarker> loot;
        std::vector<StructureMobMarker> mobs;
        applyToChunk(leftChunkX, chunkZ, left, loot, mobs, terrain);
        applyToChunk(rightChunkX, chunkZ, right, loot, mobs, terrain);
        applyToChunk(rightChunkX, chunkZ, rightAgain, loot, mobs, terrain);
        applyToChunk(leftChunkX, chunkZ, leftAgain, loot, mobs, terrain);
        const auto occupied = [](const std::vector<Block>& blocks) {
            return std::count_if(blocks.begin(), blocks.end(),
                                 [](Block block) { return block != Block::Air; });
        };
        valid = valid && left == leftAgain && right == rightAgain &&
                occupied(left) > 0 && occupied(right) > 0 &&
                !borderPlan.pieces.empty();
        const auto parallelChunk = [&](int chunkX) {
            std::vector<Block> blocks(ChunkSize * WorldHeight * ChunkSize, Block::Air);
            std::vector<StructureLootMarker> generatedLoot;
            std::vector<StructureMobMarker> generatedMobs;
            applyToChunk(chunkX, chunkZ, blocks,
                         generatedLoot, generatedMobs, terrain);
            return blocks;
        };
        auto leftJob = std::async(std::launch::async, parallelChunk, leftChunkX);
        auto rightJob = std::async(std::launch::async, parallelChunk, rightChunkX);
        const auto parallelLeft = leftJob.get();
        const auto parallelRight = rightJob.get();
        valid = valid && parallelLeft == left && parallelRight == right;
    }
    report = "regional plans, overlap, cross-chunk application, and deterministic loot: " +
        std::string(valid ? "passed" : "FAILED") + " (villages " +
        std::to_string(majorCounts[0] + majorCounts[1]) + ", outposts " +
        std::to_string(majorCounts[2]) + ", pyramids " +
        std::to_string(majorCounts[3]) + ")";
    return valid;
}
