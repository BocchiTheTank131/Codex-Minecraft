#include "World.h"
#include "Player.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <tuple>

// Opt-in release validation; not run by normal startup or world simulation.
bool World::runPatch272SelfTest(std::string& report) const {
    constexpr std::uint32_t seeds[] = {20260917U,123456789U,1U,42U,8675309U,314159U,987654321U,271828U};
    std::ofstream csv("terrain272.csv");
    csv << "seed,x,z,old,new,biome,river\n";
    double beforeMs=0,afterMs=0;
    long long oldRange=0,newRange=0;
    int plains=0,forest=0,desert=0,snow=0,ocean=0,rivers=0,shore=0,cane=0;
    std::array<int,7> structureCounts{};
    const auto elapsed=[](auto start) { return std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-start).count(); };
    for(auto seed : seeds) {
        World old(seed), fresh(seed);
        old.generationVersion_=7;
        fresh.generationVersion_=8; // Historical v2.7.2 fixture; v9 has its own audit.
        int oldMin=256,oldMax=0,newMin=256,newMax=0;
        for(int z=-2048;z<=2048;z+=32) for(int x=-2048;x<=2048;x+=32) {
            const auto a=old.sampleTerrain(x,z),b=fresh.sampleTerrain(x,z);
            csv << seed << ',' << x << ',' << z << ',' << a.height << ',' << b.height << ','
                << static_cast<int>(b.biome) << ',' << b.river << '\n';
            oldMin=std::min(oldMin,a.height); oldMax=std::max(oldMax,a.height);
            newMin=std::min(newMin,b.height); newMax=std::max(newMax,b.height);
            if(b.height<6 || b.height>WORLD_HEIGHT-12) {report="height overflow";return false;}
            plains+=b.biome==Biome::Plains;
            forest+=b.biome==Biome::Forest || b.biome==Biome::BirchForest;
            desert+=b.biome==Biome::Desert;
            snow+=b.biome==Biome::SnowyPlains || b.biome==Biome::SnowySlopes || b.biome==Biome::SnowyPeaks;
            ocean+=b.biome==Biome::Ocean; shore+=b.biome==Biome::Beach;
            rivers+=b.river>.7f;
        }
        oldRange+=oldMax-oldMin;newRange+=newMax-newMin;
        const auto spawn=fresh.findSafeSpawnNear(0,0);
        if(std::abs(spawn.x) > 49 || std::abs(spawn.z)>49 || spawn.y<3 || spawn.y>WORLD_HEIGHT-2) {
            report="bounded safe origin spawn failed";return false;
        }
        std::cout << "Terrain seed " << seed << ": v7 " << oldMin << '-' << oldMax
                  << ", v8 " << newMin << '-' << newMax << '\n';
        // Alternate order to reduce warm-cache bias, generating identical coordinates.
        std::uint64_t checksum=0;
        for(int pass=0;pass<2;++pass) for(int n=0;n<8;++n) {
            const int cx=(n%4)*19-28,cz=(n/4)*31-17;
            for(int order=0;order<2;++order) {
                World& w=(order==pass ? old : fresh);
                const auto start=std::chrono::steady_clock::now();
                const auto c=w.generateChunkData(cx,cz);
                const double ms=elapsed(start);
                (w.generationVersion_==7 ? beforeMs : afterMs)+=ms;
                for(Block b:c.blocks) checksum+=static_cast<unsigned>(b);
            }
        }
        if(!checksum) {report="empty chunks";return false;}
        const auto first=fresh.generateChunkData(0,0),second=fresh.generateChunkData(0,0);
        if(first.blocks!=second.blocks) {report="chunk nondeterminism";return false;}
        // Persisted v7 retains its generator for unexplored chunks.
        const glm::vec3 saved(100.5f,90,200.5f);
        if(!old.saveWorld("patch272-old-world.vxw",saved)) {report="save fixture failed";return false;}
        World loaded(seed);glm::vec3 restored;
        if(!loaded.loadWorld("patch272-old-world.vxw",restored) || loaded.generationVersion()!=7 ||
           loaded.generateChunkData(19,-17).blocks!=old.generateChunkData(19,-17).blocks) {
            report="v7 save/generator compatibility";return false;
        }
        std::remove("patch272-old-world.vxw");
        // Check minor structures as well as the major-structure smoke suite.
        // Stop surveying once every existing structure family has been verified.
        if(std::any_of(structureCounts.begin(),structureCounts.end(),[](int n){return n==0;})) {
            std::set<std::tuple<int,int,int>> seen;
            for(int cz=-128;cz<=128;cz+=2)for(int cx=-128;cx<=128;cx+=2) {
                const auto plans=fresh.structures_.plansForChunk(cx,cz,
                    [&](int x,int z){return fresh.structureTerrainAt(x,z);});
                for(const auto& plan:plans) {
                    const int kind=static_cast<int>(plan.kind);
                    if(!seen.emplace(kind,plan.regionX,plan.regionZ).second)continue;
                    ++structureCounts[kind];
                    for(const auto& piece:plan.pieces) {
                        const auto heights=terrainHeightForFootprint(piece.bounds,
                            [&](int x,int z){return fresh.structureTerrainAt(x,z);});
                        if(heights[0]<=SEA_LEVEL+2 || heights[1]-heights[0]>8 ||
                           piece.bounds.maxY>=WORLD_HEIGHT-2) {
                            report="unsafe structure terrain/height";return false;
                        }
                    }
                    if(structureCounts[kind]==1) {
                        const auto& piece=plan.pieces.front();
                        const auto chunk=fresh.generateChunkData(floorDiv(piece.x,16),floorDiv(piece.z,16));
                        if(!isSolid(chunk.blocks[localIndex(floorMod(piece.x,16),piece.y-1,floorMod(piece.z,16))])) {
                            report="floating structure foundation";return false;
                        }
                    }
                }
            }
        }
        // Sample bounded, widely distributed shore candidates, then check actual
        // blocks (including neighbors across chunk boundaries) for every base.
        int candidates=0;
        for(int cz=-128;cz<=128 && candidates<40;++cz) for(int cx=-128;cx<=128 && candidates<40;++cx) {
            bool near=false;
            for(int z : {0,7,15}) for(int x : {0,7,15})
                near |= fresh.terrainHeight(cx*16+x,cz*16+z)==SEA_LEVEL;
            if(!near)continue;
            ++candidates;
            const auto c=fresh.generateChunkData(cx,cz);
            std::unordered_map<std::int64_t,GeneratedChunk> neighbors;
            const auto at=[&](int x,int y,int z) {
                if(x>=0 && x<16 && z>=0 && z<16)return c.blocks[localIndex(x,y,z)];
                const int nx=cx+floorDiv(x,16),nz=cz+floorDiv(z,16);
                const auto key=chunkKey(nx,nz);auto it=neighbors.find(key);
                if(it==neighbors.end())it=neighbors.emplace(key,fresh.generateChunkData(nx,nz)).first;
                return it->second.blocks[localIndex(floorMod(x,16),y,floorMod(z,16))];
            };
            bool hadCane=false;
            for(int z=0;z<16;++z)for(int x=0;x<16;++x)for(int y=1;y<WORLD_HEIGHT-1;++y) {
                if(at(x,y,z)!=Block::SugarCane || at(x,y-1,z)==Block::SugarCane)continue;
                ++cane;hadCane=true;
                const Block ground=at(x,y-1,z);
                if((ground!=Block::Grass && ground!=Block::Dirt && ground!=Block::Sand) ||
                   !(isWater(at(x-1,y-1,z))||isWater(at(x+1,y-1,z))||
                     isWater(at(x,y-1,z-1))||isWater(at(x,y-1,z+1)))) {
                    report="invalid Sugar Cane base/water";return false;
                }
                int height=1;while(y+height<WORLD_HEIGHT && at(x,y+height,z)==Block::SugarCane)++height;
                if(height>3 || at(x,y+height,z)!=Block::Air) {report="invalid cane clearance";return false;}
            }
            if(hadCane && c.blocks!=fresh.generateChunkData(cx,cz).blocks) {
                report="Sugar Cane nondeterminism";return false;
            }
        }
    }
    std::cout << "Terrain CPU benchmark 128 chunks/version: v7 " << beforeMs << " ms; v8 "
              << afterMs << " ms; ratio " << afterMs/beforeMs << '\n';
    std::cout << "Biome samples plains/forest/desert/snow/ocean/beach/river "
              << plains << '/' << forest << '/' << desert << '/' << snow << '/' << ocean << '/'
              << shore << '/' << rivers << "; valid deterministic cane bases " << cane << '\n';
    std::cout << "Valid v8 structure plans (plains village/desert village/outpost/pyramid/ruin/well/campsite): ";
    for(int count:structureCounts)std::cout<<count<<'/';std::cout<<'\n';
    const bool structuresOk=std::all_of(structureCounts.begin(),structureCounts.end(),[](int n){return n>0;});
    const bool ok = structuresOk && newRange>oldRange && plains>0 && forest>0 && desert>0 && snow>0 &&
                    ocean>0 && shore>0 && rivers>0 && cane>0 && afterMs<beforeMs*1.20;
    report=ok ? "eight-seed terrain, bounded heights, v7 compatibility, shoreline cane and CPU budget passed"
              : "terrain diversity, cane or performance failed";
    return ok;
}
