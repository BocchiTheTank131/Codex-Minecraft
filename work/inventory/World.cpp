#include "World.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <fstream>
#include <cstring>

namespace {
constexpr std::array<glm::ivec3, 6> FaceNormals{{
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
}};

constexpr float FaceCorners[6][4][3] = {
    {{1,0,0}, {1,1,0}, {1,1,1}, {1,0,1}},
    {{0,0,1}, {0,1,1}, {0,1,0}, {0,0,0}},
    {{0,1,1}, {1,1,1}, {1,1,0}, {0,1,0}},
    {{0,0,0}, {1,0,0}, {1,0,1}, {0,0,1}},
    {{1,0,1}, {1,1,1}, {0,1,1}, {0,0,1}},
    {{0,0,0}, {0,1,0}, {1,1,0}, {1,0,0}}
};
constexpr int Indices[6] = {0, 1, 2, 0, 2, 3};
constexpr int AtlasTiles = 17;

std::uint32_t positionHash(int x, int z, std::uint32_t seed) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343U;
    h ^= static_cast<std::uint32_t>(z) * 0xd8163841U;
    h ^= seed * 0xcb1ab31fU;
    h ^= h >> 13U;
    h *= 0x85ebca6bU;
    return h ^ (h >> 16U);
}

int atlasTile(Block block, int faceIndex) {
    if (block == Block::Grass) {
        if (faceIndex == 2) return 0;
        if (faceIndex == 3) return 2;
        return 1;
    }
    if (block == Block::Dirt) return 2;
    if (block == Block::Stone) return 3;
    if (block == Block::Sand) return 4;
    if (block == Block::Log) return (faceIndex == 2 || faceIndex == 3) ? 6 : 5;
    if (block == Block::Leaves) return 7;
    if (block == Block::Water) return 8;
    if (block == Block::CoalOre) return 9;
    if (block == Block::IronOre) return 10;
    if (block == Block::GoldOre) return 11;
    if (block == Block::CopperOre) return 12;
    if (block == Block::DiamondOre) return 13;
    if (block == Block::Planks) return 14;
    if (block == Block::CraftingTable) return 15;
    if (block == Block::Farmland) return 2;
    if (isCrop(block)) return 7;
    if (block == Block::Torch) return 16;
    return 3;
}

std::array<glm::vec2, 4> tileUvs(int tile, bool verticalFace) {
    constexpr float inset = 0.0005f;
    const float tileWidth = 1.0f / static_cast<float>(AtlasTiles);
    const float left = static_cast<float>(tile) * tileWidth + inset;
    const float right = static_cast<float>(tile + 1) * tileWidth - inset;
    const float bottom = inset;
    const float top = 1.0f - inset;
    if (verticalFace) {
        return {{{left, top}, {left, bottom}, {right, bottom}, {right, top}}};
    }
    return {{{left, bottom}, {right, bottom}, {right, top}, {left, top}}};
}

std::array<glm::vec4, 6> frustumPlanes(const glm::mat4& matrix) {
    const glm::vec4 row0(matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0]);
    const glm::vec4 row1(matrix[0][1], matrix[1][1], matrix[2][1], matrix[3][1]);
    const glm::vec4 row2(matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2]);
    const glm::vec4 row3(matrix[0][3], matrix[1][3], matrix[2][3], matrix[3][3]);
    std::array<glm::vec4, 6> planes{{row3 + row0, row3 - row0, row3 + row1,
                                     row3 - row1, row3 + row2, row3 - row2}};
    for (glm::vec4& plane : planes) {
        const float length = glm::length(glm::vec3(plane));
        if (length > 0.0f) plane /= length;
    }
    return planes;
}

bool chunkInFrustum(const Chunk& chunk, const std::array<glm::vec4, 6>& planes) {
    const glm::vec3 minimum(static_cast<float>(chunk.x * CHUNK_SIZE), 0.0f,
                            static_cast<float>(chunk.z * CHUNK_SIZE));
    const glm::vec3 maximum = minimum + glm::vec3(CHUNK_SIZE, WORLD_HEIGHT, CHUNK_SIZE);
    for (const glm::vec4& plane : planes) {
        const glm::vec3 positive(plane.x >= 0.0f ? maximum.x : minimum.x,
                                 plane.y >= 0.0f ? maximum.y : minimum.y,
                                 plane.z >= 0.0f ? maximum.z : minimum.z);
        if (glm::dot(glm::vec3(plane), positive) + plane.w < 0.0f) return false;
    }
    return true;
}

void uploadMesh(GLuint& vao, GLuint& vbo, GLsizei& count, const std::vector<VoxelVertex>& vertices) {
    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(VoxelVertex)),
                 vertices.empty() ? nullptr : vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, normal)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, sunLight)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, blockLight)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, ao)));
    glBindVertexArray(0);
    count = static_cast<GLsizei>(vertices.size());
}
}

Chunk::Chunk(int chunkX, int chunkZ, std::vector<Block> data) : x(chunkX), z(chunkZ), blocks(std::move(data)) {
    if (blocks.empty()) blocks.resize(static_cast<std::size_t>(CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE), Block::Air);
    sunlight.resize(blocks.size(), 0);
    blockLight.resize(blocks.size(), 0);
}

Chunk::~Chunk() {
    if (opaqueVbo) glDeleteBuffers(1, &opaqueVbo);
    if (opaqueVao) glDeleteVertexArrays(1, &opaqueVao);
    if (waterVbo) glDeleteBuffers(1, &waterVbo);
    if (waterVao) glDeleteVertexArrays(1, &waterVao);
}

