#include "World.h"
#include "BiomeVegetation.h"
#include "AmbientOcclusion.h"

#include "Player.h"
#include "SaveFile.h"
#include "Definitions.h"
#include "RenderScratch.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
constexpr std::array<glm::ivec3, 6> FaceNormals{
    {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};

constexpr float FaceCorners[6][4][3] = {{{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
                                        {{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}},
                                        {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
                                        {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
                                        {{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}},
                                        {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}};
constexpr int Indices[6] = {0, 1, 2, 0, 2, 3};
constexpr int AtlasTiles = BlockAtlasTiles;
static_assert(AtlasTiles < 255, "Terrain material reserves tile 255 for custom UVs");

struct FaceOcclusion {
    bool hidden = false;
    float visibleMinimumY = 0.0f;
    float visibleMaximumY = 1.0f;
};

float geometryMinimum(const BlockGeometryProperties& geometry, int axis) {
    if (axis == 0)
        return geometry.minX;
    if (axis == 1)
        return geometry.minY;
    return geometry.minZ;
}

float geometryMaximum(const BlockGeometryProperties& geometry, int axis) {
    if (axis == 0)
        return geometry.maxX;
    if (axis == 1)
        return geometry.maxY;
    return geometry.maxZ;
}

FaceOcclusion calculateFaceOcclusion(
    Block block, Block neighbor, int face, float currentMinimumY, float currentMaximumY) {
    constexpr float epsilon = 0.001f;
    BlockGeometryProperties geometry = blockGeometry(block);
    geometry.minY = currentMinimumY;
    geometry.maxY = currentMaximumY;
    const BlockGeometryProperties neighborGeometry = blockGeometry(neighbor);
    FaceOcclusion result{false, geometry.minY, geometry.maxY};

    if (neighborGeometry.shape == BlockShape::Empty ||
        (!neighborGeometry.occludesNeighborFaces &&
         !matchingOcclusionGroup(block, neighbor))) {
        return result;
    }

    const int normalAxis = face < 2 ? 0 : (face < 4 ? 1 : 2);
    const bool positiveFace = (face % 2) == 0;
    const float ownBoundary = positiveFace ? geometryMaximum(geometry, normalAxis)
                                           : geometryMinimum(geometry, normalAxis);
    const float neighborBoundary = positiveFace
                                       ? geometryMinimum(neighborGeometry, normalAxis)
                                       : geometryMaximum(neighborGeometry, normalAxis);
    if ((positiveFace && (ownBoundary < 1.0f - epsilon || neighborBoundary > epsilon)) ||
        (!positiveFace && (ownBoundary > epsilon || neighborBoundary < 1.0f - epsilon))) {
        return result;
    }

    int tangentAxes[2] = {0, 0};
    int tangentCount = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (axis != normalAxis)
            tangentAxes[tangentCount++] = axis;
    }
    const auto containsAxis = [&](int axis) {
        return geometryMinimum(neighborGeometry, axis) <= geometryMinimum(geometry, axis) + epsilon &&
               geometryMaximum(neighborGeometry, axis) >= geometryMaximum(geometry, axis) - epsilon;
    };
    if (containsAxis(tangentAxes[0]) && containsAxis(tangentAxes[1])) {
        result.hidden = true;
        return result;
    }

    // All currently supported box-like partial blocks span the full horizontal
    // footprint. Their side overlap can therefore be represented by one clipped
    // vertical quad rather than emitting hidden geometry.
    if (normalAxis != 1) {
        const int horizontalTangent = normalAxis == 0 ? 2 : 0;
        if (!containsAxis(horizontalTangent))
            return result;
        const float overlapMinimum =
            std::max(geometry.minY, neighborGeometry.minY);
        const float overlapMaximum =
            std::min(geometry.maxY, neighborGeometry.maxY);
        if (overlapMaximum <= overlapMinimum + epsilon)
            return result;
        if (overlapMinimum <= geometry.minY + epsilon)
            result.visibleMinimumY = overlapMaximum;
        else if (overlapMaximum >= geometry.maxY - epsilon)
            result.visibleMaximumY = overlapMinimum;
    }
    return result;
}

std::uint32_t positionHash(int x, int z, std::uint32_t seed) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343U;
    h ^= static_cast<std::uint32_t>(z) * 0xd8163841U;
    h ^= seed * 0xcb1ab31fU;
    h ^= h >> 13U;
    h *= 0x85ebca6bU;
    return h ^ (h >> 16U);
}

int atlasTile(Block block, int faceIndex) {
    return blockTexture(block, faceIndex);
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
    std::array<glm::vec4, 6> planes{
        {row3 + row0, row3 - row0, row3 + row1, row3 - row1, row3 + row2, row3 - row2}};
    for (glm::vec4& plane : planes) {
        const float length = glm::length(glm::vec3(plane));
        if (length > 0.0f)
            plane /= length;
    }
    return planes;
}

bool boundsInFrustum(const glm::vec3& minimum, const glm::vec3& maximum,
                     const std::array<glm::vec4, 6>& planes) {
    for (const glm::vec4& plane : planes) {
        const glm::vec3 positive(plane.x >= 0.0f ? maximum.x : minimum.x,
                                 plane.y >= 0.0f ? maximum.y : minimum.y,
                                 plane.z >= 0.0f ? maximum.z : minimum.z);
        if (glm::dot(glm::vec3(plane), positive) + plane.w < 0.0f)
            return false;
    }
    return true;
}

bool chunkInFrustum(const Chunk& chunk, const std::array<glm::vec4, 6>& planes) {
    const glm::vec3 minimum(
        static_cast<float>(chunk.x * CHUNK_SIZE), 0.0f, static_cast<float>(chunk.z * CHUNK_SIZE));
    return boundsInFrustum(minimum, minimum + glm::vec3(CHUNK_SIZE, WORLD_HEIGHT, CHUNK_SIZE), planes);
}

bool meshInFrustum(const MeshBounds& bounds, const std::array<glm::vec4, 6>& planes) {
    return bounds.valid && boundsInFrustum(bounds.minimum, bounds.maximum, planes);
}

MeshBounds meshBounds(const std::vector<VoxelVertex>& vertices) {
    MeshBounds bounds;
    if (vertices.empty()) return bounds;
    bounds.valid = true;
    bounds.minimum = bounds.maximum = vertices.front().position;
    for (const VoxelVertex& vertex : vertices) {
        bounds.minimum = glm::min(bounds.minimum, vertex.position);
        bounds.maximum = glm::max(bounds.maximum, vertex.position);
    }
    // Include rasterization/rounding tolerance, scaled for distant coordinates
    // where a fixed 0.01-block margin alone may round away. The terrain shader
    // does not displace vertices; custom geometry is already in this mesh.
    const glm::vec3 magnitude = glm::max(glm::abs(bounds.minimum), glm::abs(bounds.maximum));
    const glm::vec3 padding = glm::vec3(0.01f) +
        magnitude * (32.0f * std::numeric_limits<float>::epsilon());
    bounds.minimum -= padding;
    bounds.maximum += padding;
    return bounds;
}

void configureTerrainAttributes() {
    const std::size_t offsets[] = {offsetof(VoxelVertex, position), offsetof(VoxelVertex, uv),
        offsetof(VoxelVertex, normal), offsetof(VoxelVertex, sunLight),
        offsetof(VoxelVertex, blockLight), offsetof(VoxelVertex, ao),
        offsetof(VoxelVertex, atlasTile)};
    const GLint sizes[] = {3, 2, 3, 1, 1, 1, 1};
    for (GLuint attribute = 0; attribute < 6; ++attribute) {
        glEnableVertexAttribArray(attribute);
        glVertexAttribPointer(attribute, sizes[attribute], GL_FLOAT, GL_FALSE,
                              sizeof(VoxelVertex), reinterpret_cast<void*>(offsets[attribute]));
    }
    glEnableVertexAttribArray(6);
    glVertexAttribIPointer(6,1,GL_UNSIGNED_INT,sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex,atlasTile)));
}

void uploadMesh(GLuint& vao,
                GLuint& vbo,
                GLsizei& count,
                std::size_t& capacity,
                const std::vector<VoxelVertex>& vertices) {
    if (vertices.empty()) {
        count = 0;
        return;
    }
    if (vao == 0)
        glGenVertexArrays(1, &vao);
    if (vbo == 0)
        glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    const std::size_t bytes = vertices.size() * sizeof(VoxelVertex);
    if (bytes > capacity) {
        capacity = std::max(bytes, capacity + capacity / 2);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr, GL_DYNAMIC_DRAW);
    }
    if (bytes > 0)
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), vertices.data());
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, normal)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,
                          1,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, sunLight)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4,
                          1,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, blockLight)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5,
                          1,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, ao)));
    glEnableVertexAttribArray(6);
    glVertexAttribIPointer(6,
                          1,
                          GL_UNSIGNED_INT,
                          sizeof(VoxelVertex),
                          reinterpret_cast<void*>(offsetof(VoxelVertex, atlasTile)));
    glBindVertexArray(0);
    count = static_cast<GLsizei>(vertices.size());
}
} // namespace

TerrainBufferPage::TerrainBufferPage(std::size_t vertices) : capacity(vertices) {
    freeRanges.push_back({0, capacity});
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(capacity * sizeof(VoxelVertex)),
                 nullptr, GL_DYNAMIC_DRAW);
    configureTerrainAttributes();
    glBindVertexArray(0);
}

TerrainBufferPage::~TerrainBufferPage() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

bool TerrainBufferPage::allocate(std::size_t count, std::size_t& first) {
    assert(count > 0);
    for (std::size_t index = 0; index < freeRanges.size(); ++index) {
        if (freeRanges[index].count < count) continue;
        // Reserve while allocation can fail, so chunk destruction/unload never
        // needs a heap allocation to return a range. Geometric capacity growth.
        const std::size_t releaseSlots = freeRanges.size() + allocationCount + 1;
        if (freeRanges.capacity() < releaseSlots)
            freeRanges.reserve(std::max(releaseSlots, freeRanges.capacity() * 2));
        auto& range = freeRanges[index];
        first = range.first;
        range.first += count;
        range.count -= count;
        if (!range.count) freeRanges.erase(freeRanges.begin() + index);
        allocated += count;
        ++allocationCount;
        return true;
    }
    return false;
}

void TerrainBufferPage::release(std::size_t first, std::size_t count) noexcept {
    assert(count && first <= capacity && count <= capacity - first && allocated >= count);
    assert(allocationCount && freeRanges.size() < freeRanges.capacity());
    auto next = std::lower_bound(freeRanges.begin(), freeRanges.end(), first,
        [](const Range& range, std::size_t offset) { return range.first < offset; });
    assert(next == freeRanges.end() || first + count <= next->first);
    assert(next == freeRanges.begin() || (next - 1)->first + (next - 1)->count <= first);
    auto current = freeRanges.insert(next, {first, count});
    if (current != freeRanges.begin() && (current - 1)->first + (current - 1)->count == first) {
        (current - 1)->count += count;
        current = freeRanges.erase(current) - 1;
    }
    next = current + 1;
    if (next != freeRanges.end() && current->first + current->count == next->first) {
        current->count += next->count;
        freeRanges.erase(next);
    }
    allocated -= count;
    --allocationCount;
}

std::size_t BlockEntityPositionHash::operator()(const BlockEntityPosition& position) const {
    std::size_t hash = std::hash<int>{}(position.x);
    hash ^= std::hash<int>{}(position.y) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    hash ^= std::hash<int>{}(position.z) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    return hash;
}

std::size_t World::FluidCellHash::operator()(const FluidCell& cell) const {
    std::size_t hash = std::hash<int>{}(cell.x);
    hash ^= std::hash<int>{}(cell.y) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    hash ^= std::hash<int>{}(cell.z) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    return hash;
}

Chunk::Chunk(int chunkX, int chunkZ, std::vector<Block> data)
    : x(chunkX), z(chunkZ), blocks(std::move(data)) {
    if (blocks.empty())
        blocks.resize(static_cast<std::size_t>(CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE), Block::Air);
    packedLight.resize(blocks.size(), 0);
    rebuildLightEmitterIndex();
    for (int y = WORLD_HEIGHT - 1; y >= 0 && highestRenderableY < 0; --y) {
        for (int localZ = 0; localZ < CHUNK_SIZE && highestRenderableY < 0; ++localZ) {
            for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
                if (isRenderable(blocks[static_cast<std::size_t>(
                        (y * CHUNK_SIZE + localZ) * CHUNK_SIZE + localX)])) {
                    highestRenderableY = y;
                    break;
                }
            }
        }
    }
}

Chunk::~Chunk() {
    if (opaquePage)
        opaquePage->release(opaqueFirstVertex, opaqueBufferCapacity / sizeof(VoxelVertex));
    if (waterVbo)
        glDeleteBuffers(1, &waterVbo);
    if (waterVao)
        glDeleteVertexArrays(1, &waterVao);
}

Block Chunk::getLocal(int localX, int y, int localZ) const {
    if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 || localZ >= CHUNK_SIZE || y < 0 ||
        y >= WORLD_HEIGHT)
        return Block::Air;
    return blocks[static_cast<std::size_t>((y * CHUNK_SIZE + localZ) * CHUNK_SIZE + localX)];
}

void Chunk::setLocal(int localX, int y, int localZ, Block block) {
    if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 || localZ >= CHUNK_SIZE || y < 0 ||
        y >= WORLD_HEIGHT)
        return;
    const auto index = static_cast<std::uint32_t>(
        (y * CHUNK_SIZE + localZ) * CHUNK_SIZE + localX);
    const bool wasEmitter = blockLightEmission(blocks[index]) != 0;
    const bool isEmitter = blockLightEmission(block) != 0;
    if (wasEmitter != isEmitter) {
        const auto position = std::lower_bound(lightEmitters.begin(), lightEmitters.end(), index);
        if (isEmitter)
            lightEmitters.insert(position, index);
        else
            lightEmitters.erase(position);
    }
    const auto column = static_cast<std::size_t>(localZ * CHUNK_SIZE + localX);
    const auto blocksRain = [](Block b) { return b != Block::Air && !isCrop(b) &&
        !isPlant(b) && !isWallAttachment(b) && !isTorch(b); };
    blocks[index] = block;
    if (blocksRain(block)) precipitationTop[column] = std::max(precipitationTop[column], static_cast<std::int16_t>(y));
    else if (precipitationTop[column] == y) {
        int top = y - 1;
        while (top >= 0 && !blocksRain(getLocal(localX,top,localZ))) --top;
        precipitationTop[column] = static_cast<std::int16_t>(top);
    }
    // An upper bound is sufficient. Leaving it high after a removal avoids a
    // full-height scan on the player edit path.
    if (isRenderable(block))
        highestRenderableY = std::max(highestRenderableY, y);
}

void Chunk::rebuildLightEmitterIndex() {
    lightEmitters.clear();
    precipitationTop.fill(-1);
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const Block b = blocks[index];
        if (blockLightEmission(b)) lightEmitters.push_back(static_cast<std::uint32_t>(index));
        if (b != Block::Air && !isPlant(b) && !isCrop(b) && !isWallAttachment(b) && !isTorch(b))
            precipitationTop[index % (CHUNK_SIZE*CHUNK_SIZE)] = static_cast<std::int16_t>(index / (CHUNK_SIZE*CHUNK_SIZE));
    }
}

World::World(std::uint32_t seed, std::uint32_t generationVersion)
    : terrainNoise_(seed), biomeNoise_(seed ^ 0x517cc1b7U), caveNoise_(seed ^ 0x9e3779b9U),
      continentalNoise_(seed ^ 0x5a21f29bU), erosionNoise_(seed ^ 0x8d4c03e7U),
      temperatureNoise_(seed ^ 0xb794de31U), humidityNoise_(seed ^ 0x41c9a62dU),
      weirdnessNoise_(seed ^ 0xe37219adU), ridgeNoise_(seed ^ 0x2f6b8053U),
      riverNoise_(seed ^ 0x7a316c5dU), aquiferNoise_(seed ^ 0xc45e1f09U),
      cheeseNoise_(seed ^ 0x91b37d65U), spaghettiNoise_(seed ^ 0x326fa1cbU),
      noodleNoise_(seed ^ 0xf84c2069U), canyonCarver_(seed), structures_(seed),
      seed_(seed) {
    generationVersion_ = std::clamp(generationVersion, 1U, 10U);
    const unsigned int hardware = std::thread::hardware_concurrency();
    const unsigned int workerCount =
        std::max(1U, std::min(3U, hardware > 2U ? hardware - 2U : 1U));
    for (unsigned int i = 0; i < workerCount; ++i)
        workers_.emplace_back(&World::workerLoop, this);
    meshWorker_ = std::thread(&World::meshWorkerLoop, this);
}

World::~World() {
    stopping_ = true;
    generationCv_.notify_all();
    meshCv_.notify_all();
    for (std::thread& worker : workers_)
        if (worker.joinable())
            worker.join();
    if (meshWorker_.joinable())
        meshWorker_.join();
}

std::int64_t World::chunkKey(int chunkX, int chunkZ) {
    const std::uint64_t high = static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunkX))
                               << 32U;
    return static_cast<std::int64_t>(high | static_cast<std::uint32_t>(chunkZ));
}

int World::floorDiv(int value, int divisor) {
    int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0)))
        --quotient;
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
    return generationVersion_ == 1 ? sampleTerrainLegacy(worldX, worldZ)
                                   : sampleTerrainModern(worldX, worldZ);
}

World::TerrainSample World::sampleTerrainModern(int worldX, int worldZ) const {
    if (generationVersion_ >= 10) return sampleTerrainGeography(worldX, worldZ);
    const float x = static_cast<float>(worldX);
    const float z = static_cast<float>(worldZ);
    const auto smoothRange = [](float lower, float upper, float value) {
        const float t = std::clamp((value - lower) / (upper - lower), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    TerrainSample sample;
    const float largeContinent =
        continentalNoise_.fractal2D(x * 0.0010f + 0.371f,
                                    z * 0.0010f - 0.613f, 3, 2.0f, 0.52f);
    const float continentalDetail = continentalNoise_.fractal2D(
        x * 0.00038f + 91.417f, z * 0.00038f - 37.283f, 2, 2.0f, 0.5f);
    // A wide, smoothly fading starter landmass avoids unsafe ocean spawns for
    // unlucky seeds without changing continental behavior far from the origin.
    const float spawnDistanceSquared = x * x + z * z;
    const float spawnLand = 0.43f *
        (1.0f - smoothRange(160000.0f, 1000000.0f, spawnDistanceSquared));
    sample.continentalness = largeContinent * 0.78f +
                             continentalDetail * 0.22f + spawnLand;
    const float rawErosion = erosionNoise_.fractal2D(
        x * 0.0011f - 53.271f, z * 0.0011f + 117.439f, 3, 2.0f, 0.52f);
    sample.erosion = smoothRange(-0.38f, 0.38f, rawErosion);
    sample.temperature = temperatureNoise_.fractal2D(
        x * (generationVersion_>=9?.00035f:.0009f) + 129.337f, z * (generationVersion_>=9?.00035f:.0009f) - 83.719f, 3, 2.0f, 0.53f);
    sample.humidity = humidityNoise_.fractal2D(
        x * (generationVersion_>=9?.00038f:.00095f) - 211.381f, z * (generationVersion_>=9?.00038f:.00095f) + 47.527f, 3, 2.0f, 0.53f);
    if(generationVersion_>=9) {
        sample.temperature=std::clamp(.5f+sample.temperature*1.55f,0.f,1.f);
        sample.humidity=std::clamp(.5f+sample.humidity*1.55f,0.f,1.f);
        sample.mushroomIsland=smoothRange(.24f,.37f,continentalDetail)*
            (1.f-smoothRange(-.28f,-.17f,largeContinent));
    }
    sample.weirdness = weirdnessNoise_.fractal2D(
        x * 0.00095f + 67.673f, z * 0.00095f + 173.291f, 3, 2.0f, 0.53f);

    // Continentalness supplies the broad land/ocean profile. Erosion changes the
    // amplitude of hills and ridges; it is never simply added to elevation.
    const float inland = smoothRange(-0.24f, 0.02f, sample.continentalness);
    const float mountainRegion =
        smoothRange(0.04f, generationVersion_ >= 8 ? 0.27f : 0.32f, sample.continentalness) *
        smoothRange(generationVersion_ >= 8 ? 0.035f : 0.055f,
                    generationVersion_ >= 8 ? 0.27f : 0.34f, std::abs(sample.weirdness));
    sample.peakValley = std::clamp(
        2.0f * smoothRange(0.035f, 0.39f, std::abs(sample.weirdness)) - 1.0f,
        -1.0f, 1.0f);
    const float shapeScale = generationVersion_ >= 4 ? 0.0022f : 0.0035f;
    const float ridgeScale = generationVersion_ >= 4 ? 0.0031f : 0.0043f;
    const float rolling = ridgeNoise_.fractal2D(
        x * shapeScale + 43.449f, z * shapeScale - 119.273f, 3, 2.0f, 0.52f);
    const float ridge = 1.0f - std::abs(ridgeNoise_.fractal2D(
        x * ridgeScale - 137.613f, z * ridgeScale + 39.347f, 3, 2.0f, 0.5f));
    const float smallDetail = ridgeNoise_.noise(
        x * 0.021f + 179.427f, 0.0f, z * 0.021f - 211.629f);
    const float ruggedness = 1.0f - sample.erosion;
    float height = 51.0f + sample.continentalness *
        (generationVersion_ >= 4 ? 64.0f : 72.0f);
    if (generationVersion_ >= 8) {
        // Preserve lowlands and coasts; build regional landforms before detail.
        // Reuse existing noise samples, with erosion smoothing some ranges and
        // retaining sharp ridgelines and basins in rugged regions.
        const float valley = 1.0f - smoothRange(0.035f, 0.20f, std::abs(sample.weirdness));
        height -= inland * valley * (6.0f + 9.0f * ruggedness);
        height += inland * rolling * glm::mix(4.0f, 12.0f, ruggedness);
        height += mountainRegion * glm::mix(13.0f, 26.0f, ruggedness);
        height += mountainRegion * mountainRegion * glm::mix(36.0f, 72.0f, ruggedness);
        const float ridgeProfile = smoothRange(0.40f, 0.96f, ridge);
        height += mountainRegion * mountainRegion * ridgeProfile *
                  glm::mix(25.0f, 100.0f, ruggedness);
        // Valleys between ridges prevent a uniform high plateau. Stronger
        // erosion lowers their relief without applying a global height scale.
        height -= mountainRegion * (1.0f-ridgeProfile) * ruggedness * 17.0f;
        height += inland * smallDetail * glm::mix(0.5f, 1.5f, ruggedness);
    } else if (generationVersion_ >= 4) {
        // Broad low-relief plains and wide valleys precede the gradually
        // strengthening foothill/ridge terms. Erosion controls their amplitude.
        const float valley = 1.0f - smoothRange(0.045f, 0.23f,
                                               std::abs(sample.weirdness));
        height -= inland * valley * (4.0f + 5.0f * sample.erosion);
        height += inland * rolling * glm::mix(4.0f, 9.0f, ruggedness);
        height += inland * smallDetail * glm::mix(0.5f, 1.5f, ruggedness);
        height += mountainRegion * (11.0f + std::max(rolling, 0.0f) * 7.0f);
        height += mountainRegion * mountainRegion *
                  glm::mix(30.0f, 59.0f, ruggedness);
        height += std::pow(mountainRegion, 3.0f) * std::pow(ridge, 4.0f) *
                  glm::mix(48.0f, 94.0f, ruggedness);
    } else {
        height += inland * rolling * glm::mix(5.0f, 12.0f, ruggedness);
        height += inland * smallDetail * glm::mix(0.8f, 2.5f, ruggedness);
        height += mountainRegion * (13.0f + std::max(rolling, 0.0f) * 8.0f);
        height += mountainRegion * mountainRegion * glm::mix(30.0f, 55.0f, ruggedness);
        height += std::pow(mountainRegion, 3.0f) * std::pow(ridge, 5.0f) *
                  glm::mix(44.0f, 92.0f, ruggedness);
    }
    const float cliffBand = 1.0f - std::abs(ridgeNoise_.noise(
        x * 0.0065f + 13.391f, 0.0f, z * 0.0065f - 31.537f));
    height += std::pow(mountainRegion, 3.0f) * std::pow(cliffBand, 8.0f) *
              ruggedness * (generationVersion_ >= 8 ? 8.0f : 13.0f);

    if(generationVersion_>=9) {
        // Smooth masks change landforms before biome selection, never at a label boundary.
        const float hotDry=smoothRange(.72f,.88f,sample.temperature)*
            (1.f-smoothRange(.25f,.45f,sample.humidity));
        const float plateau=hotDry*inland*smoothRange(.05f,.22f,sample.weirdness);
        height=glm::mix(height,76.f+sample.continentalness*32.f+rolling*3.f,plateau*.65f);
        height=glm::mix(height,SEA_LEVEL+5.f+rolling*3.f,sample.mushroomIsland);
        const float marsh=smoothRange(.65f,.82f,sample.humidity)*smoothRange(.40f,.62f,sample.erosion)*
            (1.f-smoothRange(55.f,65.f,height))*smoothRange(.0f,.05f,sample.continentalness)*
            smoothRange(.30f,.36f,sample.temperature)*(1.f-smoothRange(.76f,.82f,sample.temperature));
        height=glm::mix(height,float(SEA_LEVEL)+1.f+rolling*3.f,marsh);

    }
    // A domain-warped zero contour forms continuous, gently curving river
    // paths. The elevation/continental masks keep deep cuts out of peaks.
    const float warpX = riverNoise_.noise(x * 0.0008f + 81.473f, 0.0f,
                                         z * 0.0008f - 57.229f) * 72.0f;
    const float warpZ = riverNoise_.noise(x * 0.0008f - 39.571f, 0.0f,
                                         z * 0.0008f + 131.317f) * 72.0f;
    const float riverField = riverNoise_.fractal2D(
        (x + warpX) * 0.0028f + 241.359f,
        (z + warpZ) * 0.0028f - 197.473f, 2, 2.0f, 0.55f);
    const float lowlandRiverWidth = generationVersion_ >= 4
        ? 1.0f - smoothRange(SEA_LEVEL + 10.0f, SEA_LEVEL + 38.0f, height)
        : 0.0f;
    const float riverCore = 1.0f - smoothRange(
        0.018f, 0.070f + lowlandRiverWidth * 0.018f,
        std::abs(riverField));
    const float riverEligibility =
        smoothRange(-0.12f, 0.025f, sample.continentalness) *
        (1.0f - smoothRange(SEA_LEVEL + 19.0f, SEA_LEVEL + 46.0f, height));
    sample.river = riverCore * riverEligibility;
    const float riverBed = static_cast<float>(SEA_LEVEL - 3) +
                           std::max(0.0f, sample.continentalness) * 8.0f;
    if (generationVersion_ >= 4) {
        // Deform a broad, shallow valley around the channel before cutting the
        // narrow center. In rugged highlands the eligibility mask fades, so
        // rivers cannot drill near-vertical trenches down from high peaks.
        const float bank = 1.0f - smoothRange(0.055f, 0.15f,
                                               std::abs(riverField));
        const float possibleDepth = std::max(0.0f, height - riverBed);
        height -= possibleDepth * bank * riverEligibility * 0.18f;
    }
    height = glm::mix(height, std::min(height, riverBed), sample.river * sample.river);
    sample.height = std::clamp(static_cast<int>(std::round(height)), 6, WORLD_HEIGHT - 12);

    if(generationVersion_>=9) {
        sample.biome=selectClimateBiome(sample.temperature,sample.humidity,sample.continentalness,
            sample.erosion,sample.weirdness,sample.river,sample.height,sample.mushroomIsland);
        return sample;
    }
    // Climate and elevation label the already-shaped terrain. No biome branch
    // above changes the height equation, so biome borders cannot create walls.
    const float effectiveTemperature =
        sample.temperature - std::max(0, sample.height - 75) * 0.0042f;
    if (sample.height < SEA_LEVEL - 2) {
        sample.biome = Biome::Ocean;
    } else if (sample.height <= SEA_LEVEL + 3 &&
               sample.continentalness < 0.035f && mountainRegion < 0.24f) {
        sample.biome = Biome::Beach;
    } else if (sample.height >= 135 && effectiveTemperature < -0.12f) {
        sample.biome = Biome::SnowyPeaks;
    } else if (sample.height >= 137 && ruggedness > 0.48f) {
        sample.biome = Biome::StonyPeaks;
    } else if (sample.height >= 96 && effectiveTemperature < -0.14f) {
        sample.biome = Biome::SnowySlopes;
    } else if (mountainRegion > 0.48f && sample.height >= 112) {
        sample.biome = Biome::Mountains;
    } else if (effectiveTemperature < -0.23f) {
        sample.biome = Biome::SnowyPlains;
    } else if (sample.height >= 76 && sample.humidity > 0.02f &&
               sample.humidity < 0.075f) {
        sample.biome = Biome::Meadow;
    } else if (effectiveTemperature > 0.08f && sample.humidity < -0.07f) {
        sample.biome = Biome::Desert;
    } else if (sample.humidity > 0.065f) {
        sample.biome = effectiveTemperature > -0.08f &&
                               sample.humidity < 0.19f
                           ? Biome::BirchForest : Biome::Forest;
    } else {
        sample.biome = Biome::Plains;
    }
    if (generationVersion_ < 4) {
        sample.ravine = std::abs(caveNoise_.noise(
            x * 0.011f + 71.263f, 0.0f, z * 0.011f - 93.487f));
    }
    return sample;
}

World::TerrainSample World::sampleTerrainLegacy(int worldX, int worldZ) const {
    const float x = static_cast<float>(worldX);
    const float z = static_cast<float>(worldZ);
    const auto smoothRange = [](float edge0, float edge1, float value) {
        const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    const float continental =
        terrainNoise_.fractal2D(x * 0.0015f, z * 0.0015f, 5, 2.0f, 0.5f);
    const float broadHills =
        terrainNoise_.fractal2D(x * 0.0038f - 420.0f, z * 0.0038f + 260.0f, 4, 2.0f, 0.52f);
    const float detail =
        terrainNoise_.fractal2D(x * 0.027f + 41.0f, z * 0.027f - 17.0f, 3, 2.0f, 0.5f);
    const float moisture =
        biomeNoise_.fractal2D(x * 0.004f - 77.0f, z * 0.004f + 23.0f, 3, 2.0f, 0.55f);
    const float temperature =
        biomeNoise_.fractal2D(x * 0.0025f + 120.0f, z * 0.0025f - 90.0f, 2, 2.0f, 0.5f);
    const float mountainRegion =
        biomeNoise_.fractal2D(x * 0.00115f + 210.0f, z * 0.00115f + 160.0f, 3, 2.0f, 0.5f);
    const float mountainInfluence = smoothRange(-0.03f, 0.34f, mountainRegion);

    Biome biome = Biome::Plains;
    if (mountainInfluence > 0.48f)
        biome = Biome::Mountains;
    else if (temperature > -0.02f && moisture < -0.08f)
        biome = Biome::Desert;
    else if (moisture > 0.07f)
        biome = Biome::Forest;

    const float mediumShape =
        terrainNoise_.fractal2D(x * 0.0048f + 93.0f, z * 0.0048f - 137.0f, 3, 2.0f, 0.52f);
    const float ridge = 1.0f - std::abs(terrainNoise_.fractal2D(
                                         x * 0.0055f + 19.0f,
                                         z * 0.0055f - 41.0f,
                                         3,
                                         2.0f,
                                         0.5f));
    const float cliffBands =
        1.0f - std::abs(terrainNoise_.noise(x * 0.007f - 150.0f, 0.0f, z * 0.007f + 90.0f));

    // Climate affects low terrain continuously, so crossing a biome label never changes the
    // height formula abruptly. The very-low-frequency mask supplies foothills at its edge and
    // only permits the ridge and peak terms to become strong in the region's interior.
    const float climateElevation = moisture * 2.5f - temperature * 1.25f;
    const float lowlandHeight = 52.0f + continental * 14.0f + broadHills * 7.0f +
                                climateElevation + detail * 2.0f;
    const float influenceSquared = mountainInfluence * mountainInfluence;
    const float influenceCubed = influenceSquared * mountainInfluence;
    const float foothills = mountainInfluence * (12.0f + std::max(0.0f, broadHills) * 10.0f);
    const float mountainMass = influenceSquared *
                               (46.0f + continental * 12.0f + mediumShape * 18.0f);
    const float highPeaks = influenceCubed * std::pow(ridge, 4.0f) * 68.0f;
    const float cliffs = influenceCubed * std::pow(cliffBands, 9.0f) * 15.0f;
    float height = lowlandHeight + foothills + mountainMass + highPeaks + cliffs +
                   detail * mountainInfluence * 3.0f;

    const float river =
        std::abs(biomeNoise_.fractal2D(x * 0.0022f + 330.0f, z * 0.0022f - 270.0f, 3, 2.0f, 0.5f));
    if (river < 0.045f) {
        const float riverStrength = 1.0f - river / 0.045f;
        const float mountainRiverFade = 1.0f - smoothRange(0.20f, 0.62f, mountainInfluence);
        height = glm::mix(height,
                          static_cast<float>(SEA_LEVEL - 3),
                          riverStrength * riverStrength * mountainRiverFade);
    }
    return {std::clamp(static_cast<int>(std::round(height)), 6, WORLD_HEIGHT - 12), biome};
}

int World::terrainHeight(int worldX, int worldZ) const {
    return sampleTerrain(worldX, worldZ).height;
}

StructureTerrain World::structureTerrainAt(int worldX, int worldZ) const {
    const TerrainSample sample = sampleTerrainModern(worldX, worldZ);
    StructureBiome biome = StructureBiome::Other;
    switch (sample.biome) {
    case Biome::Plains:
    case Biome::Meadow:
    case Biome::Savanna: biome = StructureBiome::Plains; break;
    case Biome::Forest:
    case Biome::BirchForest:
    case Biome::Taiga:
    case Biome::OldGrowthTaiga:
    case Biome::DarkForest:
    case Biome::FlowerForest: biome = StructureBiome::Forest; break;
    case Biome::Desert:
    case Biome::DesertHills: biome = StructureBiome::Desert; break;
    case Biome::Mountains:
    case Biome::SnowySlopes:
    case Biome::StonyPeaks:
    case Biome::SnowyPeaks: biome = StructureBiome::Mountains; break;
    default: break;
    }
    return {sample.height, biome, sample.river};
}

glm::vec3 World::findSafeSpawnNear(int worldX, int worldZ) const {
    std::unordered_map<std::int64_t, GeneratedChunk> cache;
    const auto blockAt = [&](int x, int y, int z) {
        if (y < 0 || y >= WORLD_HEIGHT) return Block::Air;
        const int cx = floorDiv(x, CHUNK_SIZE), cz = floorDiv(z, CHUNK_SIZE);
        if (const Chunk* loaded = findChunk(cx, cz))
            return loaded->getLocal(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE));
        const auto key = chunkKey(cx, cz);
        auto it = cache.find(key);
        if (it == cache.end()) {
            auto generated = generateChunkData(cx, cz);
            const auto changed = edits_.find(key);
            if (changed != edits_.end())
                for (const auto& edit : changed->second) generated.blocks[edit.first] = edit.second;
            it = cache.emplace(key, std::move(generated)).first;
        }
        return it->second.blocks[localIndex(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE))];
    };
    const auto safeGround = [](Block b) {
        const auto g = blockGeometry(b);
        return isSolid(b) && !isWater(b) && !isLeaf(b) && b != Block::Cactus &&
               !isLog(b) &&
               g.shape == BlockShape::Cube && g.minY == 0 && g.maxY == 1;
    };
    for (int radius = 0; radius <= 48; ++radius) {
        glm::vec3 best(0.0f);
        int bestDistance = std::numeric_limits<int>::max();
        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue;
                const int x = worldX + dx, z = worldZ + dz;
                int y = WORLD_HEIGHT - 3;
                for (; y > 2; --y) {
                    const Block b = blockAt(x,y,z);
                    if (isSolid(b) || isWater(b)) break;
                }
                const bool snowCovered = snowLayers(blockAt(x,y,z)) > 1;
                const float snowTop = y + blockGeometry(blockAt(x,y,z)).maxY;
                if (snowCovered) --y;
                if (y <= 2 || !safeGround(blockAt(x,y,z)) ||
                    !safeGround(blockAt(x,y-1,z))) continue;
                const float spawnY = snowCovered ? snowTop + .01f : y + 1.01f;
                bool safe = true;
                // A clear body and a supported neighborhood avoid cliff edges,
                // water, cactus and dangerous immediate sideways steps.
                for (int oz = -1; oz <= 1 && safe; ++oz)
                    for (int ox = -1; ox <= 1 && safe; ++ox) {
                        bool footing = false;
                        for (int drop = 0; drop <= 2; ++drop) {
                            const Block support=blockAt(x+ox,y-drop,z+oz);
                            if (isSolid(support) || isWater(support)) {
                                footing=safeGround(support);
                                break;
                            }
                        }
                        const Block feet = blockAt(x+ox,y+1,z+oz);
                        const Block head = blockAt(x+ox,y+2,z+oz);
                        safe = footing && !isSolid(feet) && !isWater(feet) &&
                               !isSolid(head) && !isWater(head);
                        if (snowCovered && footing) {
                            safe = true;
                            // Accumulated snow is safe footing on supported terrain.
                            // Check actual heights so neither snow nor a low roof
                            // overlaps the complete player body at the new surface.
                            for (int by=y+1; by<=static_cast<int>(std::floor(spawnY+1.8f)) && safe; ++by) {
                                const Block b=blockAt(x+ox,by,z+oz);
                                const auto g=blockGeometry(b);
                                safe = !isWater(b) && (!isSolid(b) ||
                                    by+g.maxY<=spawnY || by+g.minY>=spawnY+1.8f);
                            }
                        }
                    }
                const int distance = dx*dx + dz*dz;
                if (safe && distance < bestDistance) {
                    bestDistance = distance;
                    best = {x + .5f, spawnY, z + .5f};
                }
            }
        }
        if (bestDistance != std::numeric_limits<int>::max()) return best;
    }
    // Never silently return an airborne/embedded fallback.
    throw std::runtime_error("No safe spawn surface within 48 blocks of world spawn");
}

