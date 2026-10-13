#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

struct VoxelVertex;
// Exact world positions and UVs are retained, including arbitrary custom shapes.
// The finite normal/light/AO domains share one integer attribute. No half floats.
struct PackedTerrainVertex {
    glm::vec3 position;
    glm::vec2 uv;
    std::uint32_t material;
    std::uint32_t shade;
};
static_assert(sizeof(PackedTerrainVertex)==28,"Packed terrain vertex layout");
namespace TerrainPacking {
const std::array<std::uint32_t,64>& sunBits();
PackedTerrainVertex pack(const VoxelVertex& vertex);
VoxelVertex unpack(const PackedTerrainVertex& vertex);
bool selfTest();
void indexQuads(const std::vector<VoxelVertex>& source,std::vector<PackedTerrainVertex>& vertices,
                std::vector<std::uint32_t>& indices);
}

bool validateTerrainPackingGpu(const char* vertexShader,const char* fragmentShader,unsigned atlasTexture);