Block Chunk::getLocal(int localX, int y, int localZ) const {
    if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 || localZ >= CHUNK_SIZE || y < 0 || y >= WORLD_HEIGHT) return Block::Air;
    return blocks[static_cast<std::size_t>((y * CHUNK_SIZE + localZ) * CHUNK_SIZE + localX)];
}

void Chunk::setLocal(int localX, int y, int localZ, Block block) {
    if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 || localZ >= CHUNK_SIZE || y < 0 || y >= WORLD_HEIGHT) return;
    blocks[static_cast<std::size_t>((y * CHUNK_SIZE + localZ) * CHUNK_SIZE + localX)] = block;
}

World::World(std::uint32_t seed)
    : terrainNoise_(seed), biomeNoise_(seed ^ 0x517cc1b7U), caveNoise_(seed ^ 0x9e3779b9U), seed_(seed) {
    const unsigned int hardware = std::thread::hardware_concurrency();
    const unsigned int workerCount = std::max(1U, std::min(4U, hardware > 1U ? hardware - 1U : 1U));
    for (unsigned int i = 0; i < workerCount; ++i) workers_.emplace_back(&World::workerLoop, this);
}

World::~World() {
    stopping_ = true;
    generationCv_.notify_all();
    for (std::thread& worker : workers_) if (worker.joinable()) worker.join();
}

std::int64_t World::chunkKey(int chunkX, int chunkZ) {
    const std::uint64_t high = static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunkX)) << 32U;
    return static_cast<std::int64_t>(high | static_cast<std::uint32_t>(chunkZ));
}

int World::floorDiv(int value, int divisor) {
    int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) --quotient;
    return quotient;
}

int World::floorMod(int value, int divisor) {
    const int remainder = value % divisor;
    return remainder < 0 ? remainder + std::abs(divisor) : remainder;
}

std::size_t World::localIndex(int localX, int y, int localZ) {
    return static_cast<std::size_t>((y * CHUNK_SIZE + localZ) * CHUNK_SIZE + localX);
}

Chunk* World::findChunk(int chunkX, int chunkZ) {
    const auto found = chunks_.find(chunkKey(chunkX, chunkZ));
    return found == chunks_.end() ? nullptr : found->second.get();
}

const Chunk* World::findChunk(int chunkX, int chunkZ) const {
    const auto found = chunks_.find(chunkKey(chunkX, chunkZ));
    return found == chunks_.end() ? nullptr : found->second.get();
}

World::TerrainSample World::sampleTerrain(int worldX, int worldZ) const {
    const float x = static_cast<float>(worldX);
    const float z = static_cast<float>(worldZ);
    const float continental = terrainNoise_.fractal2D(x * 0.006f, z * 0.006f, 5, 2.0f, 0.5f);
    const float detail = terrainNoise_.fractal2D(x * 0.027f + 41.0f, z * 0.027f - 17.0f, 3, 2.0f, 0.5f);
    const float moisture = biomeNoise_.fractal2D(x * 0.004f - 77.0f, z * 0.004f + 23.0f, 3, 2.0f, 0.55f);
    const float temperature = biomeNoise_.fractal2D(x * 0.0025f + 120.0f, z * 0.0025f - 90.0f, 2, 2.0f, 0.5f);
    const float mountainNoise = biomeNoise_.fractal2D(x * 0.0032f + 210.0f, z * 0.0032f + 160.0f, 4, 2.0f, 0.52f);

    Biome biome = Biome::Plains;
    if (mountainNoise > 0.24f) biome = Biome::Mountains;
    else if (temperature > -0.02f && moisture < -0.08f) biome = Biome::Desert;
    else if (moisture > 0.07f) biome = Biome::Forest;

    float height = 28.0f + continental * 11.0f + detail * 2.5f;
    if (biome == Biome::Plains) height = 31.0f + continental * 7.0f + detail * 2.0f;
    if (biome == Biome::Forest) height = 32.0f + continental * 9.0f + detail * 2.5f;
    if (biome == Biome::Desert) height = 29.0f + continental * 6.0f + detail * 1.5f;
    if (biome == Biome::Mountains) {
        const float ridge = std::clamp((mountainNoise - 0.16f) / 0.40f, 0.0f, 1.0f);
        const float sharpRidge = 1.0f - std::abs(terrainNoise_.noise(x * 0.018f + 19.0f, 0.0f, z * 0.018f - 41.0f));
        height = 35.0f + continental * 10.0f + ridge * ridge * 38.0f + std::pow(sharpRidge, 3.0f) * ridge * 15.0f;
    }
    const float river = std::abs(biomeNoise_.fractal2D(x * 0.0022f + 330.0f, z * 0.0022f - 270.0f, 3, 2.0f, 0.5f));
    if (biome != Biome::Mountains && river < 0.045f) {
        const float riverStrength = 1.0f - river / 0.045f;
        height = glm::mix(height, static_cast<float>(SEA_LEVEL - 3), riverStrength * riverStrength);
    }
    return {std::clamp(static_cast<int>(std::round(height)), 6, WORLD_HEIGHT - 12), biome};
}

int World::terrainHeight(int worldX, int worldZ) const { return sampleTerrain(worldX, worldZ).height; }