void World::prepareSpawnTerrain(const glm::vec3& position) {
    // Synchronously make only the local safety footprint collidable. Normal
    // streaming and all mesh builds remain asynchronous.
    const int x = static_cast<int>(std::floor(position.x));
    const int z = static_cast<int>(std::floor(position.z));
    for (int cz = floorDiv(z-1,CHUNK_SIZE); cz <= floorDiv(z+1,CHUNK_SIZE); ++cz)
        for (int cx = floorDiv(x-1,CHUNK_SIZE); cx <= floorDiv(x+1,CHUNK_SIZE); ++cx) {
            if (findChunk(cx,cz)) continue;
            auto generated = generateChunkData(cx,cz);
            const auto changed = edits_.find(chunkKey(cx,cz));
            if (changed != edits_.end())
                for (const auto& edit : changed->second) generated.blocks[edit.first] = edit.second;
            initializeGeneratedLoot(generated);
            queueGeneratedMobs(generated);
            auto chunk = std::make_unique<Chunk>(cx,cz,std::move(generated.blocks));
            chunk->meshIdentity = nextMeshIdentity_++;
            computeSunlight(*chunk);
            chunks_.emplace(chunkKey(cx,cz),std::move(chunk));
            queueLightingUpdate(cx,cz,false);
            scheduleFluidBoundaryUpdates(cx,cz);
            markDirty(cx,cz,true);
            markDirty(cx-1,cz); markDirty(cx+1,cz);
            markDirty(cx,cz-1); markDirty(cx,cz+1);
        }
}

std::string World::biomeNameAt(int worldX, int worldZ) const {
    return biomeDefinition(sampleTerrain(worldX,worldZ).biome).name;
}

BiomeClimate World::biomeClimateAt(int x,int z,float elevation) const {
    const auto sample=sampleTerrain(x,z);
    const auto& definition=biomeDefinition(sample.biome);
    BiomeClimate result;
    result.biome=sample.biome;
    result.temperature=generationVersion_>=9?sample.temperature:std::clamp(.5f+sample.temperature*1.55f,0.f,1.f);
    result.humidity=generationVersion_>=9?sample.humidity:std::clamp(.5f+sample.humidity*1.55f,0.f,1.f);
    result.effectiveTemperature=altitudeTemperature(result.temperature,elevation<0?float(sample.height):elevation);
    result.dry=definition.dry;
    result.freezes=definition.frozen || result.effectiveTemperature<.17f;
    result.naturalHostiles=definition.naturalHostiles;
    return result;
}

WorldGenerationDebug World::generationDebugAt(int worldX, int worldZ,
                                               int worldY, bool detailed) const {
    const TerrainSample sample = sampleTerrain(worldX, worldZ);
    WorldGenerationDebug debug{sample.continentalness, sample.erosion, sample.temperature,
            sample.humidity, sample.weirdness, sample.peakValley,
            sample.river, sample.height, biomeNameAt(worldX, worldZ)};
    if(generationVersion_>=9) {
        debug.effectiveTemperature=altitudeTemperature(sample.temperature,float(worldY));
        debug.climate=climateRegion(debug.effectiveTemperature);
    }
    if (!detailed)
        return debug;
    debug.depthBelowSurface = sample.height - worldY;
    debug.caveCheese = cheeseNoise_.fractal3D(worldX * 0.022f,
        worldY * 0.027f, worldZ * 0.022f, 3, 2.0f, 0.52f);
    debug.caveSpaghetti = std::abs(spaghettiNoise_.noise(
        worldX * 0.028f + 17.0f, worldY * 0.032f,
        worldZ * 0.028f - 29.0f));
    debug.caveNoodle = std::abs(noodleNoise_.noise(
        worldX * 0.044f + 61.0f, worldY * 0.048f,
        worldZ * 0.044f - 71.0f));
    debug.canyonRegionX = floorDiv(worldX, 256);
    debug.canyonRegionZ = floorDiv(worldZ, 256);
    debug.canyonActive = canyonCarver_.hasCanyonInRegion(
        debug.canyonRegionX, debug.canyonRegionZ, generationVersion_ >= 5);
    debug.canyonSurfaceExposure = canyonCarver_.hasSurfaceCanyonInRegion(
        debug.canyonRegionX, debug.canyonRegionZ, generationVersion_ >= 5);
    if (generationVersion_ >= 5) {
        const int caveRegionX = floorDiv(worldX, 96);
        const int caveRegionZ = floorDiv(worldZ, 96);
        float nearestDistance = 1000.0f;
        for (int regionZ = caveRegionZ - 1; regionZ <= caveRegionZ + 1; ++regionZ) {
            for (int regionX = caveRegionX - 1; regionX <= caveRegionX + 1; ++regionX) {
                int entranceX = 0;
                int entranceZ = 0;
                if (!canyonCarver_.surfaceCaveStartInRegion(
                        regionX, regionZ, entranceX, entranceZ,
                        generationVersion_ >= 6))
                    continue;
                const float dx = static_cast<float>(worldX - entranceX);
                const float dz = static_cast<float>(worldZ - entranceZ);
                nearestDistance = std::min(nearestDistance, std::sqrt(dx * dx + dz * dz));
            }
        }
        debug.caveEntranceCandidate = nearestDistance < 12.0f;
        const float radius = std::clamp(1.0f - nearestDistance / 12.0f, 0.0f, 1.0f);
        debug.caveEntranceInfluence = radius * radius * (3.0f - 2.0f * radius);
    }
    const float aquiferRegion = aquiferNoise_.fractal2D(
        worldX * 0.005f + 97.331f, worldZ * 0.005f - 143.527f,
        2, 2.0f, 0.5f);
    debug.aquiferLevel = std::min(sample.height - 4,
        SEA_LEVEL - 28 + static_cast<int>(aquiferRegion * 26.0f) +
        static_cast<int>(std::max(0.0f, sample.continentalness) * 14.0f));
    return debug;
}

World::GeneratedChunk World::generateChunkData(int chunkX, int chunkZ) const {
    return generationVersion_ == 1 ? generateChunkDataLegacy(chunkX, chunkZ)
                                   : generateChunkDataModern(chunkX, chunkZ);
}

World::GeneratedChunk World::generateChunkDataLegacy(int chunkX, int chunkZ) const {
    GeneratedChunk result{
        chunkX,
        chunkZ,
        std::vector<Block>(static_cast<std::size_t>(CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE),
                           Block::Air)};
    auto getLocal = [&](int lx, int y, int lz) -> Block& {
        return result.blocks[localIndex(lx, y, lz)];
    };

    for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
        for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
            const int worldX = chunkX * CHUNK_SIZE + localX;
            const int worldZ = chunkZ * CHUNK_SIZE + localZ;
            const TerrainSample sample = sampleTerrainLegacy(worldX, worldZ);
            const std::uint32_t columnHash = positionHash(worldX, worldZ, seed_);
            const bool snowy =
                sample.biome == Biome::Mountains && sample.height >= SEA_LEVEL + 48;
            const bool clayShore =
                sample.height < SEA_LEVEL && (columnHash % 11U) < 3U;
            const float ravine =
                std::abs(caveNoise_.noise(worldX * 0.013f + 71.0f, 0.0f, worldZ * 0.013f - 93.0f));
            for (int y = 0; y <= sample.height; ++y) {
                Block block = Block::Stone;
                const bool sandy = sample.biome == Biome::Desert || sample.height <= SEA_LEVEL + 2;
                if (y == sample.height)
                    block = snowy ? Block::SnowBlock
                                  : (clayShore ? Block::Clay
                                               : (sandy ? Block::Sand : Block::Grass));
                else if (y >= sample.height - 3)
                    block = clayShore ? Block::Clay : (sandy ? Block::Sand : Block::Dirt);
                if (y > 4 && y < sample.height - 5) {
                    const float cavern = caveNoise_.fractal3D(
                        worldX * 0.034f, y * 0.041f, worldZ * 0.034f, 4, 2.0f, 0.52f);
                    const float tunnelA = std::abs(caveNoise_.noise(
                        worldX * 0.026f + 17.0f, y * 0.031f, worldZ * 0.026f - 29.0f));
                    const float tunnelB = caveNoise_.noise(
                        worldX * 0.019f - 61.0f, y * 0.024f + 13.0f, worldZ * 0.019f + 44.0f);
                    const bool ravineCut =
                        ravine < 0.022f && y > 7 && y < sample.height - 3 && tunnelB < 0.42f;
                    if (cavern > 0.455f || (tunnelA < 0.047f && tunnelB > 0.08f) || ravineCut)
                        block = Block::Air;
                }
                if (block == Block::Stone && y < SEA_LEVEL + 22 &&
                    terrainNoise_.noise(
                        worldX * 0.091f + 313.0f, y * 0.087f, worldZ * 0.091f - 277.0f) >
                        0.58f) {
                    block = Block::Gravel;
                }
                if (block == Block::Stone) {
                    const float coal = terrainNoise_.noise(
                        worldX * 0.105f + 19.0f, y * 0.115f, worldZ * 0.105f - 37.0f);
                    const float iron = terrainNoise_.noise(
                        worldX * 0.125f - 83.0f, y * 0.130f + 31.0f, worldZ * 0.125f + 71.0f);
                    const float gold = terrainNoise_.noise(
                        worldX * 0.145f + 151.0f, y * 0.150f - 47.0f, worldZ * 0.145f - 113.0f);
                    const float copper = terrainNoise_.noise(
                        worldX * 0.118f - 211.0f, y * 0.122f + 81.0f, worldZ * 0.118f + 193.0f);
                    const float diamond = terrainNoise_.noise(
                        worldX * 0.165f + 271.0f, y * 0.170f + 117.0f, worldZ * 0.165f - 239.0f);
                    if (y < 30 && diamond > 0.59f)
                        block = Block::DiamondOre;
                    else if (y < 52 && gold > 0.54f)
                        block = Block::GoldOre;
                    else if (y < 94 && copper > 0.50f)
                        block = Block::CopperOre;
                    else if (y < 112 && iron > 0.52f)
                        block = Block::IronOre;
                    else if (y < 176 && coal > 0.50f)
                        block = Block::CoalOre;
                }
                if (block == Block::Stone && y < sample.height - 4) {
                    const float granite = terrainNoise_.noise(
                        worldX * 0.072f + 411.0f, y * 0.075f, worldZ * 0.072f - 337.0f);
                    const float diorite = terrainNoise_.noise(
                        worldX * 0.069f - 281.0f, y * 0.073f + 51.0f, worldZ * 0.069f + 443.0f);
                    const float andesite = terrainNoise_.noise(
                        worldX * 0.076f + 127.0f, y * 0.071f - 89.0f, worldZ * 0.076f + 211.0f);
                    if (granite > 0.56f)
                        block = Block::Granite;
                    else if (diorite > 0.57f)
                        block = Block::Diorite;
                    else if (andesite > 0.57f)
                        block = Block::Andesite;
                }
                if (block == Block::Air && y <= 12)
                    block = Block::Water;
                getLocal(localX, y, localZ) = block;
            }
            for (int y = sample.height + 1; y <= SEA_LEVEL; ++y)
                getLocal(localX, y, localZ) = Block::Water;
            if (snowy && sample.height + 1 < WORLD_HEIGHT) {
                if ((columnHash >> 7U) % 19U == 0U)
                    getLocal(localX, sample.height, localZ) = Block::Ice;
                else
                    getLocal(localX, sample.height + 1, localZ) = Block::Snow;
            } else if ((sample.biome == Biome::Plains || sample.biome == Biome::Forest) &&
                       sample.height > SEA_LEVEL && sample.height + 1 < WORLD_HEIGHT &&
                       getLocal(localX, sample.height, localZ) == Block::Grass) {
                const std::uint32_t plantChance = (columnHash >> 5U) % 100U;
                if (plantChance < 12U)
                    getLocal(localX, sample.height + 1, localZ) = Block::TallGrass;
                else if (plantChance == 12U)
                    getLocal(localX, sample.height + 1, localZ) = Block::RedFlower;
                else if (plantChance == 13U)
                    getLocal(localX, sample.height + 1, localZ) = Block::YellowFlower;
            }
        }
    }

    // Evaluate roots outside this chunk too, so trees and cacti cross borders deterministically.
    for (int rootZ = chunkZ * CHUNK_SIZE - 3; rootZ < (chunkZ + 1) * CHUNK_SIZE + 3; ++rootZ) {
        for (int rootX = chunkX * CHUNK_SIZE - 3; rootX < (chunkX + 1) * CHUNK_SIZE + 3; ++rootX) {
            const TerrainSample ground = sampleTerrainLegacy(rootX, rootZ);
            const std::uint32_t hash = positionHash(rootX, rootZ, seed_);
            auto place = [&](int wx, int y, int wz, Block block) {
                const int localFeatureX = wx - chunkX * CHUNK_SIZE;
                const int localFeatureZ = wz - chunkZ * CHUNK_SIZE;
                if (localFeatureX < 0 || localFeatureX >= CHUNK_SIZE || localFeatureZ < 0 ||
                    localFeatureZ >= CHUNK_SIZE || y < 0 || y >= WORLD_HEIGHT)
                    return;
                Block& current = getLocal(localFeatureX, y, localFeatureZ);
                if (block == Block::Log || block == Block::BirchLog || block == Block::Cactus ||
                    current == Block::Air || current == Block::Snow || isWater(current) ||
                    isPlant(current))
                    current = block;
            };

            if (ground.biome == Biome::Desert && ground.height > SEA_LEVEL + 1 &&
                hash % 61U == 0U) {
                const int cactusHeight = 2 + static_cast<int>((hash >> 9U) % 3U);
                for (int y = 1; y <= cactusHeight; ++y)
                    place(rootX, ground.height + y, rootZ, Block::Cactus);
                continue;
            }

            if (ground.biome != Biome::Forest || ground.height <= SEA_LEVEL + 1 ||
                hash % 47U != 0U)
                continue;

            const bool birch = ((hash >> 16U) % 3U) == 0U;
            const Block trunk = birch ? Block::BirchLog : Block::Log;
            const Block leaves = birch ? Block::BirchLeaves : Block::Leaves;
            const int trunkHeight = 4 + static_cast<int>((hash >> 8U) % 3U);
            for (int y = 1; y <= trunkHeight; ++y)
                place(rootX, ground.height + y, rootZ, trunk);
            const int crownY = ground.height + trunkHeight;
            for (int dy = -2; dy <= 2; ++dy) {
                const int radius = dy >= 1 ? 1 : 2;
                for (int dz = -radius; dz <= radius; ++dz) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        if (std::abs(dx) == radius && std::abs(dz) == radius &&
                            ((hash + dy) & 1U))
                            continue;
                        if (dx == 0 && dz == 0 && dy <= 0)
                            continue;
                        place(rootX + dx, crownY + dy, rootZ + dz, leaves);
                    }
                }
            }
            place(rootX, crownY + 2, rootZ, leaves);
        }
    }
    return result;
}

