#include "TerrainExploration.h"
#include "TerrainDensity.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
namespace TerrainExploration {
namespace {
int floorDiv(int v,int d) { const int q=v/d;return q-(v%d<0); }
float smooth(float a,float b,float v) { const float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t); }
}
float heightVariation(float c,float e,float river,float rolling,float ridge) {
    return smooth(.04f,.16f,c)*(1-smooth(.05f,.5f,river))*
        (rolling*(8.f+14.f*(1-e))+(1.f-std::abs(ridge))*6.f-3.f);
}
Biome variant(Biome base,float temperature,float h,float habitat,int elevation) {
    // Stay within the existing climate family. No tiny temperature islands.
    switch(base) {
    case Biome::Forest: case Biome::BirchForest: case Biome::FlowerForest:
        if(habitat<-.18f) return Biome::Plains;
        if(habitat>.20f && h>.55f) return Biome::FlowerForest;
        if(habitat>.05f) return Biome::BirchForest;
        return Biome::Forest;
    case Biome::Plains:
        return h>.28f && habitat>.10f ? Biome::Forest :
            h>.32f && habitat<-.18f && elevation>64 ? Biome::Meadow : base;
    case Biome::Meadow:
        return habitat>.10f ? Biome::BirchForest : habitat<-.13f ? Biome::FlowerForest : base;
    case Biome::DarkForest:
        return habitat<-.12f ? Biome::BirchForest : habitat>.20f ? Biome::Forest : base;
    case Biome::Swamp:
        return elevation>64 && habitat>.10f ?
            (temperature>.60f ? Biome::SparseJungle : Biome::Forest) : base;
    case Biome::SnowyTaiga:
        return habitat<-.17f ? Biome::SnowyPlains : base;
    case Biome::SnowyPlains:
        return h>.30f && habitat>.10f ? Biome::SnowyTaiga : base;
    case Biome::WindsweptHills:
        if(h>.38f && habitat>.06f && elevation<108)
            return altitudeTemperature(temperature,float(elevation))<.34f ? Biome::Taiga : Biome::Forest;
        return base;
    case Biome::Mountains:
        if(elevation<128 && h>.38f && habitat<-.10f && altitudeTemperature(temperature,float(elevation))>.34f) return Biome::Meadow;
        return base;
    case Biome::Taiga: case Biome::OldGrowthTaiga:
        return habitat>.20f && h>.55f ? Biome::OldGrowthTaiga : Biome::Taiga;
    case Biome::Jungle: case Biome::SparseJungle:
        return habitat<-.08f ? Biome::SparseJungle : Biome::Jungle;
    case Biome::Savanna:
        return elevation>74 && habitat>.14f ? Biome::SavannaPlateau : Biome::Savanna;
    case Biome::Desert:
        return elevation>70 && habitat>.12f ? Biome::DesertHills : Biome::Desert;
    case Biome::Badlands: case Biome::WoodedBadlands:
        return h>.24f && habitat>.08f ? Biome::WoodedBadlands : Biome::Badlands;
    default: return base;
    }
}
bool mushroomCluster(int x,int z,std::uint32_t seed,Biome biome) {
    if(biome==Biome::MushroomFields) return TerrainDensity::hash(x,0,z,seed^0x5a81U)%5U==0;
    const int spacing=biome==Biome::DarkForest?18:28;
    const int cx=floorDiv(x,spacing),cz=floorDiv(z,spacing);
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
        const auto h=TerrainDensity::hash(cx+dx,0,cz+dz,seed^0x5a81U);
        if(h%(biome==Biome::DarkForest?2U:4U)) continue;
        const int ax=(cx+dx)*spacing+int((h>>6)%unsigned(spacing));
        const int az=(cz+dz)*spacing+int((h>>13)%unsigned(spacing));
        const int px=x-ax,pz=z-az;
        if(px*px+pz*pz<=5 && TerrainDensity::hash(x,0,z,seed^0x91abU)%3U==0) return true;
    }
    return false;
}