glm::vec3 World::findSafeSpawnNear(int worldX, int worldZ) const {
    const int centerChunkX = floorDiv(worldX, CHUNK_SIZE);
    const int centerChunkZ = floorDiv(worldZ, CHUNK_SIZE);
    glm::vec3 best(static_cast<float>(worldX) + 0.5f,
                   static_cast<float>(terrainHeight(worldX, worldZ)) + 2.01f,
                   static_cast<float>(worldZ) + 0.5f);

    for (int ring = 0; ring <= 3; ++ring) {
        float bestScore = std::numeric_limits<float>::max();
        bool found = false;
        for (int dz = -ring; dz <= ring; ++dz) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (ring > 0 && std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                const int chunkX = centerChunkX + dx;
                const int chunkZ = centerChunkZ + dz;
                GeneratedChunk generated = generateChunkData(chunkX, chunkZ);
                const auto changed = edits_.find(chunkKey(chunkX, chunkZ));
                if (changed != edits_.end()) {
                    for (const auto& edit : changed->second) generated.blocks[edit.first] = edit.second;
                }

                for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
                    for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
                        int surfaceY = -1;
                        Block surface = Block::Air;
                        for (int y = WORLD_HEIGHT - 3; y >= 1; --y) {
                            const Block block = generated.blocks[localIndex(localX, y, localZ)];
                            if (block == Block::Air || block == Block::Torch || isCrop(block)) continue;
                            surfaceY = y;
                            surface = block;
                            break;
                        }
                        if (surfaceY < 0 || surface == Block::Water || surface == Block::Leaves ||
                            surface == Block::Log || !isSolid(surface)) continue;
                        const Block feet = generated.blocks[localIndex(localX, surfaceY + 1, localZ)];
                        const Block head = generated.blocks[localIndex(localX, surfaceY + 2, localZ)];
                        if (isSolid(feet) || isWater(feet) || isSolid(head) || isWater(head)) continue;

                        const int candidateX = chunkX * CHUNK_SIZE + localX;
                        const int candidateZ = chunkZ * CHUNK_SIZE + localZ;
                        const float offsetX = static_cast<float>(candidateX - worldX);
                        const float offsetZ = static_cast<float>(candidateZ - worldZ);
                        const float naturalPenalty = (surface == Block::Grass || surface == Block::Sand) ? 0.0f : 96.0f;
                        const float score = offsetX * offsetX + offsetZ * offsetZ + naturalPenalty;
                        if (score < bestScore) {
                            bestScore = score;
                            best = {static_cast<float>(candidateX) + 0.5f,
                                    static_cast<float>(surfaceY) + 1.01f,
                                    static_cast<float>(candidateZ) + 0.5f};
                            found = true;
                        }
                    }
                }
            }
        }
        if (found) return best;
    }
    return best;
}

std::string World::biomeNameAt(int worldX, int worldZ) const {
    switch (sampleTerrain(worldX, worldZ).biome) {
        case Biome::Forest: return "FOREST";
        case Biome::Desert: return "DESERT";
        case Biome::Mountains: return "MOUNTAINS";
        default: return "PLAINS";
    }
}