World::GeneratedChunk World::generateChunkDataModern(int chunkX, int chunkZ) const {
    GeneratedChunk result{
        chunkX, chunkZ,
        std::vector<Block>(static_cast<std::size_t>(CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE),
                           Block::Air)};
    auto localBlock = [&](int x, int y, int z) -> Block& {
        return result.blocks[localIndex(x, y, z)];
    };
    constexpr int sampleWidth = CHUNK_SIZE + 2;
    std::array<TerrainSample, sampleWidth * sampleWidth> columns;
    auto column = [&](int x, int z) -> const TerrainSample& {
        return columns[static_cast<std::size_t>((z + 1) * sampleWidth + x + 1)];
    };
    std::array<int, CHUNK_SIZE * CHUNK_SIZE> surfaceHeights{};
    std::array<int, CHUNK_SIZE * CHUNK_SIZE> aquiferLevels{};
    std::array<bool, CHUNK_SIZE * CHUNK_SIZE> wetAquifers{};
    for (int z = -1; z <= CHUNK_SIZE; ++z) {
        for (int x = -1; x <= CHUNK_SIZE; ++x) {
            columns[static_cast<std::size_t>((z + 1) * sampleWidth + x + 1)] =
                sampleTerrainModern(chunkX * CHUNK_SIZE + x, chunkZ * CHUNK_SIZE + z);
        }
    }

    // Shape and climate are sampled once per column. Only caves and veins need
    // three-dimensional noise, and those tests are restricted to solid depth.
    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            const int worldX = chunkX * CHUNK_SIZE + x;
            const int worldZ = chunkZ * CHUNK_SIZE + z;
            const TerrainSample& ground = column(x, z);
            const int slope = std::max({std::abs(column(x - 1, z).height - ground.height),
                                        std::abs(column(x + 1, z).height - ground.height),
                                        std::abs(column(x, z - 1).height - ground.height),
                                        std::abs(column(x, z + 1).height - ground.height)});
            const std::uint32_t hash = positionHash(worldX, worldZ, seed_);
            const float aquiferRegion = aquiferNoise_.fractal2D(
                worldX * 0.005f + 97.331f, worldZ * 0.005f - 143.527f,
                2, 2.0f, 0.5f);
            const bool wetAquifer = aquiferRegion > 0.075f ||
                                    (ground.height < SEA_LEVEL && aquiferRegion > -0.16f);
            const int aquiferLevel = std::min(
                ground.height - 4,
                SEA_LEVEL - 28 + static_cast<int>(aquiferRegion * 26.0f) +
                    static_cast<int>(std::max(0.0f, ground.continentalness) * 14.0f));
            const std::size_t columnIndex = static_cast<std::size_t>(z * CHUNK_SIZE + x);
            surfaceHeights[columnIndex] = ground.height;
            aquiferLevels[columnIndex] = aquiferLevel;
            wetAquifers[columnIndex] = wetAquifer;
            const float noodleRegion = noodleNoise_.noise(
                worldX * 0.006f + 47.433f, 0.0f,
                worldZ * 0.006f - 79.317f);

            const auto& biome=biomeDefinition(ground.biome);
            const bool sandy = (generationVersion_>=9 && biome.surface==Block::Sand) || ground.biome == Biome::Desert ||
                               ground.biome == Biome::Beach ||
                               ground.biome == Biome::Ocean;
            const bool snowy = (generationVersion_>=9 && biome.frozen) || ground.biome == Biome::SnowyPlains ||
                               ground.biome == Biome::SnowySlopes ||
                               ground.biome == Biome::SnowyPeaks;
            const bool steepStone = slope >= 5;
            const bool gravelSlope = slope >= 3 && !steepStone;
            const bool gravelRiverBank = generationVersion_ >= 4 &&
                ground.river > 0.09f && ground.river < 0.70f &&
                ground.height <= SEA_LEVEL + 9 && (hash % 5U) < 3U;
            const bool clayShore = ground.height <= SEA_LEVEL + 1 &&
                                   (hash % 17U) < 3U;
            const Block surface = generationVersion_>=9 ? (steepStone?Block::Stone:gravelSlope||gravelRiverBank?Block::Gravel:biome.surface) : steepStone ? Block::Stone
                                  : gravelSlope || gravelRiverBank ? Block::Gravel
                                  : snowy ? Block::SnowBlock
                                  : clayShore ? Block::Clay
                                  : sandy ? Block::Sand : Block::Grass;
            const Block subsoil = generationVersion_>=9 ? (steepStone?Block::Stone:gravelSlope||gravelRiverBank?Block::Gravel:biome.soil) : steepStone ? Block::Stone
                                  : gravelSlope || gravelRiverBank ? Block::Gravel
                                  : clayShore ? Block::Clay
                                  : sandy ? Block::Sand : Block::Dirt;

            std::array<PerlinNoise::Column, 15> noiseColumns;
            std::array<bool, 15> noiseColumnsReady{};
            const auto columnNoise = [&](int slot, const PerlinNoise& noise, float nx, float ny, float nz) {
                if (generationVersion_ < 10) return noise.noise(nx, ny, nz);
                if (!noiseColumnsReady[slot]) {
                    noiseColumns[slot] = noise.column(nx, nz);
                    noiseColumnsReady[slot] = true;
                }
                return noiseColumns[slot].noise(ny);
            };

            for (int y = 0; y <= ground.height; ++y) {
                Block block = y == ground.height ? surface
                              : y >= ground.height - 3 ? subsoil : Block::Stone;
                if (y > 5) {
                    bool carved = false;
                    const int depth = ground.height - y;
                    if ((generationVersion_ >= 5 && depth > 0) ||
                        (generationVersion_ < 5 &&
                         depth > (generationVersion_ >= 4 ? 7 : 3))) {
                        const float nearSurface = generationVersion_ >= 5
                            ? std::clamp(static_cast<float>(depth) / 13.0f, 0.0f, 1.0f)
                            : 1.0f;
                        const float caveOpening = nearSurface * nearSurface *
                            (3.0f - 2.0f * nearSurface);
                        float cheese;
                        if (generationVersion_ >= 10) {
                            float value = 0, amplitude = 1, totalAmplitude = 0, frequency = 1;
                            for (int octave = 0; octave < 3; ++octave) {
                                value += columnNoise(octave, cheeseNoise_, (worldX * .022f) * frequency,
                                    (y * .027f) * frequency, (worldZ * .022f) * frequency) * amplitude;
                                totalAmplitude += amplitude;
                                amplitude *= .52f;
                                frequency *= 2.f;
                            }
                            cheese = value / totalAmplitude;
                        } else {
                            cheese = cheeseNoise_.fractal3D(worldX * .022f, y * .027f, worldZ * .022f,
                                3, 2.f, .52f);
                        }
                        const float cheeseThreshold = generationVersion_ >= 4
                            ? (generationVersion_ >= 5
                                ? 0.59f - 0.09f * std::clamp(
                                    (static_cast<float>(depth) - 16.0f) / 22.0f,
                                    0.0f, 1.0f) -
                                  0.07f * std::clamp(
                                    (static_cast<float>(depth) - 38.0f) / 18.0f,
                                    0.0f, 1.0f) +
                                  0.26f * (1.0f - caveOpening)
                                : depth > 38 ? 0.43f : depth > 21 ? 0.50f : 0.59f)
                            : 0.46f;
                        carved = cheese > cheeseThreshold;
                        if (!carved) {
                            const float spaghettiA = std::abs(columnNoise(3, spaghettiNoise_,
                                worldX * 0.028f + 17.0f, y * 0.032f,
                                worldZ * 0.028f - 29.0f));
                            const float spaghettiWidth = generationVersion_ >= 4
                                ? (generationVersion_ >= 5
                                    ? (0.043f + 0.019f * std::clamp(
                                        (static_cast<float>(depth) - 15.0f) / 14.0f,
                                        0.0f, 1.0f)) * (0.18f + 0.82f * caveOpening)
                                    : depth > 22 ? 0.062f : 0.043f)
                                : 0.075f;
                            if (spaghettiA < spaghettiWidth) {
                                const float spaghettiB = std::abs(columnNoise(4, caveNoise_,
                                    worldX * 0.027f - 61.0f, y * 0.029f + 13.0f,
                                    worldZ * 0.027f + 44.0f));
                                carved = spaghettiB < (generationVersion_ >= 4
                                    ? (generationVersion_ >= 5
                                        ? (0.044f + 0.021f * std::clamp(
                                            (static_cast<float>(depth) - 15.0f) / 14.0f,
                                            0.0f, 1.0f)) *
                                          (0.18f + 0.82f * caveOpening)
                                        : depth > 22 ? 0.065f : 0.044f)
                                    : 0.078f);
                            }
                        }
                        if (!carved && noodleRegion >
                            (generationVersion_ >= 4 ? 0.18f : 0.13f)) {
                            const float noodleA = std::abs(columnNoise(5, noodleNoise_,
                                worldX * 0.044f + 61.0f, y * 0.048f,
                                worldZ * 0.044f - 71.0f));
                            if (noodleA < (generationVersion_ >= 4
                                    ? (generationVersion_ >= 5
                                        ? 0.028f * (0.12f + 0.88f * caveOpening)
                                        : 0.028f)
                                    : 0.035f)) {
                                carved = std::abs(columnNoise(6, cheeseNoise_,
                                    worldX * 0.041f - 113.0f, y * 0.045f + 23.0f,
                                    worldZ * 0.041f + 37.0f)) <
                                    (generationVersion_ >= 4
                                        ? (generationVersion_ >= 5
                                            ? 0.032f * (0.12f + 0.88f * caveOpening)
                                            : 0.032f)
                                        : 0.045f);
                            }
                        }
                    }
                    const float ravineDepth = generationVersion_ < 4 &&
                        ground.ravine < 0.017f
                        ? caveNoise_.noise(worldX * 0.008f + 103.0f, y * 0.015f,
                                           worldZ * 0.008f - 139.0f)
                        : 1.0f;
                    const bool ravine = generationVersion_ < 4 &&
                                        ground.ravine < 0.017f &&
                                        ravineDepth > -0.38f && ravineDepth < 0.26f &&
                                        y <= std::min(ground.height, SEA_LEVEL + 35);
                    if (carved || ravine)
                        block = wetAquifer && y <= aquiferLevel ? Block::Water : Block::Air;
                }
                if (block == Block::Stone && y < ground.height - 4) {
                    const float diamondChance = std::clamp((48.0f - y) / 38.0f, 0.0f, 1.0f);
                    const float goldChance = std::clamp((82.0f - y) / 52.0f, 0.0f, 1.0f);
                    const float copperChance = std::clamp(1.0f - std::abs(y - 67.0f) / 65.0f,
                                                         0.0f, 1.0f);
                    const float ironChance = std::clamp(
                        0.75f + std::max(0.0f, static_cast<float>(y - 110) / 90.0f),
                        0.0f, 1.0f);
                    const float coalChance = y < 180 ? 1.0f : 0.0f;
                    if (diamondChance > 0.0f && columnNoise(7, terrainNoise_,
                            worldX * 0.15f + 271.0f, y * 0.16f + 117.0f,
                            worldZ * 0.15f - 239.0f) > 0.63f + (1.0f - diamondChance) * 0.2f)
                        block = Block::DiamondOre;
                    else if (goldChance > 0.0f && columnNoise(8, terrainNoise_,
                            worldX * 0.14f + 151.0f, y * 0.15f - 47.0f,
                            worldZ * 0.14f - 113.0f) > 0.57f + (1.0f - goldChance) * 0.16f)
                        block = Block::GoldOre;
                    else if (copperChance > 0.0f && columnNoise(9, terrainNoise_,
                            worldX * 0.115f - 211.0f, y * 0.12f + 81.0f,
                            worldZ * 0.115f + 193.0f) > 0.53f + (1.0f - copperChance) * 0.13f)
                        block = Block::CopperOre;
                    else if (ironChance > 0.0f && columnNoise(10, terrainNoise_,
                            worldX * 0.12f - 83.0f, y * 0.125f + 31.0f,
                            worldZ * 0.12f + 71.0f) > 0.55f + (1.0f - ironChance) * 0.09f)
                        block = Block::IronOre;
                    else if (coalChance > 0.0f && columnNoise(11, terrainNoise_,
                            worldX * 0.10f + 19.0f, y * 0.11f,
                            worldZ * 0.10f - 37.0f) > 0.53f)
                        block = Block::CoalOre;
                    else {
                        const float patch = columnNoise(12, ridgeNoise_,
                            worldX * 0.075f + 411.0f, y * 0.075f,
                            worldZ * 0.075f - 337.0f);
                        if (patch > 0.57f) block = Block::Granite;
                        else if (patch < -0.57f) block = Block::Diorite;
                        else if (columnNoise(13, caveNoise_, worldX * 0.072f + 127.0f,
                                                  y * 0.071f - 89.0f,
                                                  worldZ * 0.072f + 211.0f) > 0.59f)
                            block = Block::Andesite;
                    }
                }
                if(generationVersion_>=9 &&
                   (ground.biome==Biome::Badlands||ground.biome==Biome::WoodedBadlands) &&
                   block!=Block::Air && !isWater(block) && ground.height-y<=18) {
                    constexpr Block bands[]={Block::TerracottaOrange,Block::TerracottaBrown,
                        Block::Terracotta,Block::TerracottaRed,Block::TerracottaYellow,Block::TerracottaWhite};
                    block=bands[(y/3+int(seed_%6))%6];
                    if(y==ground.height && ground.biome==Biome::WoodedBadlands) block=Block::CoarseDirt;
                }
                localBlock(x, y, z) = block;
            }
            for (int y = ground.height + 1; y <= SEA_LEVEL; ++y)
                localBlock(x, y, z) = Block::Water;
            if(generationVersion_>=9 && ground.height<SEA_LEVEL && biome.frozen)
                localBlock(x,SEA_LEVEL,z)=Block::Ice;
            if (snowy && ground.height > SEA_LEVEL && slope <= 4 &&
                ground.height + 1 < WORLD_HEIGHT &&
                localBlock(x, ground.height, z) == Block::SnowBlock) {
                if ((hash >> 7U) % 31U == 0U)
                    localBlock(x, ground.height, z) = Block::Ice;
                else
                    localBlock(x, ground.height + 1, z) = Block::Snow;
            }
        }
    }

    if (generationVersion_ >= 4) {
        canyonCarver_.carveChunk(chunkX, chunkZ, CHUNK_SIZE, WORLD_HEIGHT,
                                surfaceHeights, aquiferLevels, wetAquifers,
                                result.blocks, [&](int x, int z) {
                                    return sampleTerrainModern(x, z).height;
                                }, generationVersion_ >= 5,
                                generationVersion_ >= 6);
        if (generationVersion_ >= 5) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    const int surface = surfaceHeights[static_cast<std::size_t>(
                        z * CHUNK_SIZE + x)];
                    if (surface + 1 < WORLD_HEIGHT &&
                        localBlock(x, surface, z) == Block::Air &&
                        localBlock(x, surface + 1, z) == Block::Snow)
                        localBlock(x, surface + 1, z) = Block::Air;
                }
            }
        }
    }

    // Surface features are deliberately last. Border roots are evaluated by
    // both chunks using world coordinates, so crowns do not stop at seams.
    if (generationVersion_ < 9) for (int rootZ = chunkZ * CHUNK_SIZE - 3;
         rootZ < (chunkZ + 1) * CHUNK_SIZE + 3; ++rootZ) {
        for (int rootX = chunkX * CHUNK_SIZE - 3;
             rootX < (chunkX + 1) * CHUNK_SIZE + 3; ++rootX) {
            const std::uint32_t hash = positionHash(rootX, rootZ, seed_);
            const bool possibleTree = hash % 47U == 0U;
            const bool possibleCactus = hash % 61U == 0U;
            const bool possiblePlant = ((hash >> 5U) % 100U) < 16U;
            if (!possibleTree && !possibleCactus && !possiblePlant)
                continue;
            const TerrainSample root = sampleTerrainModern(rootX, rootZ);
            if (root.height <= SEA_LEVEL + 1 || root.height >= WORLD_HEIGHT - 12)
                continue;
            const bool forest = root.biome == Biome::Forest ||
                                root.biome == Biome::BirchForest;
            const bool desert = root.biome == Biome::Desert;
            const bool tree = forest && possibleTree;
            const bool cactus = desert && possibleCactus;
            const bool plant = !tree && !cactus &&
                               (forest || root.biome == Biome::Plains ||
                                root.biome == Biome::Meadow) &&
                               ((hash >> 5U) % 100U) < 16U;
            if (!tree && !cactus && !plant)
                continue;
            const int slope = std::max({
                std::abs(sampleTerrainModern(rootX - 1, rootZ).height - root.height),
                std::abs(sampleTerrainModern(rootX + 1, rootZ).height - root.height),
                std::abs(sampleTerrainModern(rootX, rootZ - 1).height - root.height),
                std::abs(sampleTerrainModern(rootX, rootZ + 1).height - root.height)});
            if (slope > (tree ? 1 : 2) || root.river > 0.08f)
                continue;
            if (generationVersion_ >= 5 &&
                rootX >= chunkX * CHUNK_SIZE && rootX < (chunkX + 1) * CHUNK_SIZE &&
                rootZ >= chunkZ * CHUNK_SIZE && rootZ < (chunkZ + 1) * CHUNK_SIZE &&
                localBlock(rootX - chunkX * CHUNK_SIZE, root.height,
                           rootZ - chunkZ * CHUNK_SIZE) == Block::Air)
                continue;
            auto place = [&](int wx, int y, int wz, Block block) {
                const int x = wx - chunkX * CHUNK_SIZE;
                const int z = wz - chunkZ * CHUNK_SIZE;
                if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE ||
                    y < 0 || y >= WORLD_HEIGHT)
                    return;
                Block& current = localBlock(x, y, z);
                if (current == Block::Air || current == Block::Snow ||
                    isPlant(current) || isWater(current))
                    current = block;
            };
            if (cactus) {
                for (int y = 1; y <= 2 + static_cast<int>((hash >> 9U) % 3U); ++y)
                    place(rootX, root.height + y, rootZ, Block::Cactus);
            } else if (tree) {
                const bool birch = root.biome == Biome::BirchForest;
                const Block trunk = birch ? Block::BirchLog : Block::Log;
                const Block leaves = birch ? Block::BirchLeaves : Block::Leaves;
                const int trunkHeight = 4 + static_cast<int>((hash >> 8U) % 3U);
                for (int y = 1; y <= trunkHeight; ++y)
                    place(rootX, root.height + y, rootZ, trunk);
                const int crownY = root.height + trunkHeight;
                for (int dy = -2; dy <= 2; ++dy) {
                    const int radius = dy >= 1 ? 1 : 2;
                    for (int dz = -radius; dz <= radius; ++dz) {
                        for (int dx = -radius; dx <= radius; ++dx) {
                            if (dx == 0 && dz == 0 && dy <= 0) continue;
                            if (std::abs(dx) == radius && std::abs(dz) == radius &&
                                ((hash + dy) & 1U)) continue;
                            place(rootX + dx, crownY + dy, rootZ + dz, leaves);
                        }
                    }
                }
                if (generationVersion_ >= 7 && hash % 3U == 0U) {
                    const int facing = static_cast<int>((hash >> 12U) & 3U);
                    constexpr int dx[] = {0,0,-1,1};
                    constexpr int dz[] = {1,-1,0,0};
                    for (int level = 1; level <= 3; ++level)
                        place(rootX + dx[facing], root.height + level, rootZ + dz[facing],
                              static_cast<Block>(static_cast<int>(Block::VineNorth) + facing));
                }
            } else {
                const std::uint32_t choice = (hash >> 5U) % 100U;
                const Block flower = choice == 12U ? Block::RedFlower
                                   : choice == 13U ? Block::YellowFlower
                                   : Block::TallGrass;
                place(rootX, root.height + 1, rootZ, flower);
            }
        }
    }
    if (generationVersion_ == 7) {
        // Shore plants add decoration only; terrain/carving remains unchanged.
        // The column halo makes water-edge decisions deterministic at seams.
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                const int y = column(x,z).height;
                if (y < SEA_LEVEL || y >= WORLD_HEIGHT - 3) continue;
                const Block ground = localBlock(x,y,z);
                if (ground != Block::Grass && ground != Block::Dirt && ground != Block::Sand) continue;
                const bool shoreline = column(x-1,z).height < SEA_LEVEL ||
                    column(x+1,z).height < SEA_LEVEL || column(x,z-1).height < SEA_LEVEL ||
                    column(x,z+1).height < SEA_LEVEL;
                const std::uint32_t hash = positionHash(chunkX * CHUNK_SIZE + x,
                                                        chunkZ * CHUNK_SIZE + z, seed_ ^ 0xca9eU);
                if (!shoreline || y != SEA_LEVEL || hash % 5U > 1U) continue;
                const int height = 1 + static_cast<int>((hash >> 9U) % 3U);
                for (int level = 1; level <= height && localBlock(x,y+level,z) == Block::Air; ++level)
                    localBlock(x,y+level,z) = Block::SugarCane;
            }
        }
    }
    if(generationVersion_>=9) {
        std::array<std::vector<StructurePlan>,9> vegetationPlans;
        std::array<bool,9> planned{};
        const auto reserved=[&](int x,int z) {
            const int cx=floorDiv(x,CHUNK_SIZE),cz=floorDiv(z,CHUNK_SIZE);
            const int slot=(cz-chunkZ+1)*3+cx-chunkX+1;
            if(!planned[slot]) {
                vegetationPlans[slot]=structures_.plansForChunk(cx,cz,
                    [&](int nx,int nz) {return structureTerrainAt(nx,nz);});
                planned[slot]=true;
            }
            for(const auto& plan:vegetationPlans[slot])
                if(x>=plan.bounds.minX-1&&x<=plan.bounds.maxX+1&&z>=plan.bounds.minZ-1&&z<=plan.bounds.maxZ+1) return true;
            return false;
        };
        decorateBiomeChunk(chunkX,chunkZ,seed_,result.blocks,[&](int x,int z) {
            const int lx=x-chunkX*CHUNK_SIZE,lz=z-chunkZ*CHUNK_SIZE;
            const auto ground=(lx>=-1&&lx<=CHUNK_SIZE&&lz>=-1&&lz<=CHUNK_SIZE)?column(lx,lz):sampleTerrainModern(x,z);
            return BiomeTerrain{ground.height,ground.biome,ground.river};
        },reserved,[&](int x,int z) {
            const auto ground=sampleTerrainModern(x,z);
            return canyonCarver_.surfaceSurvives(x,z,ground.height,biomeDefinition(ground.biome).surface,
                [&](int nx,int nz) {return sampleTerrainModern(nx,nz).height;});
        });
    }
    if (generationVersion_ >= 3) {
        const auto structureTerrain = [&](int x, int z) {
            return structureTerrainAt(x, z);
        };
        result.hasStructure = structures_.applyToChunk(
            chunkX, chunkZ, result.blocks, result.loot, result.mobs, structureTerrain);
    }
    if (generationVersion_ >= 8) {
        std::vector<StructurePlan> shoreStructures;
        bool shoreStructuresReady=false;
        // Decorate after trees and structures. Plants never overwrite geometry.
        // World-space patch anchors plus the terrain halo are identical on both
        // sides of a chunk border; each chunk writes only its own columns.
        for (int z=0; z<CHUNK_SIZE; ++z) for (int x=0; x<CHUNK_SIZE; ++x) {
            const int wx=chunkX*CHUNK_SIZE+x, wz=chunkZ*CHUNK_SIZE+z;
            const int y=column(x,z).height;
            if (y<SEA_LEVEL || y>=WORLD_HEIGHT-4) continue;
            const Block ground=localBlock(x,y,z);
            if (ground!=Block::Grass && ground!=Block::Dirt && ground!=Block::Sand) continue;
            if (!isSolid(localBlock(x,y-1,z))) continue;
            const auto waterNeighbor=[&](int nx,int nz) {
                const int h=column(nx,nz).height;
                if (nx>=0 && nx<CHUNK_SIZE && nz>=0 && nz<CHUNK_SIZE)
                    return isWater(localBlock(nx,y,nz)) && localBlock(nx,y+1,nz)==Block::Air;
                // An exposed natural water edge at base-ground level, using the
                // exact pre-decoration terrain profile of the neighboring chunk.
                return h<y && y<=SEA_LEVEL;
            };
            if (!(waterNeighbor(x-1,z)||waterNeighbor(x+1,z)||
                  waterNeighbor(x,z-1)||waterNeighbor(x,z+1))) continue;
            bool inPatch=false;
            const int cellX=floorDiv(wx,4), cellZ=floorDiv(wz,4);
            for(int dz=-1; dz<=1; ++dz) for(int dx=-1; dx<=1; ++dx) {
                const auto h=positionHash(cellX+dx,cellZ+dz,seed_^0xca9eU);
                if(h%3U!=0) continue;
                const int ax=(cellX+dx)*4+static_cast<int>((h>>5U)%4U);
                const int az=(cellZ+dz)*4+static_cast<int>((h>>9U)%4U);
                const int distance=std::abs(wx-ax)+std::abs(wz-az);
                inPatch |= distance<=1; // At most five irregular shoreline bases.
            }
            const auto h=positionHash(wx,wz,seed_^0xcafeU);
            if(!inPatch || h%7U==0) continue;
            const int plantHeight=1+static_cast<int>((h>>9U)%3U);
            bool clear=true;
            for(int level=1;level<=plantHeight;++level)
                clear &= localBlock(x,y+level,z)==Block::Air;
            // Keep out of structure footprints, including flat roofs/paths.
            if(clear && !shoreStructuresReady) {
                shoreStructures=structures_.plansForChunk(chunkX,chunkZ,
                    [&](int x,int z) { return structureTerrainAt(x,z); });
                shoreStructuresReady=true;
            }
            for(const auto& plan : shoreStructures)
                if(plan.bounds.intersectsXZ(wx-1,wz-1,wx+1,wz+1)) clear=false;
            if(clear) for(int level=1;level<=plantHeight;++level)
                localBlock(x,y+level,z)=Block::SugarCane;
        }
    }
    return result;
}

bool World::runCraftingContentSmokeTest(std::string& report) {
    constexpr int span = CHUNK_SIZE + 6;
    MeshInput input;
    input.meshHeight = 8;
    input.blocks.assign(static_cast<std::size_t>(span * span * 8), Block::Air);
    input.packedLight.assign(input.blocks.size(), 0xf0U);
    const std::size_t index = static_cast<std::size_t>((4 * span + 7) * span + 7);
    for (int id = static_cast<int>(Block::CoalBlock); id < static_cast<int>(Block::Count); ++id) {
        const Block block = static_cast<Block>(id);
        input.blocks[index] = block;
        const MeshOutput mesh = buildMesh(input);
        const std::size_t expected = isVine(block) ? 12 : isPlant(block) ? 24 : 36;
        if (mesh.opaque.size() != expected || !mesh.water.empty()) {
            report = "new block mesh vertex count: " + std::to_string(id); return false;
        }
        for (const VoxelVertex& vertex : mesh.opaque)
            if (vertex.uv.x < 0 || vertex.uv.x > 1 || vertex.uv.y < 0 || vertex.uv.y > 1 ||
                (vertex.atlasTile >= 0 && vertex.atlasTile >= BlockAtlasTiles)) {
                report = "new block atlas bounds"; return false;
            }
    }
    World historicalPlants(seed_);
    historicalPlants.generationVersion_=8;
    int cane = 0, vines = 0, generated = 0;
    const int plantSurveyRadius = generationVersion_ >= 8 ? 128 : 32;
    for (int cz = -plantSurveyRadius; cz <= plantSurveyRadius && (cane == 0 || vines == 0) && generated < 128; ++cz) {
        for (int cx = -plantSurveyRadius; cx <= plantSurveyRadius && (cane == 0 || vines == 0) && generated < 128; ++cx) {
            // Only generate candidate shore/forest chunks, with a bounded budget.
            bool shore = false, forest = false;
            for (int z = 0; z < CHUNK_SIZE; ++z)
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    const TerrainSample sample = historicalPlants.sampleTerrainModern(cx * CHUNK_SIZE+x, cz * CHUNK_SIZE+z);
                    if (sample.height == SEA_LEVEL && (generationVersion_ < 8 ||
                        historicalPlants.terrainHeight(cx*CHUNK_SIZE+x-1,cz*CHUNK_SIZE+z)<SEA_LEVEL ||
                        historicalPlants.terrainHeight(cx*CHUNK_SIZE+x+1,cz*CHUNK_SIZE+z)<SEA_LEVEL ||
                        historicalPlants.terrainHeight(cx*CHUNK_SIZE+x,cz*CHUNK_SIZE+z-1)<SEA_LEVEL ||
                        historicalPlants.terrainHeight(cx*CHUNK_SIZE+x,cz*CHUNK_SIZE+z+1)<SEA_LEVEL)) shore=true;
                    forest |= sample.biome == Biome::Forest || sample.biome == Biome::BirchForest;
                }
            if ((!shore || cane > 0) && (!forest || vines > 0)) continue;
            if (generated >= 128) continue;
            ++generated;
            const auto first = historicalPlants.generateChunkDataModern(cx,cz);
            const auto second = historicalPlants.generateChunkDataModern(cx,cz);
            if (first.blocks != second.blocks) { report = "plant generation nondeterministic"; return false; }
            for (Block block : first.blocks) {
                cane += block == Block::SugarCane ? 1 : 0;
                vines += isVine(block) ? 1 : 0;
            }
        }
    }
    report = "new block meshes/UVs, deterministic Sugar Cane " + std::to_string(cane) +
             ", Vines " + std::to_string(vines) + " in " + std::to_string(generated) + " candidate chunks";
    return cane > 0 && vines > 0;
}

bool World::runGenerationSmokeTest(std::string& report) const {
    if (generationVersion_ < 2) {
        report = "modern generation test requires a new world";
        return false;
    }
    constexpr std::array<glm::ivec2, 5> positions{{
        {0, 0}, {1, 0}, {0, 1}, {8, 8}, {-8, -8}}};
    int dryCaveBlocks = 0;
    int aquiferBlocks = 0;
    int oreBlocks = 0;
    int entranceBlocks = 0;
    int maximumBorderRise = 0;
    int originSurfaceBlock = -1;
    bool deterministic = true;
    for (const glm::ivec2& position : positions) {
        const GeneratedChunk first = generateChunkDataModern(position.x, position.y);
        const GeneratedChunk second = generateChunkDataModern(position.x, position.y);
        deterministic = deterministic && first.blocks == second.blocks;
        if (position.x == 0 && position.y == 0)
            originSurfaceBlock = static_cast<int>(
                first.blocks[localIndex(0, terrainHeight(0, 0), 0)]);
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                const int worldX = position.x * CHUNK_SIZE + x;
                const int worldZ = position.y * CHUNK_SIZE + z;
                const int height = terrainHeight(worldX, worldZ);
                if (x == CHUNK_SIZE - 1) {
                    maximumBorderRise = std::max(maximumBorderRise,
                        std::abs(height - terrainHeight(worldX + 1, worldZ)));
                }
                if (z == CHUNK_SIZE - 1) {
                    maximumBorderRise = std::max(maximumBorderRise,
                        std::abs(height - terrainHeight(worldX, worldZ + 1)));
                }
                if (first.blocks[localIndex(x, height, z)] == Block::Air)
                    ++entranceBlocks;
                for (int y = 7; y < height - 4; ++y) {
                    const Block block = first.blocks[localIndex(x, y, z)];
                    dryCaveBlocks += block == Block::Air ? 1 : 0;
                    aquiferBlocks += block == Block::Water ? 1 : 0;
                    oreBlocks += (block == Block::CoalOre || block == Block::CopperOre ||
                                  block == Block::IronOre || block == Block::GoldOre ||
                                  block == Block::DiamondOre) ? 1 : 0;
                }
            }
        }
    }
    report = "deterministic " + std::string(deterministic ? "yes" : "no") +
             ", dry cave " + std::to_string(dryCaveBlocks) +
             ", aquifer " + std::to_string(aquiferBlocks) +
             ", ore " + std::to_string(oreBlocks) +
             ", entrances " + std::to_string(entranceBlocks) +
             ", max seam rise " + std::to_string(maximumBorderRise);
    report += ", origin surface block " + std::to_string(originSurfaceBlock);
    return deterministic && dryCaveBlocks > 0 && oreBlocks > 0 &&
           maximumBorderRise <= 8;
}

WorldgenSurvey World::runWorldgenSurvey() const {
    WorldgenSurvey survey;
    const std::array<glm::ivec2, 4> positions{{
        {static_cast<int>(seed_ % 31U) - 15, static_cast<int>((seed_ >> 5U) % 31U) - 15},
        {static_cast<int>((seed_ >> 9U) % 47U) + 24,
         static_cast<int>((seed_ >> 15U) % 47U) - 23},
        {-static_cast<int>((seed_ >> 3U) % 53U) - 30,
         static_cast<int>((seed_ >> 11U) % 53U) + 20},
        {static_cast<int>((seed_ >> 17U) % 59U) + 40,
         -static_cast<int>((seed_ >> 23U) % 59U) - 40}}};
    for (const glm::ivec2& position : positions) {
        const GeneratedChunk chunk = generateChunkDataModern(position.x, position.y);
        const GeneratedChunk repeated = generateChunkDataModern(position.x, position.y);
        survey.deterministic = survey.deterministic && chunk.blocks == repeated.blocks;
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                const int worldX = position.x * CHUNK_SIZE + x;
                const int worldZ = position.y * CHUNK_SIZE + z;
                const int height = terrainHeight(worldX, worldZ);
                const Block top = chunk.blocks[localIndex(x, height, z)];
                ++survey.surfaceColumns;
                if (top == Block::Air || top == Block::Water)
                    ++survey.exposedColumns;
                if (x == CHUNK_SIZE - 1) {
                    survey.maximumBorderRise = std::max(survey.maximumBorderRise,
                        std::abs(height - terrainHeight(worldX + 1, worldZ)));
                }
                if (z == CHUNK_SIZE - 1) {
                    survey.maximumBorderRise = std::max(survey.maximumBorderRise,
                        std::abs(height - terrainHeight(worldX, worldZ + 1)));
                }
                for (int y = 8; y < height - 8; ++y) {
                    const Block block = chunk.blocks[localIndex(x, y, z)];
                    survey.dryCaveBlocks += block == Block::Air ? 1 : 0;
                    survey.floodedCaveBlocks += block == Block::Water ? 1 : 0;
                }
            }
        }
    }
    if (generationVersion_ >= 5) {
        const int firstRegionX = static_cast<int>(seed_ % 11U) - 5;
        const int firstRegionZ = static_cast<int>((seed_ >> 8U) % 11U) - 5;
        for (int regionZ = firstRegionZ; regionZ < firstRegionZ + 3; ++regionZ) {
            for (int regionX = firstRegionX; regionX < firstRegionX + 3; ++regionX) {
                int entranceX = 0;
                int entranceZ = 0;
                if (!canyonCarver_.surfaceCaveStartInRegion(
                        regionX, regionZ, entranceX, entranceZ,
                        generationVersion_ >= 6))
                    continue;
                ++survey.entranceCandidates;
                const int chunkX = floorDiv(entranceX, CHUNK_SIZE);
                const int chunkZ = floorDiv(entranceZ, CHUNK_SIZE);
                const GeneratedChunk chunk = generateChunkDataModern(chunkX, chunkZ);
                if (survey.entranceCandidates == 1) {
                    const GeneratedChunk neighbor =
                        generateChunkDataModern(chunkX + 1, chunkZ);
                    survey.deterministic = survey.deterministic &&
                        !neighbor.blocks.empty() &&
                        chunk.blocks == generateChunkDataModern(chunkX, chunkZ).blocks;
                }
                const int localX = entranceX - chunkX * CHUNK_SIZE;
                const int localZ = entranceZ - chunkZ * CHUNK_SIZE;
                const int surface = terrainHeight(entranceX, entranceZ);
                const Block top = chunk.blocks[localIndex(localX, surface, localZ)];
                if (top == Block::Air || top == Block::Water) {
                    ++survey.openEntrances;
                    int openingColumns = 0;
                    for (int dz = -2; dz <= 2; ++dz) {
                        for (int dx = -2; dx <= 2; ++dx) {
                            const int neighborX = localX + dx;
                            const int neighborZ = localZ + dz;
                            if (neighborX < 0 || neighborX >= CHUNK_SIZE ||
                                neighborZ < 0 || neighborZ >= CHUNK_SIZE)
                                continue;
                            const int neighborSurface = terrainHeight(
                                entranceX + dx, entranceZ + dz);
                            const Block neighborTop = chunk.blocks[localIndex(
                                neighborX, neighborSurface, neighborZ)];
                            openingColumns += neighborTop == Block::Air ||
                                              neighborTop == Block::Water;
                        }
                    }
                    survey.entranceOpeningColumns += openingColumns;
                    survey.broadEntrances += openingColumns >= 10;
                }
            }
        }
    }
    return survey;
}

