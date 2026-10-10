#pragma once

#include "Block.h"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

// Finite, seed-derived carvers. Plans depend only on region coordinates, so
// neighboring chunks and generation workers never need to share mutable state.
class CanyonCarver {
public:
    explicit CanyonCarver(std::uint32_t seed) : seed_(seed) {}
    bool hasCanyonInRegion(int regionX, int regionZ, bool improved = true) const;
    bool hasSurfaceCanyonInRegion(int regionX, int regionZ,
                                  bool improved = true) const;
    bool surfaceCaveStartInRegion(int regionX, int regionZ,
                                  int& worldX, int& worldZ,
                                  bool moreEntrances = false) const;

    bool surfaceSurvives(int x, int z, int surface, Block block,
                         const std::function<int(int,int)>& terrainHeight) const;
    void carveChunk(int chunkX, int chunkZ, int chunkSize, int worldHeight,
                    const std::array<int, 256>& surfaceHeights,
                    const std::array<int, 256>& aquiferLevels,
                    const std::array<bool, 256>& wetAquifers,
                    std::vector<Block>& blocks,
                    const std::function<int(int, int)>& terrainHeight,
                    bool improved, bool moreEntrances = false) const;

private:
    enum class Kind { Canyon, Cave };
    struct Path {
        Kind kind = Kind::Canyon;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yaw = 0.0f;
        float pitch = 0.0f;
        int steps = 0;
        float width = 0.0f;
        float height = 0.0f;
        bool surfaceEntrance = false;
        std::uint32_t variation = 0;
    };

    std::uint32_t seed_;
    bool regionalPath(int regionX, int regionZ, Kind kind, bool improved,
                      bool moreEntrances,
                      Path& path) const;
    void carvePath(const Path& path, int chunkX, int chunkZ, int chunkSize,
                   int worldHeight, const std::array<int, 256>& surfaceHeights,
                   const std::array<int, 256>& aquiferLevels,
                   const std::array<bool, 256>& wetAquifers,
                   std::vector<Block>& blocks, bool improved) const;
};