World::GeneratedChunk World::generateChunkData(int chunkX, int chunkZ) const {
    GeneratedChunk result{chunkX, chunkZ,
        std::vector<Block>(static_cast<std::size_t>(CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE), Block::Air)};
    auto getLocal = [&](int lx, int y, int lz) -> Block& { return result.blocks[localIndex(lx, y, lz)]; };

    for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
        for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
            const int worldX = chunkX * CHUNK_SIZE + localX;
            const int worldZ = chunkZ * CHUNK_SIZE + localZ;
            const TerrainSample sample = sampleTerrain(worldX, worldZ);
            const float ravine = std::abs(caveNoise_.noise(worldX * 0.013f + 71.0f, 0.0f, worldZ * 0.013f - 93.0f));
            for (int y = 0; y <= sample.height; ++y) {
                Block block = Block::Stone;
                const bool sandy = sample.biome == Biome::Desert || sample.height <= SEA_LEVEL + 2;
                if (y == sample.height) block = sandy ? Block::Sand : Block::Grass;
                else if (y >= sample.height - 3) block = sandy ? Block::Sand : Block::Dirt;
                if (y > 4 && y < sample.height - 5) {
                    const float cavern = caveNoise_.fractal3D(worldX * 0.034f, y * 0.041f,
                                                              worldZ * 0.034f, 4, 2.0f, 0.52f);
                    const float tunnelA = std::abs(caveNoise_.noise(worldX * 0.026f + 17.0f,
                                                                   y * 0.031f, worldZ * 0.026f - 29.0f));
                    const float tunnelB = caveNoise_.noise(worldX * 0.019f - 61.0f,
                                                           y * 0.024f + 13.0f, worldZ * 0.019f + 44.0f);
                    const bool ravineCut = ravine < 0.022f && y > 7 && y < sample.height - 3 && tunnelB < 0.42f;
                    if (cavern > 0.455f || (tunnelA < 0.047f && tunnelB > 0.08f) || ravineCut) block = Block::Air;
                }
                if (block == Block::Stone) {
                    const float coal = terrainNoise_.noise(worldX * 0.105f + 19.0f, y * 0.115f,
                                                           worldZ * 0.105f - 37.0f);
                    const float iron = terrainNoise_.noise(worldX * 0.125f - 83.0f, y * 0.130f + 31.0f,
                                                           worldZ * 0.125f + 71.0f);
                    const float gold = terrainNoise_.noise(worldX * 0.145f + 151.0f, y * 0.150f - 47.0f,
                                                           worldZ * 0.145f - 113.0f);
                    const float copper = terrainNoise_.noise(worldX * 0.118f - 211.0f, y * 0.122f + 81.0f,
                                                             worldZ * 0.118f + 193.0f);
                    const float diamond = terrainNoise_.noise(worldX * 0.165f + 271.0f, y * 0.170f + 117.0f,
                                                              worldZ * 0.165f - 239.0f);
                    if (y < 16 && diamond > 0.59f) block = Block::DiamondOre;
                    else if (y < 24 && gold > 0.54f) block = Block::GoldOre;
                    else if (y < 42 && copper > 0.50f) block = Block::CopperOre;
                    else if (y < 48 && iron > 0.52f) block = Block::IronOre;
                    else if (y < 76 && coal > 0.50f) block = Block::CoalOre;
                }
                if (block == Block::Air && y <= 12) block = Block::Water;
                getLocal(localX, y, localZ) = block;
            }
            for (int y = sample.height + 1; y <= SEA_LEVEL; ++y) getLocal(localX, y, localZ) = Block::Water;
        }
    }

    // Evaluate roots outside this chunk too, so trees cross chunk borders deterministically.
    for (int rootZ = chunkZ * CHUNK_SIZE - 3; rootZ < (chunkZ + 1) * CHUNK_SIZE + 3; ++rootZ) {
        for (int rootX = chunkX * CHUNK_SIZE - 3; rootX < (chunkX + 1) * CHUNK_SIZE + 3; ++rootX) {
            const TerrainSample ground = sampleTerrain(rootX, rootZ);
            const std::uint32_t hash = positionHash(rootX, rootZ, seed_);
            if (ground.biome != Biome::Forest || ground.height <= SEA_LEVEL + 1 || hash % 47U != 0U) continue;
            const int trunkHeight = 4 + static_cast<int>((hash >> 8U) % 3U);
            auto place = [&](int wx, int y, int wz, Block block) {
                const int lx = wx - chunkX * CHUNK_SIZE;
                const int lz = wz - chunkZ * CHUNK_SIZE;
                if (lx < 0 || lx >= CHUNK_SIZE || lz < 0 || lz >= CHUNK_SIZE || y < 0 || y >= WORLD_HEIGHT) return;
                Block& current = getLocal(lx, y, lz);
                if (block == Block::Log || current == Block::Air || current == Block::Water) current = block;
            };
            for (int y = 1; y <= trunkHeight; ++y) place(rootX, ground.height + y, rootZ, Block::Log);
            const int crownY = ground.height + trunkHeight;
            for (int dy = -2; dy <= 2; ++dy) {
                const int radius = dy >= 1 ? 1 : 2;
                for (int dz = -radius; dz <= radius; ++dz) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        if (std::abs(dx) == radius && std::abs(dz) == radius && ((hash + dy) & 1U)) continue;
                        if (dx == 0 && dz == 0 && dy <= 0) continue;
                        place(rootX + dx, crownY + dy, rootZ + dz, Block::Leaves);
                    }
                }
            }
            place(rootX, crownY + 2, rootZ, Block::Leaves);
        }
    }
    return result;
}

void World::workerLoop() {
    while (!stopping_) {
        glm::ivec2 request;
        {
            std::unique_lock<std::mutex> lock(generationMutex_);
            generationCv_.wait(lock, [&] { return stopping_ || !generationQueue_.empty(); });
            if (stopping_) return;
            request = generationQueue_.front();
            generationQueue_.pop_front();
        }
        GeneratedChunk generated = generateChunkData(request.x, request.y);
        {
            std::lock_guard<std::mutex> lock(generationMutex_);
            completedQueue_.push_back(std::move(generated));
        }
    }
}

void World::generate(int renderDistance, const glm::vec3& initialPosition) {
    renderDistance_ = std::clamp(renderDistance, 2, 16);
    chunks_.clear();
    const int centerX = floorDiv(static_cast<int>(std::floor(initialPosition.x)), CHUNK_SIZE);
    const int centerZ = floorDiv(static_cast<int>(std::floor(initialPosition.z)), CHUNK_SIZE);
    GeneratedChunk center = generateChunkData(centerX, centerZ);
    const std::int64_t key = chunkKey(centerX, centerZ);
    const auto savedEdits = edits_.find(key);
    if (savedEdits != edits_.end()) {
        for (const auto& edit : savedEdits->second) center.blocks[edit.first] = edit.second;
    }
    auto centerChunk = std::make_unique<Chunk>(centerX, centerZ, std::move(center.blocks));
    computeSunlight(*centerChunk);
    chunks_.emplace(key, std::move(centerChunk));
    rebuildChunk(centerX, centerZ);
    streamCenter_ = {1000000, 1000000};
    requestChunksAround(centerX, centerZ);
}

void World::setRenderDistance(int value){renderDistance_=std::clamp(value,2,16);streamCenter_={1000000,1000000};}