bool World::runStructureGenerationSmokeTest(
    std::string& report, glm::ivec3* representativeChest, int regionRadius) const {
    if (generationVersion_ < 3) {
        report = "legacy generator intentionally unchanged";
        return true;
    }
    const auto terrain = [&](int x, int z) { return structureTerrainAt(x, z); };
    std::array<int, 4> counts{};
    std::vector<StructureBox> majorBounds;
    bool valid = true;
    bool chestChecked = false;
    for (int regionZ = -regionRadius; regionZ <= regionRadius; ++regionZ) {
        for (int regionX = -regionRadius; regionX <= regionRadius; ++regionX) {
            const StructurePlan plan = structures_.majorPlanForRegion(
                regionX, regionZ, terrain);
            if (plan.pieces.empty()) continue;
            ++counts[static_cast<std::size_t>(plan.kind)];
            for (const StructureBox& previous : majorBounds)
                valid = valid && !plan.bounds.overlapsXZ(previous);
            majorBounds.push_back(plan.bounds);
            const StructureTerrain center = terrain(plan.pieces.front().x,
                                                    plan.pieces.front().z);
            if (plan.kind == StructureKind::DesertVillage ||
                plan.kind == StructureKind::DesertPyramid)
                valid = valid && center.biome == StructureBiome::Desert;
            if (plan.kind == StructureKind::PlainsVillage)
                valid = valid && center.biome == StructureBiome::Plains;
            for (const StructurePiece& piece : plan.pieces) {
                const auto heights = terrainHeightForFootprint(piece.bounds, terrain);
                valid = valid && heights[1] - heights[0] <= 4 &&
                        heights[0] > SEA_LEVEL + 2;
                if (chestChecked) continue;
                const StructureTemplate& templ = structureTemplate(piece.kind);
                if (templ.loot.empty()) continue;
                const StructureLootMarker& marker = templ.loot.front();
                const auto transformed = rotateLocalPosition(
                    marker.x, marker.z, piece.rotation, piece.mirror);
                const int worldX = piece.x + transformed[0];
                const int worldZ = piece.z + transformed[1];
                const int worldY = piece.y + marker.y;
                const int chunkX = floorDiv(worldX, CHUNK_SIZE);
                const int chunkZ = floorDiv(worldZ, CHUNK_SIZE);
                const GeneratedChunk generated = generateChunkDataModern(chunkX, chunkZ);
                valid = valid && generated.blocks[localIndex(
                    floorMod(worldX, CHUNK_SIZE), worldY,
                    floorMod(worldZ, CHUNK_SIZE))] == Block::Chest;
                valid = valid && std::any_of(generated.loot.begin(), generated.loot.end(),
                    [&](const StructureLootMarker& found) {
                        return found.x == worldX && found.y == worldY &&
                               found.z == worldZ;
                    });
                if (representativeChest)
                    *representativeChest = {worldX, worldY, worldZ};
                chestChecked = true;
            }
        }
    }
    const int total = counts[0] + counts[1] + counts[2] + counts[3];
    report = "real terrain regions " + std::to_string(total) +
             " (plains villages " + std::to_string(counts[0]) +
             ", desert villages " + std::to_string(counts[1]) +
             ", outposts " + std::to_string(counts[2]) +
             ", pyramids " + std::to_string(counts[3]) +
             "), slope/biome/loot " + (valid && chestChecked ? "passed" : "FAILED");
    return valid && total > 0 && chestChecked;
}

void World::workerLoop() {
    while (!stopping_) {
        glm::ivec2 request;
        {
            std::unique_lock<std::mutex> lock(generationMutex_);
            generationCv_.wait(lock, [&] { return stopping_ || !generationQueue_.empty(); });
            if (stopping_)
                return;
            request = generationQueue_.front();
            generationQueue_.pop_front();
        }
        const auto relevant = [&] {
            const std::uint64_t center = static_cast<std::uint64_t>(
                generationCenterKey_.load(std::memory_order_relaxed));
            const int centerX = static_cast<std::int32_t>(center >> 32U);
            const int centerZ = static_cast<std::int32_t>(center);
            const int radius = generationLoadRadius_.load(std::memory_order_relaxed) + 1;
            const std::int64_t dx = static_cast<std::int64_t>(request.x) - centerX;
            const std::int64_t dz = static_cast<std::int64_t>(request.y) - centerZ;
            return dx * dx + dz * dz <= radius * radius;
        };
        if (!relevant()) {
            std::lock_guard<std::mutex> lock(generationMutex_);
            pendingKeys_.erase(chunkKey(request.x, request.y));
            continue;
        }
        GeneratedChunk generated = generateChunkData(request.x, request.y);
        {
            std::lock_guard<std::mutex> lock(generationMutex_);
            if (relevant())
                completedQueue_.push_back(std::move(generated));
            else
                pendingKeys_.erase(chunkKey(request.x, request.y));
        }
    }
}

void World::generate(int renderDistance, const glm::vec3& initialPosition) {
    ++meshEpoch_;
    {
        std::lock_guard<std::mutex> lock(meshMutex_);
        meshQueue_.clear();
        completedMeshes_.clear();
        pendingMeshKeys_.clear();
    }
    dirtyQueue_.clear();
    dirtySet_.clear();
    priorityDirtyKeys_.clear();
    uploadedVertexCount_ = 0;
    renderDistance_ = std::clamp(renderDistance, 2, 64);
    requestRadius_ = -1;
    nextRequestOffset_ = 0;
    chunks_.clear();
    releaseEmptyTerrainPages();
    pendingStructureMobs_.clear();
    pendingStructureMobIds_.clear();
    fluidQueue_.clear();
    fluidQueued_.clear();
    lightingQueue_.clear();
    lightingSet_.clear();
    fluidUpdateAccumulator_ = 0.0f;
    blockEntityUpdateAccumulator_ = 0.0f;
    const int centerX = floorDiv(static_cast<int>(std::floor(initialPosition.x)), CHUNK_SIZE);
    const int centerZ = floorDiv(static_cast<int>(std::floor(initialPosition.z)), CHUNK_SIZE);
    GeneratedChunk center = generateChunkData(centerX, centerZ);
    const std::int64_t key = chunkKey(centerX, centerZ);
    const auto savedEdits = edits_.find(key);
    if (savedEdits != edits_.end()) {
        for (const auto& edit : savedEdits->second)
            center.blocks[edit.first] = edit.second;
    }
    initializeGeneratedLoot(center);
    queueGeneratedMobs(center);
    auto centerChunk = std::make_unique<Chunk>(centerX, centerZ, std::move(center.blocks));
    centerChunk->meshIdentity = nextMeshIdentity_++;
    computeSunlight(*centerChunk);
    chunks_.emplace(key, std::move(centerChunk));
    if (center.hasStructure)
        queueLightingUpdate(centerX, centerZ, false);
    if (savedEdits != edits_.end()) {
        bool containsSavedEmitter = false;
        for (const auto& edit : savedEdits->second) {
            const int localX = static_cast<int>(edit.first % CHUNK_SIZE);
            const int localZ = static_cast<int>((edit.first / CHUNK_SIZE) % CHUNK_SIZE);
            const int y = static_cast<int>(edit.first / (CHUNK_SIZE * CHUNK_SIZE));
            queueFluidNeighborhood(
                centerX * CHUNK_SIZE + localX, y, centerZ * CHUNK_SIZE + localZ);
            containsSavedEmitter = containsSavedEmitter || blockLightEmission(edit.second) != 0;
        }
        if (containsSavedEmitter)
            queueLightingUpdate(centerX, centerZ, false);
    }
    rebuildChunk(centerX, centerZ);
    streamCenter_ = {centerX, centerZ};
    generationLoadRadius_.store(
        std::max(renderDistance_, simulationDistance_), std::memory_order_relaxed);
    generationCenterKey_.store(chunkKey(centerX, centerZ), std::memory_order_relaxed);
    requestChunksAround(centerX, centerZ);
}

void World::setRenderDistance(int value) {
    value = std::clamp(value, 2, 64);
    if (renderDistance_ == value)
        return;
    renderDistance_ = value;
    requestRadius_ = -1;
    streamCenter_ = {1000000, 1000000};
}

void World::setSimulationDistance(int value) {
    value = std::clamp(value, 2, 32);
    if (simulationDistance_ == value)
        return;
    simulationDistance_ = value;
    requestRadius_ = -1;
    streamCenter_ = {1000000, 1000000};
}

void World::rebuildRequestOffsets() {
    requestRadius_ = std::max(renderDistance_, simulationDistance_);
    requestOffsets_.clear();
    requestOffsets_.reserve(static_cast<std::size_t>(
        (2 * requestRadius_ + 1) * (2 * requestRadius_ + 1)));
    for (int dz = -requestRadius_; dz <= requestRadius_; ++dz) {
        for (int dx = -requestRadius_; dx <= requestRadius_; ++dx) {
            if (dx * dx + dz * dz <= requestRadius_ * requestRadius_)
                requestOffsets_.push_back({dx, dz});
        }
    }
    std::sort(requestOffsets_.begin(), requestOffsets_.end(),
              [](const glm::ivec2& a, const glm::ivec2& b) {
                  return a.x * a.x + a.y * a.y < b.x * b.x + b.y * b.y;
              });
    nextRequestOffset_ = 0;
}

void World::discardObsoleteRequests(int centerX, int centerZ) {
    const int maximumDistance = std::max(renderDistance_, simulationDistance_) + 1;
    const auto obsolete = [&](int x, int z) {
        const int dx = x - centerX;
        const int dz = z - centerZ;
        return dx * dx + dz * dz > maximumDistance * maximumDistance;
    };
    std::lock_guard<std::mutex> lock(generationMutex_);
    generationQueue_.erase(
        std::remove_if(generationQueue_.begin(), generationQueue_.end(),
                       [&](const glm::ivec2& request) {
                           if (!obsolete(request.x, request.y))
                               return false;
                           pendingKeys_.erase(chunkKey(request.x, request.y));
                           return true;
                       }),
        generationQueue_.end());
    completedQueue_.erase(
        std::remove_if(completedQueue_.begin(), completedQueue_.end(),
                       [&](const GeneratedChunk& chunk) {
                           if (!obsolete(chunk.x, chunk.z))
                               return false;
                           pendingKeys_.erase(chunkKey(chunk.x, chunk.z));
                           return true;
                       }),
        completedQueue_.end());
}

void World::requestChunksAround(int centerX, int centerZ) {
    constexpr std::size_t MaxPendingChunks = 96;
    constexpr std::size_t RefillThreshold = 64;
    if (requestRadius_ != std::max(renderDistance_, simulationDistance_))
        rebuildRequestOffsets();
    {
        std::lock_guard<std::mutex> lock(generationMutex_);
        if (pendingKeys_.size() >= RefillThreshold)
            return;
    }
    std::lock_guard<std::mutex> lock(generationMutex_);
    while (nextRequestOffset_ < requestOffsets_.size() &&
           pendingKeys_.size() < MaxPendingChunks) {
        const glm::ivec2 offset = requestOffsets_[nextRequestOffset_++];
        const int x = centerX + offset.x;
        const int z = centerZ + offset.y;
        const std::int64_t key = chunkKey(x, z);
        if (!findChunk(x, z) && pendingKeys_.insert(key).second)
            generationQueue_.push_back({x, z});
    }
    generationCv_.notify_all();
}

void World::initializeGeneratedLoot(const GeneratedChunk& generated) {
    for (const StructureLootMarker& marker : generated.loot) {
        const int localX = marker.x - generated.x * CHUNK_SIZE;
        const int localZ = marker.z - generated.z * CHUNK_SIZE;
        if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 ||
            localZ >= CHUNK_SIZE || marker.y < 0 || marker.y >= WORLD_HEIGHT ||
            generated.blocks[localIndex(localX, marker.y, localZ)] != Block::Chest)
            continue;
        const BlockEntityPosition key{marker.x, marker.y, marker.z};
        if (chests_.find(key) != chests_.end())
            continue;
        ChestData chest;
        chest.slots = structures_.rollLoot(marker);
        chests_.emplace(key, std::move(chest));
    }
}

void World::queueGeneratedMobs(const GeneratedChunk& generated) {
    for (const StructureMobMarker& marker : generated.mobs) {
        const int localX = marker.x - generated.x * CHUNK_SIZE;
        const int localZ = marker.z - generated.z * CHUNK_SIZE;
        if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 ||
            localZ >= CHUNK_SIZE || marker.y < 2 || marker.y + 1 >= WORLD_HEIGHT)
            continue;
        if (!isSolid(generated.blocks[localIndex(localX, marker.y - 1, localZ)]) ||
            isSolid(generated.blocks[localIndex(localX, marker.y, localZ)]) ||
            isSolid(generated.blocks[localIndex(localX, marker.y + 1, localZ)]))
            continue;
        const std::uint64_t id = structureMobMarkerId(
            marker.x, marker.y, marker.z, marker.hostile);
        if (pendingStructureMobIds_.insert(id).second)
            pendingStructureMobs_.push_back(marker);
    }
}

std::vector<StructureMobMarker> World::takeActiveStructureMobMarkers(bool spawnAllowed) {
    std::vector<StructureMobMarker> active;
    for (std::size_t index = 0; index < pendingStructureMobs_.size();) {
        const StructureMobMarker& marker = pendingStructureMobs_[index];
        const bool loaded = findChunk(floorDiv(marker.x, CHUNK_SIZE),
                                      floorDiv(marker.z, CHUNK_SIZE)) != nullptr;
        if (loaded && (!spawnAllowed ||
            !simulationActiveAt(static_cast<float>(marker.x),
                                static_cast<float>(marker.z)))) {
            ++index;
            continue;
        }
        if (loaded && isSolidAt(marker.x, marker.y - 1, marker.z) &&
            !isSolidAt(marker.x, marker.y, marker.z) &&
            !isSolidAt(marker.x, marker.y + 1, marker.z))
            active.push_back(marker);
        pendingStructureMobIds_.erase(structureMobMarkerId(
            marker.x, marker.y, marker.z, marker.hostile));
        pendingStructureMobs_[index] = pendingStructureMobs_.back();
        pendingStructureMobs_.pop_back();
    }
    return active;
}

void World::integrateCompleted(int budget, int centerX, int centerZ) {
    for (int integrated = 0; integrated < budget; ++integrated) {
        GeneratedChunk generated;
        {
            std::lock_guard<std::mutex> lock(generationMutex_);
            if (completedQueue_.empty())
                break;
            generated = std::move(completedQueue_.front());
            completedQueue_.pop_front();
            pendingKeys_.erase(chunkKey(generated.x, generated.z));
        }
        const int dx = generated.x - centerX;
        const int dz = generated.z - centerZ;
        const int loadDistance = std::max(renderDistance_, simulationDistance_);
        if (dx * dx + dz * dz > (loadDistance + 1) * (loadDistance + 1))
            continue;
        const std::int64_t key = chunkKey(generated.x, generated.z);
        if (chunks_.find(key) != chunks_.end())
            continue;
        const auto edits = edits_.find(key);
        bool containsSavedEmitter = false;
        if (edits != edits_.end()) {
            for (const auto& edit : edits->second) {
                generated.blocks[edit.first] = edit.second;
                if (blockLightEmission(edit.second) != 0)
                    containsSavedEmitter = true;
            }
        }
        initializeGeneratedLoot(generated);
        queueGeneratedMobs(generated);
        auto loadedChunk =
            std::make_unique<Chunk>(generated.x, generated.z, std::move(generated.blocks));
        loadedChunk->meshIdentity = nextMeshIdentity_++;
        computeSunlight(*loadedChunk);
        chunks_.emplace(key, std::move(loadedChunk));
        if (containsSavedEmitter || generated.hasStructure)
            queueLightingUpdate(generated.x, generated.z, false);
        if (edits != edits_.end()) {
            for (const auto& edit : edits->second) {
                const int localX = static_cast<int>(edit.first % CHUNK_SIZE);
                const int localZ = static_cast<int>((edit.first / CHUNK_SIZE) % CHUNK_SIZE);
                const int y = static_cast<int>(edit.first / (CHUNK_SIZE * CHUNK_SIZE));
                queueFluidNeighborhood(generated.x * CHUNK_SIZE + localX,
                                       y,
                                       generated.z * CHUNK_SIZE + localZ);
            }
        }
        scheduleFluidBoundaryUpdates(generated.x, generated.z);
        markDirty(generated.x, generated.z);
        markDirty(generated.x + 1, generated.z);
        markDirty(generated.x - 1, generated.z);
        markDirty(generated.x, generated.z + 1);
        markDirty(generated.x, generated.z - 1);
    }
}

void World::unloadDistant(int centerX, int centerZ) {
    const int unloadDistance = std::max(renderDistance_, simulationDistance_) + 2;
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        const int dx = it->second->x - centerX;
        const int dz = it->second->z - centerZ;
        if (dx * dx + dz * dz > unloadDistance * unloadDistance) {
            dirtySet_.erase(it->first);
            priorityDirtyKeys_.erase(it->first);
            uploadedVertexCount_ -= static_cast<std::size_t>(
                it->second->opaqueVertexCount + it->second->waterVertexCount);
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
    releaseEmptyTerrainPages();
    std::lock_guard<std::mutex> lock(meshMutex_);
    const auto obsolete = [&](int x, int z) {
        const int dx = x - centerX;
        const int dz = z - centerZ;
        return dx * dx + dz * dz > unloadDistance * unloadDistance;
    };
    meshQueue_.erase(
        std::remove_if(meshQueue_.begin(), meshQueue_.end(),
                       [&](const MeshInput& input) {
                           if (!obsolete(input.x, input.z))
                               return false;
                           pendingMeshKeys_.erase(chunkKey(input.x, input.z));
                           return true;
                       }),
        meshQueue_.end());
    completedMeshes_.erase(
        std::remove_if(completedMeshes_.begin(), completedMeshes_.end(),
                       [&](const MeshOutput& output) {
                           if (!obsolete(output.x, output.z))
                               return false;
                           pendingMeshKeys_.erase(chunkKey(output.x, output.z));
                           return true;
                       }),
        completedMeshes_.end());
    for (auto pending = pendingMeshKeys_.begin(); pending != pendingMeshKeys_.end();) {
        const std::uint32_t x = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(pending->first) >> 32U);
        const std::uint32_t z = static_cast<std::uint32_t>(pending->first);
        if (obsolete(static_cast<std::int32_t>(x), static_cast<std::int32_t>(z)))
            pending = pendingMeshKeys_.erase(pending);
        else
            ++pending;
    }
}

void World::updateStreaming(const glm::vec3& playerPosition, int meshBudget) {
    const int centerX = floorDiv(static_cast<int>(std::floor(playerPosition.x)), CHUNK_SIZE);
    const int centerZ = floorDiv(static_cast<int>(std::floor(playerPosition.z)), CHUNK_SIZE);
    if (streamCenter_.x != centerX || streamCenter_.y != centerZ) {
        streamCenter_ = {centerX, centerZ};
        generationLoadRadius_.store(
            std::max(renderDistance_, simulationDistance_), std::memory_order_relaxed);
        generationCenterKey_.store(chunkKey(centerX, centerZ), std::memory_order_relaxed);
        nextRequestOffset_ = 0;
        discardObsoleteRequests(centerX, centerZ);
        unloadDistant(centerX, centerZ);
    }
    integrateCompleted(4, centerX, centerZ);
    if (!lightingQueue_.empty()) {
        const glm::ivec2 update = lightingQueue_.front();
        lightingQueue_.pop_front();
        lightingSet_.erase(chunkKey(update.x, update.y));
        rebuildBlockLightingNear(update.x, update.y);
    }
    requestChunksAround(centerX, centerZ);
    uploadCompletedMeshes(meshBudget);
    dispatchMeshJobs(meshBudget);
}

int World::pendingCpuMeshCount() const {
    std::lock_guard<std::mutex> lock(meshMutex_);
    return static_cast<int>(meshQueue_.size()) + (meshWorkerBusy_ ? 1 : 0);
}

int World::completedMeshCount() const {
    std::lock_guard<std::mutex> lock(meshMutex_);
    return static_cast<int>(completedMeshes_.size());
}

void World::dispatchMeshJobs(int budget) {
    constexpr std::size_t MaximumMeshJobs = 12;
    constexpr std::size_t MaximumBackgroundJobs = 8;
    // Priority inserts can leave obsolete queue entries behind. Bound the
    // number examined so a burst of edits cannot drain them in one frame.
    int dispatched = 0;
    int examined = 0;
    const int maximumExamined = std::max(32, budget * 8);
    while (dispatched < budget && examined < maximumExamined &&
           !dirtyQueue_.empty()) {
        ++examined;
        const std::int64_t key = dirtyQueue_.front();
        dirtyQueue_.pop_front();
        if (dirtySet_.find(key) == dirtySet_.end())
            continue;
        const auto found = chunks_.find(key);
        if (found == chunks_.end()) {
            dirtySet_.erase(key);
            priorityDirtyKeys_.erase(key);
            continue;
        }
        const bool priority = priorityDirtyKeys_.find(key) != priorityDirtyKeys_.end();
        {
            std::lock_guard<std::mutex> lock(meshMutex_);
            if (pendingMeshKeys_.find(key) != pendingMeshKeys_.end())
                continue;
            const std::size_t limit = priority ? MaximumMeshJobs : MaximumBackgroundJobs;
            if (pendingMeshKeys_.size() >= limit) {
                dirtyQueue_.push_front(key);
                break;
            }
        }
        MeshInput input = captureMeshInput(found->second->x, found->second->z);
        {
            std::lock_guard<std::mutex> lock(meshMutex_);
            pendingMeshKeys_.emplace(key, input.identity);
            if (priority)
                meshQueue_.push_front(std::move(input));
            else
                meshQueue_.push_back(std::move(input));
        }
        priorityDirtyKeys_.erase(key);
        ++dispatched;
    }
    if (dispatched > 0)
        meshCv_.notify_one();
}

World::MeshInput World::captureMeshInput(int chunkX, int chunkZ) const {
    constexpr int sunlightHalo = 3;
    constexpr int span = CHUNK_SIZE + sunlightHalo * 2;
    MeshInput input;
    input.x = chunkX;
    input.z = chunkZ;
    input.epoch = meshEpoch_;
    const Chunk* center = findChunk(chunkX, chunkZ);
    if (center) {
        if(generationVersion_>=9) {
            if(!center->climateColorsReady) {
                for(int corner=0;corner<4;++corner) {
                    const auto c=biomeClimateAt(chunkX*CHUNK_SIZE+(corner%2)*CHUNK_SIZE,
                                                chunkZ*CHUNK_SIZE+(corner/2)*CHUNK_SIZE);
                    center->climateColors[0][corner]=climateGrassColor(c);
                    center->climateColors[1][corner]=climateFoliageColor(c);
                    center->climateColors[2][corner]=climateWaterColor(c);
                }
                center->climateColorsReady=true;
            }
            input.climateTinted=true; input.climateColors=center->climateColors;
        }
        input.revision = center->meshRevision;
        input.identity = center->meshIdentity;
        input.meshHeight = std::clamp(center->highestRenderableY + 2, 1, WORLD_HEIGHT);
    }
    const std::size_t count = static_cast<std::size_t>(span * span * input.meshHeight);
    input.blocks.resize(count, Block::Air);
    input.packedLight.resize(count, 0);
    std::array<std::array<const Chunk*, 3>, 3> neighbors{};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            neighbors[static_cast<std::size_t>(dz + 1)][static_cast<std::size_t>(dx + 1)] =
                findChunk(chunkX + dx, chunkZ + dz);
    // X is contiguous in both arrays. Keep every original halo coordinate and
    // value, but traverse Y/Z/X instead of striding vertically through columns.
    for (int y = 0; y < input.meshHeight; ++y) {
        for (int z = -sunlightHalo; z < CHUNK_SIZE + sunlightHalo; ++z) {
            const int offsetZ = z < 0 ? -1 : (z >= CHUNK_SIZE ? 1 : 0);
            for (int x = -sunlightHalo; x < CHUNK_SIZE + sunlightHalo; ++x) {
                const int offsetX = x < 0 ? -1 : (x >= CHUNK_SIZE ? 1 : 0);
                const Chunk* source = neighbors[static_cast<std::size_t>(offsetZ + 1)]
                                               [static_cast<std::size_t>(offsetX + 1)];
                if (!source) continue;
                const int localX = x - offsetX * CHUNK_SIZE;
                const int localZ = z - offsetZ * CHUNK_SIZE;
                const std::size_t target = static_cast<std::size_t>(
                    (y * span + z + sunlightHalo) * span + x + sunlightHalo);
                const std::size_t sourceIndex = localIndex(localX, y, localZ);
                input.blocks[target] = source->blocks[sourceIndex];
                input.packedLight[target] = source->packedLight[sourceIndex];
            }
        }
    }
    return input;
}

void World::meshWorkerLoop() {
    while (!stopping_) {
        MeshInput input;
        MeshOutput reusable;
        {
            std::unique_lock<std::mutex> lock(meshMutex_);
            meshCv_.wait(lock, [&] { return stopping_ || !meshQueue_.empty(); });
            if (stopping_)
                return;
            input = std::move(meshQueue_.front());
            meshQueue_.pop_front();
            meshWorkerBusy_ = true;
            if (!recycledMeshes_.empty()) {
                reusable = std::move(recycledMeshes_.front());
                recycledMeshes_.pop_front();
            }
        }
        MeshOutput result = buildMesh(input, std::move(reusable));
        {
            std::lock_guard<std::mutex> lock(meshMutex_);
            meshWorkerBusy_ = false;
            if (!stopping_)
                completedMeshes_.push_back(std::move(result));
        }
    }
}

void World::recycleMeshOutput(MeshOutput result) {
    // At most two returned outputs, at most 6 MiB total retained capacity.
    if (result.opaque.capacity() > (2 * 1024 * 1024) / sizeof(VoxelVertex))
        std::vector<VoxelVertex>().swap(result.opaque);
    if (result.water.capacity() > (1024 * 1024) / sizeof(VoxelVertex))
        std::vector<VoxelVertex>().swap(result.water);
    result.opaque.clear();
    result.water.clear();
    std::lock_guard<std::mutex> lock(meshMutex_);
    if (recycledMeshes_.size() < 2 && !stopping_)
        recycledMeshes_.push_back(std::move(result));
}

void World::releaseEmptyTerrainPages() {
    terrainPages_.erase(std::remove_if(terrainPages_.begin(), terrainPages_.end(),
        [](const auto& page) { return page->allocated == 0; }), terrainPages_.end());
}

