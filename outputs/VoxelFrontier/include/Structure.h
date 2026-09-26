#pragma once

#include "Block.h"
#include "Survival.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <deque>
#include <vector>

enum class StructureBiome : std::uint8_t {
    Plains, Forest, Desert, Mountains, Other
};

struct StructureTerrain {
    int height = 0;
    StructureBiome biome = StructureBiome::Other;
    float river = 0.0f;
};

enum class StructureKind : std::uint8_t {
    PlainsVillage, DesertVillage, Outpost, DesertPyramid,
    RuinedHouse, Well, Campsite
};

enum class StructureLoot : std::uint8_t { Village, Outpost, Pyramid, Ruin };

struct StructureLootMarker {
    int x = 0;
    int y = 0;
    int z = 0;
    StructureLoot table = StructureLoot::Village;
    std::uint32_t salt = 0;
};

struct StructureMobMarker {
    int x = 0;
    int y = 0;
    int z = 0;
    bool hostile = false;
};
std::uint64_t structureMobMarkerId(int x, int y, int z, bool hostile);

struct StructureBox {
    int minX = 0, minY = 0, minZ = 0;
    int maxX = 0, maxY = 0, maxZ = 0;
    bool intersectsXZ(int minX_, int minZ_, int maxX_, int maxZ_) const;
    bool overlapsXZ(const StructureBox& other, int padding = 0) const;
};

struct StructureConnector {
    int x = 0, y = 0, z = 0;
};

enum class TemplateOperation : std::uint8_t { Place, Clear };

struct StructureCell {
    int x = 0, y = 0, z = 0;
    Block block = Block::Air;
    TemplateOperation operation = TemplateOperation::Place;
};

struct StructureTemplate {
    std::vector<StructureCell> cells;
    std::vector<StructureConnector> connectors;
    std::vector<StructureLootMarker> loot;
    std::vector<StructureMobMarker> mobs;
    StructureBox bounds{};
};

enum class StructurePieceKind : std::uint8_t {
    Well, House, LargeHouse, Farmhouse, Smith, Storage, Tower,
    Farm, Pen, Lamp, OutpostTower, Tent, LogPile,
    Pyramid, Ruin, DesertWell, Campsite
};

struct StructurePiece {
    StructurePieceKind kind = StructurePieceKind::House;
    int x = 0, y = 0, z = 0;
    int rotation = 0;
    bool mirror = false;
    bool desert = false;
    StructureBox bounds{};
};

struct StructureRoadCell {
    int x = 0, y = 0, z = 0;
    Block block = Block::Gravel;
};

struct StructurePlan {
    StructureKind kind = StructureKind::RuinedHouse;
    int regionX = 0, regionZ = 0;
    StructureBox bounds{};
    std::vector<StructurePiece> pieces;
    std::vector<StructureRoadCell> roads;
};

// A missing template cell means DoNotReplace; an explicit Clear cell means Air.
const StructureTemplate& structureTemplate(StructurePieceKind kind);
std::array<int, 2> rotateLocalPosition(int x, int z, int rotation, bool mirror);
StructureBox structureBoundingBox(const StructurePiece& piece);
std::array<int, 3> terrainHeightForFootprint(
    const StructureBox& footprint,
    const std::function<StructureTerrain(int, int)>& terrain);
bool canPlaceStructure(const StructureBox& footprint, int maximumSlope,
                       const std::function<StructureTerrain(int, int)>& terrain);

class StructureGenerator {
public:
    explicit StructureGenerator(std::uint32_t seed) : seed_(seed) {}

    // Pure regional planning: no loaded chunks, world edits, OpenGL or shared RNG.
    std::vector<StructurePlan> plansForChunk(
        int chunkX, int chunkZ,
        const std::function<StructureTerrain(int, int)>& terrain) const;
    StructurePlan majorPlanForRegion(
        int regionX, int regionZ,
        const std::function<StructureTerrain(int, int)>& terrain) const;
    void placeTemplate(const StructurePiece& piece, int chunkX, int chunkZ,
                       std::vector<Block>& blocks,
                       std::vector<StructureLootMarker>& loot,
                       std::vector<StructureMobMarker>& mobs) const;
    bool applyToChunk(int chunkX, int chunkZ, std::vector<Block>& blocks,
                      std::vector<StructureLootMarker>& loot,
                      std::vector<StructureMobMarker>& mobs,
                      const std::function<StructureTerrain(int, int)>& terrain) const;
    std::array<ItemStack, 27> rollLoot(const StructureLootMarker& marker) const;
    bool runSelfTest(std::string& report) const;

private:
    std::uint32_t seed_;
    mutable std::mutex cacheMutex_;
    mutable std::unordered_map<std::int64_t, std::shared_ptr<const StructurePlan>> majorCache_;
    mutable std::deque<std::int64_t> majorCacheOrder_;
    StructurePlan planMajor(int regionX, int regionZ,
                            const std::function<StructureTerrain(int, int)>& terrain) const;
    std::shared_ptr<const StructurePlan> cachedMajorPlan(
        int regionX, int regionZ,
        const std::function<StructureTerrain(int, int)>& terrain) const;
    StructurePlan planSmall(int regionX, int regionZ,
                            const std::function<StructureTerrain(int, int)>& terrain) const;
};
