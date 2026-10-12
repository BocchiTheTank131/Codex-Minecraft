#include "World.h"
#include "TerrainDensity.h"
#include "Definitions.h"
#include "Player.h"
#include "Weather.h"
#include "TerrainChaos.h"
#include "BiomeVegetation.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>
#include <cstring>
#include <iostream>

namespace {
std::uint64_t hashBlocks(const std::vector<Block>& blocks) {
    std::uint64_t h=14695981039346656037ULL;
    for(Block b:blocks) { h^=std::uint8_t(b); h*=1099511628211ULL; } return h;
}
struct Pixel {
    std::uint8_t biome, reserved;
    std::int16_t height;
    float temperature,humidity,continent,river,macroTemperature,macroHumidity;
};
static_assert(sizeof(Pixel)==28,"Density map layout");
}
bool World::runDensityAudit(const std::string& directory,std::string& report,unsigned modernVersion) const {
    namespace fs=std::filesystem;
    fs::create_directories(directory);
    std::ofstream generation(directory+"/generation.csv"),oreCounts(directory+"/ores.csv"),
        distributions(directory+"/biomes.csv"),sites(directory+"/sites.csv"),materials(directory+"/materials.csv"),
        metrics(directory+"/underground.csv");
    generation<<"seed,version,x,z,hash,ms,allocated_bytes\n";
    oreCounts<<"seed,y,solid,coal,iron,copper,gold,diamond,air,water,lava\n";
    distributions<<"seed,version,extent,biome,samples\n";
    sites<<"seed,biome,x,z,height\n";
    std::ofstream veins(directory+"/veins.csv");
    veins<<"seed,region_x,region_z,iron_blocks,largest_connected_deposit\n";
    metrics<<"seed,cave_air,cave_water,cave_lava,solid,above_surface_solid,allocated_bytes\n";
    materials<<"id,name\n";
    for(int i=0;i<int(Block::Count);++i) {
        const auto b=Block(i); const auto item=blockToItem(b);
        materials<<i<<','<<(b==Block::Air?"Air":itemDefinition(item).displayName)<<'\n';
    }
    const auto require=[&](bool value,const char* message) {
        if(!value) throw std::runtime_error(message);
    };
    try {
        if(modernVersion>=15) {
            float previous=TerrainChaos::compress(199.f);
            for(int i=1;i<20000;++i) {
                const float value=TerrainChaos::compress(199.f+i*.05f);
                require(std::isfinite(value)&&value>=previous&&value<=255.f,"Natural ceiling compression");
                previous=value;
            }
            require(std::abs((TerrainChaos::compress(200.01f)-200.f)/.01f-1.f)<.01f,"Compression shoulder is discontinuous");
        }
        for(auto seed:{20260917U,123456789U,42U}) {
            std::array<int,35> totalBiomes{};
            glm::ivec2 mountain(0);
            World modern(seed,modernVersion);
            World previousClimate(seed,15);
            World preservedClimate(seed,16);
            require(modern.minimumY()==-64&&modern.maximumY()==320&&modern.seaLevel()==64,"V11 vertical bounds");
            std::vector<unsigned> mapVersions{modernVersion-1,modernVersion};
            if(modernVersion>=15) mapVersions.insert(mapVersions.begin(),9);
            for(unsigned version:mapVersions) {
                World probe(seed,version);
                for(int extent:{4096,30720}) {
                    std::ofstream map(directory+"/map-"+std::to_string(seed)+"-v"+std::to_string(version)+"-"+std::to_string(extent)+".bin",std::ios::binary);
                    std::array<int,35> counts{};
                    for(int z=0;z<512;++z) for(int x=0;x<512;++x) {
                        const int wx=x*(extent/512)-extent/2,wz=z*(extent/512)-extent/2;
                        const auto t=probe.sampleTerrain(wx,wz); ++counts[int(t.biome)];
                        if(version==16) {
                            const auto old=previousClimate.sampleTerrain(wx,wz);
                            require(t.height==old.height&&t.unclampedHeight==old.unclampedHeight&&
                                t.humidity==old.humidity&&t.macroHumidity==old.macroHumidity&&
                                t.macroTemperature==old.macroTemperature&&t.continentalness==old.continentalness&&
                                t.naturalContinentalness==old.naturalContinentalness&&t.erosion==old.erosion&&
                                t.weirdness==old.weirdness&&t.peakValley==old.peakValley&&
                                t.river==old.river&&t.ravine==old.ravine&&t.mushroomIsland==old.mushroomIsland,
                                "V16 changed a non-temperature terrain field");
                            require(t.temperature>=0&&t.temperature<=1,"V16 normalized temperature");
                            if(x%64==0&&z%64==0) {
                                const auto climate=probe.biomeClimateAt(wx,wz);
                                require(climate.temperature==t.temperature&&climate.humidity==t.humidity&&
                                    climate.biome==t.biome&&climate.dry==biomeDefinition(t.biome).dry&&
                                    climate.effectiveTemperature==altitudeTemperature(t.temperature,float(t.height-16)),
                                    "V16 biome/weather climate disagreement");
                            }
                        }
                        if(version==17) {
                            const auto old=preservedClimate.sampleTerrain(wx,wz);
                            require(t.temperature==old.temperature && t.humidity==old.humidity &&
                                t.macroTemperature==old.macroTemperature && t.macroHumidity==old.macroHumidity,
                                "V17 changed V16 climate sampling");
                        }
                        if(version==modernVersion) {
                            ++totalBiomes[int(t.biome)];
                            if(totalBiomes[int(t.biome)]==1) sites<<seed<<','<<biomeDefinition(t.biome).name<<','<<wx<<','<<wz<<','<<t.height<<'\n';
                            if(t.height>modern.terrainHeight(mountain.x,mountain.y)&&t.ravine<.02f) mountain={wx,wz};
                        }
                        require(t.height>=probe.minimumY()&&t.height<=probe.maximumY()-12,"Density map height bounds");
                        if(version>=15) require(t.height<=255,"Surface exceeds natural height ceiling");
                        const Pixel p{std::uint8_t(t.biome),0,std::int16_t(t.height),t.temperature,t.humidity,t.continentalness,t.river,t.macroTemperature,t.macroHumidity};
                        map.write(reinterpret_cast<const char*>(&p),sizeof(p));
                    }
                    require(bool(map),"Density map write");
                    for(int b=0;b<35;++b) distributions<<seed<<','<<version<<','<<extent<<','<<biomeDefinition(Biome(b)).name<<','<<counts[b]<<'\n';
                }
            }
            for(int b=0;b<35;++b) if(!totalBiomes[b]) throw std::runtime_error("Missing V"+std::to_string(modernVersion)+" biome: "+biomeDefinition(Biome(b)).name);
            std::array<std::array<std::uint64_t,9>,384> histogram{};
            std::uint64_t caveAir=0,caveWater=0,caveLava=0,solid=0,above=0,bytes=0;
            for(unsigned version:{modernVersion-1,modernVersion}) {
                World probe(seed,version);
                for(int i=0;i<48;++i) {
                    const int cx=(i%8)*173-606,cz=(i/8)*197-492;
                    std::uint64_t expected=0;
                    for(int repeat=0;repeat<3;++repeat) {
                        const auto start=std::chrono::steady_clock::now();
                        const auto data=probe.generateChunkData(cx,cz);
                        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                        const auto h=hashBlocks(data.blocks);
                        if(repeat) require(expected==h,"Repeated density generation changed"); expected=h;
                        generation<<seed<<','<<version<<','<<cx<<','<<cz<<','<<h<<','<<ms<<','<<data.blocks.size()<<'\n';
                        if(version!=modernVersion||repeat!=0) continue;
                        bytes+=data.blocks.size();
                        std::array<int,256> surfaceHeights{};
                        for(int z=0;z<16;++z)for(int x=0;x<16;++x) surfaceHeights[z*16+x]=probe.terrainHeight(cx*16+x,cz*16+z);
                        for(std::size_t j=0;j<data.blocks.size();++j) {
                            const int y=-64+int(j/256),x=int(j%16),z=int((j/16)%16); const auto b=data.blocks[j];
                            auto& row=histogram[y+64];
                            const int surface=surfaceHeights[z*16+x];
                            require(y<316 || b==Block::Air,"Generated geometry touches world ceiling");
                            if(version>=15) require(y<TerrainChaos::NaturalMaximumY||b==Block::Air,"Natural blocks exceed Y256");
                            if(isSolid(b)) { ++row[0];++solid; if(y>surface)++above; }
                            int ore=-1;
                            if(b==Block::CoalOre||b==Block::DeepslateCoalOre)ore=1;
                            if(b==Block::IronOre||b==Block::DeepslateIronOre)ore=2;
                            if(b==Block::CopperOre||b==Block::DeepslateCopperOre)ore=3;
                            if(b==Block::GoldOre||b==Block::DeepslateGoldOre)ore=4;
                            if(b==Block::DiamondOre||b==Block::DeepslateDiamondOre)ore=5;
                            if(ore>=0)++row[ore];
                            if(y>=surface-5)continue;
                            if(b==Block::Air) {++row[6];++caveAir;}
                            if(isWater(b)) {++row[7];++caveWater;}
                            if(isLava(b)) {++row[8];++caveLava;}
                        }
                    }
                    auto parallel=std::async(std::launch::async,[&probe,cx,cz] {return hashBlocks(probe.generateChunkData(cx,cz).blocks);});
                    require(parallel.wait_for(std::chrono::seconds(30))==std::future_status::ready,"Parallel density timeout");
                    require(parallel.get()==expected,"Parallel density mismatch");
                }
            }
            if(modernVersion>=15) {
                int highestNatural=-64;
                for(int dz=-2;dz<=2;++dz)for(int dx=-2;dx<=2;++dx) {
                    const int cx=floorDiv(mountain.x,16)+dx,cz=floorDiv(mountain.y,16)+dz;
                    const auto data=modern.generateChunkData(cx,cz);
                    for(std::size_t i=0;i<data.blocks.size();++i) if(data.blocks[i]!=Block::Air) {
                        const int y=-64+int(i/256);highestNatural=std::max(highestNatural,y);
                        require(y<TerrainChaos::NaturalMaximumY,"High peak feature ceiling");
                    }
                    for(const auto& plan:modern.structures_.plansForChunk(cx,cz,[&](int x,int z){return modern.structureTerrainAt(x,z);}))
                        for(const auto& piece:plan.pieces) require(piece.bounds.maxY<257,"Structure truncated at natural ceiling");
                }
                for(int root:{231,232,240,255}) {
                    std::vector<Block> blocks(256*(272+64),Block::Air);
                    for(int y=-64;y<=root;++y)for(int i=0;i<256;++i) blocks[(y+64)*256+i]=Block::Grass;
                    decorateBiomeChunk(-1,-1,seed,blocks,[&](int,int){return BiomeTerrain{root,Biome::Jungle,0};},
                        [](int,int){return false;},[](int,int){return true;},-64,257,64,modernVersion);
                    for(std::size_t i=0;i<blocks.size();++i)
                        require(i/256<321||blocks[i]==Block::Air,"Vegetation above Y256");
                    if(root>=233) for(int y=root+1;y<272;++y)for(int i=0;i<256;++i) {
                        const Block b=blocks[(y+64)*256+i];
                        require(b!=Block::JungleLog&&b!=Block::JungleLeaves,"High root generated a clipped tree");
                    }
                }
                std::cout<<"V15 "<<seed<<" high-peak natural ceiling: Y"<<highestNatural<<"; 25 chunks, full vegetation models and structure bounds passed\n";
            }
            for(int y=-64;y<320;++y) {
                oreCounts<<seed<<','<<y; for(auto value:histogram[y+64])oreCounts<<','<<value; oreCounts<<'\n';
            }
            metrics<<seed<<','<<caveAir<<','<<caveWater<<','<<caveLava<<','<<solid<<','<<above<<','<<bytes<<'\n';
            // Raw Y/Z-facing cutaways at a mountain and at world spawn.
            for(int scene=0;scene<2;++scene) {
                const int startX=scene?mountain.x-256:-256,z=scene?mountain.y:0;
                std::ofstream slice(directory+"/section-"+std::to_string(seed)+"-"+std::to_string(scene)+".bin",std::ios::binary);
                std::vector<std::uint8_t> image(512*384);
                for(int cx=floorDiv(startX,16);cx<=floorDiv(startX+511,16);++cx) {
                    const auto chunk=modern.generateChunkData(cx,floorDiv(z,16));
                    for(int x=std::max(startX,cx*16);x<=std::min(startX+511,cx*16+15);++x)
                        for(int y=-64;y<320;++y) {
                            const auto index=modern.localIndex(floorMod(x,16),y,floorMod(z,16));
                            image[(319-y)*512+x-startX]=index<chunk.blocks.size()?std::uint8_t(chunk.blocks[index]):0;
                        }
                }
                slice.write(reinterpret_cast<const char*>(image.data()),image.size());
                sites<<seed<<",cutaway"<<scene<<','<<startX<<','<<z<<",0\n";
            }
            // A generated 64x64 volume for an actual isometric underground cutaway.
            const int vcx=floorDiv(mountain.x,16)-2,vcz=floorDiv(mountain.y,16)-2;
            std::vector<std::uint8_t> volume(64*64*384);
            for(int cz=0;cz<4;++cz)for(int cx=0;cx<4;++cx) {
                const auto data=modern.generateChunkData(vcx+cx,vcz+cz);
                for(int y=-64;y<320;++y)for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
                    const auto index=modern.localIndex(x,y,z);
                    volume[((y+64)*64+cz*16+z)*64+cx*16+x]=index<data.blocks.size()?std::uint8_t(data.blocks[index]):0;
                }
            }
            std::ofstream volumeFile(directory+"/volume-"+std::to_string(seed)+".bin",std::ios::binary);
            volumeFile.write(reinterpret_cast<const char*>(volume.data()),volume.size());
            // Verify a rare vein is a genuinely large connected deposit.
            glm::ivec2 veinRegion(0); bool foundVein=false;
            for(int z=-32;z<=32 && !foundVein;++z)for(int x=-32;x<=32 && !foundVein;++x)
                if(TerrainDensity::hash(x,0,z,seed^0x7ea1U)%211U==0U) {veinRegion={x,z};foundVein=true;}
            require(foundVein,"Rare ore region availability");
            std::vector<std::uint8_t> iron(64*64*56);
            int totalIron=0;
            for(int cz=0;cz<4;++cz)for(int cx=0;cx<4;++cx) {
                const auto data=modern.generateChunkData(veinRegion.x*8+cx+2,veinRegion.y*8+cz+2);
                for(int y=-64;y<-8;++y)for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
                    const auto i=modern.localIndex(x,y,z);
                    const bool present=i<data.blocks.size()&&data.blocks[i]==Block::DeepslateIronOre;
                    iron[((y+64)*64+cz*16+z)*64+cx*16+x]=present;
                    totalIron+=present;
                }
            }
            std::vector<int> flood; int largestVein=0;
            for(int start=0;start<int(iron.size());++start) {
                if(!iron[start])continue;
                flood.clear(); flood.push_back(start);iron[start]=0;
                for(std::size_t i=0;i<flood.size();++i) {
                    const int id=flood[i],x=id%64,z=(id/64)%64,y=id/4096;
                    const auto visit=[&](int next) {if(iron[next]){iron[next]=0;flood.push_back(next);}};
                    if(x>0)visit(id-1);if(x<63)visit(id+1);
                    if(z>0)visit(id-64);if(z<63)visit(id+64);
                    if(y>0)visit(id-4096);if(y<55)visit(id+4096);
                }
                largestVein=std::max(largestVein,int(flood.size()));
            }
            veins<<seed<<','<<veinRegion.x<<','<<veinRegion.y<<','<<totalIron<<','<<largestVein<<'\n';
            require(largestVein>=256,"Rare vein lacks a large connected deposit");
            // Global lattice samples must be bit-identical across chunk borders.
            TerrainDensity::Grid a(-1,-1,modern.ridgeNoise_,modern.cheeseNoise_,modern.spaghettiNoise_,modern.noodleNoise_,modern.caveNoise_),
                b(0,-1,modern.ridgeNoise_,modern.cheeseNoise_,modern.spaghettiNoise_,modern.noodleNoise_,modern.caveNoise_);
            for(int y=-64;y<320;++y)for(int z=-16;z<0;++z) {
                const auto left=a.at(0,y,z),right=b.at(0,y,z);
                require(std::memcmp(&left,&right,sizeof(left))==0,"Density lattice seam");
            }
            std::string structureReport;
            require(modern.runStructureGenerationSmokeTest(structureReport,nullptr,48),"V11 structures/loot/footprint support");
            std::cout << 'V' << modernVersion << ' ' << seed << " structures: " << structureReport << '\n';
            modern.prepareSpawnTerrain({.5f,80,.5f});
            modern.setBlock(2,-40,2,Block::Torch);
            modern.setBlock(2,-40,3,Block::Air);
            for(int i=0;i<12;++i) modern.updateStreaming({.5f,80,.5f},0);
            require(modern.blockLightAt(2,-40,3)==14,"Negative Y block lighting");
            modern.setBlock(2,319,2,Block::DiamondBlock);
            require(modern.getBlock(2,319,2)==Block::DiamondBlock,"Upper band edit growth");
            const std::string save=directory+"/roundtrip-"+std::to_string(seed)+".vxw";
            require(modern.saveWorld(save,{2,-39,2}),"V11 save write");
            World restored(seed,10);glm::vec3 position;
            require(restored.loadWorld(save,position)&&restored.generationVersion()==int(modernVersion),"V11 load metadata");
            restored.prepareSpawnTerrain({.5f,80,.5f});
            require(restored.getBlock(2,-40,2)==Block::Torch&&restored.getBlock(2,319,2)==Block::DiamondBlock,"V11 edit roundtrip");
            const auto mesh=buildMesh(restored.captureMeshInput(0,0));
            if (!mesh.opaqueBounds.valid || mesh.opaqueBounds.minimum.y > -64 || mesh.opaqueBounds.maximum.y < 320) {
                std::ostringstream detail; detail << "Negative and upper mesh bounds: " << mesh.opaqueBounds.valid << " " << mesh.opaqueBounds.minimum.y << " " << mesh.opaqueBounds.maximum.y; throw std::runtime_error(detail.str());
            }
            modern.setBlock(2,-40,2,Block::Air);
            for(int i=0;i<12;++i)modern.updateStreaming({.5f,80,.5f},0);
            require(modern.blockLightAt(2,-40,3)==0,"Negative Y light removal");
            for(int z=1;z<=3;++z)for(int x=1;x<=3;++x)for(int y=-40;y<-25;++y)
                modern.setBlock(x,y,z,y==-40?Block::Stone:Block::Air);
            Player underground({2.5f,-30.f,2.5f}); PlayerInput still{};
            for(int i=0;i<180;++i) underground.update(1.f/60.f,still,modern);
            require(!underground.isDead() && underground.position().y>=-39.f && underground.position().y<-38.98f,"Negative Y player landing / void threshold");
            modern.setBlock(2,-39,2,Block::Lava);
            Player lavaTarget({2.5f,-38.99f,2.5f});
            lavaTarget.update(1.f/60.f,still,modern);
            require(lavaTarget.health()==16.f,"Lava contact damage");
            lavaTarget.update(1.f/60.f,still,modern);
            require(lavaTarget.health()==16.f,"Lava damage cooldown");
            lavaTarget.setCreativeMode(true);
            lavaTarget.update(1.f/60.f,still,modern);
            require(lavaTarget.health()==20.f,"Creative lava immunity");
            for(int i=0;i<12;++i)modern.updateStreaming({2.5f,-39.f,2.5f},0);
            require(modern.blockLightAt(2,-39,3)==14,"Negative Y lava emission");
            modern.setBlock(2,-39,2,Block::Air);
            for(int i=0;i<12;++i)modern.updateStreaming({2.5f,-39.f,2.5f},0);
            require(modern.blockLightAt(2,-39,3)==0,"Lava source removal lighting");
            RayHit hit;
            require(modern.raycast({2.5f,-30.f,2.5f},{0,-1,0},12,hit)&&hit.block.y==-40,"Negative Y raycast");
            require(!Weather::exposed(modern,{2.5f,-38.f,2.5f}),"Underground roof exposure");
            for(unsigned version=1;version<modernVersion;++version) {
                World old(seed,version);require(old.minimumY()==(version>=11?-64:0)&&old.maximumY()==(version>=11?320:256)&&old.seaLevel()==(version>=11?64:48),"Legacy vertical bounds");
                const auto path=directory+"/legacy-"+std::to_string(seed)+"-"+std::to_string(version)+".vxw";
                require(old.saveWorld(path,{0,72,0}),"Legacy save write");
                World loaded(seed,11); require(loaded.loadWorld(path,position)&&loaded.generationVersion()==int(version),"Legacy generator save roundtrip");
                require(hashBlocks(old.generateChunkData(-1,0).blocks)==hashBlocks(loaded.generateChunkData(-1,0).blocks),"Legacy geography after load");
            }
        }
        report="V"+std::to_string(modernVersion)+" density audit passed: all 35 biomes, repeat/parallel generation, lattice seams, negative/upper Y edits/meshes/lighting, and versioned generator save roundtrips.";
        return true;
    } catch(const std::exception& error) { report=error.what();return false; }
}