void World::requestChunksAround(int centerX, int centerZ) {
    struct Request { int x; int z; int distanceSquared; };
    std::vector<Request> requests;
    for (int dz = -renderDistance_; dz <= renderDistance_; ++dz) {
        for (int dx = -renderDistance_; dx <= renderDistance_; ++dx) {
            if (dx * dx + dz * dz > renderDistance_ * renderDistance_) continue;
            const int x = centerX + dx;
            const int z = centerZ + dz;
            if (findChunk(x, z)) continue;
            requests.push_back({x, z, dx * dx + dz * dz});
        }
    }
    std::sort(requests.begin(), requests.end(), [](const Request& a, const Request& b) {
        return a.distanceSquared < b.distanceSquared;
    });
    {
        std::lock_guard<std::mutex> lock(generationMutex_);
        for (const Request& request : requests) {
            const std::int64_t key = chunkKey(request.x, request.z);
            if (pendingKeys_.insert(key).second) generationQueue_.push_back({request.x, request.z});
        }
    }
    generationCv_.notify_all();
}

void World::integrateCompleted(int budget, int centerX, int centerZ) {
    for (int integrated = 0; integrated < budget; ++integrated) {
        GeneratedChunk generated;
        {
            std::lock_guard<std::mutex> lock(generationMutex_);
            if (completedQueue_.empty()) break;
            generated = std::move(completedQueue_.front());
            completedQueue_.pop_front();
            pendingKeys_.erase(chunkKey(generated.x, generated.z));
        }
        const int dx = generated.x - centerX;
        const int dz = generated.z - centerZ;
        if (dx * dx + dz * dz > (renderDistance_ + 1) * (renderDistance_ + 1)) continue;
        const std::int64_t key = chunkKey(generated.x, generated.z);
        if (chunks_.find(key) != chunks_.end()) continue;
        const auto edits = edits_.find(key);
        if (edits != edits_.end()) {
            for (const auto& edit : edits->second) {
                generated.blocks[edit.first] = edit.second;
                if (edit.second == Block::Torch) blockLightingDirty_ = true;
            }
        }
        auto loadedChunk = std::make_unique<Chunk>(generated.x, generated.z, std::move(generated.blocks));
        computeSunlight(*loadedChunk);
        chunks_.emplace(key, std::move(loadedChunk));
        markDirty(generated.x, generated.z);
        markDirty(generated.x + 1, generated.z);
        markDirty(generated.x - 1, generated.z);
        markDirty(generated.x, generated.z + 1);
        markDirty(generated.x, generated.z - 1);
    }
}

void World::unloadDistant(int centerX, int centerZ) {
    const int unloadDistance = renderDistance_ + 2;
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        const int dx = it->second->x - centerX;
        const int dz = it->second->z - centerZ;
        if (dx * dx + dz * dz > unloadDistance * unloadDistance) {
            dirtySet_.erase(it->first);
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
}

void World::updateStreaming(const glm::vec3& playerPosition, int meshBudget) {
    const int centerX = floorDiv(static_cast<int>(std::floor(playerPosition.x)), CHUNK_SIZE);
    const int centerZ = floorDiv(static_cast<int>(std::floor(playerPosition.z)), CHUNK_SIZE);
    integrateCompleted(4, centerX, centerZ);
    if (blockLightingDirty_) { rebuildBlockLighting(); blockLightingDirty_ = false; }
    if (streamCenter_.x != centerX || streamCenter_.y != centerZ) {
        streamCenter_ = {centerX, centerZ};
        unloadDistant(centerX, centerZ);
        requestChunksAround(centerX, centerZ);
    }
    for (int i = 0; i < meshBudget && !dirtyQueue_.empty(); ++i) {
        const std::int64_t key = dirtyQueue_.front();
        dirtyQueue_.pop_front();
        if (dirtySet_.erase(key) == 0) { --i; continue; }
        const auto found = chunks_.find(key);
        if (found != chunks_.end()) rebuildChunk(found->second->x, found->second->z);
    }
}

bool World::loadWorld(const std::string& path, glm::vec3& playerPosition) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    char magic[8]{};
    std::uint32_t version = 0;
    std::uint32_t savedSeed = 0;
    std::uint64_t editCount = 0;
    input.read(magic, sizeof(magic));
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    input.read(reinterpret_cast<char*>(&savedSeed), sizeof(savedSeed));
    input.read(reinterpret_cast<char*>(&playerPosition.x), sizeof(float));
    input.read(reinterpret_cast<char*>(&playerPosition.y), sizeof(float));
    input.read(reinterpret_cast<char*>(&playerPosition.z), sizeof(float));
    input.read(reinterpret_cast<char*>(&editCount), sizeof(editCount));
    const char expected[8] = {'V','X','W','O','R','L','D','3'};
    if (!input || std::memcmp(magic, expected, sizeof(magic)) != 0 || version != 1U ||
        savedSeed != seed_ || editCount > 10000000ULL) return false;

    edits_.clear();
    for (std::uint64_t i = 0; i < editCount; ++i) {
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;
        std::uint32_t index = 0;
        std::uint8_t value = 0;
        input.read(reinterpret_cast<char*>(&chunkX), sizeof(chunkX));
        input.read(reinterpret_cast<char*>(&chunkZ), sizeof(chunkZ));
        input.read(reinterpret_cast<char*>(&index), sizeof(index));
        input.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (!input || index >= static_cast<std::uint32_t>(CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE) ||
            value > static_cast<std::uint8_t>(Block::Crop3)) {
            edits_.clear();
            return false;
        }
        edits_[chunkKey(chunkX, chunkZ)][index] = static_cast<Block>(value);
    }
    return true;
}

