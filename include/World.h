#pragma once

#include "Block.h"
#include "Noise.h"
#include "Structure.h"
#include "CanyonCarver.h"
#include "Survival.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <atomic>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

constexpr int CHUNK_SIZE = 16;
constexpr int WORLD_HEIGHT = 256;
constexpr int SEA_LEVEL = 48;

struct RayHit {
    glm::ivec3 block{0};
    glm::ivec3 adjacent{0};
    glm::ivec3 normal{0};
    float distance = 0.0f;
};

struct WorldGenerationDebug {
    float continentalness = 0.0f;
    float erosion = 0.0f;
    float temperature = 0.0f;
    float humidity = 0.0f;
    float weirdness = 0.0f;
    float peakValley = 0.0f;
    float river = 0.0f;
    int terrainHeight = 0;
    std::string biome;
    float caveCheese = 0.0f;
    float caveSpaghetti = 0.0f;
    float caveNoodle = 0.0f;
    int canyonRegionX = 0;
    int canyonRegionZ = 0;
    bool canyonActive = false;
    bool canyonSurfaceExposure = false;
    bool caveEntranceCandidate = false;
    float caveEntranceInfluence = 0.0f;
    int depthBelowSurface = 0;
    int aquiferLevel = 0;
};

struct WorldgenSurvey {
    int surfaceColumns = 0;
    int exposedColumns = 0;
    int entranceCandidates = 0;
    int openEntrances = 0;
    int entranceOpeningColumns = 0;
    int broadEntrances = 0;
    int dryCaveBlocks = 0;
    int floodedCaveBlocks = 0;
    int maximumBorderRise = 0;
    bool deterministic = true;
};

struct VoxelVertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
    float sunLight = 1.0f;
    float blockLight = 0.0f;
    float ao = 1.0f;
    float atlasTile = -1.0f;
};

