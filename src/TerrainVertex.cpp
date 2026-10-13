#include "TerrainVertex.h"
#include "World.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
std::uint32_t bits(float value) {std::uint32_t result;std::memcpy(&result,&value,4);return result;}
float value(std::uint32_t raw) {float result;std::memcpy(&result,&raw,4);return result;}
const std::array<glm::vec3,10>& normals() {
    static const auto result=[] {
        const float d=glm::normalize(glm::vec3(1,0,1)).x;
        return std::array<glm::vec3,10>{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
            {-d,0,d},{d,0,-d},{d,0,d},{-d,0,-d}}};
    }();return result;
}
}
namespace TerrainPacking {
void indexQuads(const std::vector<VoxelVertex>& source,std::vector<PackedTerrainVertex>& vertices,
                std::vector<std::uint32_t>& indices) {
    vertices.clear();indices.clear();vertices.reserve(source.size()*2/3);indices.reserve(source.size());
    const auto equal=[](const VoxelVertex& a,const VoxelVertex& b){return std::memcmp(&a,&b,sizeof(VoxelVertex))==0;};
    for(std::size_t i=0;i<source.size();) {
        const auto base=std::uint32_t(vertices.size());
        if(i+6<=source.size() && equal(source[i],source[i+3]) && equal(source[i+2],source[i+4])) {
            for(int corner:{0,1,2,5})vertices.push_back(pack(source[i+corner]));
            for(unsigned corner:{0U,1U,2U,0U,2U,3U})indices.push_back(base+corner);i+=6;
        } else if(i+6<=source.size() && equal(source[i+1],source[i+3]) && equal(source[i+2],source[i+5])) {
            for(int corner:{0,1,2,4})vertices.push_back(pack(source[i+corner]));
            for(unsigned corner:{0U,1U,2U,1U,3U,2U})indices.push_back(base+corner);i+=6;
        } else {vertices.push_back(pack(source[i++]));indices.push_back(base);}
    }
}
const std::array<std::uint32_t,64>& sunBits() {
    static const auto table=[] {
        std::array<std::uint32_t,64> result{};
        constexpr float weights[]={1.f,.90f,.70f,.48f};
        for(int group=0;group<4;++group)for(int level=0;level<16;++level)
            result[group*16+level]=bits((float(level)/15.f)*weights[group]);
        return result;
    }();return table;
}
PackedTerrainVertex pack(const VoxelVertex& vertex) {
    unsigned normal=10;
    for(unsigned i=0;i<10;++i) if(vertex.normal==normals()[i]) {normal=i;break;}
    if(normal==10) throw std::logic_error("Unsupported terrain normal: packing would lose precision");
    constexpr float weights[]={1.f,.90f,.70f,.48f};
    unsigned sun=64;
    for(unsigned group=0;group<4;++group) {
        const int level=int(std::round(vertex.sunLight*15.f/weights[group]));
        if(level>=0 && level<16 && sunBits()[group*16+level]==bits(vertex.sunLight)) {sun=group*16+level;break;}
    }
    const int light=int(std::round(vertex.blockLight*15.f)),ao=int(std::round(vertex.ao*64.f));
    if(sun==64 || light<0 || light>15 || sunBits()[light]!=bits(vertex.blockLight) ||
       ao<0 || ao>64 || float(ao)/64.f!=vertex.ao)
        throw std::logic_error("Unsupported terrain light/AO: packing would lose precision");
    std::uint32_t shade=normal|(sun<<4U)|(unsigned(light)<<10U)|(unsigned(ao)<<14U);
    for(unsigned axis=0;axis<3;++axis)
        if(vertex.normal[axis]==0 && std::signbit(vertex.normal[axis])) shade|=1U<<(23U+axis);
    return {vertex.position,vertex.uv,vertex.atlasTile.packed,shade};
}
VoxelVertex unpack(const PackedTerrainVertex& vertex) {
    VoxelVertex result;
    result.position=vertex.position;result.uv=vertex.uv;
    result.atlasTile.packed=vertex.material;
    result.normal=normals()[vertex.shade&15U];
    for(unsigned axis=0;axis<3;++axis) if(vertex.shade&(1U<<(23U+axis))) result.normal[axis]=value(0x80000000U);
    result.sunLight=value(sunBits()[(vertex.shade>>4U)&63U]);
    result.blockLight=value(sunBits()[(vertex.shade>>10U)&15U]);
    result.ao=float((vertex.shade>>14U)&127U)/64.f;
    return result;
}
bool selfTest() {
    for(const auto& normal:normals())for(unsigned sun=0;sun<64;++sun)for(int light=0;light<16;++light)for(int ao=49;ao<=64;++ao) {
        VoxelVertex vertex{{-1048576.035f,-63.8125f,1048576.08f},{.0005f,383.f},normal,
            value(sunBits()[sun]),value(sunBits()[light]),float(ao)/64.f};
        vertex.atlasTile.packed=0x123456ffU;
        const auto restored=unpack(pack(vertex));
        if(std::memcmp(&vertex,&restored,sizeof(vertex))) return false;
    }
    return true;
}
}