bool World::saveWorld(const std::string& path, const glm::vec3& playerPosition) const {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    const char magic[8] = {'V','X','W','O','R','L','D','3'};
    const std::uint32_t version = 1U;
    std::uint64_t editCount = 0;
    for (const auto& chunkEdits : edits_) editCount += chunkEdits.second.size();
    output.write(magic, sizeof(magic));
    output.write(reinterpret_cast<const char*>(&version), sizeof(version));
    output.write(reinterpret_cast<const char*>(&seed_), sizeof(seed_));
    output.write(reinterpret_cast<const char*>(&playerPosition.x), sizeof(float));
    output.write(reinterpret_cast<const char*>(&playerPosition.y), sizeof(float));
    output.write(reinterpret_cast<const char*>(&playerPosition.z), sizeof(float));
    output.write(reinterpret_cast<const char*>(&editCount), sizeof(editCount));
    for (const auto& chunkEdits : edits_) {
        const std::uint64_t packed = static_cast<std::uint64_t>(chunkEdits.first);
        const std::int32_t chunkX = static_cast<std::int32_t>(packed >> 32U);
        const std::int32_t chunkZ = static_cast<std::int32_t>(packed & 0xffffffffU);
        for (const auto& edit : chunkEdits.second) {
            const std::uint32_t index = static_cast<std::uint32_t>(edit.first);
            const std::uint8_t value = static_cast<std::uint8_t>(edit.second);
            output.write(reinterpret_cast<const char*>(&chunkX), sizeof(chunkX));
            output.write(reinterpret_cast<const char*>(&chunkZ), sizeof(chunkZ));
            output.write(reinterpret_cast<const char*>(&index), sizeof(index));
            output.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
    }
    return output.good();
}
int World::pendingChunkCount() const {
    std::lock_guard<std::mutex> lock(generationMutex_);
    return static_cast<int>(pendingKeys_.size());
}

void World::computeSunlight(Chunk& chunk) {
    for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
        for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
            std::uint8_t light = 15;
            for (int y = WORLD_HEIGHT - 1; y >= 0; --y) {
                const std::size_t index = localIndex(localX, y, localZ);
                const Block block = chunk.blocks[index];
                if (occludesLight(block)) light = 0;
                else if ((block == Block::Leaves || block == Block::Water) && light > 0) --light;
                chunk.sunlight[index] = light;
            }
        }
    }
}

void World::rebuildBlockLighting() {
    struct LightNode { int x; int y; int z; std::uint8_t level; };
    std::deque<LightNode> queue;
    for (auto& entry : chunks_) {
        Chunk& chunk = *entry.second;
        std::fill(chunk.blockLight.begin(), chunk.blockLight.end(), std::uint8_t{0});
        for (int y = 0; y < WORLD_HEIGHT; ++y) for (int z = 0; z < CHUNK_SIZE; ++z) for (int x = 0; x < CHUNK_SIZE; ++x) {
            if (chunk.getLocal(x, y, z) == Block::Torch) {
                chunk.blockLight[localIndex(x, y, z)] = 15;
                queue.push_back({chunk.x * CHUNK_SIZE + x, y, chunk.z * CHUNK_SIZE + z, 15});
            }
        }
    }
    constexpr glm::ivec3 directions[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    while (!queue.empty()) {
        const LightNode node = queue.front(); queue.pop_front();
        if (node.level <= 1) continue;
        const std::uint8_t nextLevel = static_cast<std::uint8_t>(node.level - 1);
        for (const glm::ivec3& direction : directions) {
            const int x = node.x + direction.x, y = node.y + direction.y, z = node.z + direction.z;
            if (y < 0 || y >= WORLD_HEIGHT || occludesLight(getBlock(x, y, z))) continue;
            Chunk* chunk = findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE));
            if (!chunk) continue;
            const std::size_t index = localIndex(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE));
            if (chunk->blockLight[index] >= nextLevel) continue;
            chunk->blockLight[index] = nextLevel;
            queue.push_back({x, y, z, nextLevel});
        }
    }
    for (const auto& entry : chunks_) markDirty(entry.second->x, entry.second->z);
}

std::uint8_t World::sunlightAt(int x, int y, int z) const {
    if (y >= WORLD_HEIGHT) return 15;
    if (y < 0) return 0;
    const Chunk* chunk = findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE));
    if (!chunk) return 0;
    return chunk->sunlight[localIndex(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE))];
}

std::uint8_t World::blockLightAt(int x, int y, int z) const {
    if (y < 0 || y >= WORLD_HEIGHT) return 0;
    const Chunk* chunk = findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE));
    if (!chunk) return 0;
    return chunk->blockLight[localIndex(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE))];
}

bool World::hasBlockNear(const glm::vec3& position, Block block, float radius) const {
    const glm::ivec3 minimum = glm::ivec3(glm::floor(position - glm::vec3(radius)));
    const glm::ivec3 maximum = glm::ivec3(glm::floor(position + glm::vec3(radius)));
    for (int y = std::max(0, minimum.y); y <= std::min(WORLD_HEIGHT - 1, maximum.y); ++y)
        for (int z = minimum.z; z <= maximum.z; ++z)
            for (int x = minimum.x; x <= maximum.x; ++x)
                if (getBlock(x, y, z) == block) return true;
    return false;
}
Block World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= WORLD_HEIGHT) return Block::Air;
    const Chunk* chunk = findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE));
    if (!chunk) return Block::Air;
    return chunk->getLocal(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE));
}

bool World::isSolidAt(int x, int y, int z) const { return isSolid(getBlock(x, y, z)); }

