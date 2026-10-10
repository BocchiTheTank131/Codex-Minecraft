#include "CanyonCarver.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float Pi = 3.14159265358979323846f;

std::uint32_t mix(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    return value ^ (value >> 16U);
}

std::uint32_t randomStep(std::uint32_t& state) {
    state = mix(state + 0x9e3779b9U);
    return state;
}

float unit(std::uint32_t& state) {
    return static_cast<float>(randomStep(state) & 0xffffU) / 65535.0f;
}

int floorDiv(int value, int divisor) {
    const int quotient = value / divisor;
    return value < 0 && value % divisor != 0 ? quotient - 1 : quotient;
}

float smoothstep(float start, float end, float value) {
    const float t = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

bool CanyonCarver::regionalPath(int regionX, int regionZ, Kind kind,
                                bool improved, bool moreEntrances,
                                Path& path) const {
    const int spacing = kind == Kind::Canyon ? 256 : improved ? 96 : 160;
    std::uint32_t state = mix(seed_ ^
        mix(static_cast<std::uint32_t>(regionX) * 0x8da6b343U) ^
        mix(static_cast<std::uint32_t>(regionZ) * 0xd8163841U) ^
        (kind == Kind::Canyon ? 0xcaf3110fU : 0x6173b823U));
    // Fewer than one in four canyon regions contain a carver. Cave paths are
    // more common, but remain finite and mostly beneath the surface.
    if (unit(state) >= (kind == Kind::Canyon ? 0.21f : improved ? 0.72f : 0.56f))
        return false;
    path.kind = kind;
    path.x = static_cast<float>(regionX * spacing + 24) +
             unit(state) * static_cast<float>(spacing - 48);
    path.z = static_cast<float>(regionZ * spacing + 24) +
             unit(state) * static_cast<float>(spacing - 48);
    path.y = 22.0f + unit(state) * 39.0f;
    path.yaw = unit(state) * 2.0f * Pi;
    path.pitch = (unit(state) - 0.5f) * 0.13f;
    path.steps = kind == Kind::Canyon ? 22 + static_cast<int>(unit(state) * 25.0f)
                                      : 20 + static_cast<int>(unit(state) * 22.0f);
    path.width = kind == Kind::Canyon ? 2.0f + unit(state) * 4.2f
                                      : 1.4f + unit(state) * 1.7f;
    path.height = kind == Kind::Canyon ? 11.0f + unit(state) * 20.0f
                                       : 2.0f + unit(state) * 3.0f;
    path.surfaceEntrance = unit(state) < (kind == Kind::Canyon
        ? (improved ? 0.20f : 0.14f)
        : (improved ? (moreEntrances ? 0.30f : 0.25f) : 0.09f));
    path.variation = state;
    return true;
}

bool CanyonCarver::hasCanyonInRegion(int regionX, int regionZ,
                                     bool improved) const {
    Path path;
    return regionalPath(regionX, regionZ, Kind::Canyon, improved, false, path);
}

bool CanyonCarver::hasSurfaceCanyonInRegion(int regionX, int regionZ,
                                            bool improved) const {
    Path path;
    return regionalPath(regionX, regionZ, Kind::Canyon, improved, false, path) &&
           path.surfaceEntrance;
}

bool CanyonCarver::surfaceCaveStartInRegion(int regionX, int regionZ,
                                            int& worldX, int& worldZ,
                                            bool moreEntrances) const {
    Path path;
    if (!regionalPath(regionX, regionZ, Kind::Cave, true, moreEntrances, path) ||
        !path.surfaceEntrance)
        return false;
    worldX = static_cast<int>(std::floor(path.x));
    worldZ = static_cast<int>(std::floor(path.z));
    return true;
}

void CanyonCarver::carvePath(
    const Path& path, int chunkX, int chunkZ, int chunkSize, int worldHeight,
    const std::array<int, 256>& surfaceHeights,
    const std::array<int, 256>& aquiferLevels,
    const std::array<bool, 256>& wetAquifers,
    std::vector<Block>& blocks, bool improved) const {
    std::uint32_t state = path.variation;
    float x = path.x;
    float y = path.y;
    float z = path.z;
    float yaw = path.yaw;
    float pitch = path.pitch;
    const int originX = chunkX * chunkSize;
    const int originZ = chunkZ * chunkSize;
    for (int step = 0; step < path.steps; ++step) {
        const float progress = static_cast<float>(step) /
                               static_cast<float>(path.steps - 1);
        float taper = std::max(0.12f, std::sin(Pi * progress));
        if (path.kind == Kind::Cave && step < 3)
            taper = std::max(taper, 1.45f - static_cast<float>(step) * 0.18f);
        const float width = path.width * taper * (0.85f + 0.3f * unit(state));
        const float height = path.height * taper * (0.88f + 0.24f * unit(state));
        const int minX = std::max(originX, static_cast<int>(std::floor(x - width - 1)));
        const int maxX = std::min(originX + chunkSize - 1,
                                  static_cast<int>(std::ceil(x + width + 1)));
        const int minZ = std::max(originZ, static_cast<int>(std::floor(z - width - 1)));
        const int maxZ = std::min(originZ + chunkSize - 1,
                                  static_cast<int>(std::ceil(z + width + 1)));
        if (minX <= maxX && minZ <= maxZ) {
            for (int worldZ = minZ; worldZ <= maxZ; ++worldZ) {
                for (int worldX = minX; worldX <= maxX; ++worldX) {
                    const float dx = (static_cast<float>(worldX) + 0.5f - x) / width;
                    const float dz = (static_cast<float>(worldZ) + 0.5f - z) / width;
                    const float horizontal = dx * dx + dz * dz;
                    if (horizontal >= 1.0f) continue;
                    const int localX = worldX - originX;
                    const int localZ = worldZ - originZ;
                    const std::size_t column = static_cast<std::size_t>(
                        localZ * chunkSize + localX);
                    const int surface = surfaceHeights[column];
                    const bool entranceOpening = path.surfaceEntrance &&
                        (path.kind == Kind::Canyon
                             ? progress > 0.16f && progress < 0.84f
                             : progress < 0.23f);
                    const int protection = entranceOpening ? 0 : 7;
                    const int minimumY = std::max(6, static_cast<int>(std::floor(y - height)));
                    const int maximumY = std::min({worldHeight - 2,
                        improved ? surface : surface - protection,
                        static_cast<int>(std::ceil(y + height))});
                    for (int blockY = minimumY; blockY <= maximumY; ++blockY) {
                        const float dy = (static_cast<float>(blockY) + 0.5f - y) / height;
                        const float ledge = path.kind == Kind::Canyon && blockY < y &&
                            ((blockY / 7 + static_cast<int>(path.variation & 3U)) & 3) == 0
                                ? 0.82f : 1.0f;
                        float allowance = 1.0f;
                        if (improved) {
                            const float depth = static_cast<float>(surface - blockY);
                            const float entranceInfluence = path.surfaceEntrance
                                ? (path.kind == Kind::Canyon
                                    ? smoothstep(0.08f, 0.24f, progress) *
                                      (1.0f - smoothstep(0.76f, 0.92f, progress))
                                    : 1.0f - smoothstep(0.12f, 0.38f, progress))
                                : 0.0f;
                            const float protectionFactor = smoothstep(0.0f, 11.0f, depth);
                            allowance = std::max(protectionFactor, entranceInfluence);
                        }
                        if (horizontal / ledge + dy * dy >= allowance) continue;
                        const std::size_t index = static_cast<std::size_t>(
                            (blockY * chunkSize + localZ) * chunkSize + localX);
                        if (blocks[index] == Block::Stone || blocks[index] == Block::Dirt ||
                            blocks[index] == Block::Granite || blocks[index] == Block::Diorite ||
                            blocks[index] == Block::Andesite ||
                            ((entranceOpening || (improved && path.surfaceEntrance)) &&
                             blocks[index] != Block::Water)) {
                            blocks[index] = wetAquifers[column] &&
                                    blockY <= aquiferLevels[column]
                                ? Block::Water : Block::Air;
                        }
                    }
                }
            }
        }
        yaw += (unit(state) - 0.5f) * 0.12f;
        pitch = std::clamp(pitch + (unit(state) - 0.5f) * 0.025f,
                           -0.19f, 0.19f);
        x += std::cos(yaw) * 2.8f;
        z += std::sin(yaw) * 2.8f;
        y += pitch * 1.2f;
        if (path.kind == Kind::Cave && path.surfaceEntrance)
            y -= 0.45f;
    }
}

void CanyonCarver::carveChunk(
    int chunkX, int chunkZ, int chunkSize, int worldHeight,
    const std::array<int, 256>& surfaceHeights,
    const std::array<int, 256>& aquiferLevels,
    const std::array<bool, 256>& wetAquifers,
    std::vector<Block>& blocks,
    const std::function<int(int, int)>& terrainHeight, bool improved,
    bool moreEntrances) const {
    for (const Kind kind : {Kind::Canyon, Kind::Cave}) {
        const int spacing = kind == Kind::Canyon ? 256 : improved ? 96 : 160;
        const int centerRegionX = floorDiv(chunkX * chunkSize, spacing);
        const int centerRegionZ = floorDiv(chunkZ * chunkSize, spacing);
        for (int regionZ = centerRegionZ - 1; regionZ <= centerRegionZ + 1; ++regionZ) {
            for (int regionX = centerRegionX - 1; regionX <= centerRegionX + 1; ++regionX) {
                Path path;
                if (regionalPath(regionX, regionZ, kind, improved,
                                 moreEntrances, path)) {
                    if (path.surfaceEntrance) {
                        const int startX = static_cast<int>(std::floor(path.x));
                        const int startZ = static_cast<int>(std::floor(path.z));
                        const int surface = terrainHeight(startX, startZ);
                        path.y = static_cast<float>(surface) -
                            (kind == Kind::Canyon ? 10.0f : improved ? 2.0f : 3.0f);
                        if (improved && kind == Kind::Cave) {
                            const int east = terrainHeight(startX + 6, startZ);
                            const int west = terrainHeight(startX - 6, startZ);
                            const int south = terrainHeight(startX, startZ + 6);
                            const int north = terrainHeight(startX, startZ - 6);
                            const float riseX = static_cast<float>(east - west);
                            const float riseZ = static_cast<float>(south - north);
                            const float slope = std::sqrt(riseX * riseX + riseZ * riseZ);
                            if (slope >= 3.0f) {
                                // A hillside mouth leads into the rising ground.
                                path.yaw = std::atan2(riseZ, riseX);
                                path.width *= slope >= 8.0f ? 1.2f : 1.08f;
                            }
                        }
                    }
                    carvePath(path, chunkX, chunkZ, chunkSize, worldHeight,
                              surfaceHeights, aquiferLevels, wetAquifers, blocks,
                              improved);
                    if (kind == Kind::Cave) {
                        std::uint32_t branchState = path.variation;
                        const int branchCount = 1 + static_cast<int>(
                            randomStep(branchState) % 3U);
                        for (int branch = 0; branch < branchCount; ++branch) {
                            Path branchPath = path;
                            branchPath.surfaceEntrance = false;
                            branchPath.y -= path.surfaceEntrance ? 7.0f : 0.0f;
                            branchPath.yaw += (static_cast<float>(branch + 1) /
                                               static_cast<float>(branchCount + 1)) *
                                              2.0f * Pi;
                            branchPath.steps = 12 + static_cast<int>(
                                randomStep(branchState) % 16U);
                            branchPath.width *= 0.65f + 0.25f * unit(branchState);
                            branchPath.variation = randomStep(branchState);
                            carvePath(branchPath, chunkX, chunkZ, chunkSize,
                                      worldHeight, surfaceHeights, aquiferLevels,
                                      wetAquifers, blocks, improved);
                        }
                    }
                }
            }
        }
    }
}

// Use the exact existing finite carvers on a one-column probe. This is only
// used for sparse v9 vegetation roots, never for every terrain block.
bool CanyonCarver::surfaceSurvives(int x,int z,int surface,Block block,
                                  const std::function<int(int,int)>& terrainHeight) const {
    thread_local std::vector<Block> column;
    column.assign(256,Block::Air); column[static_cast<std::size_t>(surface)]=block;
    std::array<int,256> heights{},aquifers{}; std::array<bool,256> wet{};
    heights[0]=surface;
    carveChunk(x,z,1,256,heights,aquifers,wet,column,terrainHeight,true,true);
    return column[static_cast<std::size_t>(surface)]!=Block::Air;
}
