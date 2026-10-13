#include "World.h"
#include "TerrainExploration.h"
#include "TerrainDensity.h"
#include "TerrainAesthetics.h"
#include "BiomeVegetation.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <set>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
std::uint64_t hashBytes(const void* pointer,std::size_t size) {
    auto bytes=static_cast<const std::uint8_t*>(pointer); std::uint64_t h=14695981039346656037ULL;
    for(std::size_t i=0;i<size;++i) {h^=bytes[i];h*=1099511628211ULL;} return h;
}
}
bool World::runStreamingSelfTest(std::string& report) {
    try {
        for(unsigned cores:{1U,2U,4U,8U,16U,28U}) for(auto quality:{ChunkStreamingQuality::Auto,ChunkStreamingQuality::Low,ChunkStreamingQuality::Balanced,ChunkStreamingQuality::High}) {
            const auto plan=streamingWorkerPlan(cores,quality);
            require(plan.generation>=1&&plan.generation<=8&&plan.mesh>=1&&plan.mesh<=4,"Worker plan bounds");
            require(cores>3 || (plan.generation==1&&plan.mesh==1),"Dual-core worker safety");
            require(cores<=3 || plan.generation+plan.mesh<=std::max(2U,cores/2),"Worker oversubscription");
        }
        require(streamingPriority(0,-3,{0,-1})<streamingPriority(0,3,{0,-1}),"Forward priority");
        require(streamingPriority(0,1,{0,-1})<streamingPriority(0,-8,{0,-1}),"Near priority");
        require(streamingPriority(30,30,{0,-1},true)<streamingPriority(0,0,{0,-1}),"Edit priority");
        std::array<std::uint64_t,128> generatedHashes;
        const auto coordinates=[](int i){return glm::ivec2(i*617-24000,12000-i*347);};
        for(int i=0;i<128;++i) {
            const auto p=coordinates(i);const auto data=generateChunkData(p.x,p.y);
            generatedHashes[i]=hashBytes(data.blocks.data(),data.blocks.size()*sizeof(Block));
        }
        std::vector<std::future<void>> generationTests;
        for(int worker=0;worker<8;++worker) generationTests.push_back(std::async(std::launch::async,[&,worker] {
            for(int i=worker;i<128;i+=8) {
                // Churn more than 128 regional plans while other workers retain
                // and copy plans; cache eviction must not invalidate their reads.
                for(int r=0;r<3;++r) structures_.majorPlanForRegion(i*7+r,500-i*5,
                    [&](int x,int z){return structureTerrainAt(x,z);});
                const auto p=coordinates(i);const auto data=generateChunkData(p.x,p.y);
                require(hashBytes(data.blocks.data(),data.blocks.size()*sizeof(Block))==generatedHashes[i],"Concurrent generation/cache eviction changed blocks");
            }
        }));
        for(auto& test:generationTests) test.get();
        generate(3,{-.5f,120.f,-.5f});setSimulationDistance(2);
        const auto snapshot=captureMeshInput(-1,-1);
        const auto reference=buildMesh(snapshot);
        const auto originalLight=findChunk(-1,-1)->packedLight;
        for(int z=0;z<16;++z)for(int x=0;x<16;++x) computeSunlightColumn(*findChunk(-1,-1),x,z);
        require(originalLight==findChunk(-1,-1)->packedLight,"Sunlight traversal changed values");
        std::vector<std::future<MeshOutput>> builds;
        for(int i=0;i<8;++i) builds.push_back(std::async(std::launch::async,[&]{return buildMesh(snapshot);}));
        for(auto& future:builds) {
            const auto result=future.get();
            require(result.opaque.size()==reference.opaque.size()&&result.water.size()==reference.water.size(),"Parallel mesh size");
            require(hashBytes(result.opaque.data(),result.opaque.size()*sizeof(VoxelVertex))==hashBytes(reference.opaque.data(),reference.opaque.size()*sizeof(VoxelVertex)),"Parallel opaque mesh attributes/order");
            require(hashBytes(result.water.data(),result.water.size()*sizeof(VoxelVertex))==hashBytes(reference.water.data(),reference.water.size()*sizeof(VoxelVertex)),"Parallel transparent mesh order");
        }
        for(int tick=0;tick<120;++tick) {
            if(tick==40) setStreamingQuality(ChunkStreamingQuality::Low);
            if(tick==70) setStreamingQuality(ChunkStreamingQuality::Auto);
            const glm::vec3 position=tick<30?glm::vec3(-.5f,120.f,-.5f):tick<60?glm::vec3(-2048.5f,120.f,2048.5f):glm::vec3(.5f,120.f,.5f);
            if(tick==75) generate(3,position); // Cancel in-flight epoch results.
            updateStreaming(position,8);
            const auto stats=streamingDiagnostics();
            require(pendingChunkCount()<=96,"Generation admission queue bound");
            require(pendingCpuMeshCount()+completedMeshCount()<=int(workerPlan_.mesh*8+4),"Mesh queue bound");
            require(stats.meshActive<=int(meshWorkers_.size()),"Mesh active worker accounting");
            {
                std::lock_guard<std::mutex> lock(generationMutex_);
                for(const auto& job:generationQueue_) require(job.epoch==generationEpoch_.load(),"Stale queued generation epoch");
                for(const auto& job:completedQueue_) require(job.epoch==generationEpoch_.load(),"Stale completed generation epoch");
            }
            if(tick>80) setBlock(1,120,1,tick%2?Block::Torch:Block::Stone);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        setBlock(1,120,1,Block::Stone);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        bool settled=false;
        while(std::chrono::steady_clock::now()<deadline) {
            updateStreaming({.5f,120.f,.5f},8);
            if(!pendingChunkCount()&&!pendingCpuMeshCount()&&!completedMeshCount()&&!queuedMeshRebuildCount()&&lightingQueue_.empty()) {settled=true;break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        require(settled,"Teleport/edit queues failed to recover");
        auto* chunk=findChunk(0,0);require(chunk!=nullptr,"Destination chunk missing");
        const auto expected=buildMesh(captureMeshInput(0,0));
        require(std::size_t(chunk->opaqueVertexCount)==expected.opaque.size(),"Stale opaque upload");
        std::vector<VoxelVertex> actual(expected.opaque.size());
        glBindBuffer(GL_ARRAY_BUFFER,chunk->opaquePage->vbo);
        std::vector<PackedTerrainVertex> packed(chunk->opaqueUniqueVertices);
        glGetBufferSubData(GL_ARRAY_BUFFER,chunk->opaqueFirstVertex*sizeof(PackedTerrainVertex),packed.size()*sizeof(PackedTerrainVertex),packed.data());
        std::vector<std::uint32_t> indices(actual.size());
        if(chunk->opaquePage->indexBytes) {
            glBindBuffer(GL_COPY_READ_BUFFER,chunk->opaquePage->ebo);
            if(chunk->opaquePage->indexBytes==2) {
                std::vector<std::uint16_t> narrow(actual.size());
                glGetBufferSubData(GL_COPY_READ_BUFFER,chunk->opaqueFirstVertex*4,narrow.size()*2,narrow.data());
                for(std::size_t i=0;i<indices.size();++i)indices[i]=narrow[i];
            } else glGetBufferSubData(GL_COPY_READ_BUFFER,chunk->opaqueFirstVertex*8,indices.size()*4,indices.data());
            glBindBuffer(GL_COPY_READ_BUFFER,0);
        } else for(std::size_t i=0;i<indices.size();++i)indices[i]=std::uint32_t(i);
        for(std::size_t i=0;i<actual.size();++i){require(indices[i]<packed.size(),"GPU terrain index range");actual[i]=TerrainPacking::unpack(packed[indices[i]]);}
        require(hashBytes(actual.data(),actual.size()*sizeof(VoxelVertex))==hashBytes(expected.opaque.data(),expected.opaque.size()*sizeof(VoxelVertex)),"Stale GPU attributes or upload order");
        require(glGetError()==GL_NO_ERROR,"Streaming OpenGL validation error");
        report="Streaming passed: worker plans, 8 parallel byte-identical meshes, negative coordinates, teleports, epoch/revision cancellation, bounded queues and exact GPU upload.";
        return true;
    } catch(const std::exception& error) {report=error.what();return false;}
}

bool World::runExplorationAudit(const std::string& directory,std::string& report,unsigned modernVersion) const {
    try {
        std::filesystem::create_directories(directory);
        std::ofstream routes(directory+"/routes.csv"),vegetation(directory+"/vegetation.csv"),caves(directory+"/entrances.csv");
        routes<<"seed,version,route,land_fraction,biome_types,transitions,min_height,max_height,biomes\n";
        vegetation<<"biome,version,mushrooms,decoration_blocks,columns\n";
        caves<<"seed,x,y,z,target_x,target_y,target_z,radius,sinkhole\n";
        // Flat synthetic habitats isolate spacing from changed terrain/biomes.
        for(auto biome:{Biome::Swamp,Biome::DarkForest,Biome::MushroomFields}) for(unsigned version:{modernVersion-1,modernVersion}) {
            std::uint64_t mushrooms=0,decorations=0;
            for(int cz=-4;cz<4;++cz)for(int cx=-4;cx<4;++cx) {
                std::vector<Block> blocks(16*16*128,Block::Air);
                for(int y=0;y<=70;++y)for(int i=0;i<256;++i) blocks[y*256+i]=y==70?biomeDefinition(biome).surface:Block::Dirt;
                decorateBiomeChunk(cx,cz,123456789,blocks,[&](int,int){return BiomeTerrain{70,biome,0};},
                    [](int,int){return false;},[](int,int){return true;},0,128,64,version);
                for(Block b:blocks) {mushrooms+=b==Block::RedMushroom||b==Block::BrownMushroom;decorations+=isPlant(b);}
            }
            vegetation<<biomeDefinition(biome).name<<','<<version<<','<<mushrooms<<','<<decorations<<",16384\n";
        }
        for(auto seed:{20260917U,123456789U,42U}) {
            World modern(seed,modernVersion);
            for(unsigned version:{modernVersion-1,modernVersion}) {
                World probe(seed,version);
                for(int route=0;route<64;++route) {
                    const int x=(route%8)*1600-5600,z=(route/8)*1600-5600;
                    std::set<Biome> biomes;int transitions=0,land=0,minY=320,maxY=-64;Biome previous=Biome::Count;
                    for(int distance=0;distance<=2000;distance+=8) {
                        const auto t=probe.sampleTerrain(x+distance,z+distance/3);
                        biomes.insert(t.biome); transitions+=previous!=Biome::Count&&previous!=t.biome;previous=t.biome;
                        land+=t.height>=64;minY=std::min(minY,t.height);maxY=std::max(maxY,t.height);
                    }
                    routes<<seed<<','<<version<<','<<route<<','<<land/251.f<<','<<biomes.size()<<','<<transitions<<','<<minY<<','<<maxY<<',';
                    for(auto biome:biomes) routes<<biomeDefinition(biome).name<<';';
                    routes<<'\n';
                }
            }
            const auto nativeCavity=[&](int x,int y,int z) {
                const auto t=modern.sampleTerrain(x,z);
                const float aquifer=modern.aquiferNoise_.fractal2D(x*.0018f+113.7f,z*.0018f-67.3f,2,2.f,.3f);
                const auto smooth=[](float a,float b,float v){const float p=std::clamp((v-a)/(b-a),0.f,1.f);return p*p*(3-2*p);};
                const TerrainDensity::Column c{t.height,smooth(95,180,float(t.height))*(1-smooth(.40f,.83f,t.erosion)),aquifer,-24+aquifer*30,t.ravine};
                const auto f=TerrainDensity::sampleAt(x,y,z,modern.ridgeNoise_,modern.cheeseNoise_,modern.spaghettiNoise_,modern.noodleNoise_,modern.caveNoise_);
                if(modernVersion<13) return TerrainDensity::density(f,c,y)<=0;
                const float slope=float(std::max({std::abs(modern.terrainHeight(x-4,z)-t.height),std::abs(modern.terrainHeight(x+4,z)-t.height),
                    std::abs(modern.terrainHeight(x,z-4)-t.height),std::abs(modern.terrainHeight(x,z+4)-t.height),
                    std::abs(modern.terrainHeight(x-1,z)-t.height),std::abs(modern.terrainHeight(x+1,z)-t.height),
                    std::abs(modern.terrainHeight(x,z-1)-t.height),std::abs(modern.terrainHeight(x,z+1)-t.height)}))*.25f;
                return TerrainAesthetics::density(f,c,y,slope)<=0;
            };
            std::set<std::pair<int,int>> found;
            std::map<std::pair<int,int>,GeneratedChunk> entranceChunks;
            const auto generatedBlock=[&](int x,int y,int z) {
                const int cx=floorDiv(x,16),cz=floorDiv(z,16);
                auto it=entranceChunks.find({cx,cz});
                if(it==entranceChunks.end()) it=entranceChunks.emplace(std::make_pair(cx,cz),modern.generateChunkData(cx,cz)).first;
                const auto index=std::size_t(y+64)*256+std::size_t(z-cz*16)*16+std::size_t(x-cx*16);
                return index<it->second.blocks.size()?it->second.blocks[index]:Block::Air;
            };
            for(int z=-768;z<=768;z+=96)for(int x=-768;x<=768;x+=96) {
                auto plans=TerrainExploration::caveEntrances(floorDiv(x,16),floorDiv(z,16),seed,[&](int wx,int wz){const auto t=modern.sampleTerrain(wx,wz);return TerrainExploration::SurfaceColumn{t.height,t.river,t.ravine};},nativeCavity,[&](int wx,int wz){return modern.explorationStructureReservedAt(wx,wz);},modernVersion);
                for(const auto& plan:plans) {
                    const int wx=int(std::floor(plan.mouth.x)),wz=int(std::floor(plan.mouth.z));
                    if(!found.insert({wx,wz}).second)continue;
                    require(nativeCavity(int(std::floor(plan.target.x)),int(plan.target.y)-1,int(std::floor(plan.target.z))),"Entrance misses native cave");
                    // Dense samples prove a continuous >=2-block bore along the
                    // two real carved segments, rather than a decorative hole.
                    for(auto segment:{std::pair<glm::vec3,glm::vec3>{plan.mouth,plan.bend},{plan.bend,plan.target}})
                    for(int i=0;i<=80;++i) {
                        const auto p=glm::mix(segment.first,segment.second,i/80.f);
                        require(TerrainExploration::carves(plan,int(std::floor(p.x)),int(std::floor(p.y)),int(std::floor(p.z))),"Disconnected entrance bore");
                        for(int dy=0;dy<2;++dy) require(generatedBlock(int(std::floor(p.x)),int(std::floor(p.y))+dy,int(std::floor(p.z)))==Block::Air,"Actual generated entrance obstructed or flooded");
                    }
                    caves<<seed<<','<<wx<<','<<plan.mouth.y<<','<<wz<<','<<plan.target.x<<','<<plan.target.y<<','<<plan.target.z<<','<<plan.radius<<','<<plan.sinkhole<<'\n';
                }
            }
            require(!found.empty(),"No discoverable cave entrances near explored area");
            std::cout<<"V"<<modernVersion<<" "<<seed<<" entrance plans in sampled 1.5km area: "<<found.size()<<'\n';
            // Camera altitude diagnostics do not affect surface climate.
            for(int z=-128;z<=128;z+=16)for(int x=-128;x<=128;x+=16) {
                const auto debug=modern.generationDebugAt(x,z,310);
                const auto climate=modern.biomeClimateAt(x,z);
                require(debug.surfaceTemperature==climate.effectiveTemperature,"Surface/camera climate confusion");
            }
            // Sparse lattice queries match the existing cached interpolation,
            // including negative coordinates and adjacent chunk halos.
            TerrainDensity::Grid grid(-1,-1,modern.ridgeNoise_,modern.cheeseNoise_,modern.spaghettiNoise_,modern.noodleNoise_,modern.caveNoise_);
            for(int y=-64;y<320;y+=3)for(int z=-16;z<0;z+=3)for(int x=-16;x<0;x+=3) {
                const auto a=grid.at(x,y,z),b=TerrainDensity::sampleAt(x,y,z,modern.ridgeNoise_,modern.cheeseNoise_,modern.spaghettiNoise_,modern.noodleNoise_,modern.caveNoise_);
                require(std::memcmp(&a,&b,sizeof(a))==0,"Sparse density interpolation mismatch");
            }
            std::string streaming;
            if(!modern.runStreamingSelfTest(streaming)) throw std::runtime_error(streaming);
            std::cout<<streaming<<'\n';
        }
        // An optional untouched save from the released binary checks real
        // backwards compatibility in addition to the synthetic roundtrips.
        const auto releasedSave=directory+"/released-v2111.vxw";
        if(std::filesystem::exists(releasedSave)) {
            std::ifstream header(releasedSave,std::ios::binary);
            header.seekg(12);std::uint32_t seed=0;header.read(reinterpret_cast<char*>(&seed),sizeof(seed));
            require(bool(header),"Released save header");
            World loaded(seed,12);glm::vec3 position;
            require(loaded.loadWorld(releasedSave,position)&&loaded.generationVersion()==11,"Released V11 save load/version");
            const auto reference=loaded.generateChunkData(-1,0);
            require(loaded.saveWorld(directory+"/released-resaved.vxw",position),"Released save resave");
            World reloaded(seed,12);glm::vec3 restored;
            require(reloaded.loadWorld(directory+"/released-resaved.vxw",restored)&&reloaded.generationVersion()==11&&position==restored,"Released save roundtrip");
            const auto actual=reloaded.generateChunkData(-1,0);
            require(reference.blocks==actual.blocks,"Released save geography changed");
            std::cout<<"Actual released v2.11.1 save loaded and resaved with generator 11 preserved.\n";
        }
        if(!runDensityAudit(directory,report,modernVersion)) throw std::runtime_error(report);
        report+=" Exploration route/vegetation metrics, connected entrance plans, surface climate and streaming regressions passed.";
        return true;
    } catch(const std::exception& error) {report=error.what();return false;}
}
