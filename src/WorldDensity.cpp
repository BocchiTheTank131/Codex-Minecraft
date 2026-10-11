#include "World.h"
#include "TerrainDensity.h"
#include "BiomeVegetation.h"
#include "TerrainExploration.h"
#include <bitset>
#include <algorithm>
#include <array>
#include <cmath>

namespace {
float smooth(float a,float b,float v) {
    const float t=std::clamp((v-a)/(b-a),0.f,1.f); return t*t*(3.f-2.f*t);
}
}
World::TerrainSample World::sampleTerrainDensity(int x,int z) const {
    auto s=sampleTerrainGeography(x,z);
    // Preserve V10 continents/macroclimate. Medium habitat variation only
    // chooses compatible regional variants, rather than warping the coast.
    const float habitat=biomeNoise_.fractal2D(x*(generationVersion_>=12?.0031f:.0130f)+71.3f,z*(generationVersion_>=12?.0031f:.0130f)-19.7f,2,2.f,generationVersion_>=12?.30f:.32f);
    const float wet=humidityNoise_.noise(x*(generationVersion_>=12?.0027f:.0140f)-31.7f,0,z*(generationVersion_>=12?.0027f:.0140f)+73.9f);
    s.humidity=std::clamp(s.macroHumidity+wet*(generationVersion_>=12?.24f:.22f),0.f,1.f);
    s.temperature=std::clamp(s.macroTemperature+habitat*.018f,0.f,1.f);
    const float mountain=smooth(90.f,160.f,float(s.height))*(1.f-smooth(.45f,.85f,s.erosion));
    float height=s.unclampedHeight+16.f+mountain*45.f;
    // Compress the tallest peaks smoothly instead of flattening them at a cap.
    if(height>240.f) height=240.f+64.f*(1.f-std::exp(-(height-240.f)/64.f));
    s.height=std::clamp(int(std::round(height)),-48,304);
    s.biome=selectGeographicalBiome(s.temperature,s.humidity,s.continentalness,s.erosion,
        habitat,s.river,s.height-16,s.mushroomIsland);
    // Wind a rare, surface-open canyon through inland hills, away from shores.
    const float contour=riverNoise_.noise(x*.0012f+63.7f,.713f,z*.0012f-131.9f);
    const float region=erosionNoise_.noise(x*.0007f-217.3f,0,z*.0007f+97.7f);
    s.ravine=(1.f-smooth(.003f,.016f,std::abs(contour)))*smooth(.05f,.19f,region)*
        smooth(.055f,.16f,s.continentalness)*(1.f-smooth(.1f,.45f,s.river));
    if(generationVersion_>=12) {
        // Medium-scale habitat changes never replace the V10 continental or
        // macroclimate fields. Elevation adds local diversity to inland routes.
        const float local=habitat;
        const float rolling=ridgeNoise_.fractal2D(x*.0032f+43.4f,z*.0032f-119.2f,2,2.f,.32f);
        const float delta=TerrainExploration::heightVariation(s.continentalness,s.erosion,s.river,rolling,local);
        s.height=std::clamp(int(std::round(height+delta)),-48,304);
        s.biome=selectGeographicalBiome(s.temperature,s.humidity,s.continentalness,s.erosion,
            local,s.river,s.height-16,s.mushroomIsland);
        s.biome=TerrainExploration::variant(s.biome,s.temperature,s.humidity,local,s.height-16);
    }
    return s;
}