struct BlockEntityPosition {
    int x = 0;
    int y = 0;
    int z = 0;
    bool operator==(const BlockEntityPosition& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct BlockEntityPositionHash {
    std::size_t operator()(const BlockEntityPosition& position) const;
};

struct FurnaceData {
    ItemStack input{};
    ItemStack fuel{};
    ItemStack output{};
    float fuelRemaining = 0.0f;
    float fuelCapacity = 0.0f;
    float progress = 0.0f;
};

struct ChestData {
    std::array<ItemStack, 27> slots{};
};

class Chunk {
public:
    Chunk(int chunkX, int chunkZ, std::vector<Block> data = {});
    ~Chunk();

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;

    int x = 0;
    int z = 0;
    std::vector<Block> blocks;
    std::vector<std::uint8_t> packedLight;
    int highestRenderableY = -1;
    GLuint opaqueVao = 0;
    GLuint opaqueVbo = 0;
    GLsizei opaqueVertexCount = 0;
    std::size_t opaqueBufferCapacity = 0;
    GLuint waterVao = 0;
    GLuint waterVbo = 0;
    GLsizei waterVertexCount = 0;
    std::size_t waterBufferCapacity = 0;
    std::uint64_t meshRevision = 0;
    std::uint64_t meshIdentity = 0;

    Block getLocal(int localX, int y, int localZ) const;
    void setLocal(int localX, int y, int localZ, Block block);
    std::uint8_t sunlightLocal(std::size_t index) const {
        return static_cast<std::uint8_t>(packedLight[index] >> 4U);
    }
    std::uint8_t blockLightLocal(std::size_t index) const {
        return static_cast<std::uint8_t>(packedLight[index] & 0x0fU);
    }
    void setSunlightLocal(std::size_t index, std::uint8_t value) {
        packedLight[index] = static_cast<std::uint8_t>(
            (packedLight[index] & 0x0fU) | ((value & 0x0fU) << 4U));
    }
    void setBlockLightLocal(std::size_t index, std::uint8_t value) {
        packedLight[index] = static_cast<std::uint8_t>(
            (packedLight[index] & 0xf0U) | (value & 0x0fU));
    }
};

class World {
public:
    explicit World(std::uint32_t seed = 2026);
    ~World();

    void generate(int renderDistance, const glm::vec3& initialPosition = glm::vec3(0.0f));
    bool loadWorld(const std::string& path, glm::vec3& playerPosition);
    bool saveWorld(const std::string& path, const glm::vec3& playerPosition) const;
    std::uint32_t seed() const {
        return seed_;
    }
    void updateStreaming(const glm::vec3& playerPosition, int meshBudget = 2);
    void updateFluids(float deltaTime, int updateBudget = 96);
    void updateBlockEntities(float deltaTime);
    std::vector<StructureMobMarker> takeActiveStructureMobMarkers(bool spawnAllowed = true);
    void drawOpaque(const glm::mat4& viewProjection) const;
    void drawWater(const glm::mat4& viewProjection, const glm::vec3& cameraPosition) const;

    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, Block block);
    bool hasLoadedChunkAt(int x, int z) const;
    bool canPlacePlant(const glm::ivec3& position, Block block) const;
    int fillBlocks(const glm::ivec3& low, const glm::ivec3& high, Block block);
    bool isSolidAt(int x, int y, int z) const;
    std::uint8_t sunlightAt(int x, int y, int z) const;
    std::uint8_t blockLightAt(int x, int y, int z) const;
    bool hasBlockNear(const glm::vec3& position, Block block, float radius) const;
    bool hasClimbableNear(const glm::vec3& position, float radius) const;
    FurnaceData* furnaceAt(const glm::ivec3& position, bool create = false);
    ChestData* chestAt(const glm::ivec3& position, bool create = false);
    std::vector<ItemStack> takeBlockEntityContents(const glm::ivec3& position);
    int terrainHeight(int worldX, int worldZ) const;
    glm::vec3 findSafeSpawnNear(int worldX, int worldZ) const;
    void prepareSpawnTerrain(const glm::vec3& position);
    bool runPatch272SelfTest(std::string& report) const;
    std::string biomeNameAt(int worldX, int worldZ) const;
    WorldGenerationDebug generationDebugAt(int worldX, int worldZ,
                                           int worldY = SEA_LEVEL,
                                           bool detailed = false) const;
    int generationVersion() const { return generationVersion_; }

    bool raycast(const glm::vec3& origin,
                 const glm::vec3& direction,
                 float maxDistance,
                 RayHit& hit) const;
    bool aabbIntersectsSolid(const glm::vec3& minimum, const glm::vec3& maximum) const;
    bool blockIntersectsAabb(const glm::ivec3& block,
                             const glm::vec3& minimum,
                             const glm::vec3& maximum) const;
    bool blockCollisionBounds(const glm::ivec3& block,
                              glm::vec3& minimum,
                              glm::vec3& maximum) const;
    bool aabbTouchesBlock(const glm::vec3& minimum,
                          const glm::vec3& maximum,
                          Block block,
                          float tolerance = 0.02f) const;
    bool simulationActiveAt(float worldX, float worldZ) const;

    int loadedChunkCount() const {
        return static_cast<int>(chunks_.size());
    }
    int pendingChunkCount() const;
    int queuedMeshRebuildCount() const {
        return static_cast<int>(dirtySet_.size());
    }
    int pendingCpuMeshCount() const;
    int completedMeshCount() const;
    int renderedChunkCount() const { return renderedChunkCount_; }
    float lastChunkRebuildMilliseconds() const { return lastChunkRebuildMilliseconds_; }
    float lastMeshUploadMilliseconds() const { return lastMeshUploadMilliseconds_; }
    std::size_t uploadedVertexCount() const { return uploadedVertexCount_; }
    float lastBlockEditMilliseconds() const { return lastBlockEditMilliseconds_; }
    bool runMeshEditSmokeTest(std::string& report);
    bool runAsyncMeshSmokeTest(std::string& report);
    bool runFluidSmokeTest(std::string& report);
    bool runGenerationSmokeTest(std::string& report) const;
    bool runCraftingContentSmokeTest(std::string& report);
    WorldgenSurvey runWorldgenSurvey() const;
    bool runStructureGenerationSmokeTest(std::string& report,
                                         glm::ivec3* representativeChest = nullptr) const;
    int renderDistance() const {
        return renderDistance_;
    }
    void setRenderDistance(int value);
    void setSimulationDistance(int value);
    int simulationDistance() const { return simulationDistance_; }

private:
    enum class Biome {
        Plains, Forest, Desert, Mountains, SnowyPlains, Meadow, SnowySlopes,
        StonyPeaks, SnowyPeaks, BirchForest, Beach, Ocean
    };
    struct TerrainSample {
        int height = SEA_LEVEL;
        Biome biome = Biome::Plains;
        float continentalness = 0.0f;
        float erosion = 0.0f;
        float temperature = 0.0f;
        float humidity = 0.0f;
        float weirdness = 0.0f;
        float peakValley = 0.0f;
        float river = 0.0f;
        float ravine = 0.0f;
    };
    struct GeneratedChunk {
        int x;
        int z;
        std::vector<Block> blocks;
        std::vector<StructureLootMarker> loot;
        std::vector<StructureMobMarker> mobs;
        bool hasStructure = false;
    };
    struct MeshInput {
        int x = 0;
        int z = 0;
        std::uint64_t revision = 0;
        std::uint64_t identity = 0;
        std::uint64_t epoch = 0;
        int meshHeight = WORLD_HEIGHT;
        std::vector<Block> blocks;
        std::vector<std::uint8_t> packedLight;
    };
    struct MeshOutput {
        int x = 0;
        int z = 0;
        std::uint64_t revision = 0;
        std::uint64_t identity = 0;
        std::uint64_t epoch = 0;
        std::vector<VoxelVertex> opaque;
        std::vector<VoxelVertex> water;
        float buildMilliseconds = 0.0f;
    };
    struct FluidCell {
        int x;
        int y;
        int z;

        bool operator==(const FluidCell& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    struct FluidCellHash {
        std::size_t operator()(const FluidCell& cell) const;
    };

    PerlinNoise terrainNoise_;
    PerlinNoise biomeNoise_;
    PerlinNoise caveNoise_;
    PerlinNoise continentalNoise_;
    PerlinNoise erosionNoise_;
    PerlinNoise temperatureNoise_;
    PerlinNoise humidityNoise_;
    PerlinNoise weirdnessNoise_;
    PerlinNoise ridgeNoise_;
    PerlinNoise riverNoise_;
    PerlinNoise aquiferNoise_;
    PerlinNoise cheeseNoise_;
    PerlinNoise spaghettiNoise_;
    PerlinNoise noodleNoise_;
    CanyonCarver canyonCarver_;
    StructureGenerator structures_;
    std::uint32_t seed_ = 0;
    std::uint32_t generationVersion_ = 8;
    std::unordered_map<std::int64_t, std::unique_ptr<Chunk>> chunks_;
    std::unordered_map<std::int64_t, std::unordered_map<std::size_t, Block>> edits_;
    std::unordered_map<BlockEntityPosition, FurnaceData, BlockEntityPositionHash> furnaces_;
    std::unordered_map<BlockEntityPosition, ChestData, BlockEntityPositionHash> chests_;
    std::vector<StructureMobMarker> pendingStructureMobs_;
    std::unordered_set<std::uint64_t> pendingStructureMobIds_;
    std::deque<std::int64_t> dirtyQueue_;
    std::unordered_set<std::int64_t> dirtySet_;
    std::deque<glm::ivec2> lightingQueue_;
    std::unordered_set<std::int64_t> lightingSet_;
    std::deque<FluidCell> fluidQueue_;
    std::unordered_set<FluidCell, FluidCellHash> fluidQueued_;
    float fluidUpdateAccumulator_ = 0.0f;
    float blockEntityUpdateAccumulator_ = 0.0f;

    mutable std::mutex generationMutex_;
    std::condition_variable generationCv_;
    std::deque<glm::ivec2> generationQueue_;
    std::deque<GeneratedChunk> completedQueue_;
    std::unordered_set<std::int64_t> pendingKeys_;
    std::vector<glm::ivec2> requestOffsets_;
    std::size_t nextRequestOffset_ = 0;
    int requestRadius_ = -1;
    std::vector<std::thread> workers_;
    std::atomic<std::int64_t> generationCenterKey_{0};
    std::atomic<int> generationLoadRadius_{7};
    std::thread meshWorker_;
    std::atomic<bool> stopping_{false};
    mutable std::mutex meshMutex_;
    std::condition_variable meshCv_;
    std::deque<MeshInput> meshQueue_;
    std::deque<MeshOutput> completedMeshes_;
    // Returned only after upload/discard; the worker owns buffers while building.
    std::deque<MeshOutput> recycledMeshes_;
    bool meshWorkerBusy_ = false;
    std::unordered_map<std::int64_t, std::uint64_t> pendingMeshKeys_;
    std::unordered_set<std::int64_t> priorityDirtyKeys_;
    std::uint64_t meshEpoch_ = 0;
    std::uint64_t nextMeshIdentity_ = 1;

    int renderDistance_ = 7;
    int simulationDistance_ = 7;
    glm::ivec2 streamCenter_{1000000, 1000000};
    bool blockLightingDirty_ = false;
    mutable int renderedChunkCount_ = 0;
    float lastChunkRebuildMilliseconds_ = 0.0f;
    float lastMeshUploadMilliseconds_ = 0.0f;
    float lastBlockEditMilliseconds_ = 0.0f;
    std::size_t uploadedVertexCount_ = 0;
    bool synchronousMeshForSmokeTest_ = false;

    static std::int64_t chunkKey(int chunkX, int chunkZ);
    static int floorDiv(int value, int divisor);
    static int floorMod(int value, int divisor);
    static std::size_t localIndex(int localX, int y, int localZ);

    Chunk* findChunk(int chunkX, int chunkZ);
    const Chunk* findChunk(int chunkX, int chunkZ) const;
    TerrainSample sampleTerrain(int worldX, int worldZ) const;
    TerrainSample sampleTerrainLegacy(int worldX, int worldZ) const;
    TerrainSample sampleTerrainModern(int worldX, int worldZ) const;
    StructureTerrain structureTerrainAt(int worldX, int worldZ) const;
    GeneratedChunk generateChunkData(int chunkX, int chunkZ) const;
    GeneratedChunk generateChunkDataLegacy(int chunkX, int chunkZ) const;
    GeneratedChunk generateChunkDataModern(int chunkX, int chunkZ) const;
    void workerLoop();
    void meshWorkerLoop();
    MeshInput captureMeshInput(int chunkX, int chunkZ) const;
    static MeshOutput buildMesh(const MeshInput& input);
    static MeshOutput buildMesh(const MeshInput& input, MeshOutput result);
    void recycleMeshOutput(MeshOutput result);
    void uploadMeshResult(const MeshOutput& result);
    void dispatchMeshJobs(int budget);
    void uploadCompletedMeshes(int budget);
    void requestChunksAround(int centerX, int centerZ);
    void rebuildRequestOffsets();
    void discardObsoleteRequests(int centerX, int centerZ);
    void integrateCompleted(int budget, int centerX, int centerZ);
    void initializeGeneratedLoot(const GeneratedChunk& generated);
    void queueGeneratedMobs(const GeneratedChunk& generated);
    void unloadDistant(int centerX, int centerZ);
    void markDirty(int chunkX, int chunkZ, bool highPriority = false);
    void rebuildChunk(int chunkX, int chunkZ);
    void computeSunlight(Chunk& chunk);
    void computeSunlightColumn(Chunk& chunk, int localX, int localZ);
    void rebuildBlockLighting();
    void queueLightingUpdate(int chunkX, int chunkZ, bool highPriority = true);
    void rebuildBlockLightingNear(int chunkX, int chunkZ);
    void rebuildTouchedChunks(
        int worldX, int worldZ, bool rebuildImmediately, bool highPriority = true,
        bool nearbySkyChanged = false);
    bool setBlockInternal(int x, int y, int z, Block block, bool rebuildImmediately,
                          bool dispatchImmediately = true);
    void queueFluidUpdate(int x, int y, int z);
    void queueFluidNeighborhood(int x, int y, int z);
    void updateFluidCell(const FluidCell& cell);
    void scheduleFluidBoundaryUpdates(int chunkX, int chunkZ);
};