void World::setBlock(int x, int y, int z, Block block) {
    if (y < 0 || y >= WORLD_HEIGHT) return;
    const int chunkX = floorDiv(x, CHUNK_SIZE);
    const int chunkZ = floorDiv(z, CHUNK_SIZE);
    Chunk* chunk = findChunk(chunkX, chunkZ);
    if (!chunk) return;
    const int localX = floorMod(x, CHUNK_SIZE);
    const int localZ = floorMod(z, CHUNK_SIZE);
    chunk->setLocal(localX, y, localZ, block);
    computeSunlight(*chunk);
    blockLightingDirty_ = true;
    edits_[chunkKey(chunkX, chunkZ)][localIndex(localX, y, localZ)] = block;
    rebuildTouchedChunks(x, z);
}

void World::markDirty(int chunkX, int chunkZ) {
    if (!findChunk(chunkX, chunkZ)) return;
    const std::int64_t key = chunkKey(chunkX, chunkZ);
    if (dirtySet_.insert(key).second) dirtyQueue_.push_back(key);
}

void World::rebuildTouchedChunks(int worldX, int worldZ) {
    const int chunkX = floorDiv(worldX, CHUNK_SIZE);
    const int chunkZ = floorDiv(worldZ, CHUNK_SIZE);
    markDirty(chunkX, chunkZ);
    const int localX = floorMod(worldX, CHUNK_SIZE);
    const int localZ = floorMod(worldZ, CHUNK_SIZE);
    if (localX == 0) markDirty(chunkX - 1, chunkZ);
    if (localX == CHUNK_SIZE - 1) markDirty(chunkX + 1, chunkZ);
    if (localZ == 0) markDirty(chunkX, chunkZ - 1);
    if (localZ == CHUNK_SIZE - 1) markDirty(chunkX, chunkZ + 1);
}

void World::rebuildChunk(int chunkX, int chunkZ) {
    Chunk* chunk = findChunk(chunkX, chunkZ);
    if (!chunk) return;
    std::vector<VoxelVertex> opaqueVertices;
    std::vector<VoxelVertex> waterVertices;
    opaqueVertices.reserve(12000);
    waterVertices.reserve(3000);


    for (int y = 0; y < WORLD_HEIGHT; ++y) {
        for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
            for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
                const Block block = chunk->getLocal(localX, y, localZ);
                if (!isRenderable(block)) continue;
                const int worldX = chunkX * CHUNK_SIZE + localX;
                const int worldZ = chunkZ * CHUNK_SIZE + localZ;
                for (int face = 0; face < 6; ++face) {
                    const glm::ivec3 normal = FaceNormals[static_cast<std::size_t>(face)];
                    const Block neighbor = getBlock(worldX + normal.x, y + normal.y, worldZ + normal.z);
                    if (isWater(block)) {
                        if (neighbor != Block::Air) continue;
                    } else if (neighbor != Block::Air && !isWater(neighbor)) {
                        continue;
                    }
                    const auto uvs = tileUvs(atlasTile(block, face), face != 2 && face != 3);
                    float cornerSun[4]{}, cornerBlock[4]{}, cornerAo[4]{1,1,1,1};
                    int tangentAxes[2] = {0, 0};
                    int tangentCount = 0;
                    for (int axis = 0; axis < 3; ++axis) if (normal[axis] == 0) tangentAxes[tangentCount++] = axis;
                    for (int corner = 0; corner < 4; ++corner) {
                        const float* cornerPosition = FaceCorners[face][corner];
                        glm::ivec3 sideA = normal, sideB = normal, diagonal = normal;
                        const int signA = cornerPosition[tangentAxes[0]] > 0.5f ? 1 : -1;
                        const int signB = cornerPosition[tangentAxes[1]] > 0.5f ? 1 : -1;
                        sideA[tangentAxes[0]] += signA;
                        sideB[tangentAxes[1]] += signB;
                        diagonal[tangentAxes[0]] += signA;
                        diagonal[tangentAxes[1]] += signB;
                        const auto occupied = [&](const glm::ivec3& offset) {
                            return isSolid(getBlock(worldX + offset.x, y + offset.y, worldZ + offset.z));
                        };
                        if (!isWater(block)) {
                            const bool a = occupied(sideA), b = occupied(sideB), c = occupied(diagonal);
                            const int ao = (a && b) ? 0 : 3 - static_cast<int>(a) - static_cast<int>(b) - static_cast<int>(c);
                            cornerAo[corner] = 0.64f + static_cast<float>(ao) * 0.12f;
                        }
                        const std::array<glm::ivec3, 4> samples{{normal, sideA, sideB, diagonal}};
                        std::uint8_t sun = 0, emitted = 0;
                        for (const glm::ivec3& offset : samples) {
                            sun = std::max(sun, sunlightAt(worldX + offset.x, y + offset.y, worldZ + offset.z));
                            emitted = std::max(emitted, blockLightAt(worldX + offset.x, y + offset.y, worldZ + offset.z));
                        }
                        cornerSun[corner] = static_cast<float>(sun) / 15.0f;
                        cornerBlock[corner] = static_cast<float>(emitted) / 15.0f;
                    }
                    std::vector<VoxelVertex>& vertices = isWater(block) ? waterVertices : opaqueVertices;
                    for (int vertex = 0; vertex < 6; ++vertex) {
                        const int corner = Indices[vertex];
                        const float* p = FaceCorners[face][corner];
                        float py = p[1];
                        if (isWater(block) && py > 0.5f) py = 0.86f;
                        vertices.push_back({{worldX + p[0], y + py, worldZ + p[2]},
                                            uvs[static_cast<std::size_t>(corner)], glm::vec3(normal),
                                            cornerSun[corner], cornerBlock[corner], cornerAo[corner]});
                    }
                }
            }
        }
    }
    uploadMesh(chunk->opaqueVao, chunk->opaqueVbo, chunk->opaqueVertexCount, opaqueVertices);
    uploadMesh(chunk->waterVao, chunk->waterVbo, chunk->waterVertexCount, waterVertices);
}