void World::uploadOpaqueMesh(Chunk& chunk, const std::vector<VoxelVertex>& vertices) {
    const std::size_t bytes = vertices.size() * sizeof(VoxelVertex);
    if (vertices.empty()) {
        if (chunk.opaquePage) {
            chunk.opaquePage->release(chunk.opaqueFirstVertex,
                                      chunk.opaqueBufferCapacity / sizeof(VoxelVertex));
            chunk.opaquePage.reset();
        }
        chunk.opaqueFirstVertex = chunk.opaqueBufferCapacity = 0;
        chunk.opaqueVertexCount = 0;
        releaseEmptyTerrainPages();
        return;
    }
    if (vertices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::length_error("Terrain mesh exceeds OpenGL vertex count");
    if (!chunk.opaquePage || bytes > chunk.opaqueBufferCapacity) {
        // A small aligned growth margin avoids relocating on every small edit.
        constexpr std::size_t alignment = 256;
        const std::size_t needed = (vertices.size() + vertices.size() / 8 + alignment - 1) /
                                   alignment * alignment;
        if (needed > static_cast<std::size_t>(std::numeric_limits<GLint>::max()))
            throw std::length_error("Terrain allocation exceeds OpenGL first-vertex range");
        std::shared_ptr<TerrainBufferPage> page;
        std::size_t first = 0;
        for (const auto& candidate : terrainPages_) {
            if (candidate->allocate(needed, first)) { page = candidate; break; }
        }
        if (!page) {
            // Fixed 8 MiB pages; only an oversized individual mesh gets a larger page.
            constexpr std::size_t pageVertices = (8 * 1024 * 1024 / sizeof(VoxelVertex)) /
                                                 alignment * alignment;
            page = std::make_shared<TerrainBufferPage>(std::max(pageVertices, needed));
            terrainPages_.push_back(page);
            const bool allocated = page->allocate(needed, first);
            assert(allocated);
            (void)allocated;
        }
        if (chunk.opaquePage)
            chunk.opaquePage->release(chunk.opaqueFirstVertex,
                                      chunk.opaqueBufferCapacity / sizeof(VoxelVertex));
        chunk.opaquePage = std::move(page);
        chunk.opaqueFirstVertex = first;
        chunk.opaqueBufferCapacity = needed * sizeof(VoxelVertex);
        releaseEmptyTerrainPages();
    }
    glBindBuffer(GL_ARRAY_BUFFER, chunk.opaquePage->vbo);
    glBufferSubData(GL_ARRAY_BUFFER,
        static_cast<GLintptr>(chunk.opaqueFirstVertex * sizeof(VoxelVertex)),
        static_cast<GLsizeiptr>(bytes), vertices.data());
    chunk.opaqueVertexCount = static_cast<GLsizei>(vertices.size());
}

void World::uploadMeshResult(const MeshOutput& result) {
    Chunk* chunk = findChunk(result.x, result.z);
    if (!chunk)
        return;
    const auto uploadStart = std::chrono::steady_clock::now();
    uploadedVertexCount_ -= static_cast<std::size_t>(
        chunk->opaqueVertexCount + chunk->waterVertexCount);
    uploadOpaqueMesh(*chunk, result.opaque);
    uploadMesh(chunk->waterVao, chunk->waterVbo, chunk->waterVertexCount,
               chunk->waterBufferCapacity, result.water);
    chunk->opaqueBounds = result.opaqueBounds;
    chunk->waterBounds = result.waterBounds;
    uploadedVertexCount_ += result.opaque.size() + result.water.size();
    lastChunkRebuildMilliseconds_ = result.buildMilliseconds;
    lastMeshUploadMilliseconds_ = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - uploadStart).count();
}

void World::uploadCompletedMeshes(int budget) {
    for (int uploaded = 0; uploaded < budget; ++uploaded) {
        MeshOutput result;
        {
            std::lock_guard<std::mutex> lock(meshMutex_);
            if (completedMeshes_.empty())
                break;
            result = std::move(completedMeshes_.front());
            completedMeshes_.pop_front();
            if (result.epoch == meshEpoch_) {
                const std::int64_t key = chunkKey(result.x, result.z);
                const auto pending = pendingMeshKeys_.find(key);
                if (pending != pendingMeshKeys_.end() &&
                    pending->second == result.identity)
                    pendingMeshKeys_.erase(pending);
            }
        }
        if (result.epoch != meshEpoch_) {
            recycleMeshOutput(std::move(result));
            continue;
        }
        const std::int64_t key = chunkKey(result.x, result.z);
        Chunk* chunk = findChunk(result.x, result.z);
        if (!chunk) {
            recycleMeshOutput(std::move(result));
            continue;
        }
        if (chunk->meshIdentity != result.identity ||
            chunk->meshRevision != result.revision) {
            if (dirtySet_.find(key) != dirtySet_.end()) {
                priorityDirtyKeys_.insert(key);
                dirtyQueue_.push_front(key);
            }
            recycleMeshOutput(std::move(result));
            continue;
        }
        dirtySet_.erase(key);
        priorityDirtyKeys_.erase(key);
        uploadMeshResult(result);
        recycleMeshOutput(std::move(result));
    }
}

bool World::loadWorld(const std::string& path, glm::vec3& playerPosition) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    char magic[8]{};
    std::uint32_t version = 0;
    std::uint32_t savedSeed = 0;
    std::uint64_t editCount = 0;
    glm::vec3 loadedPlayerPosition{};
    input.read(magic, sizeof(magic));
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    input.read(reinterpret_cast<char*>(&savedSeed), sizeof(savedSeed));
    input.read(reinterpret_cast<char*>(&loadedPlayerPosition.x), sizeof(float));
    input.read(reinterpret_cast<char*>(&loadedPlayerPosition.y), sizeof(float));
    input.read(reinterpret_cast<char*>(&loadedPlayerPosition.z), sizeof(float));
    input.read(reinterpret_cast<char*>(&editCount), sizeof(editCount));
    const char expected[8] = {'V', 'X', 'W', 'O', 'R', 'L', 'D', '3'};
    if (!input || std::memcmp(magic, expected, sizeof(magic)) != 0 ||
        (version != 1U && version != 2U) ||
        savedSeed != seed_ || editCount > 10000000ULL ||
        !std::isfinite(loadedPlayerPosition.x) ||
        !std::isfinite(loadedPlayerPosition.y) ||
        !std::isfinite(loadedPlayerPosition.z))
        return false;

    std::uint32_t loadedGenerationVersion = 1U;
    if (version >= 2U) {
        input.read(reinterpret_cast<char*>(&loadedGenerationVersion),
                   sizeof(loadedGenerationVersion));
        if (!input || (loadedGenerationVersion != 1U && loadedGenerationVersion != 2U &&
                       loadedGenerationVersion != 3U && loadedGenerationVersion != 4U &&
                       loadedGenerationVersion != 5U &&
                       loadedGenerationVersion != 6U && loadedGenerationVersion != 7U &&
                       loadedGenerationVersion != 8U && loadedGenerationVersion != 9U &&
                       loadedGenerationVersion != 10U))
            return false;
    }

    decltype(edits_) loadedEdits;
    decltype(furnaces_) loadedFurnaces;
    decltype(chests_) loadedChests;
    const auto commit = [&] {
        edits_.swap(loadedEdits);
        furnaces_.swap(loadedFurnaces);
        chests_.swap(loadedChests);
        fluidQueue_.clear();
        fluidQueued_.clear();
        fluidUpdateAccumulator_ = 0.0f;
        generationVersion_ = loadedGenerationVersion;
        playerPosition = loadedPlayerPosition;
    };
    for (std::uint64_t i = 0; i < editCount; ++i) {
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;
        std::uint32_t index = 0;
        std::uint8_t value = 0;
        input.read(reinterpret_cast<char*>(&chunkX), sizeof(chunkX));
        input.read(reinterpret_cast<char*>(&chunkZ), sizeof(chunkZ));
        input.read(reinterpret_cast<char*>(&index), sizeof(index));
        input.read(reinterpret_cast<char*>(&value), sizeof(value));
        Block loadedBlock = Block::Air;
        if (!input || index >= static_cast<std::uint32_t>(CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE) ||
            !blockFromSaveId(value, loadedBlock)) {
            return false;
        }
        loadedEdits[chunkKey(chunkX, chunkZ)][index] = loadedBlock;
    }

    char entityMagic[8]{};
    input.read(entityMagic, sizeof(entityMagic));
    if (input.eof() && input.gcount() == 0) {
        input.clear();
        commit();
        return true;
    }
    const char expectedEntities[8] = {'V', 'X', 'E', 'N', 'T', 'S', '1', '\0'};
    if (!input || std::memcmp(entityMagic, expectedEntities, sizeof(entityMagic)) != 0)
        return false;
    auto readStack = [&](ItemStack& stack) {
        std::uint8_t item = 0;
        std::int16_t count = 0;
        std::int16_t durability = 0;
        input.read(reinterpret_cast<char*>(&item), sizeof(item));
        input.read(reinterpret_cast<char*>(&count), sizeof(count));
        input.read(reinterpret_cast<char*>(&durability), sizeof(durability));
        Item loadedItem = Item::None;
        if (!input || !itemFromSaveId(item, loadedItem))
            return false;
        stack = {loadedItem, count, durability};
        if (stack.count <= 0 || stack.item == Item::None)
            stack.clear();
        return true;
    };
    std::uint32_t furnaceCount = 0;
    input.read(reinterpret_cast<char*>(&furnaceCount), sizeof(furnaceCount));
    if (!input || furnaceCount > 1000000U)
        return false;
    for (std::uint32_t i = 0; i < furnaceCount; ++i) {
        BlockEntityPosition position;
        FurnaceData furnace;
        input.read(reinterpret_cast<char*>(&position.x), sizeof(position.x));
        input.read(reinterpret_cast<char*>(&position.y), sizeof(position.y));
        input.read(reinterpret_cast<char*>(&position.z), sizeof(position.z));
        if (!readStack(furnace.input) || !readStack(furnace.fuel) ||
            !readStack(furnace.output))
            return false;
        input.read(reinterpret_cast<char*>(&furnace.fuelRemaining), sizeof(float));
        input.read(reinterpret_cast<char*>(&furnace.fuelCapacity), sizeof(float));
        input.read(reinterpret_cast<char*>(&furnace.progress), sizeof(float));
        if (!input)
            return false;
        furnace.fuelRemaining = std::max(0.0f, furnace.fuelRemaining);
        furnace.fuelCapacity = std::max(0.0f, furnace.fuelCapacity);
        furnace.progress = std::clamp(furnace.progress, 0.0f, 1.0f);
        loadedFurnaces[position] = furnace;
    }
    std::uint32_t chestCount = 0;
    input.read(reinterpret_cast<char*>(&chestCount), sizeof(chestCount));
    if (!input || chestCount > 1000000U)
        return false;
    for (std::uint32_t i = 0; i < chestCount; ++i) {
        BlockEntityPosition position;
        ChestData chest;
        input.read(reinterpret_cast<char*>(&position.x), sizeof(position.x));
        input.read(reinterpret_cast<char*>(&position.y), sizeof(position.y));
        input.read(reinterpret_cast<char*>(&position.z), sizeof(position.z));
        for (ItemStack& stack : chest.slots)
            if (!readStack(stack))
                return false;
        loadedChests[position] = chest;
    }
    commit();
    return true;
}

bool World::saveWorld(const std::string& path, const glm::vec3& playerPosition) const {
    return SaveFile::write(path, std::ios::binary, [&](std::ofstream& output) {
    const char magic[8] = {'V', 'X', 'W', 'O', 'R', 'L', 'D', '3'};
    const std::uint32_t version = 2U;
    const std::uint32_t savedGenerationVersion = generationVersion_;
    std::uint64_t editCount = 0;
    for (const auto& chunkEdits : edits_)
        editCount += chunkEdits.second.size();
    output.write(magic, sizeof(magic));
    output.write(reinterpret_cast<const char*>(&version), sizeof(version));
    output.write(reinterpret_cast<const char*>(&seed_), sizeof(seed_));
    output.write(reinterpret_cast<const char*>(&playerPosition.x), sizeof(float));
    output.write(reinterpret_cast<const char*>(&playerPosition.y), sizeof(float));
    output.write(reinterpret_cast<const char*>(&playerPosition.z), sizeof(float));
    output.write(reinterpret_cast<const char*>(&editCount), sizeof(editCount));
    output.write(reinterpret_cast<const char*>(&savedGenerationVersion),
                 sizeof(savedGenerationVersion));
    constexpr std::size_t EditRecordBytes = sizeof(std::int32_t) * 2 +
                                            sizeof(std::uint32_t) + sizeof(std::uint8_t);
    thread_local std::array<char, 64 * 1024> editBuffer{};
    std::size_t buffered = 0;
    const auto flushEdits = [&] {
        if (buffered) output.write(editBuffer.data(), static_cast<std::streamsize>(buffered));
        buffered = 0;
    };
    const auto appendField = [&](const auto& value) {
        std::memcpy(editBuffer.data() + buffered, &value, sizeof(value));
        buffered += sizeof(value);
    };
    for (const auto& chunkEdits : edits_) {
        const std::uint64_t packed = static_cast<std::uint64_t>(chunkEdits.first);
        const std::int32_t chunkX = static_cast<std::int32_t>(packed >> 32U);
        const std::int32_t chunkZ = static_cast<std::int32_t>(packed & 0xffffffffU);
        for (const auto& edit : chunkEdits.second) {
            if (buffered + EditRecordBytes > editBuffer.size()) flushEdits();
            const std::uint32_t index = static_cast<std::uint32_t>(edit.first);
            const std::uint8_t value = blockDefinition(edit.second).saveId;
            appendField(chunkX); appendField(chunkZ); appendField(index); appendField(value);
        }
    }
    flushEdits();
    const char entityMagic[8] = {'V', 'X', 'E', 'N', 'T', 'S', '1', '\0'};
    output.write(entityMagic, sizeof(entityMagic));
    auto writeStack = [&](const ItemStack& stack) {
        const std::uint8_t item = itemDefinition(stack.item).saveId;
        const std::int16_t count = static_cast<std::int16_t>(stack.count);
        const std::int16_t durability = static_cast<std::int16_t>(stack.durability);
        output.write(reinterpret_cast<const char*>(&item), sizeof(item));
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        output.write(reinterpret_cast<const char*>(&durability), sizeof(durability));
    };
    const std::uint32_t furnaceCount = static_cast<std::uint32_t>(furnaces_.size());
    output.write(reinterpret_cast<const char*>(&furnaceCount), sizeof(furnaceCount));
    for (const auto& entry : furnaces_) {
        output.write(reinterpret_cast<const char*>(&entry.first.x), sizeof(entry.first.x));
        output.write(reinterpret_cast<const char*>(&entry.first.y), sizeof(entry.first.y));
        output.write(reinterpret_cast<const char*>(&entry.first.z), sizeof(entry.first.z));
        writeStack(entry.second.input);
        writeStack(entry.second.fuel);
        writeStack(entry.second.output);
        output.write(reinterpret_cast<const char*>(&entry.second.fuelRemaining), sizeof(float));
        output.write(reinterpret_cast<const char*>(&entry.second.fuelCapacity), sizeof(float));
        output.write(reinterpret_cast<const char*>(&entry.second.progress), sizeof(float));
    }
    const std::uint32_t chestCount = static_cast<std::uint32_t>(chests_.size());
    output.write(reinterpret_cast<const char*>(&chestCount), sizeof(chestCount));
    for (const auto& entry : chests_) {
        output.write(reinterpret_cast<const char*>(&entry.first.x), sizeof(entry.first.x));
        output.write(reinterpret_cast<const char*>(&entry.first.y), sizeof(entry.first.y));
        output.write(reinterpret_cast<const char*>(&entry.first.z), sizeof(entry.first.z));
        for (const ItemStack& stack : entry.second.slots)
            writeStack(stack);
    }
    return output.good();
    });
}
int World::pendingChunkCount() const {
    std::lock_guard<std::mutex> lock(generationMutex_);
    return static_cast<int>(pendingKeys_.size());
}

void World::computeSunlight(Chunk& chunk) {
    for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
        for (int localX = 0; localX < CHUNK_SIZE; ++localX)
            computeSunlightColumn(chunk, localX, localZ);
    }
}

void World::computeSunlightColumn(Chunk& chunk, int localX, int localZ) {
    std::uint8_t light = 15;
    for (int y = WORLD_HEIGHT - 1; y >= 0; --y) {
        const std::size_t index = localIndex(localX, y, localZ);
        const Block block = chunk.blocks[index];
        if (occludesLight(block))
            light = 0;
        else if ((isLeaf(block) || isWater(block)) && light > 0)
            --light;
        chunk.setSunlightLocal(index, light);
    }
}

void World::queueLightingUpdate(int chunkX, int chunkZ, bool highPriority) {
    const std::int64_t key = chunkKey(chunkX, chunkZ);
    if (!lightingSet_.insert(key).second)
        return;
    if (highPriority)
        lightingQueue_.push_front({chunkX, chunkZ});
    else
        lightingQueue_.push_back({chunkX, chunkZ});
}

void World::rebuildBlockLightingNear(int centerChunkX, int centerChunkZ) {
    constexpr int TargetRadiusChunks = 1;
    constexpr int LightRadius = 15;
    constexpr int SourceSpan = 2 * TargetRadiusChunks + 3;
    static_assert(LightRadius < CHUNK_SIZE, "source cache covers one halo chunk");
    const int targetMinimumX = (centerChunkX - TargetRadiusChunks) * CHUNK_SIZE;
    const int targetMaximumX = (centerChunkX + TargetRadiusChunks + 1) * CHUNK_SIZE - 1;
    const int targetMinimumZ = (centerChunkZ - TargetRadiusChunks) * CHUNK_SIZE;
    const int targetMaximumZ = (centerChunkZ + TargetRadiusChunks + 1) * CHUNK_SIZE - 1;
    const int calculationMinimumX = targetMinimumX - LightRadius;
    const int calculationMaximumX = targetMaximumX + LightRadius;
    const int calculationMinimumZ = targetMinimumZ - LightRadius;
    const int calculationMaximumZ = targetMaximumZ + LightRadius;
    const int calculationWidth = calculationMaximumX - calculationMinimumX + 1;
    const int calculationDepth = calculationMaximumZ - calculationMinimumZ + 1;
    const int sourceMinimumChunkX = floorDiv(calculationMinimumX, CHUNK_SIZE);
    const int sourceMinimumChunkZ = floorDiv(calculationMinimumZ, CHUNK_SIZE);
    const auto lightIndex = [&](int x, int y, int z) {
        return static_cast<std::size_t>(
            (y * calculationDepth + (z - calculationMinimumZ)) * calculationWidth +
            (x - calculationMinimumX));
    };
    // Chunk membership cannot change during this synchronous main-thread solve.
    // Missing chunks remain Air, including propagation through their positions.
    std::array<Chunk*, SourceSpan * SourceSpan> sourceChunks{};
    auto& queue = blockLightQueueScratch_;
    queue.clear();
    for (int z = 0; z < SourceSpan; ++z) {
        for (int x = 0; x < SourceSpan; ++x) {
            const int chunkX = sourceMinimumChunkX + x;
            const int chunkZ = sourceMinimumChunkZ + z;
            Chunk* chunk = findChunk(chunkX, chunkZ);
            sourceChunks[z * SourceSpan + x] = chunk;
            if (!chunk)
                continue;
            // Sorted indices preserve the original y/z/x source insertion order.
            for (const std::uint32_t index : chunk->lightEmitters) {
                const int worldX = chunkX * CHUNK_SIZE + index % CHUNK_SIZE;
                const int worldZ = chunkZ * CHUNK_SIZE + (index / CHUNK_SIZE) % CHUNK_SIZE;
                if (worldX < calculationMinimumX || worldX > calculationMaximumX ||
                    worldZ < calculationMinimumZ || worldZ > calculationMaximumZ)
                    continue;
                queue.push_back({worldX, static_cast<int>(index / (CHUNK_SIZE * CHUNK_SIZE)),
                                 worldZ, blockLightEmission(chunk->blocks[index])});
            }
        }
    }
    const bool hasSources = !queue.empty();
    if (hasSources) {
        blockLightScratch_.resize(
            static_cast<std::size_t>(calculationWidth * calculationDepth * WORLD_HEIGHT));
        std::fill(blockLightScratch_.begin(), blockLightScratch_.end(), std::uint8_t{0});
        for (const BlockLightNode& source : queue)
            blockLightScratch_[lightIndex(source.x, source.y, source.z)] = source.level;
    }
    constexpr std::array<glm::ivec3, 6> Directions{
        {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
    // FIFO traversal is unchanged. A copy is necessary because push_back can
    // reallocate the vector; processed entries never retain dangling references.
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const BlockLightNode node = queue[head];
        if (node.level <= 1)
            continue;
        const std::uint8_t nextLevel = static_cast<std::uint8_t>(node.level - 1);
        for (const glm::ivec3& direction : Directions) {
            const int x = node.x + direction.x;
            const int y = node.y + direction.y;
            const int z = node.z + direction.z;
            if (x < calculationMinimumX || x > calculationMaximumX ||
                z < calculationMinimumZ || z > calculationMaximumZ || y < 0 ||
                y >= WORLD_HEIGHT)
                continue;
            const int cacheX = x - sourceMinimumChunkX * CHUNK_SIZE;
            const int cacheZ = z - sourceMinimumChunkZ * CHUNK_SIZE;
            const Chunk* chunk = sourceChunks[
                (cacheZ / CHUNK_SIZE) * SourceSpan + cacheX / CHUNK_SIZE];
            const Block block = chunk ? chunk->blocks[localIndex(
                cacheX % CHUNK_SIZE, y, cacheZ % CHUNK_SIZE)] : Block::Air;
            if (occludesLight(block))
                continue;
            std::uint8_t& value = blockLightScratch_[lightIndex(x, y, z)];
            if (value >= nextLevel)
                continue;
            value = nextLevel;
            queue.push_back({x, y, z, nextLevel});
        }
    }
    for (int chunkZ = centerChunkZ - TargetRadiusChunks;
         chunkZ <= centerChunkZ + TargetRadiusChunks; ++chunkZ) {
        for (int chunkX = centerChunkX - TargetRadiusChunks;
             chunkX <= centerChunkX + TargetRadiusChunks; ++chunkX) {
            Chunk* chunk = sourceChunks[
                (chunkZ - sourceMinimumChunkZ) * SourceSpan + chunkX - sourceMinimumChunkX];
            if (!chunk)
                continue;
            bool changed = false;
            if (!hasSources) {
                // Empty-source solves must still erase the previous propagated
                // field, preserve sunlight and dirty exactly the changed chunks.
                for (std::uint8_t& light : chunk->packedLight) {
                    changed = changed || (light & 0x0fU) != 0;
                    light &= 0xf0U;
                }
            } else {
                for (int y = 0; y < WORLD_HEIGHT; ++y) {
                    for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
                        const int worldZ = chunkZ * CHUNK_SIZE + localZ;
                        const std::size_t fieldRow = lightIndex(chunkX * CHUNK_SIZE, y, worldZ);
                        const std::size_t chunkRow = localIndex(0, y, localZ);
                        for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
                            const std::uint8_t light = blockLightScratch_[fieldRow + localX];
                            const std::size_t index = chunkRow + localX;
                            if (chunk->blockLightLocal(index) != light) {
                                chunk->setBlockLightLocal(index, light);
                                changed = true;
                            }
                        }
                    }
                }
            }
            if (changed)
                markDirty(chunkX, chunkZ, true);
        }
    }
    queue.clear();
    // Keep at most 2 MiB of queue capacity after exceptional source density.
    // The 78x78x256 field is fixed at 1.49 MiB and allocated only when needed.
    if (queue.capacity() > 2 * 1024 * 1024 / sizeof(BlockLightNode))
        std::vector<BlockLightNode>().swap(queue);
}

void World::rebuildBlockLighting() {
    struct LightNode {
        int x;
        int y;
        int z;
        std::uint8_t level;
    };
    std::deque<LightNode> queue;
    for (auto& entry : chunks_) {
        Chunk& chunk = *entry.second;
        for (std::uint8_t& light : chunk.packedLight)
            light &= 0xf0U;
        for (const std::uint32_t index : chunk.lightEmitters) {
            const std::uint8_t level = blockLightEmission(chunk.blocks[index]);
            chunk.setBlockLightLocal(index, level);
            queue.push_back({chunk.x * CHUNK_SIZE + static_cast<int>(index % CHUNK_SIZE),
                             static_cast<int>(index / (CHUNK_SIZE * CHUNK_SIZE)),
                             chunk.z * CHUNK_SIZE + static_cast<int>((index / CHUNK_SIZE) % CHUNK_SIZE),
                             level});
        }
    }
    constexpr glm::ivec3 directions[6] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    while (!queue.empty()) {
        const LightNode node = queue.front();
        queue.pop_front();
        if (node.level <= 1)
            continue;
        const std::uint8_t nextLevel = static_cast<std::uint8_t>(node.level - 1);
        for (const glm::ivec3& direction : directions) {
            const int x = node.x + direction.x, y = node.y + direction.y, z = node.z + direction.z;
            if (y < 0 || y >= WORLD_HEIGHT || occludesLight(getBlock(x, y, z)))
                continue;
            Chunk* chunk = findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE));
            if (!chunk)
                continue;
            const std::size_t index =
                localIndex(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE));
            if (chunk->blockLightLocal(index) >= nextLevel)
                continue;
            chunk->setBlockLightLocal(index, nextLevel);
            queue.push_back({x, y, z, nextLevel});
        }
    }
    for (const auto& entry : chunks_)
        markDirty(entry.second->x, entry.second->z);
}

std::uint8_t World::sunlightAt(int x, int y, int z) const {
    if (y >= WORLD_HEIGHT)
        return 15;
    if (y < 0)
        return 0;
    const Chunk* chunk = findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE));
    if (!chunk)
        return 0;
    const std::size_t index = localIndex(floorMod(x, CHUNK_SIZE), y,
                                         floorMod(z, CHUNK_SIZE));
    if (occludesLight(chunk->blocks[index]))
        return 0;
    std::uint8_t light = chunk->sunlightLocal(index);
    if (light == 15)
        return light;
    constexpr glm::ivec2 directions[4] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    constexpr float weights[3] = {0.90f, 0.70f, 0.48f};
    for (const glm::ivec2& direction : directions) {
        for (int distance = 1; distance <= 3; ++distance) {
            const int sampleX = x + direction.x * distance;
            const int sampleZ = z + direction.y * distance;
            const Chunk* neighbor = findChunk(
                floorDiv(sampleX, CHUNK_SIZE), floorDiv(sampleZ, CHUNK_SIZE));
            if (!neighbor)
                break;
            const std::size_t neighborIndex = localIndex(
                floorMod(sampleX, CHUNK_SIZE), y, floorMod(sampleZ, CHUNK_SIZE));
            if (occludesLight(neighbor->blocks[neighborIndex]))
                break;
            const int nearbyLight = static_cast<int>(std::round(
                neighbor->sunlightLocal(neighborIndex) * weights[distance - 1]));
            light = std::max(light, static_cast<std::uint8_t>(nearbyLight));
        }
    }
    return light;
}

std::uint8_t World::blockLightAt(int x, int y, int z) const {
    if (y < 0 || y >= WORLD_HEIGHT)
        return 0;
    const Chunk* chunk = findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE));
    if (!chunk)
        return 0;
    return chunk->blockLightLocal(
        localIndex(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE)));
}

