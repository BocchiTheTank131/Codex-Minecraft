#include "TerrainDensity.h"
#include <algorithm>
#include <cmath>

namespace TerrainDensity {
namespace {
float smooth(float a, float b, float value) {
    const float t = std::clamp((value-a)/(b-a), 0.f, 1.f);
    return t*t*(3.f-2.f*t);
}
Fields mix(const Fields& a, const Fields& b, float t) {
    return {a.surface+(b.surface-a.surface)*t, a.chamber+(b.chamber-a.chamber)*t,
        a.tunnelA+(b.tunnelA-a.tunnelA)*t, a.tunnelB+(b.tunnelB-a.tunnelB)*t,
        a.pillar+(b.pillar-a.pillar)*t, a.vein+(b.vein-a.vein)*t};
}
Fields sampleNode(float wx,float wy,float wz,const PerlinNoise& surface,const PerlinNoise& chamber,
    const PerlinNoise& tunnels,const PerlinNoise& pillars,const PerlinNoise& veins) {
    return {
        surface.fractal3D(wx*.019f+31.7f, wy*.028f-11.3f, wz*.019f-53.1f, 3, 2.f, .45f),
        chamber.fractal3D(wx*.009f+47.3f, wy*.014f+13.9f, wz*.009f-97.1f, 2, 2.f, .38f),
        tunnels.noise(wx*.013f+73.9f, wy*.020f-21.1f, wz*.013f+31.7f),
        tunnels.noise(wx*.013f-137.7f, wy*.020f+71.3f, wz*.013f-53.9f),
        pillars.noise(wx*.040f+17.7f, wy*.006f-3.3f, wz*.040f+91.1f),
        veins.noise(wx*.045f-37.1f, wy*.061f+11.9f, wz*.045f+71.3f)
    };
}
}
Fields sampleAt(int x,int y,int z,const PerlinNoise& surface,const PerlinNoise& chamber,
    const PerlinNoise& tunnels,const PerlinNoise& pillars,const PerlinNoise& veins) {
    const int bx=x-(x%4+4)%4,bz=z-(z%4+4)%4,by=MinimumY+(y-MinimumY)/8*8;
    const float fx=float(x-bx)*.25f,fz=float(z-bz)*.25f,fy=float(y-by)*.125f;
    const auto n=[&](int dx,int dy,int dz){return sampleNode(float(bx+dx*4),float(by+dy*8),float(bz+dz*4),surface,chamber,tunnels,pillars,veins);};
    return mix(mix(mix(n(0,0,0),n(1,0,0),fx),mix(n(0,0,1),n(1,0,1),fx),fz),
               mix(mix(n(0,1,0),n(1,1,0),fx),mix(n(0,1,1),n(1,1,1),fx),fz),fy);
}
Fields sampleAtCached(int x,int y,int z,std::uint32_t seed,unsigned version,
    const PerlinNoise& surface,const PerlinNoise& chamber,const PerlinNoise& tunnels,
    const PerlinNoise& pillars,const PerlinNoise& veins) {
    struct Entry {int x=0,y=0,z=0;bool valid=false;Fields fields{};};
    struct Cache {std::uint32_t seed=0;unsigned version=0;bool initialized=false;std::array<Entry,4096> nodes;};
    thread_local Cache cache;
    if(!cache.initialized||cache.seed!=seed||cache.version!=version) {
        for(auto& entry:cache.nodes)entry.valid=false;
        cache.seed=seed;cache.version=version;cache.initialized=true;
    }
    const int bx=x-(x%4+4)%4,bz=z-(z%4+4)%4,by=MinimumY+(y-MinimumY)/8*8;
    const float fx=float(x-bx)*.25f,fz=float(z-bz)*.25f,fy=float(y-by)*.125f;
    const auto n=[&](int dx,int dy,int dz) {
        const int nx=bx+dx*4,ny=by+dy*8,nz=bz+dz*4;
        auto& entry=cache.nodes[hash(nx,ny,nz,seed)&4095U];
        if(!entry.valid||entry.x!=nx||entry.y!=ny||entry.z!=nz) {
            entry={nx,ny,nz,true,sampleNode(float(nx),float(ny),float(nz),surface,chamber,tunnels,pillars,veins)};
        }
        return entry.fields;
    };
    return mix(mix(mix(n(0,0,0),n(1,0,0),fx),mix(n(0,0,1),n(1,0,1),fx),fz),
               mix(mix(n(0,1,0),n(1,1,0),fx),mix(n(0,1,1),n(1,1,1),fx),fz),fy);
}
Grid::Grid(int cx, int cz, const PerlinNoise& surface, const PerlinNoise& chamber,
           const PerlinNoise& tunnels, const PerlinNoise& pillars, const PerlinNoise& veins, int maximumY)
    : originX_(cx*16-4), originZ_(cz*16-4) {
    for (int y=0; y<std::min(Vertical,(maximumY-MinimumY+7)/8+1); ++y) for (int z=0; z<Horizontal; ++z) for (int x=0; x<Horizontal; ++x) {
        const float wx=float(originX_+x*4), wy=float(MinimumY+y*8), wz=float(originZ_+z*4);
        nodes_[(y*Horizontal+z)*Horizontal+x] = sampleNode(wx,wy,wz,surface,chamber,tunnels,pillars,veins);
    }
}
Fields Grid::at(int wx, int wy, int wz) const {
    const int x=(wx-originX_)/4, z=(wz-originZ_)/4, y=(wy-MinimumY)/8;
    const float fx=float((wx-originX_)%4)*.25f, fz=float((wz-originZ_)%4)*.25f, fy=float((wy-MinimumY)%8)*.125f;
    const auto node=[&](int dx,int dy,int dz)->const Fields& { return nodes_[((y+dy)*Horizontal+z+dz)*Horizontal+x+dx]; };
    return mix(mix(mix(node(0,0,0),node(1,0,0),fx),mix(node(0,0,1),node(1,0,1),fx),fz),
               mix(mix(node(0,1,0),node(1,1,0),fx),mix(node(0,1,1),node(1,1,1),fx),fz),fy);
}
float density(const Fields& f, const Column& c, int y) {
    const float depth=float(c.height-y)+.5f;
    const float envelope=(1.f-smooth(35.f,85.f,std::abs(depth)))*smooth(0.f,12.f,std::abs(depth));
    // Fade the deformation before the ceiling, avoiding clipped floating peaks.
    const float ceiling=1.f-smooth(float(MaximumY-32),float(MaximumY-4),float(y));
    float solid=depth/22.f + f.surface*c.mountain*2.3f*envelope*ceiling;
    // Pillars suppress chamber carving locally; broad tunnels join chambers.
    const float cover=smooth(3.f,17.f,depth);
    const float chamber=(f.chamber-.29f)*5.f;
    const float tunnel=(.0075f-f.tunnelA*f.tunnelA-f.tunnelB*f.tunnelB)*42.f;
    const float pillar=smooth(.19f,.36f,f.pillar);
    const float noodle=(.003f-f.surface*f.surface-f.vein*f.vein)*30.f*smooth(.23f,.38f,c.aquifer);
    const float cave=(std::max({chamber,tunnel,noodle})-pillar*.45f)*cover;
    if (cover>.01f && cave>0) solid=std::min(solid,-cave);
    // A bounded regional contour cuts from open sky into the underground.
    const float canyonBottom=float(c.height)-c.ravine*62.f;
    if (c.ravine>.02f && y>canyonBottom) solid=std::min(solid,(canyonBottom-y)/12.f);
    return solid;
}
const std::array<OreDistribution,7>& oreDistributions() {
    static const std::array<OreDistribution,7> ores{{
        {Block::CoalOre,Block::DeepslateCoalOre,OreCurve::Triangle,0,96,256,.034f},
        {Block::IronOre,Block::DeepslateIronOre,OreCurve::Triangle,-64,16,72,.026f},
        {Block::IronOre,Block::DeepslateIronOre,OreCurve::Triangle,80,232,320,.041f},
        {Block::CopperOre,Block::DeepslateCopperOre,OreCurve::Triangle,-16,48,112,.027f},
        {Block::GoldOre,Block::DeepslateGoldOre,OreCurve::Triangle,-64,-16,32,.012f},
        {Block::GoldOre,Block::DeepslateGoldOre,OreCurve::Uniform,-64,-64,-48,.003f},
        {Block::DiamondOre,Block::DeepslateDiamondOre,OreCurve::Depth,-64,-64,16,.0065f}
    }};
    return ores;
}
float oreWeight(const OreDistribution& o,float y) {
    if(y<o.minimum || y>o.maximum) return 0;
    if(o.curve==OreCurve::Uniform) return 1;
    if(o.curve==OreCurve::Depth) return (o.maximum-y)/(o.maximum-o.minimum);
    return y<o.peak ? (y-o.minimum)/(o.peak-o.minimum) : (o.maximum-y)/(o.maximum-o.peak);
}
std::uint32_t hash(int x,int y,int z,std::uint32_t seed) {
    std::uint32_t h=seed ^ std::uint32_t(x)*0x9e3779b9U ^ std::uint32_t(y)*0x85ebca6bU ^ std::uint32_t(z)*0xc2b2ae35U;
    h^=h>>16; h*=0x7feb352dU; h^=h>>15; h*=0x846ca68bU; return h^(h>>16);
}
}