std::vector<CaveEntrance> caveEntrances(int chunkX,int chunkZ,std::uint32_t seed,
    const std::function<SurfaceColumn(int,int)>& terrain,
    const std::function<bool(int,int,int)>& cavity,
    const std::function<bool(int,int)>& reserved,unsigned generationVersion) {
    if(generationVersion>=22) return diverseCaveEntrances(chunkX,chunkZ,seed,terrain,cavity,reserved);
    struct Cached { bool valid=false; CaveEntrance cave{}; };
    // Each generation worker owns its cache. Bounds keep long flights from
    // retaining an ever-growing exploration map. Plans depend only on seed.
    thread_local std::uint32_t cachedSeed=0,cachedVersion=0;
    thread_local std::unordered_map<std::uint64_t,Cached> cache;
    if(cachedSeed!=seed || cachedVersion!=generationVersion || cache.size()>256) { cache.clear();cachedSeed=seed;cachedVersion=generationVersion; }
    std::vector<CaveEntrance> caves;
    constexpr int region=96,reach=56;
    const int ox=chunkX*16,oz=chunkZ*16;
    for(int rz=floorDiv(oz-reach,region);rz<=floorDiv(oz+15+reach,region);++rz)
    for(int rx=floorDiv(ox-reach,region);rx<=floorDiv(ox+15+reach,region);++rx) {
        const auto h=TerrainDensity::hash(rx,0,rz,seed^0xca7eU);
        if(h%3U) continue;
        const auto key=(std::uint64_t(std::uint32_t(rx))<<32)|std::uint32_t(rz);
        auto found=cache.find(key);
        if(found==cache.end()) {
            Cached plan;
            const int x=rx*region+16+int((h>>5)%64U),z=rz*region+16+int((h>>12)%64U);
            const auto ground=terrain(x,z);
            if(ground.height>72 && ground.height<275 && ground.river<.05f && ground.ravine<.02f && !reserved(x,z)) {
                // A few wide shafts occur in plains; most entrances favor hills.
                const int slope=std::max(std::abs(terrain(x+8,z).height-ground.height),std::abs(terrain(x,z+8).height-ground.height));
                if(slope>=2 || h%5U==0) {
                    constexpr int directions[4][2]={{1,0},{0,1},{-1,0},{0,-1}};
                    for(int attempt=0;attempt<4 && !plan.valid;++attempt) {
                        const auto& d=directions[((h>>19)+attempt)%4U];
                        const int tx=x+d[0]*40+d[1]*7,tz=z+d[1]*40-d[0]*7;
                        const int localHeight=terrain(tx,tz).height;
                        bool protectedRoute=false;
                        for(int step=0;step<=8;++step) {
                            const int px=x+(tx-x)*step/8,pz=z+(tz-z)*step/8;
                            protectedRoute|=terrain(px,pz).height<70 || reserved(px,pz);
                            if(generationVersion>=18) {
                                // Elevated rivers can cross an otherwise dry entrance
                                // bore. Reserve its full width, including the mouth,
                                // rather than opening an underwater connection.
                                for(int dz=-5;dz<=5;dz+=5) for(int dx=-5;dx<=5;dx+=5)
                                    protectedRoute|=terrain(px+dx,pz+dz).river>.01f;
                            }
                        }
                        if(protectedRoute) continue;
                        for(int depth=20;depth<=40;depth+=4) {
                            const int y=std::min(ground.height-depth,localHeight-18);
                            if(y<8 || !cavity(tx,y,tz) || !cavity(tx,y+2,tz) ||
                               !cavity(tx+2,y+1,tz) || !cavity(tx-2,y+1,tz)) continue;
                            plan.cave={{float(x)+.5f,float(ground.height)+1.5f,float(z)+.5f},
                                {float(x+tx)*.5f+.5f,(float(ground.height)+float(y))*.5f+1.f,float(z+tz)*.5f+.5f},
                                {float(tx)+.5f,float(y)+1.f,float(tz)+.5f},2.3f+float((h>>24)%3U)*.55f,h%5U==0};
                            plan.valid=true;break;
                        }
                    }
                }
            }
            found=cache.emplace(key,plan).first;
        }
        if(!found->second.valid) continue;
        const auto& cave=found->second.cave;
        const float minX=std::min(cave.mouth.x,cave.target.x)-8,maxX=std::max(cave.mouth.x,cave.target.x)+8;
        const float minZ=std::min(cave.mouth.z,cave.target.z)-8,maxZ=std::max(cave.mouth.z,cave.target.z)+8;
        if(maxX<ox-6||minX>ox+22||maxZ<oz-6||minZ>oz+22) continue;
        // Reservation is evaluated per voxel by the caller; keep plan selection
        // independent of which chunk is generated first.
        caves.push_back(cave);
    }
    return caves;
}
bool carves(const CaveEntrance& cave,int x,int y,int z) {
    const glm::vec3 p(float(x)+.5f,float(y)+.5f,float(z)+.5f);
    if(y>cave.mouth.y+6 || y<cave.target.y-5) return false;
    // V22's supported native landing must survive the enlarged terminal
    // chamber. Older plans retain their exact historical carving shape.
    if(cave.preserveFloor && y<=int(std::floor(cave.target.y-cave.radius-1.f)))return false;
    const auto tube=[&](glm::vec3 a,glm::vec3 b,float radius) {
        const auto delta=b-a;
        const auto nearest=a+delta*std::clamp(glm::dot(p-a,delta)/glm::dot(delta,delta),0.f,1.f);
        const auto offset=p-nearest;
        return glm::dot(offset,offset)<radius*radius;
    };
    if(tube(cave.mouth,cave.bend,cave.radius)||tube(cave.bend,cave.target,cave.radius)) return true;
    const auto d=p-cave.target;
    if(d.x*d.x+d.z*d.z+d.y*d.y*2.f<25.f) return true;
    if(cave.sinkhole) {
        const auto mouthOffset=p-cave.mouth;
        return mouthOffset.x*mouthOffset.x+mouthOffset.z*mouthOffset.z<25.f && mouthOffset.y>-12 && mouthOffset.y<4;
    }
    return false;
}
}