bool World::runMeshEditSmokeTest(std::string& report) {
    synchronousMeshForSmokeTest_ = true;
    if (chunks_.empty()) {
        report = "no loaded chunk available";
        synchronousMeshForSmokeTest_ = false;
        return false;
    }

    Chunk* centerChunk = chunks_.begin()->second.get();
    const int centerX = centerChunk->x;
    const int centerZ = centerChunk->z;
    const int eastX = centerX + 1;
    const std::int64_t centerKey = chunkKey(centerX, centerZ);
    const std::int64_t eastKey = chunkKey(eastX, centerZ);

    Chunk* eastChunk = findChunk(eastX, centerZ);
    if (eastChunk == nullptr) {
        GeneratedChunk generated = generateChunkData(eastX, centerZ);
        const auto savedEdits = edits_.find(eastKey);
        if (savedEdits != edits_.end()) {
            for (const auto& edit : savedEdits->second) {
                generated.blocks[edit.first] = edit.second;
            }
        }
        auto loadedChunk =
            std::make_unique<Chunk>(eastX, centerZ, std::move(generated.blocks));
        loadedChunk->meshIdentity = nextMeshIdentity_++;
        computeSunlight(*loadedChunk);
        eastChunk = loadedChunk.get();
        chunks_.emplace(eastKey, std::move(loadedChunk));
    }

    constexpr int testY = WORLD_HEIGHT - 2;
    const int testZ = centerZ * CHUNK_SIZE + CHUNK_SIZE / 2;
    const int interiorX = centerX * CHUNK_SIZE + CHUNK_SIZE / 2;
    const int boundaryX = centerX * CHUNK_SIZE + CHUNK_SIZE - 1;
    const int eastBoundaryX = boundaryX + 1;

    const std::array<glm::ivec3, 3> testPositions{{
        {interiorX, testY, testZ},
        {boundaryX, testY, testZ},
        {eastBoundaryX, testY, testZ},
    }};
    for (const glm::ivec3& position : testPositions) {
        if (getBlock(position.x, position.y, position.z) != Block::Air) {
            report = "high-altitude mesh test area was not empty";
            synchronousMeshForSmokeTest_ = false;
            return false;
        }
    }

    const bool hadCenterEdits = edits_.find(centerKey) != edits_.end();
    const bool hadEastEdits = edits_.find(eastKey) != edits_.end();
    const std::unordered_map<std::size_t, Block> originalCenterEdits =
        hadCenterEdits ? edits_.at(centerKey) : std::unordered_map<std::size_t, Block>{};
    const std::unordered_map<std::size_t, Block> originalEastEdits =
        hadEastEdits ? edits_.at(eastKey) : std::unordered_map<std::size_t, Block>{};
    const bool lightingWasDirty = blockLightingDirty_;

    rebuildChunk(centerX, centerZ);
    rebuildChunk(eastX, centerZ);
    const GLsizei centerBaseVertices = centerChunk->opaqueVertexCount;
    const GLsizei eastBaseVertices = eastChunk->opaqueVertexCount;
    const GLsizei centerBaseWaterVertices = centerChunk->waterVertexCount;
    bool passed = true;
    const int originalRenderDistance = renderDistance_;
    float maximumEditMilliseconds = 0.0f;

    for (const int testRenderDistance : {2, 32, 64}) {
        setRenderDistance(testRenderDistance);
        const std::size_t queuedBeforeEdit = dirtySet_.size();
        setBlock(interiorX, testY, testZ, Block::Stone);
        maximumEditMilliseconds =
            std::max(maximumEditMilliseconds, lastBlockEditMilliseconds_);
        passed = passed && dirtySet_.size() == queuedBeforeEdit &&
                 centerChunk->opaqueVertexCount == centerBaseVertices + 36;
        setBlock(interiorX, testY, testZ, Block::Air);
        maximumEditMilliseconds =
            std::max(maximumEditMilliseconds, lastBlockEditMilliseconds_);
        passed = passed && dirtySet_.size() == queuedBeforeEdit &&
                 centerChunk->opaqueVertexCount == centerBaseVertices;
    }
    setRenderDistance(originalRenderDistance);
    passed = passed && maximumEditMilliseconds < 250.0f;

    // Indirect skylight should enter a shallow roof from the sides, but not
    // pass through the walls of an enclosed room.
    constexpr glm::ivec2 shadeDirections[4] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    passed = passed && sunlightAt(interiorX, testY, testZ) == 15;
    setBlock(interiorX, testY + 1, testZ, Block::Stone);
    const std::uint8_t oneBlockShade = sunlightAt(interiorX, testY, testZ);
    for (const glm::ivec2& direction : shadeDirections) {
        setBlock(interiorX + direction.x, testY + 1,
                 testZ + direction.y, Block::Stone);
    }
    const std::uint8_t porchShade = sunlightAt(interiorX, testY, testZ);
    for (const glm::ivec2& direction : shadeDirections) {
        setBlock(interiorX + direction.x, testY,
                 testZ + direction.y, Block::Stone);
    }
    const std::uint8_t enclosedShade = sunlightAt(interiorX, testY, testZ);
    passed = passed && oneBlockShade >= 13 && oneBlockShade < 15 &&
             porchShade >= 9 && porchShade < oneBlockShade &&
             enclosedShade == 0;
    for (const glm::ivec2& direction : shadeDirections) {
        setBlock(interiorX + direction.x, testY,
                 testZ + direction.y, Block::Air);
        setBlock(interiorX + direction.x, testY + 1,
                 testZ + direction.y, Block::Air);
    }
    setBlock(interiorX, testY + 1, testZ, Block::Air);
    passed = passed && sunlightAt(interiorX, testY, testZ) == 15;

    for (int iteration = 0; iteration < 12; ++iteration) {
        setBlock(interiorX, testY, testZ, Block::Stone);
        passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 36;
        setBlock(interiorX, testY, testZ, Block::Air);
        passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices;

        setBlock(boundaryX, testY, testZ, Block::Stone);
        passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 36 &&
                 eastChunk->opaqueVertexCount == eastBaseVertices;
        setBlock(eastBoundaryX, testY, testZ, Block::Stone);
        passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 30 &&
                 eastChunk->opaqueVertexCount == eastBaseVertices + 30;
        setBlock(boundaryX, testY, testZ, Block::Air);
        passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices &&
                 eastChunk->opaqueVertexCount == eastBaseVertices + 36;
        setBlock(eastBoundaryX, testY, testZ, Block::Air);
        passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices &&
                 eastChunk->opaqueVertexCount == eastBaseVertices;
    }

    setBlock(interiorX, testY, testZ, Block::TallGrass);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 24 &&
             !isSolidAt(interiorX, testY, testZ);
    setBlock(interiorX, testY, testZ, Block::LadderNorth);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 12 &&
             hasClimbableNear(glm::vec3(interiorX + .5f, testY + .5f, testZ + .5f), .45f);
    setBlock(interiorX, testY, testZ, Block::Air);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices;

    glm::vec3 collisionMinimum;
    glm::vec3 collisionMaximum;
    setBlock(interiorX, testY, testZ, Block::WoodenSlab);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 36 &&
             blockCollisionBounds({interiorX, testY, testZ},
                                  collisionMinimum,
                                  collisionMaximum) &&
             std::abs(collisionMaximum.y - (testY + 0.5f)) < 0.001f;
    setBlock(interiorX, testY, testZ, Block::WoodenSlabTop);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 36 &&
             blockCollisionBounds({interiorX, testY, testZ},
                                  collisionMinimum,
                                  collisionMaximum) &&
             std::abs(collisionMinimum.y - (testY + 0.5f)) < 0.001f;
    setBlock(interiorX, testY, testZ, Block::Air);

    setBlock(interiorX, testY, testZ, Block::DoorClosedNorthLower);
    setBlock(interiorX, testY + 1, testZ, Block::DoorClosedNorthUpper);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 60 &&
             blockCollisionBounds({interiorX, testY, testZ},
                                  collisionMinimum,
                                  collisionMaximum) &&
             collisionMaximum.z - collisionMinimum.z < 0.2f;
    setBlock(interiorX, testY, testZ, Block::DoorOpenNorthLower);
    setBlock(interiorX, testY + 1, testZ, Block::DoorOpenNorthUpper);
    passed = passed && blockCollisionBounds({interiorX, testY, testZ},
                                            collisionMinimum,
                                            collisionMaximum) &&
             collisionMaximum.x - collisionMinimum.x < 0.2f;
    setBlock(interiorX, testY + 1, testZ, Block::Air);
    setBlock(interiorX, testY, testZ, Block::Air);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices;

    // Every facing/hinge/open combination must share the same thin bounds in
    // geometry and collision, with the hinge corner stationary when opened.
    for (int facing = 0; facing < 4; ++facing) {
        for (bool right : {false, true}) {
            const Block closed = doorBlock(facing, false, right, false);
            const Block opened = doorBlock(facing, false, right, true);
            const BlockGeometryProperties closedBounds = blockGeometry(closed);
            const BlockGeometryProperties openBounds = blockGeometry(opened);
            const bool alongZ = facing < 2;
            const float thickness = .1875f;
            passed = passed && doorFacing(closed) == facing &&
                     doorFacing(opened) == facing &&
                     doorHingeRight(closed) == right && doorHingeRight(opened) == right &&
                     !isDoorOpen(closed) && isDoorOpen(opened) &&
                     std::abs((alongZ ? closedBounds.maxZ - closedBounds.minZ
                                       : closedBounds.maxX - closedBounds.minX) - thickness) < .001f &&
                     std::abs((alongZ ? openBounds.maxX - openBounds.minX
                                       : openBounds.maxZ - openBounds.minZ) - thickness) < .001f;
            const float closedHinge = alongZ
                ? ((facing == 0) == right ? closedBounds.maxX : closedBounds.minX)
                : ((facing == 2) == right ? closedBounds.maxZ : closedBounds.minZ);
            const float openHinge = alongZ
                ? ((facing == 0) == right ? openBounds.maxX : openBounds.minX)
                : ((facing == 2) == right ? openBounds.maxZ : openBounds.minZ);
            passed = passed && std::abs(closedHinge - openHinge) < .001f;
            setBlock(interiorX, testY, testZ, opened);
            passed = passed && blockCollisionBounds({interiorX, testY, testZ},
                                                    collisionMinimum, collisionMaximum) &&
                     std::abs(collisionMinimum.x - interiorX - openBounds.minX) < .001f &&
                     std::abs(collisionMaximum.z - testZ - openBounds.maxZ) < .001f;
        }
    }
    setBlock(interiorX, testY, testZ, Block::Air);
    setBlock(interiorX, testY, testZ, Block::DoorOpenNorthLower);
    setBlock(interiorX, testY, testZ - 1, Block::Stone);
    RayHit doorRay;
    passed = passed && raycast(glm::vec3(interiorX + .5f, testY + .5f,
                                          testZ + 1.5f), glm::vec3(0, 0, -1),
                               4.0f, doorRay) &&
             doorRay.block == glm::ivec3(interiorX, testY, testZ - 1);
    passed = passed && raycast(glm::vec3(interiorX + .08f, testY + .5f,
                                          testZ + 1.5f), glm::vec3(0, 0, -1),
                               4.0f, doorRay) &&
             doorRay.block == glm::ivec3(interiorX, testY, testZ);
    setBlock(interiorX, testY, testZ, Block::Air);
    setBlock(interiorX, testY, testZ - 1, Block::Air);

    setBlock(interiorX, testY, testZ, Block::Cactus);
    Player cactusPlayer(
        glm::vec3(interiorX + 1.22f, testY + 0.01f, testZ + 0.5f));
    PlayerInput noInput;
    cactusPlayer.update(0.01f, noInput, *this);
    Player creativeCactusPlayer(
        glm::vec3(interiorX + 1.22f, testY + 0.01f, testZ + 0.5f));
    creativeCactusPlayer.setCreativeMode(true);
    creativeCactusPlayer.update(0.01f, noInput, *this);
    passed = passed && cactusPlayer.health() < cactusPlayer.maxHealth() &&
             creativeCactusPlayer.health() == creativeCactusPlayer.maxHealth();
    setBlock(interiorX, testY, testZ, Block::Air);

    for (const Block fullBlock : {Block::Grass, Block::Dirt, Block::Stone}) {
        setBlock(interiorX, testY, testZ, fullBlock);
        setBlock(interiorX + 1, testY, testZ, Block::Snow);
        passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 66;
        setBlock(interiorX + 1, testY, testZ, Block::Air);
        setBlock(interiorX, testY, testZ, Block::Air);
    }

    setBlock(interiorX, testY, testZ, Block::Glass);
    setBlock(interiorX + 1, testY, testZ, Block::Snow);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 36 &&
             centerChunk->waterVertexCount == centerBaseWaterVertices + 36;
    setBlock(interiorX + 1, testY, testZ, Block::Air);
    setBlock(interiorX, testY, testZ, Block::Air);

    setBlock(interiorX, testY, testZ, Block::WoodenSlab);
    setBlock(interiorX + 1, testY, testZ, Block::Snow);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 66;
    setBlock(interiorX, testY, testZ, Block::WoodenSlabTop);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 72;
    setBlock(interiorX + 1, testY, testZ, Block::Air);
    setBlock(interiorX, testY, testZ, Block::Air);

    setBlock(interiorX, testY, testZ, Block::Stone);
    setBlock(interiorX, testY + 1, testZ, Block::Snow);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 60;
    setBlock(interiorX, testY + 1, testZ, Block::Air);
    setBlock(interiorX, testY, testZ, Block::Air);

    // A partial block's internal top does not share the cell boundary with a
    // block in the cell above. Both surfaces must remain visible across the gap.
    setBlock(interiorX, testY, testZ, Block::Snow);
    setBlock(interiorX, testY + 1, testZ, Block::Stone);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 72;
    setBlock(interiorX, testY + 1, testZ, Block::Air);
    setBlock(interiorX, testY, testZ, Block::WoodenSlab);
    setBlock(interiorX, testY + 1, testZ, Block::Stone);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 72;
    setBlock(interiorX, testY + 1, testZ, Block::Air);
    setBlock(interiorX, testY, testZ, Block::Air);

    // Exercise the geometry descriptor for every block pair and face. A hidden
    // face must be backed by real shared-boundary coverage, never merely by a
    // non-air block category.
    const int lastBlock = static_cast<int>(Block::DoorRightOpenWestUpper);
    for (int blockValue = static_cast<int>(Block::Grass); blockValue <= lastBlock;
         ++blockValue) {
        const Block testedBlock = static_cast<Block>(blockValue);
        const BlockGeometryProperties testedGeometry = blockGeometry(testedBlock);
        for (int neighborValue = static_cast<int>(Block::Air); neighborValue <= lastBlock;
             ++neighborValue) {
            const Block testedNeighbor = static_cast<Block>(neighborValue);
            const BlockGeometryProperties neighborGeometry = blockGeometry(testedNeighbor);
            for (int face = 0; face < 6; ++face) {
                const FaceOcclusion occlusion = calculateFaceOcclusion(
                    testedBlock,
                    testedNeighbor,
                    face,
                    testedGeometry.minY,
                    testedGeometry.maxY);
                if (!neighborGeometry.occludesNeighborFaces &&
                    !matchingOcclusionGroup(testedBlock, testedNeighbor)) {
                    passed = passed && !occlusion.hidden;
                }
                if (face == 2 && testedGeometry.maxY < 0.999f)
                    passed = passed && !occlusion.hidden;
                if (face == 3 && testedGeometry.minY > 0.001f)
                    passed = passed && !occlusion.hidden;
            }
        }
    }

    setBlock(boundaryX, testY, testZ, Block::Stone);
    setBlock(eastBoundaryX, testY, testZ, Block::Snow);
    passed = passed && centerChunk->opaqueVertexCount == centerBaseVertices + 36 &&
             eastChunk->opaqueVertexCount == eastBaseVertices + 30;
    setBlock(eastBoundaryX, testY, testZ, Block::Air);
    setBlock(boundaryX, testY, testZ, Block::Air);

    if (hadCenterEdits) {
        edits_[centerKey] = originalCenterEdits;
    } else {
        edits_.erase(centerKey);
    }
    if (hadEastEdits) {
        edits_[eastKey] = originalEastEdits;
    } else {
        edits_.erase(eastKey);
    }
    centerChunk->setLocal(CHUNK_SIZE / 2, testY, CHUNK_SIZE / 2, Block::Air);
    centerChunk->setLocal(CHUNK_SIZE - 1, testY, CHUNK_SIZE / 2, Block::Air);
    eastChunk->setLocal(0, testY, CHUNK_SIZE / 2, Block::Air);
    computeSunlight(*centerChunk);
    computeSunlight(*eastChunk);
    rebuildChunk(centerX, centerZ);
    rebuildChunk(eastX, centerZ);
    blockLightingDirty_ = lightingWasDirty;

    report = passed ? "local edits at render 2/32/64, lateral skylight, boundary Snow, slabs, and doors passed (" +
                          std::to_string(maximumEditMilliseconds) + " ms max)"
                    : "localized mesh or partial-face regression";
    synchronousMeshForSmokeTest_ = false;
    return passed;
}

bool World::runAsyncMeshSmokeTest(std::string& report) {
    MeshInput greedyInput;
    greedyInput.x = 0;
    greedyInput.z = 0;
    constexpr int smokeSpan = CHUNK_SIZE + 6;
    greedyInput.blocks.assign(
        static_cast<std::size_t>(smokeSpan * smokeSpan * WORLD_HEIGHT), Block::Air);
    greedyInput.packedLight.assign(greedyInput.blocks.size(), 0xf0U);
    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            const std::size_t index = static_cast<std::size_t>(
                (4 * smokeSpan + z + 3) * smokeSpan + x + 3);
            greedyInput.blocks[index] = Block::Stone;
        }
    }
    const MeshOutput greedyResult = buildMesh(greedyInput);
    const bool greedyPassed = greedyResult.opaque.size() == 36 &&
                              greedyResult.water.empty();
    MeshInput aoInput;
    aoInput.meshHeight = 8;
    aoInput.blocks.assign(static_cast<std::size_t>(smokeSpan * smokeSpan * aoInput.meshHeight),
                          Block::Air);
    aoInput.packedLight.assign(aoInput.blocks.size(), 0xf0U);
    const auto aoIndex = [&](int x, int y, int z) {
        return static_cast<std::size_t>((y * smokeSpan + z + 3) * smokeSpan + x + 3);
    };
    aoInput.blocks[aoIndex(8, 4, 8)] = Block::Stone;
    const auto sampledCorner = [&](Block neighbor) {
        aoInput.blocks[aoIndex(9, 5, 8)] = neighbor;
        const MeshOutput mesh = buildMesh(aoInput);
        for (const VoxelVertex& vertex : mesh.opaque) {
            if (std::abs(vertex.position.x - 9.0f) < 0.001f &&
                std::abs(vertex.position.y - 5.0f) < 0.001f &&
                std::abs(vertex.position.z - 9.0f) < 0.001f && vertex.normal.x > 0.9f)
                return vertex.ao;
        }
        return -1.0f;
    };
    const float airAo = sampledCorner(Block::Air);
    const float cubeAo = sampledCorner(Block::Stone);
    const float lowerSlabAo = sampledCorner(Block::WoodenSlab);
    const float upperSlabAo = sampledCorner(Block::WoodenSlabTop);
    const float snowAo = sampledCorner(Block::Snow);
    const float leavesAo = sampledCorner(Block::Leaves);
    const float glassAo = sampledCorner(Block::Glass);
    const float torchAo = sampledCorner(Block::Torch);
    const float doorClosedAo = sampledCorner(doorBlock(0, false, false, false));
    const float doorOpenAo = sampledCorner(doorBlock(0, false, false, true));
    const bool aoMeshPassed = airAo == 1.0f && cubeAo < lowerSlabAo &&
        lowerSlabAo < snowAo && snowAo < airAo &&
        upperSlabAo == airAo && leavesAo < airAo &&
        glassAo == airAo && torchAo == airAo &&
        doorClosedAo == airAo && doorOpenAo < airAo;
    const auto shadedWall = [&](int roofDepth, bool enclosed,
                                Block wallBlock, bool torch, int wallX = 8) {
        MeshInput lightingInput;
        lightingInput.meshHeight = 8;
        lightingInput.blocks.assign(
            static_cast<std::size_t>(smokeSpan * smokeSpan * lightingInput.meshHeight),
            Block::Air);
        lightingInput.packedLight.assign(lightingInput.blocks.size(), 0);
        const auto index = [&](int x, int y, int z) {
            return static_cast<std::size_t>(
                (y * smokeSpan + z + 3) * smokeSpan + x + 3);
        };
        lightingInput.blocks[index(wallX, 4, 8)] = wallBlock;
        for (int x = wallX; x <= wallX + roofDepth; ++x)
            for (int z = 7; z <= 9; ++z)
                lightingInput.blocks[index(x, 5, z)] = Block::Stone;
        if (enclosed)
            lightingInput.blocks[index(wallX + 2, 4, 8)] = Block::Stone;
        for (int z = -3; z < CHUNK_SIZE + 3; ++z) {
            for (int x = -3; x < CHUNK_SIZE + 3; ++x) {
                std::uint8_t light = 15;
                for (int y = lightingInput.meshHeight - 1; y >= 0; --y) {
                    const std::size_t cell = index(x, y, z);
                    if (occludesLight(lightingInput.blocks[cell]))
                        light = 0;
                    lightingInput.packedLight[cell] =
                        static_cast<std::uint8_t>(light << 4U);
                }
            }
        }
        if (torch)
            lightingInput.packedLight[index(wallX + 1, 4, 8)] |= 0x0fU;
        const MeshOutput mesh = buildMesh(lightingInput);
        float totalSun = 0.0f;
        float maximumBlockLight = 0.0f;
        int count = 0;
        const auto measure = [&](const std::vector<VoxelVertex>& vertices) {
            for (const VoxelVertex& vertex : vertices) {
                if (std::abs(vertex.position.x - static_cast<float>(wallX + 1)) >
                        0.001f ||
                    vertex.position.y < 4.0f || vertex.position.y > 5.0f ||
                    vertex.position.z < 8.0f || vertex.position.z > 9.0f ||
                    vertex.normal.x < 0.9f)
                    continue;
                totalSun += vertex.sunLight;
                maximumBlockLight = std::max(maximumBlockLight, vertex.blockLight);
                ++count;
            }
        };
        measure(mesh.opaque);
        measure(mesh.water);
        return std::pair<float, float>{count > 0 ? totalSun / count : -1.0f,
                                       maximumBlockLight};
    };
    const float openSun = shadedWall(0, false, Block::Stone, false).first;
    const float eaveSun = shadedWall(1, false, Block::Stone, false).first;
    const float porchSun = shadedWall(2, false, Block::Stone, false).first;
    const float windowSun = shadedWall(1, false, Block::Glass, false).first;
    const float boundarySun = shadedWall(1, false, Block::Stone, false, 15).first;
    const auto enclosedLight = shadedWall(2, true, Block::Stone, true);
    const bool shadePassed = openSun > 0.95f && eaveSun > 0.60f &&
        eaveSun < openSun && porchSun > 0.40f && porchSun < eaveSun &&
        windowSun > 0.60f && boundarySun > 0.60f &&
        enclosedLight.first < 0.10f &&
        enclosedLight.second > 0.95f;
    if (chunks_.empty()) {
        report = "no loaded chunk";
        return false;
    }
    Chunk* center = chunks_.begin()->second.get();
    const int centerX = center->x;
    const int centerZ = center->z;
    const int eastX = centerX + 1;
    Chunk* east = findChunk(eastX, centerZ);
    if (!east) {
        GeneratedChunk generated = generateChunkData(eastX, centerZ);
        auto newChunk =
            std::make_unique<Chunk>(eastX, centerZ, std::move(generated.blocks));
        newChunk->meshIdentity = nextMeshIdentity_++;
        computeSunlight(*newChunk);
        east = newChunk.get();
        chunks_.emplace(chunkKey(eastX, centerZ), std::move(newChunk));
    }
    rebuildChunk(centerX, centerZ);
    rebuildChunk(eastX, centerZ);
    const GLsizei centerBase = center->opaqueVertexCount;
    const GLsizei eastBase = east->opaqueVertexCount;
    const int y = WORLD_HEIGHT - 2;
    const int z = centerZ * CHUNK_SIZE + CHUNK_SIZE / 2;
    const int interiorX = centerX * CHUNK_SIZE + CHUNK_SIZE / 2;
    const int boundaryX = centerX * CHUNK_SIZE + CHUNK_SIZE - 1;
    const int neighborX = boundaryX + 1;
    if (getBlock(interiorX, y, z) != Block::Air ||
        getBlock(boundaryX, y, z) != Block::Air ||
        getBlock(neighborX, y, z) != Block::Air) {
        report = "test positions occupied";
        return false;
    }
    const auto waitForMeshes = [&] {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            uploadCompletedMeshes(4);
            dispatchMeshJobs(4);
            if (dirtySet_.find(chunkKey(centerX, centerZ)) == dirtySet_.end() &&
                dirtySet_.find(chunkKey(eastX, centerZ)) == dirtySet_.end())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    };
    float maximumEditMilliseconds = 0.0f;
    for (int edit = 0; edit < 12; ++edit) {
        setBlock(interiorX, y, z, Block::Stone);
        maximumEditMilliseconds =
            std::max(maximumEditMilliseconds, lastBlockEditMilliseconds_);
        setBlock(interiorX, y, z, Block::Air);
        maximumEditMilliseconds =
            std::max(maximumEditMilliseconds, lastBlockEditMilliseconds_);
    }
    bool passed = greedyPassed && aoMeshPassed && shadePassed && waitForMeshes() &&
                  center->opaqueVertexCount == centerBase &&
                  getBlock(interiorX, y, z) == Block::Air;
    setBlock(boundaryX, y, z, Block::Stone);
    setBlock(neighborX, y, z, Block::Stone);
    passed = waitForMeshes() && passed &&
             center->opaqueVertexCount == centerBase + 30 &&
             east->opaqueVertexCount == eastBase + 30;
    setBlock(boundaryX, y, z, Block::Air);
    setBlock(neighborX, y, z, Block::Air);
    passed = waitForMeshes() && passed &&
             center->opaqueVertexCount == centerBase &&
             east->opaqueVertexCount == eastBase;
    report = passed
                 ? "rapid edits, stale revisions, and boundary uploads passed (" +
                       std::to_string(maximumEditMilliseconds) +
                       " ms max edit; 3456 to " +
                       std::to_string(greedyResult.opaque.size()) +
                       " vertices on a flat test platform; eave/porch/window/room/torch "
                       "lighting, geometry-aware AO, and boundary daylight passed)"
                 : "asynchronous mesh or stale-result regression (shade " +
                       std::to_string(shadePassed) + ", open " +
                       std::to_string(openSun) + ", eave " +
                       std::to_string(eaveSun) + ", porch " +
                       std::to_string(porchSun) + ", glass " +
                       std::to_string(windowSun) + ", boundary " +
                       std::to_string(boundarySun) + ", room " +
                       std::to_string(enclosedLight.first) + ", torch " +
                       std::to_string(enclosedLight.second) + ", AO " +
                       std::to_string(aoMeshPassed) + ')';
    return passed;
}