World::GeneratedChunk World::generateChunkDataDensity(int cx,int cz) const {
    using namespace TerrainDensity;
    constexpr int Span=18;
    const int ox=cx*CHUNK_SIZE,oz=cz*CHUNK_SIZE;
    GeneratedChunk result{cx,cz,{}};
    std::array<TerrainSample,Span*Span> terrain{};
    std::array<Column,Span*Span> columns{};
    for(int z=-1;z<=16;++z) for(int x=-1;x<=16;++x) {
        const auto i=(z+1)*Span+x+1;
        auto& t=terrain[i]; t=sampleTerrainDensity(ox+x,oz+z);
        const float aquifer=aquiferNoise_.fractal2D((ox+x)*.0018f+113.7f,(oz+z)*.0018f-67.3f,2,2.f,.3f);
        columns[i]={t.height,smooth(95.f,180.f,float(t.height))*(1.f-smooth(.40f,.83f,t.erosion)),
            aquifer,-24.f+aquifer*30.f,t.ravine};
    }
    std::array<TerrainSample,32*32> neighborColumns;
    std::bitset<32*32> cachedColumns;
    const auto terrainAt=[&](int x,int z) {
        const int lx=x-ox,lz=z-oz;
        if(lx>=-1&&lx<=16&&lz>=-1&&lz<=16) return terrain[(lz+1)*Span+lx+1];
        if(lx>=-8&&lx<24&&lz>=-8&&lz<24) {
            const auto i=(lz+8)*32+lx+8;
            if(!cachedColumns.test(i)) {neighborColumns[i]=sampleTerrainDensity(x,z);cachedColumns.set(i);}
            return neighborColumns[i];
        }
        return sampleTerrainDensity(x,z);
    };
    const auto structureTerrain=[&](int x,int z) { return structureTerrainAt(x,z); };
    const auto plans=structures_.plansForChunk(cx,cz,structureTerrain);
    const auto reserved=[&](int x,int z) {
        for(const auto& p:plans) if(x>=p.bounds.minX-1&&x<=p.bounds.maxX+1&&z>=p.bounds.minZ-1&&z<=p.bounds.maxZ+1) return true;
        return false;
    };
    std::vector<TerrainExploration::CaveEntrance> entrances;
    std::array<std::bitset<384>,Span*Span> entranceMask{};
    if(generationVersion_>=12) {
        entrances=TerrainExploration::caveEntrances(cx,cz,seed_,[&](int x,int z) {
            const auto t=terrainAt(x,z);return TerrainExploration::SurfaceColumn{t.height,t.river,t.ravine};
        },[&](int x,int y,int z) {
            const auto t=terrainAt(x,z);
            const float aquifer=aquiferNoise_.fractal2D(x*.0018f+113.7f,z*.0018f-67.3f,2,2.f,.3f);
            const Column c{t.height,smooth(95.f,180.f,float(t.height))*(1.f-smooth(.40f,.83f,t.erosion)),
                aquifer,-24.f+aquifer*30.f,t.ravine};
            return density(sampleAt(x,y,z,ridgeNoise_,cheeseNoise_,spaghettiNoise_,noodleNoise_,caveNoise_),c,y)<=0;
        },[&](int x,int z){return explorationStructureReservedAt(x,z);});
        for(int z=-1;z<=16;++z)for(int x=-1;x<=16;++x) {
            if(reserved(ox+x,oz+z)) continue;
            auto& mask=entranceMask[(z+1)*Span+x+1];
            for(const auto& cave:entrances) {
                for(int y=std::max(MinimumY+8,int(cave.target.y)-6);y<std::min(MaximumY,int(cave.mouth.y)+7);++y)
                    if(TerrainExploration::carves(cave,ox+x,y,oz+z)) mask.set(y-MinimumY);
            }
        }
    }
    int highest=SeaLevel;
    for(const auto& t:terrain) highest=std::max(highest,t.height);
    const int scanTop=std::min(MaximumY,MinimumY+((highest+53-MinimumY+15)/16)*16);
    const Grid grid(cx,cz,ridgeNoise_,cheeseNoise_,spaghettiNoise_,noodleNoise_,caveNoise_,scanTop);
    result.blocks.resize(CHUNK_SIZE*CHUNK_SIZE*(scanTop-MinimumY),Block::Air);
    // Immutable local occupancy includes a halo for exact exposed-ore tests.
    std::vector<std::uint8_t> solid(Span*Span*(scanTop-MinimumY));
    const auto occupancyIndex=[](int x,int y,int z) { return ((y-MinimumY)*Span+z+1)*Span+x+1; };
    for(int y=MinimumY;y<scanTop;++y) for(int z=-1;z<=16;++z) for(int x=-1;x<=16;++x) {
        const auto& c=columns[(z+1)*Span+x+1];
        solid[occupancyIndex(x,y,z)]=static_cast<std::uint8_t>(y<=c.height+51 && density(grid.at(ox+x,y,oz+z),c,y)>0 && !entranceMask[(z+1)*Span+x+1].test(y-MinimumY));
    }
    // Ore probabilities are constant per depth and random rolls per 3^3 cell.
    // Compute both once instead of repeating seven hashes for every rock voxel.
    constexpr int OreCount=7;
    std::array<std::array<float,OreCount>,384> oreChances{};
    for(int y=MinimumY;y<scanTop;++y) for(int ore=0;ore<OreCount;++ore) {
        const auto& definition=oreDistributions()[ore];
        oreChances[y-MinimumY][ore]=definition.probability*oreWeight(definition,float(y));
    }
    const int cellX=floorDiv(ox,3),cellZ=floorDiv(oz,3),cellY=floorDiv(MinimumY,3);
    const int cellsX=floorDiv(ox+15,3)-cellX+1,cellsZ=floorDiv(oz+15,3)-cellZ+1,
        cellsY=floorDiv(scanTop-1,3)-cellY+1;
    std::vector<std::array<float,OreCount>> oreRolls(cellsX*cellsZ*cellsY);
    for(int y=0;y<cellsY;++y)for(int z=0;z<cellsZ;++z)for(int x=0;x<cellsX;++x)
        for(int ore=0;ore<OreCount;++ore) {
            const auto cluster=hash(cellX+x,cellY+y,cellZ+z,seed_^std::uint32_t((ore+1)*7319));
            oreRolls[(y*cellsZ+z)*cellsX+x][ore]=float(cluster&0xffffU)/65536.f;
        }
    const auto at=[&](int x,int y,int z)->Block& { return result.blocks[localIndex(x,y,z)]; };
    const auto exposed=[&](int x,int y,int z) {
        return y<=MinimumY||y>=scanTop-1|| !solid[occupancyIndex(x-1,y,z)]||!solid[occupancyIndex(x+1,y,z)]||
            !solid[occupancyIndex(x,y-1,z)]||!solid[occupancyIndex(x,y+1,z)]||
            !solid[occupancyIndex(x,y,z-1)]||!solid[occupancyIndex(x,y,z+1)];
    };
    for(int z=0;z<16;++z) for(int x=0;x<16;++x) {
        const auto& t=terrain[(z+1)*Span+x+1]; const auto& c=columns[(z+1)*Span+x+1];
        const auto& biome=biomeDefinition(t.biome);
        for(int y=MinimumY;y<scanTop;++y) {
            const auto h=hash(ox+x,y,oz+z,seed_);
            if(y==MinimumY || (y<MinimumY+4 && h%4U>=static_cast<unsigned>(y-MinimumY))) {
                at(x,y,z)=Block::Bedrock; continue;
            }
            if(!solid[occupancyIndex(x,y,z)]) {
                // Sea water fills only genuinely open surface columns. Local
                // underground aquifers are sparse and independent of sea level.
                if(y>t.height && y<=SeaLevel) at(x,y,z)=Block::Water;
                else if(y<t.height-18 && c.aquifer>.20f && y<=c.aquiferLevel) at(x,y,z)=Block::Water;
                else if(y<-54 && c.aquifer<-.16f) at(x,y,z)=Block::Lava;
                continue;
            }
            Block b=(y<0 || (y<8 && h%8U>static_cast<unsigned>(y)))?Block::Deepslate:Block::Stone;
            if(y==t.height) b=biome.surface;
            else if(y>t.height-4) b=biome.soil;
            if(b==Block::Stone || b==Block::Deepslate) {
                const auto f=grid.at(ox+x,y,oz+z);
                const bool deep=b==Block::Deepslate;
                // Cluster-local hashes create coherent small deposits. Shared
                // global cells ensure veins never depend on chunk load order.
                if(f.vein>-.38f) {
                    const auto& rolls=oreRolls[((floorDiv(y,3)-cellY)*cellsZ+floorDiv(oz+z,3)-cellZ)*cellsX+floorDiv(ox+x,3)-cellX];
                    for(int i=0;i<OreCount;++i) {
                        const auto& ore=oreDistributions()[i];
                        if(rolls[i]>=oreChances[y-MinimumY][i]) continue;
                        if(ore.stone==Block::DiamondOre && exposed(x,y,z) && h%4U!=0) continue;
                        b=deep?ore.deep:ore.stone; break;
                    }
                    const auto region=hash(floorDiv(ox+x,128),0,floorDiv(oz+z,128),seed_^0x7ea1U);
                    if(region%211U==0U && std::abs(f.vein)<.045f && h%5U<3U) {
                        if(y<-8) b=Block::DeepslateIronOre;
                        else if(y>12 && y<72) b=Block::CopperOre;
                    }
                }
                // Reuse the same sampled field for familiar rock patches.
                if(b==Block::Stone) {
                    if(f.vein>.39f) b=Block::Granite;
                    else if(f.vein<-.39f) b=Block::Diorite;
                    else if(f.pillar>.44f) b=Block::Andesite;
                }
            }
            if((t.biome==Biome::Badlands || t.biome==Biome::WoodedBadlands) && t.height-y<=18) {
                constexpr Block bands[]={Block::TerracottaOrange,Block::TerracottaBrown,
                    Block::Terracotta,Block::TerracottaRed,Block::TerracottaYellow,Block::TerracottaWhite};
                b=bands[floorMod(floorDiv(y,3)+int(seed_%6),6)];
                if(y==t.height && t.biome==Biome::WoodedBadlands) b=Block::CoarseDirt;
            }
            at(x,y,z)=b;
        }
        if(t.height<SeaLevel && biome.frozen) at(x,SeaLevel,z)=Block::Ice;
        if(biome.frozen && t.height>SeaLevel && t.height+1<MaximumY && at(x,t.height,z)!=Block::Air)
            at(x,t.height+1,z)=Block::Snow;
    }
    decorateBiomeChunk(cx,cz,seed_,result.blocks,[&](int x,int z) {
        const auto t=terrainAt(x,z); return BiomeTerrain{t.height,t.biome,t.river};
    },reserved,[&](int x,int z) {
        const auto t=terrainAt(x,z);
        if(t.ravine>=.02f) return false;
        if(generationVersion_>=12) for(const auto& cave:entrances)
            if(TerrainExploration::carves(cave,x,t.height,z)) return false;
        return true;
    },MinimumY,scanTop,SeaLevel,generationVersion_);
    result.hasStructure=structures_.applyToChunk(cx,cz,result.blocks,result.loot,result.mobs,structureTerrain,MinimumY,scanTop);
    // Shore vegetation uses actual generated water, not a biome-name heuristic.
    for(int z=0;z<16;++z) for(int x=0;x<16;++x) {
        const auto t=terrainAt(ox+x,oz+z); const int y=t.height;
        if(y!=SeaLevel || reserved(ox+x,oz+z)) continue;
        const Block base=at(x,y,z);
        if(base!=Block::Grass&&base!=Block::Dirt&&base!=Block::Sand) continue;
        bool water=false;
        constexpr int directions[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
        for(const auto& d:directions) {
            const int nx=x+d[0],nz=z+d[1];
            water|=nx>=0&&nx<16&&nz>=0&&nz<16 ? isWater(at(nx,y,nz)) : terrainAt(ox+nx,oz+nz).height<y;
        }
        bool patch=false;
        const int wx=ox+x,wz=oz+z,patchX=floorDiv(wx,4),patchZ=floorDiv(wz,4);
        for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
            const auto anchor=hash(patchX+dx,0,patchZ+dz,seed_^0xca9eU);
            if(anchor%3U) continue;
            const int ax=(patchX+dx)*4+int((anchor>>5U)%4U),az=(patchZ+dz)*4+int((anchor>>9U)%4U);
            patch |= std::abs(wx-ax)+std::abs(wz-az)<=1;
        }
        const auto h=hash(wx,0,wz,seed_^0xcafeU);
        if(water && patch && h%7U) for(int dy=1;dy<=1+int((h>>9U)%3U);++dy) {
            if(at(x,y+dy,z)!=Block::Air) break;
            at(x,y+dy,z)=Block::SugarCane;
        }
    }
    // Retain complete lower bands and omit unused upper air. High edits grow
    // storage in 16-block bands; saved indices stay tied to the versioned datum.
    std::size_t top=result.blocks.size();
    while(top>4096 && std::all_of(result.blocks.begin()+top-4096,result.blocks.begin()+top,
        [](Block b){return b==Block::Air;})) top-=4096;
    result.blocks.resize(top);
    result.blocks.shrink_to_fit();
    return result;
}
