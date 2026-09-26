#pragma once

#include "Block.h"
#include "Noise.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <atomic>
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
constexpr int WORLD_HEIGHT = 128;
constexpr int SEA_LEVEL = 29;

struct RayHit {
    glm::ivec3 block{0};
    glm::ivec3 adjacent{0};
    glm::ivec3 normal{0};
    float distance = 0.0f;
};

struct VoxelVertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
    float sunLight = 1.0f;
    float blockLight = 0.0f;
    float ao = 1.0f;
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
    std::vector<std::uint8_t> sunlight;
    std::vector<std::uint8_t> blockLight;
    GLuint opaqueVao = 0;
    GLuint opaqueVbo = 0;
    GLsizei opaqueVertexCount = 0;
    GLuint waterVao = 0;
    GLuint waterVbo = 0;
    GLsizei waterVertexCount = 0;

    Block getLocal(int localX, int y, int localZ) const;
    void setLocal(int localX, int y, int localZ, Block block);
};

class World {
public:
    explicit World(std::uint32_t seed = 2026);
    ~World();

    void generate(int renderDistance, const glm::vec3& initialPosition = glm::vec3(0.0f));
    bool loadWorld(const std::string& path, glm::vec3& playerPosition);
    bool saveWorld(const std::string& path, const glm::vec3& playerPosition) const;
    std::uint32_t seed() const { return seed_; }
    void updateStreaming(const glm::vec3& playerPosition, int meshBudget = 2);
    void drawOpaque(const glm::mat4& viewProjection) const;
    void drawWater(const glm::mat4& viewProjection, const glm::vec3& cameraPosition) const;

    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, Block block);
    bool isSolidAt(int x, int y, int z) const;
    std::uint8_t sunlightAt(int x, int y, int z) const;
    std::uint8_t blockLightAt(int x, int y, int z) const;
    bool hasBlockNear(const glm::vec3& position, Block block, float radius) const;
    int terrainHeight(int worldX, int worldZ) const;
    glm::vec3 findSafeSpawnNear(int worldX, int worldZ) const;
    std::string biomeNameAt(int worldX, int worldZ) const;

    bool raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RayHit& hit) const;
    bool aabbIntersectsSolid(const glm::vec3& minimum, const glm::vec3& maximum) const;
    bool blockIntersectsAabb(const glm::ivec3& block, const glm::vec3& minimum, const glm::vec3& maximum) const;

    int loadedChunkCount() const { return static_cast<int>(chunks_.size()); }
    int pendingChunkCount() const;
    int renderDistance() const { return renderDistance_; }
    void setRenderDistance(int value);

private:
    enum class Biome { Plains, Forest, Desert, Mountains };
    struct TerrainSample { int height; Biome biome; };
    struct GeneratedChunk { int x; int z; std::vector<Block> blocks; };

    PerlinNoise terrainNoise_;
    PerlinNoise biomeNoise_;
    PerlinNoise caveNoise_;
    std::uint32_t seed_ = 0;
    std::unordered_map<std::int64_t, std::unique_ptr<Chunk>> chunks_;
    std::unordered_map<std::int64_t, std::unordered_map<std::size_t, Block>> edits_;
    std::deque<std::int64_t> dirtyQueue_;
    std::unordered_set<std::int64_t> dirtySet_;

    mutable std::mutex generationMutex_;
    std::condition_variable generationCv_;
    std::deque<glm::ivec2> generationQueue_;
    std::deque<GeneratedChunk> completedQueue_;
    std::unordered_set<std::int64_t> pendingKeys_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stopping_{false};

    int renderDistance_ = 7;
    glm::ivec2 streamCenter_{1000000, 1000000};
    bool blockLightingDirty_ = true;

    static std::int64_t chunkKey(int chunkX, int chunkZ);
    static int floorDiv(int value, int divisor);
    static int floorMod(int value, int divisor);
    static std::size_t localIndex(int localX, int y, int localZ);

    Chunk* findChunk(int chunkX, int chunkZ);
    const Chunk* findChunk(int chunkX, int chunkZ) const;
    TerrainSample sampleTerrain(int worldX, int worldZ) const;
    GeneratedChunk generateChunkData(int chunkX, int chunkZ) const;
    void workerLoop();
    void requestChunksAround(int centerX, int centerZ);
    void integrateCompleted(int budget, int centerX, int centerZ);
    void unloadDistant(int centerX, int centerZ);
    void markDirty(int chunkX, int chunkZ);
    void rebuildChunk(int chunkX, int chunkZ);
    void computeSunlight(Chunk& chunk);
    void rebuildBlockLighting();
    void rebuildTouchedChunks(int worldX, int worldZ);
};