bool World::runFluidSmokeTest(std::string& report) {
    if (chunks_.empty()) {
        report = "no loaded chunk available";
        return false;
    }

    Chunk* centerChunk = chunks_.begin()->second.get();
    const int centerX = centerChunk->x;
    const int centerZ = centerChunk->z;
    const int eastX = centerX + 1;
    const std::int64_t centerKey = chunkKey(centerX, centerZ);
    const std::int64_t eastKey = chunkKey(eastX, centerZ);

    Chunk* eastChunk = findChunk(eastX, centerZ);
    if (eastChunk == nullptr) {
        GeneratedChunk generated = generateChunkData(eastX, centerZ);
        const auto savedEdits = edits_.find(eastKey);
        if (savedEdits != edits_.end()) {
            for (const auto& edit : savedEdits->second)
                generated.blocks[edit.first] = edit.second;
        }
        auto loadedChunk =
            std::make_unique<Chunk>(eastX, centerZ, std::move(generated.blocks));
        loadedChunk->meshIdentity = nextMeshIdentity_++;
        computeSunlight(*loadedChunk);
        eastChunk = loadedChunk.get();
        chunks_.emplace(eastKey, std::move(loadedChunk));
    }

    const std::vector<Block> originalCenterBlocks = centerChunk->blocks;
    const std::vector<Block> originalEastBlocks = eastChunk->blocks;
    const bool hadCenterEdits = edits_.find(centerKey) != edits_.end();
    const bool hadEastEdits = edits_.find(eastKey) != edits_.end();
    const std::unordered_map<std::size_t, Block> originalCenterEdits =
        hadCenterEdits ? edits_.at(centerKey) : std::unordered_map<std::size_t, Block>{};
    const std::unordered_map<std::size_t, Block> originalEastEdits =
        hadEastEdits ? edits_.at(eastKey) : std::unordered_map<std::size_t, Block>{};
    const std::deque<std::int64_t> originalDirtyQueue = dirtyQueue_;
    const std::unordered_set<std::int64_t> originalDirtySet = dirtySet_;
    const std::deque<FluidCell> originalFluidQueue = fluidQueue_;
    const std::unordered_set<FluidCell, FluidCellHash> originalFluidQueued = fluidQueued_;
    const float originalFluidAccumulator = fluidUpdateAccumulator_;
    const bool lightingWasDirty = blockLightingDirty_;

    fluidQueue_.clear();
    fluidQueued_.clear();
    fluidUpdateAccumulator_ = 0.0f;

    const int baseY = WORLD_HEIGHT - 8;
    const int baseX = centerX * CHUNK_SIZE + CHUNK_SIZE / 2;
    const int baseZ = centerZ * CHUNK_SIZE + CHUNK_SIZE / 2;
    const auto setDirect = [&](int x, int y, int z, Block block) {
        Chunk* chunk = findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE));
        if (chunk != nullptr)
            chunk->setLocal(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE), block);
    };
    const auto clearTestArea = [&] {
        for (int z = baseZ - 4; z <= baseZ + 4; ++z) {
            for (int x = centerX * CHUNK_SIZE + 1;
                 x <= eastX * CHUNK_SIZE + 7;
                 ++x) {
                for (int y = baseY; y <= baseY + 5; ++y)
                    setDirect(x, y, z, Block::Air);
            }
        }
    };
    const auto advanceFluid = [&](int ticks) {
        for (int tick = 0; tick < ticks; ++tick)
            updateFluids(0.11f, 4096);
    };

    clearTestArea();
    for (int x = baseX - 3; x <= baseX + 4; ++x)
        setDirect(x, baseY, baseZ, Block::Stone);

    bool passed = waterHeight(Block::WaterFlow1) > waterHeight(Block::WaterFlow7);
    setBlock(baseX, baseY + 4, baseZ, Block::Water);
    advanceFluid(1);
    passed = passed && getBlock(baseX, baseY + 3, baseZ) == Block::WaterFalling &&
             getBlock(baseX + 1, baseY + 4, baseZ) == Block::Air;
    advanceFluid(4);
    passed = passed && getBlock(baseX, baseY + 1, baseZ) == Block::WaterFalling &&
             getBlock(baseX + 1, baseY + 1, baseZ) == Block::WaterFlow1 &&
             getBlock(baseX + 2, baseY + 1, baseZ) == Block::WaterFlow2;

    setBlock(baseX, baseY + 4, baseZ, Block::Air);
    advanceFluid(14);
    passed = passed && getBlock(baseX, baseY + 3, baseZ) == Block::Air &&
             getBlock(baseX, baseY + 1, baseZ) == Block::Air &&
             getBlock(baseX + 2, baseY + 1, baseZ) == Block::Air;

    clearTestArea();
    for (int x = baseX - 2; x <= baseX + 3; ++x)
        setDirect(x, baseY, baseZ + 2, Block::Stone);
    setDirect(baseX + 1, baseY + 1, baseZ + 2, Block::Stone);
    setBlock(baseX, baseY + 1, baseZ + 2, Block::Water);
    advanceFluid(1);
    passed = passed && getBlock(baseX + 1, baseY + 1, baseZ + 2) == Block::Stone;
    setBlock(baseX + 1, baseY + 1, baseZ + 2, Block::Air);
    advanceFluid(1);
    passed = passed && getBlock(baseX + 1, baseY + 1, baseZ + 2) == Block::WaterFlow1;

    clearTestArea();
    const int boundaryX = centerX * CHUNK_SIZE + CHUNK_SIZE - 1;
    for (int x = boundaryX - 2; x <= boundaryX + 4; ++x)
        setDirect(x, baseY, baseZ - 2, Block::Stone);
    setBlock(boundaryX, baseY + 1, baseZ - 2, Block::Water);
    advanceFluid(1);
    passed = passed &&
             getBlock(boundaryX + 1, baseY + 1, baseZ - 2) == Block::WaterFlow1 &&
             dirtySet_.find(centerKey) != dirtySet_.end() &&
             dirtySet_.find(eastKey) != dirtySet_.end();
    rebuildChunk(centerX, centerZ);
    rebuildChunk(eastX, centerZ);
    passed = passed &&
             (centerChunk->waterVertexCount > 0 || eastChunk->waterVertexCount > 0);

    clearTestArea();
    setDirect(baseX, baseY, baseZ, Block::Water);
    setDirect(baseX, baseY, baseZ - 1, Block::Stone);
    const glm::vec3 swimmerStart(
        static_cast<float>(baseX) + 0.5f,
        static_cast<float>(baseY) + 0.05f,
        static_cast<float>(baseZ) + 0.32f);
    Player swimmer(swimmerStart);
    PlayerInput swimInput;
    swimInput.enabled = true;
    swimInput.moveForward = true;
    swimInput.jump = true;
    swimmer.update(1.0f / 60.0f, swimInput, *this);
    passed = passed && swimmer.position().y > swimmerStart.y + 0.08f;
    for (int frame = 0; frame < 24; ++frame)
        swimmer.update(1.0f / 60.0f, swimInput, *this);
    passed = passed && swimmer.position().y > static_cast<float>(baseY) + 0.7f &&
             swimmer.position().z < static_cast<float>(baseZ) + 0.2f;

    centerChunk->blocks = originalCenterBlocks;
    eastChunk->blocks = originalEastBlocks;
    centerChunk->rebuildLightEmitterIndex();
    eastChunk->rebuildLightEmitterIndex();
    if (hadCenterEdits)
        edits_[centerKey] = originalCenterEdits;
    else
        edits_.erase(centerKey);
    if (hadEastEdits)
        edits_[eastKey] = originalEastEdits;
    else
        edits_.erase(eastKey);
    computeSunlight(*centerChunk);
    computeSunlight(*eastChunk);
    rebuildChunk(centerX, centerZ);
    rebuildChunk(eastX, centerZ);
    dirtyQueue_ = originalDirtyQueue;
    dirtySet_ = originalDirtySet;
    fluidQueue_ = originalFluidQueue;
    fluidQueued_ = originalFluidQueued;
    fluidUpdateAccumulator_ = originalFluidAccumulator;
    blockLightingDirty_ = lightingWasDirty;

    report = passed
                 ? "shoreline climb, downward flow, level spread, retraction, opened-block flow, boundary flow, and water meshing passed"
                 : "queued water behavior regression";
    return passed;
}
bool World::hasBlockNear(const glm::vec3& position, Block block, float radius) const {
    const glm::ivec3 minimum = glm::ivec3(glm::floor(position - glm::vec3(radius)));
    const glm::ivec3 maximum = glm::ivec3(glm::floor(position + glm::vec3(radius)));
    for (int y = std::max(0, minimum.y); y <= std::min(WORLD_HEIGHT - 1, maximum.y); ++y)
        for (int z = minimum.z; z <= maximum.z; ++z)
            for (int x = minimum.x; x <= maximum.x; ++x)
                if (getBlock(x, y, z) == block)
                    return true;
    return false;
}
bool World::hasClimbableNear(const glm::vec3& position, float radius) const {
    const glm::ivec3 minimum = glm::ivec3(glm::floor(position - glm::vec3(radius)));
    const glm::ivec3 maximum = glm::ivec3(glm::floor(position + glm::vec3(radius)));
    for (int y = std::max(0, minimum.y); y <= std::min(WORLD_HEIGHT - 1, maximum.y); ++y)
        for (int z = minimum.z; z <= maximum.z; ++z)
            for (int x = minimum.x; x <= maximum.x; ++x)
                if (isLadder(getBlock(x, y, z)))
                    return true;
    return false;
}
bool World::simulationActiveAt(float worldX, float worldZ) const {
    if (std::abs(streamCenter_.x) > 100000 || std::abs(streamCenter_.y) > 100000)
        return true;
    const int chunkX = floorDiv(static_cast<int>(std::floor(worldX)), CHUNK_SIZE);
    const int chunkZ = floorDiv(static_cast<int>(std::floor(worldZ)), CHUNK_SIZE);
    const int dx = chunkX - streamCenter_.x;
    const int dz = chunkZ - streamCenter_.y;
    return dx * dx + dz * dz <= simulationDistance_ * simulationDistance_;
}
Block World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= WORLD_HEIGHT)
        return Block::Air;
    const Chunk* chunk = findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE));
    if (!chunk)
        return Block::Air;
    return chunk->getLocal(floorMod(x, CHUNK_SIZE), y, floorMod(z, CHUNK_SIZE));
}

float World::precipitationHeight(int x, int z) const {
    const Chunk* chunk = findChunk(floorDiv(x,CHUNK_SIZE),floorDiv(z,CHUNK_SIZE));
    if (!chunk) return -1.0f;
    const int lx = x - chunk->x*CHUNK_SIZE, lz = z - chunk->z*CHUNK_SIZE;
    const int y = chunk->precipitationTop[static_cast<std::size_t>(lz*CHUNK_SIZE+lx)];
    return y < 0 ? 0.0f : static_cast<float>(y) + blockGeometry(chunk->getLocal(lx,y,lz)).maxY;
}

bool World::isSolidAt(int x, int y, int z) const {
    return isSolid(getBlock(x, y, z));
}

void World::setBlock(int x, int y, int z, Block block) {
    setBlockInternal(x, y, z, block, true);
}

bool World::canPlacePlant(const glm::ivec3& position, Block block) const {
    if (position.y <= 0 || position.y >= WORLD_HEIGHT) return false;
    if (isVine(block)) {
        constexpr glm::ivec3 support[] = {{0,0,-1}, {0,0,1}, {1,0,0}, {-1,0,0}};
        const glm::ivec3 wall = position + support[static_cast<int>(block) - static_cast<int>(Block::VineNorth)];
        const Block backing = getBlock(wall.x, wall.y, wall.z);
        return blockGeometry(backing).occludesNeighborFaces || isLeaf(backing);
    }
    const Block support=getBlock(position.x,position.y-1,position.z);
    if(block==Block::LilyPad) return isWater(support);
    if(block==Block::Bamboo && support==Block::Bamboo) return true;
    if(block==Block::Bamboo||block==Block::Fern||block==Block::DeadBush||
       block==Block::RedMushroom||block==Block::BrownMushroom) {
        if(block==Block::DeadBush && (support==Block::Sand||support==Block::Terracotta||
            support==Block::TerracottaOrange)) return true;
        return support==Block::Grass||support==Block::Dirt||support==Block::CoarseDirt||
               support==Block::Podzol||support==Block::Mycelium||support==Block::Mud;
    }
    if (block != Block::SugarCane) return true;
    const Block below = getBlock(position.x, position.y - 1, position.z);
    if (below == Block::SugarCane) return true;
    if (below != Block::Grass && below != Block::Dirt && below != Block::Sand) return false;
    for (const glm::ivec3& offset : {glm::ivec3(1,0,0), glm::ivec3(-1,0,0),
                                    glm::ivec3(0,0,1), glm::ivec3(0,0,-1)})
        if (isWater(getBlock(position.x + offset.x, position.y - 1, position.z + offset.z)))
            return true;
    return false;
}

bool World::hasLoadedChunkAt(int x, int z) const {
    return findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE)) != nullptr;
}

int World::fillBlocks(const glm::ivec3& low, const glm::ivec3& high, Block block) {
    int changed = 0;
    for (int x = low.x; x <= high.x; ++x)
        for (int z = low.z; z <= high.z; ++z)
            for (int y = low.y; y <= high.y; ++y)
                if (setBlockInternal(x, y, z, block, true, false)) ++changed;
    if (changed && !synchronousMeshForSmokeTest_) dispatchMeshJobs(2);
    return changed;
}

bool World::setBlockInternal(int x, int y, int z, Block block, bool rebuildImmediately,
                             bool dispatchImmediately) {
    const auto editStart = std::chrono::steady_clock::now();
    if (y < 0 || y >= WORLD_HEIGHT)
        return false;

    const int chunkX = floorDiv(x, CHUNK_SIZE);
    const int chunkZ = floorDiv(z, CHUNK_SIZE);
    Chunk* chunk = findChunk(chunkX, chunkZ);
    if (!chunk)
        return false;

    const int localX = floorMod(x, CHUNK_SIZE);
    const int localZ = floorMod(z, CHUNK_SIZE);
    const Block previous = chunk->getLocal(localX, y, localZ);
    if (previous == block)
        return false;
    const auto skylightAttenuation = [](Block value) {
        return occludesLight(value) ? 15 :
               (isLeaf(value) || isWater(value) ? 1 : 0);
    };
    const bool nearbySkyChanged =
        skylightAttenuation(previous) != skylightAttenuation(block);

    bool affectsBlockLighting = blockLightEmission(previous) || blockLightEmission(block) ||
                                blockLightAt(x, y, z) > 0;
    if (!affectsBlockLighting) {
        for (const glm::ivec3& direction : FaceNormals) {
            if (blockLightAt(x + direction.x, y + direction.y, z + direction.z) > 0) {
                affectsBlockLighting = true;
                break;
            }
        }
    }

    const BlockEntityPosition entityPosition{x, y, z};
    if (previous == Block::Furnace && block != Block::Furnace)
        furnaces_.erase(entityPosition);
    if (previous == Block::Chest && block != Block::Chest)
        chests_.erase(entityPosition);
    if (block == Block::Furnace)
        furnaces_.try_emplace(entityPosition);
    if (block == Block::Chest)
        chests_.try_emplace(entityPosition);

    chunk->setLocal(localX, y, localZ, block);
    computeSunlightColumn(*chunk, localX, localZ);
    if (affectsBlockLighting)
        queueLightingUpdate(chunkX, chunkZ, true);
    // Fluid propagation is transient. Player edits and placed source blocks
    // are the permanent state reconstructed on load.
    if (rebuildImmediately)
        edits_[chunkKey(chunkX, chunkZ)][localIndex(localX, y, localZ)] = block;
    rebuildTouchedChunks(x, z, rebuildImmediately, rebuildImmediately,
                         nearbySkyChanged);
    if (rebuildImmediately && dispatchImmediately && !synchronousMeshForSmokeTest_)
        dispatchMeshJobs(2);
    queueFluidNeighborhood(x, y, z);
    if (rebuildImmediately && isDoor(previous) && !isDoorUpper(previous) &&
        !isDoor(block) && y + 1 < WORLD_HEIGHT) {
        const Block upper = getBlock(x, y + 1, z);
        if (isDoor(upper) && isDoorUpper(upper))
            setBlockInternal(x, y + 1, z, Block::Air, true);
    }
    if (rebuildImmediately && block == Block::Air && y + 1 < WORLD_HEIGHT) {
        const Block above = getBlock(x, y + 1, z);
        if (isDoor(above) && !isDoorUpper(above)) {
            setBlockInternal(x, y + 2, z, Block::Air, true);
            setBlockInternal(x, y + 1, z, Block::Air, true);
        }
    }
    lastBlockEditMilliseconds_ = std::chrono::duration<float, std::milli>(
                                     std::chrono::steady_clock::now() - editStart)
                                     .count();
    return true;
}

FurnaceData* World::furnaceAt(const glm::ivec3& position, bool create) {
    const BlockEntityPosition key{position.x, position.y, position.z};
    auto found = furnaces_.find(key);
    if (found != furnaces_.end())
        return &found->second;
    if (!create || getBlock(position.x, position.y, position.z) != Block::Furnace)
        return nullptr;
    return &furnaces_.try_emplace(key).first->second;
}

ChestData* World::chestAt(const glm::ivec3& position, bool create) {
    const BlockEntityPosition key{position.x, position.y, position.z};
    auto found = chests_.find(key);
    if (found != chests_.end())
        return &found->second;
    if (!create || getBlock(position.x, position.y, position.z) != Block::Chest)
        return nullptr;
    return &chests_.try_emplace(key).first->second;
}

std::vector<ItemStack> World::takeBlockEntityContents(const glm::ivec3& position) {
    std::vector<ItemStack> contents;
    const BlockEntityPosition key{position.x, position.y, position.z};
    if (auto furnace = furnaces_.find(key); furnace != furnaces_.end()) {
        for (const ItemStack& stack :
             {furnace->second.input, furnace->second.fuel, furnace->second.output})
            if (!stack.empty())
                contents.push_back(stack);
        furnaces_.erase(furnace);
    }
    if (auto chest = chests_.find(key); chest != chests_.end()) {
        for (const ItemStack& stack : chest->second.slots)
            if (!stack.empty())
                contents.push_back(stack);
        chests_.erase(chest);
    }
    return contents;
}

void World::updateBlockEntities(float deltaTime) {
    blockEntityUpdateAccumulator_ += deltaTime;
    if (blockEntityUpdateAccumulator_ < 0.2f)
        return;
    deltaTime = blockEntityUpdateAccumulator_;
    blockEntityUpdateAccumulator_ = 0.0f;
    constexpr float SmeltSeconds = 5.0f;
    for (auto& entry : furnaces_) {
        if (!simulationActiveAt(static_cast<float>(entry.first.x),
                                static_cast<float>(entry.first.z)))
            continue;
        FurnaceData& furnace = entry.second;
        const Item result = furnace.input.empty() ? Item::None : smeltingResult(furnace.input.item);
        const bool outputAccepts = result != Item::None &&
                                   (furnace.output.empty() ||
                                    (furnace.output.item == result &&
                                     furnace.output.count < Inventory::maxStack(result)));
        if (!outputAccepts) {
            furnace.progress = 0.0f;
            continue;
        }
        if (furnace.fuelRemaining <= 0.0f && furnaceFuelSeconds(furnace.fuel.item) > 0.0f &&
            furnace.fuel.count > 0) {
            const float burnSeconds = furnaceFuelSeconds(furnace.fuel.item);
            --furnace.fuel.count;
            if (furnace.fuel.count <= 0)
                furnace.fuel.clear();
            furnace.fuelRemaining = burnSeconds;
            furnace.fuelCapacity = burnSeconds;
        }
        if (furnace.fuelRemaining <= 0.0f)
            continue;
        furnace.fuelRemaining = std::max(0.0f, furnace.fuelRemaining - deltaTime);
        furnace.progress += deltaTime / SmeltSeconds;
        while (furnace.progress >= 1.0f && !furnace.input.empty()) {
            const Item currentResult = smeltingResult(furnace.input.item);
            if (currentResult == Item::None ||
                (!furnace.output.empty() && furnace.output.item != currentResult) ||
                furnace.output.count >= Inventory::maxStack(currentResult)) {
                furnace.progress = 0.0f;
                break;
            }
            --furnace.input.count;
            if (furnace.input.count <= 0)
                furnace.input.clear();
            if (furnace.output.empty())
                furnace.output = {currentResult, 1, 0};
            else
                ++furnace.output.count;
            furnace.progress -= 1.0f;
        }
    }
}

void World::queueFluidUpdate(int x, int y, int z) {
    if (y < 0 || y >= WORLD_HEIGHT)
        return;
    if (!findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE)))
        return;

    const FluidCell cell{x, y, z};
    if (fluidQueued_.insert(cell).second)
        fluidQueue_.push_back(cell);
}

void World::queueFluidNeighborhood(int x, int y, int z) {
    queueFluidUpdate(x, y, z);
    queueFluidUpdate(x, y - 1, z);
    queueFluidUpdate(x, y + 1, z);
    queueFluidUpdate(x - 1, y, z);
    queueFluidUpdate(x + 1, y, z);
    queueFluidUpdate(x, y, z - 1);
    queueFluidUpdate(x, y, z + 1);
}