void World::drawOpaque(const glm::mat4& viewProjection) const {
    const auto planes = frustumPlanes(viewProjection);
    for (const auto& entry : chunks_) {
        const Chunk& chunk = *entry.second;
        if (chunk.opaqueVertexCount == 0 || !chunkInFrustum(chunk, planes)) continue;
        glBindVertexArray(chunk.opaqueVao);
        glDrawArrays(GL_TRIANGLES, 0, chunk.opaqueVertexCount);
    }
    glBindVertexArray(0);
}

void World::drawWater(const glm::mat4& viewProjection, const glm::vec3& cameraPosition) const {
    const auto planes = frustumPlanes(viewProjection);
    std::vector<const Chunk*> visible;
    for (const auto& entry : chunks_) {
        if (entry.second->waterVertexCount > 0 && chunkInFrustum(*entry.second, planes)) visible.push_back(entry.second.get());
    }
    std::sort(visible.begin(), visible.end(), [&](const Chunk* a, const Chunk* b) {
        const glm::vec2 ac(a->x * CHUNK_SIZE + CHUNK_SIZE * 0.5f, a->z * CHUNK_SIZE + CHUNK_SIZE * 0.5f);
        const glm::vec2 bc(b->x * CHUNK_SIZE + CHUNK_SIZE * 0.5f, b->z * CHUNK_SIZE + CHUNK_SIZE * 0.5f);
        const glm::vec2 camera(cameraPosition.x, cameraPosition.z);
        return glm::dot(ac - camera, ac - camera) > glm::dot(bc - camera, bc - camera);
    });
    for (const Chunk* chunk : visible) {
        glBindVertexArray(chunk->waterVao);
        glDrawArrays(GL_TRIANGLES, 0, chunk->waterVertexCount);
    }
    glBindVertexArray(0);
}

bool World::raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RayHit& hit) const {
    const glm::vec3 ray = glm::normalize(direction);
    glm::ivec3 cell = glm::ivec3(glm::floor(origin));
    const glm::ivec3 step(ray.x >= 0.0f ? 1 : -1, ray.y >= 0.0f ? 1 : -1, ray.z >= 0.0f ? 1 : -1);
    const float infinity = std::numeric_limits<float>::infinity();
    const glm::vec3 delta(ray.x == 0.0f ? infinity : std::abs(1.0f / ray.x),
                          ray.y == 0.0f ? infinity : std::abs(1.0f / ray.y),
                          ray.z == 0.0f ? infinity : std::abs(1.0f / ray.z));
    glm::vec3 next(ray.x >= 0.0f ? (cell.x + 1.0f - origin.x) * delta.x : (origin.x - cell.x) * delta.x,
                   ray.y >= 0.0f ? (cell.y + 1.0f - origin.y) * delta.y : (origin.y - cell.y) * delta.y,
                   ray.z >= 0.0f ? (cell.z + 1.0f - origin.z) * delta.z : (origin.z - cell.z) * delta.z);
    float travelled = 0.0f;
    glm::ivec3 entryNormal(0);
    while (travelled <= maxDistance) {
        if (isRenderable(getBlock(cell.x, cell.y, cell.z)) && !isWater(getBlock(cell.x, cell.y, cell.z))) {
            hit = {cell, cell + entryNormal, entryNormal, travelled};
            return true;
        }
        if (next.x < next.y && next.x < next.z) {
            cell.x += step.x; travelled = next.x; next.x += delta.x; entryNormal = {-step.x, 0, 0};
        } else if (next.y < next.z) {
            cell.y += step.y; travelled = next.y; next.y += delta.y; entryNormal = {0, -step.y, 0};
        } else {
            cell.z += step.z; travelled = next.z; next.z += delta.z; entryNormal = {0, 0, -step.z};
        }
    }
    return false;
}

bool World::aabbIntersectsSolid(const glm::vec3& minimum, const glm::vec3& maximum) const {
    constexpr float epsilon = 0.0001f;
    const glm::ivec3 first = glm::ivec3(glm::floor(minimum + glm::vec3(epsilon)));
    const glm::ivec3 last = glm::ivec3(glm::floor(maximum - glm::vec3(epsilon)));
    for (int y = first.y; y <= last.y; ++y)
        for (int z = first.z; z <= last.z; ++z)
            for (int x = first.x; x <= last.x; ++x)
                if (isSolidAt(x, y, z)) return true;
    return false;
}

bool World::blockIntersectsAabb(const glm::ivec3& block, const glm::vec3& minimum, const glm::vec3& maximum) const {
    const glm::vec3 blockMin(block);
    const glm::vec3 blockMax = blockMin + glm::vec3(1.0f);
    return blockMin.x < maximum.x && blockMax.x > minimum.x && blockMin.y < maximum.y &&
           blockMax.y > minimum.y && blockMin.z < maximum.z && blockMax.z > minimum.z;
}