void World::updateFluidCell(const FluidCell& cell) {
    if (!simulationActiveAt(static_cast<float>(cell.x), static_cast<float>(cell.z)))
        return;
    const Block current = getBlock(cell.x, cell.y, cell.z);
    if (isWaterSource(current) || (current != Block::Air && !isWater(current)))
        return;

    Block desired = Block::Air;
    if (isWater(getBlock(cell.x, cell.y + 1, cell.z))) {
        desired = Block::WaterFalling;
    } else {
        constexpr std::array<glm::ivec3, 4> HorizontalDirections{
            {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
        int bestLevel = 8;
        for (const glm::ivec3& direction : HorizontalDirections) {
            const int neighborX = cell.x + direction.x;
            const int neighborZ = cell.z + direction.z;
            const Block provider = getBlock(neighborX, cell.y, neighborZ);
            if (!isWater(provider))
                continue;

            // A column that can still fall does not spread sideways until it reaches support.
            const Block providerBelow = getBlock(neighborX, cell.y - 1, neighborZ);
            if (!isSolid(providerBelow))
                continue;

            const int candidateLevel =
                isWaterSource(provider) || provider == Block::WaterFalling
                    ? 1
                    : waterLevel(provider) + 1;
            bestLevel = std::min(bestLevel, candidateLevel);
        }
        desired = flowingWaterBlock(bestLevel);
    }

    if (desired != current)
        setBlockInternal(cell.x, cell.y, cell.z, desired, false);
}

void World::updateFluids(float deltaTime, int updateBudget) {
    constexpr float FluidTickSeconds = 0.10f;
    fluidUpdateAccumulator_ += std::max(0.0f, deltaTime);
    int ticks = std::min(4, static_cast<int>(fluidUpdateAccumulator_ / FluidTickSeconds));
    if (ticks <= 0)
        return;
    fluidUpdateAccumulator_ -= static_cast<float>(ticks) * FluidTickSeconds;

    updateBudget = std::max(1, updateBudget);
    for (int tick = 0; tick < ticks; ++tick) {
        const int cellsThisTick =
            std::min(updateBudget, static_cast<int>(fluidQueue_.size()));
        for (int i = 0; i < cellsThisTick; ++i) {
            const FluidCell cell = fluidQueue_.front();
            fluidQueue_.pop_front();
            fluidQueued_.erase(cell);
            updateFluidCell(cell);
        }
    }
}

void World::scheduleFluidBoundaryUpdates(int chunkX, int chunkZ) {
    constexpr std::array<glm::ivec2, 4> Directions{
        {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
    for (const glm::ivec2& direction : Directions) {
        if (!findChunk(chunkX + direction.x, chunkZ + direction.y))
            continue;

        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            for (int offset = 0; offset < CHUNK_SIZE; ++offset) {
                const int localX = direction.x > 0   ? CHUNK_SIZE - 1
                                   : direction.x < 0 ? 0
                                                     : offset;
                const int localZ = direction.y > 0   ? CHUNK_SIZE - 1
                                   : direction.y < 0 ? 0
                                                     : offset;
                const int worldX = chunkX * CHUNK_SIZE + localX;
                const int worldZ = chunkZ * CHUNK_SIZE + localZ;
                const int neighborX = worldX + direction.x;
                const int neighborZ = worldZ + direction.y;
                const Block block = getBlock(worldX, y, worldZ);
                const Block neighbor = getBlock(neighborX, y, neighborZ);
                if ((isWater(block) || isWater(neighbor)) && block != neighbor) {
                    queueFluidUpdate(worldX, y, worldZ);
                    queueFluidUpdate(neighborX, y, neighborZ);
                }
            }
        }
    }
}

void World::markDirty(int chunkX, int chunkZ, bool highPriority) {
    Chunk* chunk = findChunk(chunkX, chunkZ);
    if (!chunk) {
        return;
    }

    ++chunk->meshRevision;
    const std::int64_t key = chunkKey(chunkX, chunkZ);
    if (highPriority)
        priorityDirtyKeys_.insert(key);
    const bool newlyDirty = dirtySet_.insert(key).second;
    if (newlyDirty) {
        if (highPriority) {
            dirtyQueue_.push_front(key);
        } else {
            dirtyQueue_.push_back(key);
        }
        return;
    }

    if (!highPriority) {
        return;
    }

    // The old queue entry can remain. The set skips it after this priority
    // entry has rebuilt the chunk.
    dirtyQueue_.push_front(key);
}

void World::rebuildTouchedChunks(
    int worldX, int worldZ, bool rebuildImmediately, bool highPriority,
    bool nearbySkyChanged) {
    const int chunkX = floorDiv(worldX, CHUNK_SIZE);
    const int chunkZ = floorDiv(worldZ, CHUNK_SIZE);
    const int localX = floorMod(worldX, CHUNK_SIZE);
    const int localZ = floorMod(worldZ, CHUNK_SIZE);

    std::array<glm::ivec2, 3> touchedChunks{{{chunkX, chunkZ}, {chunkX, chunkZ},
                                              {chunkX, chunkZ}}};
    int touchedCount = 1;

    const int neighborX = localX <= (nearbySkyChanged ? 2 : 0) ? -1 :
        (localX >= CHUNK_SIZE - 1 - (nearbySkyChanged ? 2 : 0) ? 1 : 0);
    const int neighborZ = localZ <= (nearbySkyChanged ? 2 : 0) ? -1 :
        (localZ >= CHUNK_SIZE - 1 - (nearbySkyChanged ? 2 : 0) ? 1 : 0);
    if (neighborX != 0) {
        touchedChunks[touchedCount++] = {chunkX + neighborX, chunkZ};
    }
    if (neighborZ != 0) {
        touchedChunks[touchedCount++] = {chunkX, chunkZ + neighborZ};
    }
    for (int index = 0; index < touchedCount; ++index) {
        const glm::ivec2 coordinates = touchedChunks[static_cast<std::size_t>(index)];
        if (rebuildImmediately && synchronousMeshForSmokeTest_ &&
            findChunk(coordinates.x, coordinates.y)) {
            const std::int64_t key = chunkKey(coordinates.x, coordinates.y);
            dirtySet_.erase(key);
            rebuildChunk(coordinates.x, coordinates.y);
        } else {
            markDirty(coordinates.x, coordinates.y, highPriority);
        }
    }
}

World::MeshOutput World::buildMesh(const MeshInput& input) {
    return buildMesh(input, MeshOutput{});
}

World::MeshOutput World::buildMesh(const MeshInput& input, MeshOutput result) {
    const int chunkX = input.x;
    const int chunkZ = input.z;
    const auto rebuildStart = std::chrono::steady_clock::now();
    result.opaque.clear();
    result.water.clear();
    result.x = chunkX;
    result.z = chunkZ;
    result.revision = input.revision;
    result.identity = input.identity;
    result.epoch = input.epoch;
    std::vector<VoxelVertex>& opaqueVertices = result.opaque;
    std::vector<VoxelVertex>& waterVertices = result.water;
    opaqueVertices.reserve(12000);
    const auto transparentVertices = [&]() -> std::vector<VoxelVertex>& {
        if (waterVertices.capacity() == 0) waterVertices.reserve(3000);
        return waterVertices;
    };
    struct GreedyFace {
        int tile = 0;
        float sun = 0.0f;
        float blockLight = 0.0f;
        float ao = 1.0f;
    };
    thread_local std::vector<GreedyFace> greedyFaceStorage;
    thread_local std::vector<int> greedySlotStorage;
    RenderScratch<GreedyFace> faceScratch(greedyFaceStorage, 1024 * 1024);
    RenderScratch<int> slotScratch(greedySlotStorage, 1024 * 1024);
    auto& greedyFaces = faceScratch.get();
    auto& greedySlots = slotScratch.get();
    greedySlots.assign(static_cast<std::size_t>(6 * input.meshHeight * CHUNK_SIZE * CHUNK_SIZE), -1);
    const auto greedySlot = [&](int face, int x, int y, int z) {
        return static_cast<std::size_t>(
            ((face * input.meshHeight + y) * CHUNK_SIZE + z) * CHUNK_SIZE + x);
    };

    // The three-column halo lets exterior faces receive daylight around short eaves.
    // This immutable snapshot is safe to read on a worker.
    constexpr int sunlightHalo = 3;
    constexpr int span = CHUNK_SIZE + sunlightHalo * 2;
    const int originX = chunkX * CHUNK_SIZE;
    const int originZ = chunkZ * CHUNK_SIZE;
    const auto sample = [&](int x, int y, int z) -> std::size_t {
        if (y < 0 || y >= input.meshHeight)
            return input.blocks.size();
        const int relativeX = x - originX;
        const int relativeZ = z - originZ;
        if (relativeX < -sunlightHalo || relativeX >= CHUNK_SIZE + sunlightHalo ||
            relativeZ < -sunlightHalo || relativeZ >= CHUNK_SIZE + sunlightHalo)
            return input.blocks.size();
        return static_cast<std::size_t>(
            (y * span + relativeZ + sunlightHalo) * span + relativeX + sunlightHalo);
    };
    const auto meshBlock = [&](int x, int y, int z) {
        const std::size_t index = sample(x, y, z);
        return index < input.blocks.size() ? input.blocks[index] : Block::Air;
    };
    const auto meshCornerAo = [&](int worldX, int y, int worldZ,
                                  const glm::ivec3& normal, const glm::vec3& corner,
                                  const BlockGeometryProperties& ownGeometry) {
        glm::ivec3 base = normal;
        int tangentAxes[2]{}, tangentCount = 0;
        for (int axis = 0; axis < 3; ++axis) {
            if (normal[axis] == 0) tangentAxes[tangentCount++] = axis;
            else if ((normal[axis] > 0 && corner[axis] < 0.999f) ||
                     (normal[axis] < 0 && corner[axis] > 0.001f))
                base[axis] = 0; // The surface lies within a partial block's cell.
        }
        const int axisA = tangentAxes[0], axisB = tangentAxes[1];
        const float middleA = (geometryMinimum(ownGeometry, axisA) +
                               geometryMaximum(ownGeometry, axisA)) * 0.5f;
        const float middleB = (geometryMinimum(ownGeometry, axisB) +
                               geometryMaximum(ownGeometry, axisB)) * 0.5f;
        glm::ivec3 sideA = base, sideB = base, diagonal = base;
        sideA[axisA] += corner[axisA] > middleA ? 1 : -1;
        sideB[axisB] += corner[axisB] > middleB ? 1 : -1;
        diagonal[axisA] = sideA[axisA];
        diagonal[axisB] = sideB[axisB];
        const auto contribution = [&](const glm::ivec3& offset) {
            return AmbientOcclusion::contribution(
                meshBlock(worldX + offset.x, y + offset.y, worldZ + offset.z),
                corner - glm::vec3(offset));
        };
        return AmbientOcclusion::cornerLevel(contribution(sideA),
                                             contribution(sideB), contribution(diagonal));
    };
    const auto meshSunlight = [&](int x, int y, int z) -> std::uint8_t {
        if (y >= input.meshHeight)
            return 15;
        const std::size_t index = sample(x, y, z);
        return index < input.packedLight.size()
                   ? static_cast<std::uint8_t>(input.packedLight[index] >> 4U) : 0;
    };
    const auto meshBlockLight = [&](int x, int y, int z) -> std::uint8_t {
        const std::size_t index = sample(x, y, z);
        return index < input.packedLight.size()
                   ? static_cast<std::uint8_t>(input.packedLight[index] & 0x0fU) : 0;
    };
    const auto sideDaylight = [&](int x, int y, int z,
                                  const glm::ivec3& normal) -> float {
        if (normal.y != 0)
            return 0.0f;
        constexpr float weights[3] = {0.90f, 0.70f, 0.48f};
        float indirect = 0.0f;
        for (int distance = 1; distance <= 3; ++distance) {
            const int sampleX = x + normal.x * distance;
            const int sampleZ = z + normal.z * distance;
            if (occludesLight(meshBlock(sampleX, y, sampleZ)))
                break;
            indirect = std::max(indirect,
                static_cast<float>(meshSunlight(sampleX, y, sampleZ)) / 15.0f *
                weights[distance - 1]);
        }
        return indirect;
    };

    for (int y = 0; y < input.meshHeight; ++y) {
        for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
            for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
                const int worldX = chunkX * CHUNK_SIZE + localX;
                const int worldZ = chunkZ * CHUNK_SIZE + localZ;
                const Block block = meshBlock(worldX, y, worldZ);
                if (!isRenderable(block))
                    continue;
                if (isPlant(block) || isWallAttachment(block)) {
                    const float sun = static_cast<float>(meshSunlight(worldX, y, worldZ)) / 15.0f;
                    const float emitted =
                        static_cast<float>(meshBlockLight(worldX, y, worldZ)) / 15.0f;
                    const auto uvs = tileUvs(atlasTile(block, 0), true);
                    auto addQuad = [&](const std::array<glm::vec3, 4>& points,
                                       const glm::vec3& normal) {
                        for (int vertex : Indices) {
                            const glm::vec3& point = points[static_cast<std::size_t>(vertex)];
                            opaqueVertices.push_back(
                                {point, uvs[static_cast<std::size_t>(vertex)], normal, sun, emitted, 1});
                        }
                        for (int vertex = 5; vertex >= 0; --vertex) {
                            const int corner = Indices[vertex];
                            opaqueVertices.push_back({points[static_cast<std::size_t>(corner)],
                                                      uvs[static_cast<std::size_t>(corner)],
                                                      -normal,
                                                      sun,
                                                      emitted,
                                                      1});
                        }
                    };
                    const float x = static_cast<float>(worldX);
                    const float z = static_cast<float>(worldZ);
                    if (isPlant(block)) {
                        addQuad({{{x + .08f, y, z + .08f},
                                  {x + .08f, y + .9f, z + .08f},
                                  {x + .92f, y + .9f, z + .92f},
                                  {x + .92f, y, z + .92f}}},
                                glm::normalize(glm::vec3(-1, 0, 1)));
                        addQuad({{{x + .92f, y, z + .08f},
                                  {x + .92f, y + .9f, z + .08f},
                                  {x + .08f, y + .9f, z + .92f},
                                  {x + .08f, y, z + .92f}}},
                                glm::normalize(glm::vec3(1, 0, 1)));
                    } else {
                        constexpr float inset = .035f;
                        if (block == Block::LadderNorth || block == Block::VineNorth)
                            addQuad({{{x, y, z + inset}, {x, y + 1, z + inset},
                                      {x + 1, y + 1, z + inset}, {x + 1, y, z + inset}}},
                                    {0, 0, 1});
                        else if (block == Block::LadderSouth || block == Block::VineSouth)
                            addQuad({{{x + 1, y, z + 1 - inset}, {x + 1, y + 1, z + 1 - inset},
                                      {x, y + 1, z + 1 - inset}, {x, y, z + 1 - inset}}},
                                    {0, 0, -1});
                        else if (block == Block::LadderEast || block == Block::VineEast)
                            addQuad({{{x + 1 - inset, y, z}, {x + 1 - inset, y + 1, z},
                                      {x + 1 - inset, y + 1, z + 1}, {x + 1 - inset, y, z + 1}}},
                                    {-1, 0, 0});
                        else
                            addQuad({{{x + inset, y, z + 1}, {x + inset, y + 1, z + 1},
                                      {x + inset, y + 1, z}, {x + inset, y, z}}},
                                    {1, 0, 0});
                    }
                    continue;
                }

                const BlockGeometryProperties ownGeometry = blockGeometry(block);
                if (isSlab(block) || isDoor(block) || isTorch(block) || block == Block::LightningRod) {
                    const BlockGeometryProperties& geometry = ownGeometry;
                    const glm::vec3 localMinimum(geometry.minX, geometry.minY, geometry.minZ);
                    const glm::vec3 localMaximum(geometry.maxX, geometry.maxY, geometry.maxZ);

                    const float sunlight =
                        static_cast<float>(meshSunlight(worldX, y, worldZ)) / 15.0f;
                    const float blockLight =
                        static_cast<float>(meshBlockLight(worldX, y, worldZ)) / 15.0f;
                    const glm::vec3 origin(worldX, y, worldZ);
                    const std::array<std::array<glm::vec3, 4>, 6> faces{{
                        {{{localMaximum.x, localMinimum.y, localMinimum.z},
                          {localMaximum.x, localMaximum.y, localMinimum.z},
                          {localMaximum.x, localMaximum.y, localMaximum.z},
                          {localMaximum.x, localMinimum.y, localMaximum.z}}},
                        {{{localMinimum.x, localMinimum.y, localMaximum.z},
                          {localMinimum.x, localMaximum.y, localMaximum.z},
                          {localMinimum.x, localMaximum.y, localMinimum.z},
                          {localMinimum.x, localMinimum.y, localMinimum.z}}},
                        {{{localMinimum.x, localMaximum.y, localMaximum.z},
                          {localMaximum.x, localMaximum.y, localMaximum.z},
                          {localMaximum.x, localMaximum.y, localMinimum.z},
                          {localMinimum.x, localMaximum.y, localMinimum.z}}},
                        {{{localMinimum.x, localMinimum.y, localMinimum.z},
                          {localMaximum.x, localMinimum.y, localMinimum.z},
                          {localMaximum.x, localMinimum.y, localMaximum.z},
                          {localMinimum.x, localMinimum.y, localMaximum.z}}},
                        {{{localMaximum.x, localMinimum.y, localMaximum.z},
                          {localMaximum.x, localMaximum.y, localMaximum.z},
                          {localMinimum.x, localMaximum.y, localMaximum.z},
                          {localMinimum.x, localMinimum.y, localMaximum.z}}},
                        {{{localMinimum.x, localMinimum.y, localMinimum.z},
                          {localMinimum.x, localMaximum.y, localMinimum.z},
                          {localMaximum.x, localMaximum.y, localMinimum.z},
                          {localMaximum.x, localMinimum.y, localMinimum.z}}},
                    }};

                    for (int face = 0; face < 6; ++face) {
                        const glm::ivec3 normal = FaceNormals[static_cast<std::size_t>(face)];
                        const Block neighbor = meshBlock(
                            worldX + normal.x, y + normal.y, worldZ + normal.z);
                        const FaceOcclusion occlusion =
                            calculateFaceOcclusion(
                                block, neighbor, face, localMinimum.y, localMaximum.y);
                        if (occlusion.hidden)
                            continue;

                        auto uvs =
                            tileUvs(atlasTile(block, face), face != 2 && face != 3);
                        if (isTorch(block)) {
                            // Crop a two-pixel shaft from the existing 16px tile.
                            // Twelve vertical texels cover the .75-block model;
                            // the existing flame pixels remain at its upper end.
                            const float left = (atlasTile(block, face) + 7.0f / 16.0f) / AtlasTiles;
                            const float right = (atlasTile(block, face) + 9.0f / 16.0f) / AtlasTiles;
                            constexpr float inset = 0.0005f;
                            const float top = (face == 3 ? 14.0f : 4.0f) / 16.0f + inset;
                            const float bottom = (face == 2 ? 6.0f : 16.0f) / 16.0f - inset;
                            if (face == 2 || face == 3)
                                uvs = {{{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
                            else
                                uvs = {{{left, bottom}, {left, top}, {right, top}, {right, bottom}}};
                        }
                        const float faceSunlight = std::max(
                            sunlight, sideDaylight(worldX, y, worldZ, normal));
                        auto visibleFace = faces[static_cast<std::size_t>(face)];
                        if (normal.y == 0 && isSlab(block)) {
                            for (glm::vec3& point : visibleFace) {
                                point.y = point.y < (localMinimum.y + localMaximum.y) * 0.5f
                                              ? occlusion.visibleMinimumY
                                              : occlusion.visibleMaximumY;
                            }
                        }
                        for (int vertex : Indices) {
                            const std::size_t corner = static_cast<std::size_t>(vertex);
                            opaqueVertices.push_back({origin + visibleFace[corner],
                                                      uvs[corner],
                                                      glm::vec3(normal),
                                                      faceSunlight,
                                                      blockLight,
                                                      meshCornerAo(worldX, y, worldZ, normal,
                                                                   visibleFace[corner], geometry)});
                        }
                    }
                    continue;
                }
                float geometryHeight =
                    isWater(block) ? waterHeight(block) : blockCollisionHeight(block);
                if (isWater(block) && isWater(meshBlock(worldX, y + 1, worldZ)))
                    geometryHeight = 1.0f;

                for (int face = 0; face < 6; ++face) {
                    const glm::ivec3 normal = FaceNormals[static_cast<std::size_t>(face)];
                    const int neighborX = worldX + normal.x;
                    const int neighborY = y + normal.y;
                    const int neighborZ = worldZ + normal.z;
                    const Block neighbor = meshBlock(neighborX, neighborY, neighborZ);
                    const float indirectDaylight = isWater(block) ? 0.0f
                        : sideDaylight(worldX, y, worldZ, normal);
                    float visibleFaceBottom = 0.0f;
                    float visibleFaceTop = geometryHeight;
                    if (isWater(block)) {
                        if (isWater(neighbor)) {
                            if (normal.y != 0)
                                continue;
                            float neighborHeight = waterHeight(neighbor);
                            if (isWater(meshBlock(neighborX, neighborY + 1, neighborZ)))
                                neighborHeight = 1.0f;
                            if (neighborHeight >= geometryHeight - 0.001f)
                                continue;
                            visibleFaceBottom = neighborHeight;
                        } else if (neighbor != Block::Air) {
                            const FaceOcclusion occlusion = calculateFaceOcclusion(
                                block, neighbor, face, 0.0f, geometryHeight);
                            if (occlusion.hidden)
                                continue;
                            visibleFaceBottom = occlusion.visibleMinimumY;
                            visibleFaceTop = occlusion.visibleMaximumY;
                        }
                    } else if (neighbor != Block::Air && !isWater(neighbor)) {
                        const FaceOcclusion occlusion =
                            calculateFaceOcclusion(
                                block, neighbor, face, 0.0f, geometryHeight);
                        if (occlusion.hidden)
                            continue;
                        visibleFaceBottom = occlusion.visibleMinimumY;
                        visibleFaceTop = occlusion.visibleMaximumY;
                    }
                    const auto uvs = tileUvs(atlasTile(block, face), face != 2 && face != 3);
                    float cornerSun[4]{}, cornerBlock[4]{}, cornerAo[4]{1, 1, 1, 1};
                    int tangentAxes[2] = {0, 0};
                    int tangentCount = 0;
                    for (int axis = 0; axis < 3; ++axis)
                        if (normal[axis] == 0)
                            tangentAxes[tangentCount++] = axis;
                    for (int corner = 0; corner < 4; ++corner) {
                        const float* cornerPosition = FaceCorners[face][corner];
                        glm::ivec3 sideA = normal, sideB = normal, diagonal = normal;
                        const int signA = cornerPosition[tangentAxes[0]] > 0.5f ? 1 : -1;
                        const int signB = cornerPosition[tangentAxes[1]] > 0.5f ? 1 : -1;
                        sideA[tangentAxes[0]] += signA;
                        sideB[tangentAxes[1]] += signB;
                        diagonal[tangentAxes[0]] += signA;
                        diagonal[tangentAxes[1]] += signB;
                        if (!isWater(block)) {
                            glm::vec3 actualCorner(cornerPosition[0], cornerPosition[1],
                                                   cornerPosition[2]);
                            if (normal.y == 0)
                                actualCorner.y = cornerPosition[1] > 0.5f
                                    ? visibleFaceTop : visibleFaceBottom;
                            else if (normal.y > 0)
                                actualCorner.y = geometryHeight;
                            cornerAo[corner] = meshCornerAo(worldX, y, worldZ, normal,
                                                             actualCorner, ownGeometry);
                        }
                        const std::array<glm::ivec3, 4> samples{{normal, sideA, sideB, diagonal}};
                        std::uint8_t sun = 0, emitted = 0;
                        for (const glm::ivec3& offset : samples) {
                            sun = std::max(
                                sun,
                                meshSunlight(worldX + offset.x, y + offset.y, worldZ + offset.z));
                            emitted = std::max(
                                emitted,
                                meshBlockLight(worldX + offset.x, y + offset.y, worldZ + offset.z));
                        }
                        cornerSun[corner] = std::max(
                            static_cast<float>(sun) / 15.0f, indirectDaylight);
                        cornerBlock[corner] = static_cast<float>(emitted) / 15.0f;
                    }
                    const bool ordinaryOpaqueCube =
                        ownGeometry.shape == BlockShape::Cube &&
                        ownGeometry.occludesNeighborFaces && !isWater(block) &&
                        block != Block::Cactus;
                    bool uniformLighting = ordinaryOpaqueCube;
                    for (int corner = 1; corner < 4 && uniformLighting; ++corner) {
                        uniformLighting = cornerSun[corner] == cornerSun[0] &&
                                          cornerBlock[corner] == cornerBlock[0] &&
                                          cornerAo[corner] == cornerAo[0];
                    }
                    if (uniformLighting) {
                        greedySlots[greedySlot(face, localX, y, localZ)] =
                            static_cast<int>(greedyFaces.size());
                        greedyFaces.push_back({atlasTile(block, face),
                                               cornerSun[0],
                                               cornerBlock[0],
                                               cornerAo[0]});
                        continue;
                    }
                    std::vector<VoxelVertex>& vertices =
                        (isWater(block) || block == Block::Glass || block == Block::Ice)
                            ? transparentVertices()
                            : opaqueVertices;
                    for (int vertex = 0; vertex < 6; ++vertex) {
                        const int corner = Indices[vertex];
                        const float* p = FaceCorners[face][corner];
                        float py = p[1];
                        if (py > 0.5f)
                            py = visibleFaceTop;
                        else if (normal.y == 0)
                            py = visibleFaceBottom;
                        vertices.push_back({{worldX + p[0], y + py, worldZ + p[2]},
                                            uvs[static_cast<std::size_t>(corner)],
                                            glm::vec3(normal),
                                            cornerSun[corner],
                                            cornerBlock[corner],
                                            cornerAo[corner]});
                    }
                }
            }
        }
    }
    // Merge only full opaque faces whose four corners have identical lighting/AO.
    // Nonuniform faces and every custom/transparent shape retain the original path.
    for (int face = 0; face < 6; ++face) {
        const int normalAxis = face < 2 ? 0 : (face < 4 ? 1 : 2);
        const int uAxis = face < 2 ? 2 : 0;
        const int vAxis = face < 4 && face >= 2 ? 2 : 1;
        const int sliceCount = normalAxis == 1 ? input.meshHeight : CHUNK_SIZE;
        const int uCount = uAxis == 1 ? input.meshHeight : CHUNK_SIZE;
        const int vCount = vAxis == 1 ? input.meshHeight : CHUNK_SIZE;
        const auto slotAt = [&](int slice, int u, int v) {
            int coordinate[3]{};
            coordinate[normalAxis] = slice;
            coordinate[uAxis] = u;
            coordinate[vAxis] = v;
            return greedySlot(face, coordinate[0], coordinate[1], coordinate[2]);
        };
        for (int slice = 0; slice < sliceCount; ++slice) {
            for (int v = 0; v < vCount; ++v) {
                for (int u = 0; u < uCount; ++u) {
                    const std::size_t firstSlot = slotAt(slice, u, v);
                    const int firstIndex = greedySlots[firstSlot];
                    if (firstIndex < 0)
                        continue;
                    const GreedyFace key = greedyFaces[static_cast<std::size_t>(firstIndex)];
                    const auto compatible = [&](int candidateU, int candidateV) {
                        const int index = greedySlots[slotAt(slice, candidateU, candidateV)];
                        if (index < 0)
                            return false;
                        const GreedyFace& candidate =
                            greedyFaces[static_cast<std::size_t>(index)];
                        return candidate.tile == key.tile && candidate.sun == key.sun &&
                               candidate.blockLight == key.blockLight && candidate.ao == key.ao;
                    };
                    int width = 1;
                    while (u + width < uCount && compatible(u + width, v))
                        ++width;
                    int height = 1;
                    bool complete = true;
                    while (v + height < vCount && complete) {
                        for (int offset = 0; offset < width; ++offset) {
                            if (!compatible(u + offset, v + height)) {
                                complete = false;
                                break;
                            }
                        }
                        if (complete)
                            ++height;
                    }
                    for (int row = 0; row < height; ++row)
                        for (int column = 0; column < width; ++column)
                            greedySlots[slotAt(slice, u + column, v + row)] = -1;

                    int origin[3]{};
                    origin[normalAxis] = slice;
                    origin[uAxis] = u;
                    origin[vAxis] = v;
                    const float worldOrigin[3]{static_cast<float>(originX + origin[0]),
                                               static_cast<float>(origin[1]),
                                               static_cast<float>(originZ + origin[2])};
                    const glm::vec3 normal(FaceNormals[static_cast<std::size_t>(face)]);
                    for (int vertex : Indices) {
                        const float* corner = FaceCorners[face][vertex];
                        float coordinates[3]{corner[0], corner[1], corner[2]};
                        coordinates[uAxis] *= static_cast<float>(width);
                        coordinates[vAxis] *= static_cast<float>(height);
                        const glm::vec3 position(worldOrigin[0] + coordinates[0],
                                                 worldOrigin[1] + coordinates[1],
                                                 worldOrigin[2] + coordinates[2]);
                        const glm::vec2 uv = face == 2 || face == 3
                                                 ? glm::vec2((vertex == 1 || vertex == 2)
                                                                 ? static_cast<float>(width) : 0.0f,
                                                             (vertex == 2 || vertex == 3)
                                                                 ? static_cast<float>(height) : 0.0f)
                                                 : glm::vec2((vertex == 2 || vertex == 3)
                                                                 ? static_cast<float>(width) : 0.0f,
                                                             (vertex == 0 || vertex == 3)
                                                                 ? static_cast<float>(height) : 0.0f);
                        opaqueVertices.push_back({position, uv, normal, key.sun,
                                                  key.blockLight, key.ao,
                                                  static_cast<float>(key.tile)});
                    }
                }
            }
        }
    }
    if(input.climateTinted) {
        auto tint=[&](std::vector<VoxelVertex>& vertices) {
            for(auto& v:vertices) {
                const int tile=v.atlasTile>=0?int(v.atlasTile):int(v.uv.x*BlockAtlasTiles);
                int category=-1;
                if(tile==0||tile==1||tile==40||tile==88) category=0;
                if(tile==7||tile==27||tile==63||tile==67||tile==71||tile==75) category=1;
                if(tile==8) category=2;
                if(category<0) continue;
                const float x=std::clamp((v.position.x-originX)/CHUNK_SIZE,0.f,1.f);
                const float z=std::clamp((v.position.z-originZ)/CHUNK_SIZE,0.f,1.f);
                const auto& colors=input.climateColors[category];
                const auto c=glm::mix(glm::mix(colors[0],colors[1],x),glm::mix(colors[2],colors[3],x),z);
                const auto channel=[](float f) { return std::uint32_t(std::clamp(f,0.f,1.f)*255.f+.5f); };
                v.atlasTile.setTint(channel(c.r)|(channel(c.g)<<8U)|(channel(c.b)<<16U));
            }
        };
        tint(opaqueVertices); tint(waterVertices);
    }
    result.opaqueBounds = meshBounds(opaqueVertices);
    result.waterBounds = meshBounds(waterVertices);
    result.buildMilliseconds = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - rebuildStart).count();
    return result;
}

void World::rebuildChunk(int chunkX, int chunkZ) {
    if (!findChunk(chunkX, chunkZ))
        return;
    uploadMeshResult(buildMesh(captureMeshInput(chunkX, chunkZ)));
}

const World::VisibleChunks& World::collectVisibleChunks(
    const glm::mat4& viewProjection, const glm::vec3& cameraPosition) const {
    const auto planes = frustumPlanes(viewProjection);
    auto& visible = visibleChunks_;
    const auto reset = [](std::vector<const Chunk*>& list) {
        list.clear();
        if (list.capacity() > (1024 * 1024) / sizeof(const Chunk*))
            std::vector<const Chunk*>().swap(list);
    };
    reset(visible.opaque);
    reset(visible.water);
    visible.traversed = visible.distanceAccepted = visible.legacyVisible = 0;
    for (const auto& entry : chunks_) {
        ++visible.traversed;
        const Chunk& chunk = *entry.second;
        const int dx = chunk.x - streamCenter_.x;
        const int dz = chunk.z - streamCenter_.y;
        if (dx * dx + dz * dz > renderDistance_ * renderDistance_) continue;
        ++visible.distanceAccepted;
        if ((!chunk.opaqueVertexCount && !chunk.waterVertexCount) ||
            !chunkInFrustum(chunk, planes)) continue;
        ++visible.legacyVisible;
        if (chunk.opaqueVertexCount && meshInFrustum(chunk.opaqueBounds, planes))
            visible.opaque.push_back(&chunk);
        // Preserve the original sort input, including off-screen mesh bounds.
        // Removing candidates before std::sort can reorder equal-distance ties.
        if (chunk.waterVertexCount) visible.water.push_back(&chunk);
    }
    std::sort(visible.water.begin(), visible.water.end(), [&](const Chunk* a, const Chunk* b) {
        const glm::vec2 ac(a->x * CHUNK_SIZE + CHUNK_SIZE * 0.5f,
                           a->z * CHUNK_SIZE + CHUNK_SIZE * 0.5f);
        const glm::vec2 bc(b->x * CHUNK_SIZE + CHUNK_SIZE * 0.5f,
                           b->z * CHUNK_SIZE + CHUNK_SIZE * 0.5f);
        const glm::vec2 camera(cameraPosition.x, cameraPosition.z);
        return glm::dot(ac - camera, ac - camera) > glm::dot(bc - camera, bc - camera);
    });
    visible.water.erase(std::remove_if(visible.water.begin(), visible.water.end(),
        [&](const Chunk* chunk) { return !meshInFrustum(chunk->waterBounds, planes); }),
        visible.water.end());
    return visible;
}

void World::drawOpaque(const VisibleChunks& visible) const {
    renderedChunkCount_ = 0;
    opaqueSubmissionCount_ = 0;
    const auto clearDrawList = [](auto& list) {
        list.clear();
        if (list.capacity() > 4096) {
            using List = std::decay_t<decltype(list)>;
            List().swap(list);
        }
    };
    clearDrawList(drawPages_);
    for (const auto& page : terrainPages_) {
        clearDrawList(page->drawFirst);
        clearDrawList(page->drawCount);
    }
    for (const Chunk* chunk : visible.opaque) {
        auto* page = chunk->opaquePage.get();
        assert(page && chunk->opaqueFirstVertex + chunk->opaqueVertexCount <= page->capacity);
        if (page->drawFirst.empty()) drawPages_.push_back(page);
        page->drawFirst.push_back(static_cast<GLint>(chunk->opaqueFirstVertex));
        page->drawCount.push_back(chunk->opaqueVertexCount);
        ++renderedChunkCount_;
    }
    for (const auto* page : drawPages_) {
        glBindVertexArray(page->vao);
        glMultiDrawArrays(GL_TRIANGLES, page->drawFirst.data(), page->drawCount.data(),
                          static_cast<GLsizei>(page->drawCount.size()));
        ++opaqueSubmissionCount_;
    }
    // The paired transparent pass restores VAO 0 once after both terrain passes.
}

void World::drawWater(const VisibleChunks& visible) const {
    for (const Chunk* chunk : visible.water) {
        glBindVertexArray(chunk->waterVao);
        glDrawArrays(GL_TRIANGLES, 0, chunk->waterVertexCount);
    }
    glBindVertexArray(0);
}

void World::drawOpaque(const glm::mat4& viewProjection) const {
    drawOpaque(collectVisibleChunks(viewProjection, glm::vec3(0)));
    glBindVertexArray(0);
}

void World::drawWater(const glm::mat4& viewProjection, const glm::vec3& cameraPosition) const {
    drawWater(collectVisibleChunks(viewProjection, cameraPosition));
}
bool World::raycast(const glm::vec3& origin,
                    const glm::vec3& direction,
                    float maxDistance,
                    RayHit& hit) const {
    const glm::vec3 ray = glm::normalize(direction);
    glm::ivec3 cell = glm::ivec3(glm::floor(origin));
    const glm::ivec3 step(ray.x >= 0.0f ? 1 : -1, ray.y >= 0.0f ? 1 : -1, ray.z >= 0.0f ? 1 : -1);
    const float infinity = std::numeric_limits<float>::infinity();
    const glm::vec3 delta(ray.x == 0.0f ? infinity : std::abs(1.0f / ray.x),
                          ray.y == 0.0f ? infinity : std::abs(1.0f / ray.y),
                          ray.z == 0.0f ? infinity : std::abs(1.0f / ray.z));
    glm::vec3 next(
        ray.x >= 0.0f ? (cell.x + 1.0f - origin.x) * delta.x : (origin.x - cell.x) * delta.x,
        ray.y >= 0.0f ? (cell.y + 1.0f - origin.y) * delta.y : (origin.y - cell.y) * delta.y,
        ray.z >= 0.0f ? (cell.z + 1.0f - origin.z) * delta.z : (origin.z - cell.z) * delta.z);
    float travelled = 0.0f;
    glm::ivec3 entryNormal(0);
    while (travelled <= maxDistance) {
        const Block block = getBlock(cell.x, cell.y, cell.z);
        if (isRenderable(block) && !isWater(block)) {
            if (isDoor(block) || isTorch(block) || block == Block::LightningRod) {
                const BlockGeometryProperties bounds = blockGeometry(block);
                const glm::vec3 lower = glm::vec3(cell) +
                    glm::vec3(bounds.minX, bounds.minY, bounds.minZ);
                const glm::vec3 upper = glm::vec3(cell) +
                    glm::vec3(bounds.maxX, bounds.maxY, bounds.maxZ);
                float enter = 0.0f;
                float leave = maxDistance;
                glm::ivec3 face = entryNormal;
                bool intersects = true;
                for (int axis = 0; axis < 3; ++axis) {
                    if (std::abs(ray[axis]) < 0.000001f) {
                        if (origin[axis] < lower[axis] || origin[axis] > upper[axis])
                            intersects = false;
                        continue;
                    }
                    const float nearTime = (lower[axis] - origin[axis]) / ray[axis];
                    const float farTime = (upper[axis] - origin[axis]) / ray[axis];
                    const float axisEnter = std::min(nearTime, farTime);
                    const float axisLeave = std::max(nearTime, farTime);
                    if (axisEnter > enter) {
                        enter = axisEnter;
                        face = glm::ivec3(0);
                        face[axis] = ray[axis] > 0.0f ? -1 : 1;
                    }
                    leave = std::min(leave, axisLeave);
                }
                if (intersects && enter <= leave &&
                    enter <= std::min({next.x, next.y, next.z, maxDistance})) {
                    hit = {cell, cell + face, face, enter};
                    return true;
                }
            } else {
                hit = {cell, cell + entryNormal, entryNormal, travelled};
                return true;
            }
        }
        if (next.x < next.y && next.x < next.z) {
            cell.x += step.x;
            travelled = next.x;
            next.x += delta.x;
            entryNormal = {-step.x, 0, 0};
        } else if (next.y < next.z) {
            cell.y += step.y;
            travelled = next.y;
            next.y += delta.y;
            entryNormal = {0, -step.y, 0};
        } else {
            cell.z += step.z;
            travelled = next.z;
            next.z += delta.z;
            entryNormal = {0, 0, -step.z};
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
            for (int x = first.x; x <= last.x; ++x) {
                const glm::ivec3 blockPosition(x, y, z);
                if (isSolidAt(x, y, z) &&
                    blockIntersectsAabb(blockPosition, minimum, maximum))
                    return true;
            }
    return false;
}

bool World::blockIntersectsAabb(const glm::ivec3& block,
                                const glm::vec3& minimum,
                                const glm::vec3& maximum) const {
    glm::vec3 blockMin;
    glm::vec3 blockMax;
    if (!blockCollisionBounds(block, blockMin, blockMax))
        return false;
    return blockMin.x < maximum.x && blockMax.x > minimum.x && blockMin.y < maximum.y &&
           blockMax.y > minimum.y && blockMin.z < maximum.z && blockMax.z > minimum.z;
}

bool World::blockCollisionBounds(const glm::ivec3& block,
                                 glm::vec3& minimum,
                                 glm::vec3& maximum) const {
    const Block type = getBlock(block.x, block.y, block.z);
    if (!isSolid(type))
        return false;

    minimum = glm::vec3(block);
    maximum = minimum + glm::vec3(1.0f);
    minimum.y += blockCollisionMinY(type);
    maximum.y = static_cast<float>(block.y) + blockCollisionMaxY(type);

    if (type == Block::Cactus) {
        minimum.x += 0.0625f;
        minimum.z += 0.0625f;
        maximum.x -= 0.0625f;
        maximum.z -= 0.0625f;
    }

    if (isDoor(type)) {
        const BlockGeometryProperties geometry = blockGeometry(type);
        minimum.x += geometry.minX;
        minimum.z += geometry.minZ;
        maximum.x = static_cast<float>(block.x) + geometry.maxX;
        maximum.z = static_cast<float>(block.z) + geometry.maxZ;
    }
    return true;
}

bool World::aabbTouchesBlock(const glm::vec3& minimum,
                             const glm::vec3& maximum,
                             Block wanted,
                             float tolerance) const {
    const glm::ivec3 first = glm::ivec3(glm::floor(minimum - glm::vec3(tolerance)));
    const glm::ivec3 last = glm::ivec3(glm::floor(maximum + glm::vec3(tolerance)));
    for (int y = first.y; y <= last.y; ++y) {
        for (int z = first.z; z <= last.z; ++z) {
            for (int x = first.x; x <= last.x; ++x) {
                if (getBlock(x, y, z) != wanted)
                    continue;
                glm::vec3 blockMin;
                glm::vec3 blockMax;
                if (!blockCollisionBounds({x, y, z}, blockMin, blockMax))
                    continue;
                if (blockMin.x <= maximum.x + tolerance &&
                    blockMax.x >= minimum.x - tolerance &&
                    blockMin.y <= maximum.y + tolerance &&
                    blockMax.y >= minimum.y - tolerance &&
                    blockMin.z <= maximum.z + tolerance &&
                    blockMax.z >= minimum.z - tolerance) {
                    return true;
                }
            }
        }
    }
    return false;
}
