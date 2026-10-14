#include "World.h"
#include "Weather.h"
#include "RiverNetworkV22.h"
#include "TerrainDensity.h"
#include "Player.h"
#include "Persistence.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>

namespace {
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
struct Pixel {std::uint8_t biome,reserved;std::int16_t height;float temperature,humidity,continent,river,macroTemperature,macroHumidity;};
static_assert(sizeof(Pixel)==28,"Geographic map layout");
std::uint64_t hashBlocks(const std::vector<Block>& blocks) {
    std::uint64_t h=14695981039346656037ULL;
    for(auto b:blocks){h^=std::uint8_t(b);h*=1099511628211ULL;}return h;
}
double elapsed(std::chrono::steady_clock::time_point start){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
}
bool World::runDiversityAudit(const std::string& directory,std::string& report,bool spawnOnly) const {
    try {
        namespace fs=std::filesystem;fs::create_directories(directory);
        std::ofstream biomes(directory+"/biomes.csv"),generation(directory+"/generation.csv"),routes(directory+"/routes.csv"),
            entrances(directory+"/entrances.csv"),spawns(directory+"/spawns.csv"),rivers(directory+"/rivers.csv"),sites(directory+"/sites.csv");
        biomes<<"seed,version,extent,biome,samples\n";
        generation<<"seed,version,x,z,repeat,hash,ms,bytes,vertices\n";
        routes<<"seed,version,route,length,land_fraction,biome_types,transitions,longest_biome_run,min_height,max_height\n";
        entrances<<"seed,version,x,y,z,target_x,target_y,target_z,radius\n";
        spawns<<"seed,version,x,y,z,ms,coarse_reachable_biomes,nearest_biome_path,nearest_cave_distance,land_samples,longest_spawn_biome_ray\n";
        rivers<<"seed,x,z,end_x,end_z,from_y,to_y,width,incoming\n";
        std::ofstream nativeRivers(directory+"/native-rivers.csv");
        nativeRivers<<"seed,x,z,water_y,actual_width\n";
        sites<<"seed,biome,x,z,height\n";
        std::array<std::size_t,35> coverage{};
        if(!spawnOnly) {for(unsigned seed:{20260917U,123456789U,42U}) {
            for(unsigned version:{9U,18U,21U,22U}) {
                World probe(seed,version),climate(seed,21);
                for(int extent:{4096,15360,46080}) {
                    std::ofstream file(directory+"/map-"+std::to_string(seed)+"-v"+std::to_string(version)+"-"+std::to_string(extent)+".bin",std::ios::binary);
                    std::array<std::size_t,35> counts{};
                    for(int z=0;z<512;++z)for(int x=0;x<512;++x) {
                        const int wx=-extent/2+x*extent/512,wz=-extent/2+z*extent/512;
                        const auto t=probe.sampleTerrain(wx,wz);++counts[int(t.biome)];
                        if(version==22) {
                            if(!coverage[int(t.biome)]++)sites<<seed<<','<<biomeDefinition(t.biome).name<<','<<wx<<','<<wz<<','<<t.height<<'\n';
                            require(t.continentalness==t.naturalContinentalness,"V22 retains artificial starter mask");
                            require(t.height<=255&&t.height>=-64,"Natural height range");
                            if(x%32==0&&z%32==0) {
                                const auto old=climate.sampleTerrain(wx,wz);
                                require(old.temperature==t.temperature&&old.humidity==t.humidity&&old.macroTemperature==t.macroTemperature&&old.macroHumidity==t.macroHumidity,"V22 changed climate fields");
                                const auto next=probe.sampleTerrain(wx+1,wz);
                                require(std::abs(next.continentalness-t.continentalness)<.01f&&std::abs(next.height-t.height)<=12,"Terrain continuity");
                                const auto c=probe.biomeClimateAt(wx,wz);
                                Weather weather(seed);
                                require(weather.precipitation(probe,wx,wz)==(c.dry?Precipitation::None:c.freezes?Precipitation::Snow:Precipitation::Rain),"Climate/weather disagreement");
                            }
                        }
                        const Pixel p{std::uint8_t(t.biome),0,std::int16_t(t.height),t.temperature,t.humidity,t.continentalness,t.river,t.macroTemperature,t.macroHumidity};
                        file.write(reinterpret_cast<const char*>(&p),sizeof(p));
                    }
                    require(bool(file),"Map output failed");
                    for(int b=0;b<35;++b)biomes<<seed<<','<<version<<','<<extent<<','<<biomeDefinition(Biome(b)).name<<','<<counts[b]<<'\n';
                    std::cout<<"Map "<<seed<<" V"<<version<<" "<<extent<<std::endl;
                }
            }
            for(unsigned version:{21U,22U}) {
                World probe(seed,version);
                for(int route=0;route<16;++route) {
                    std::set<Biome> found;int land=0,transitions=0,run=0,longest=0,minY=320,maxY=-64;Biome last=Biome::Count;
                    for(int step=0;step<=1500;step+=4) {
                        const int x=(route%4)*600-900+(route%2?step:0),z=(route/4)*600-900+(route%2?0:step);
                        const auto t=probe.sampleTerrain(x,z);
                        if(t.height>=64)++land;
                        found.insert(t.biome);minY=std::min(minY,t.height);maxY=std::max(maxY,t.height);
                        if(t.biome!=last){transitions+=last!=Biome::Count;run=0;last=t.biome;}
                        run+=4;longest=std::max(longest,run);
                    }
                    routes<<seed<<','<<version<<','<<route<<",1500,"<<land/376.f<<','<<found.size()<<','<<transitions<<','<<longest<<','<<minY<<','<<maxY<<'\n';
                }
                for(int i=0;i<32;++i)for(int repeat=0;repeat<3;++repeat) {
                    const int cx=(i%8)*173-606,cz=(i/8)*197-295;
                    const auto start=std::chrono::steady_clock::now();const auto data=probe.generateChunkData(cx,cz);const auto ms=elapsed(start);
                    // Generation metrics below remain independent of meshing;
                    // actual geometry totals come from foreground render fixtures.
                    generation<<seed<<','<<version<<','<<cx<<','<<cz<<','<<repeat<<','<<hashBlocks(data.blocks)<<','<<ms<<','<<data.blocks.size()<<",0\n";
                    if(repeat==2)require(data.blocks==probe.generateChunkData(cx,cz).blocks,"Chunk determinism");
                    for(std::size_t index=321U*256U;index<data.blocks.size();++index)require(data.blocks[index]==Block::Air,"Natural content above Y256");
                }
                std::set<std::pair<int,int>> mouths;
                for(int z=-768;z<=768;z+=80)for(int x=-768;x<=768;x+=80) {
                    for(const auto& p:probe.diversityEntrances(floorDiv(x,16),floorDiv(z,16))) {
                        const int wx=int(p.mouth.x),wz=int(p.mouth.z);
                        if(!mouths.emplace(wx,wz).second)continue;
                        require(probe.nativeCavityAt(int(std::floor(p.target.x)),int(p.target.y)-1,int(std::floor(p.target.z))),"Entrance misses native cave");
                        entrances<<seed<<','<<version<<','<<p.mouth.x<<','<<p.mouth.y<<','<<p.mouth.z<<','<<p.target.x<<','<<p.target.y<<','<<p.target.z<<','<<p.radius<<'\n';
                        if(version==22) {
                            require(!p.sinkhole&&(p.mouth.y-p.target.y)/glm::length(glm::vec2(p.mouth.x-p.target.x,p.mouth.z-p.target.z))<.58f,"Unsafe entrance grade");
                            std::map<std::pair<int,int>,GeneratedChunk> chunks;
                            for(auto segment:{std::pair<glm::vec3,glm::vec3>{p.mouth,p.bend},{p.bend,p.target}})for(int step=0;step<=80;++step) {
                                const auto point=glm::mix(segment.first,segment.second,step/80.f);
                                const int bx=int(std::floor(point.x)),by=int(std::floor(point.y)),bz=int(std::floor(point.z));
                                const auto key=std::make_pair(floorDiv(bx,16),floorDiv(bz,16));
                                auto it=chunks.find(key);if(it==chunks.end())it=chunks.emplace(key,probe.generateChunkData(key.first,key.second)).first;
                                for(int dy=0;dy<2;++dy){const auto id=probe.localIndex(floorMod(bx,16),by+dy,floorMod(bz,16));
                                    if(id<it->second.blocks.size()&&it->second.blocks[id]!=Block::Air) {
                                        std::ofstream failure(directory+"/bore-failure.csv");
                                        failure<<"x,y,z,block,mouth_x,mouth_y,mouth_z,target_x,target_y,target_z,carves,reserved\n"<<bx<<','<<by+dy<<','<<bz<<','<<int(it->second.blocks[id])<<','<<p.mouth.x<<','<<p.mouth.y<<','<<p.mouth.z<<','<<p.target.x<<','<<p.target.y<<','<<p.target.z<<','<<TerrainExploration::carves(p,bx,by+dy,bz)<<','<<probe.explorationStructureReservedAt(bx,bz)<<'\n';
                                        throw std::runtime_error("Generated bore obstructed or flooded");
                                    }
                                }
                            }
                        }
                    }
                }
                std::cout<<"Entrances "<<seed<<" V"<<version<<": "<<mouths.size()<<std::endl;
            }
            World probe(seed,22);
            int waterChecks=0;
            for(int z=-3072;z<=3072;z+=192)for(int x=-3072;x<=3072;x+=192) {
                for(const auto& r:RiverNetworkV22::inspectCell(x,z,seed,[&](int rx,int rz){return RiverNetwork::Ground{probe.drainageContinent(rx,rz)};})) {
                    require(r.from>=r.to&&r.to>=64&&r.halfWidth>=1&&r.halfWidth<=12,"River grade/width");
                    rivers<<seed<<','<<r.p[0].x<<','<<r.p[0].z<<','<<r.p[8].x<<','<<r.p[8].z<<','<<r.from<<','<<r.to<<','<<r.halfWidth*2<<','<<r.incoming<<'\n';
                    if(waterChecks<48) {
                        const auto midpoint=r.p[4];
                        const float dx=r.p[5].x-r.p[3].x,dz=r.p[5].z-r.p[3].z,len=std::hypot(dx,dz);
                        const int wx=int(std::floor(midpoint.x)),wz=int(std::floor(midpoint.z));
                        const auto column=probe.sampleTerrain(wx,wz);
                        if(column.river>.95f && column.height<column.riverWaterY && !probe.explorationStructureReservedAt(wx,wz)) {
                            std::map<std::pair<int,int>,GeneratedChunk> chunks;int actualWidth=0;
                            for(int offset=-16;offset<=16;++offset) {
                                const int px=int(std::floor(midpoint.x-dz/len*offset)),pz=int(std::floor(midpoint.z+dx/len*offset));
                                const auto key=std::make_pair(floorDiv(px,16),floorDiv(pz,16));
                                auto it=chunks.find(key);if(it==chunks.end())it=chunks.emplace(key,probe.generateChunkData(key.first,key.second)).first;
                                const auto t=probe.sampleTerrain(px,pz);const int top=std::max(64,t.riverWaterY);
                                const auto index=probe.localIndex(floorMod(px,16),top,floorMod(pz,16));
                                if(index<it->second.blocks.size()&&(isWater(it->second.blocks[index])||it->second.blocks[index]==Block::Ice)) {
                                    ++actualWidth;
                                    if(t.river>.95f) {const auto bed=probe.localIndex(floorMod(px,16),t.height,floorMod(pz,16));require(bed<it->second.blocks.size()&&isSolid(it->second.blocks[bed]),"Floating river water without bed");}
                                }
                            }
                            require(actualWidth>0,"Native river missing water");
                            nativeRivers<<seed<<','<<wx<<','<<wz<<','<<column.riverWaterY<<','<<actualWidth<<'\n';++waterChecks;
                        }
                    }
                }
            }
            std::array<std::future<std::vector<Block>>,4> futures;
            for(int i=0;i<4;++i)futures[i]=std::async(std::launch::async,[&probe,i]{return probe.generateChunkData(-3+i,-1).blocks;});
            for(int i=3;i>=0;--i)require(futures[i].get()==probe.generateChunkData(-3+i,-1).blocks,"Parallel/chunk-border determinism");
        }
        for(auto count:coverage)require(count>0,"Missing biome in V22 survey");}
        // Fifty deterministic seeds, paired V21/V22. Coarse reachable distances
        // are terrain-grid estimates, not a claim of voxel-perfect pathfinding.
        for(unsigned n=0;n<50;++n) {
            const unsigned seed=n<3?std::array<unsigned,3>{20260917U,123456789U,42U}[n]:TerrainDensity::hash(int(n),0,0,0x22f1U);
            for(unsigned version:{21U,22U}) {
                World probe(seed,version);const auto start=std::chrono::steady_clock::now();
                const auto spawn=version==22?probe.findSurvivalSpawnNear(0,0):probe.findSafeSpawnNear(0,0);const double ms=elapsed(start);
                Player respawned({10000,200,-10000});respawned.respawn(probe);
                require(respawned.position()==spawn,"Respawn changed selected world spawn");
                const int sx=int(std::floor(spawn.x)),sz=int(std::floor(spawn.z));
                const auto data=probe.generateChunkData(floorDiv(sx,16),floorDiv(sz,16));
                const int sy=int(std::floor(spawn.y));
                const auto at=[&](int y){const auto index=probe.localIndex(floorMod(sx,16),y,floorMod(sz,16));return index<data.blocks.size()?data.blocks[index]:Block::Air;};
                require(isSolid(at(sy-1))&&!isWater(at(sy))&&!isLava(at(sy))&&!isSolid(at(sy+1)),"Spawn collision safety");
                const auto initial=probe.sampleTerrain(sx,sz).biome;
                constexpr int side=33,step=64;std::array<int,side*side> distance;distance.fill(-1);std::array<TerrainSample,side*side> columns;
                int land=0;for(int z=0;z<side;++z)for(int x=0;x<side;++x){auto& t=columns[z*side+x];t=probe.sampleTerrain(sx+(x-16)*step,sz+(z-16)*step);land+=t.height>=68;}
                std::queue<int> queue;queue.push(16*side+16);distance[16*side+16]=0;std::set<Biome> reachable;int nearestBiome=9999;
                while(!queue.empty()) {
                    const int id=queue.front();queue.pop();const auto& t=columns[id];reachable.insert(t.biome);
                    if(t.biome!=initial)nearestBiome=std::min(nearestBiome,distance[id]);
                    for(auto d:{glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)}) {
                        const int x=id%side+d.x,z=id/side+d.y;if(x<0||x>=side||z<0||z>=side)continue;
                        const int next=z*side+x;const auto& b=columns[next];
                        if(distance[next]>=0||b.height<68||b.river>.6f||std::abs(b.height-t.height)>24)continue;
                        distance[next]=distance[id]+step;queue.push(next);
                    }
                }
                float nearest=9999;
                for(int z=-640;z<=640;z+=160)for(int x=-640;x<=640;x+=160)
                    for(const auto& p:probe.diversityEntrances(floorDiv(sx+x,16),floorDiv(sz+z,16)))nearest=std::min(nearest,std::hypot(p.mouth.x-spawn.x,p.mouth.z-spawn.z));
                int longest=0;
                for(auto d:{glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)}) {
                    int run=0;for(int length=0;length<=1500;length+=4){const auto t=probe.sampleTerrain(sx+d.x*length,sz+d.y*length);if(t.biome!=initial||t.height<64)break;run=length;}
                    longest=std::max(longest,run);
                }
                spawns<<seed<<','<<version<<','<<spawn.x<<','<<spawn.y<<','<<spawn.z<<','<<ms<<','<<reachable.size()<<','<<nearestBiome<<','<<nearest<<','<<land<<','<<longest<<std::endl;
                std::cout<<"Spawn "<<n+1<<"/50 V"<<version<<" "<<ms<<"ms"<<std::endl;
                if(n<3 && Persistence::enabled()) {
                    const auto path=directory+"/save-"+std::to_string(seed)+"-"+std::to_string(version)+".vxw";
                    require(probe.saveWorld(path,spawn),"V22 save failed");World loaded(seed);glm::vec3 restored;
                    require(loaded.loadWorld(path,restored)&&loaded.generationVersion()==int(version)&&restored==spawn,"Version/spawn persistence");
                    if(version==22)require(loaded.findSurvivalSpawnNear(0,0)==spawn,"Reload changed natural world spawn");
                    require(loaded.generateChunkData(-2,3).blocks==probe.generateChunkData(-2,3).blocks,"Saved unexplored geography changed");
                }
            }
        }
        // Walk real Survival physics down native bores, without teleporting or
        // flying. Surface maps/air samples alone cannot prove traversability.
        std::ofstream walks(directory+"/entrance-walks.csv");
        walks<<"seed,x,z,fps,frames,reached,health,max_fall_damage,jumps,end_x,end_y,end_z\n";
        World walkingWorld(20260917U,22);int walked=0;
        std::set<std::pair<int,int>> tested;
        for(int z=-768;z<=768 && walked<12;z+=80)for(int x=-768;x<=768 && walked<12;x+=80) {
            for(const auto& cave:walkingWorld.diversityEntrances(floorDiv(x,16),floorDiv(z,16))) {
                if(walked>=12 || !tested.emplace(int(cave.mouth.x),int(cave.mouth.z)).second)continue;
                for(int fps:{30,60}) {
                    walkingWorld.generate(2,cave.mouth);
                    Player player(cave.mouth-glm::vec3(0,.9f,0));player.setMouseSensitivity(1.f);
                    PlayerInput input;input.enabled=true;input.moveForward=true;
                    int frame=0,jumps=0;bool reached=false;float maximumDamage=0;
                    for(;frame<fps*24 && !player.isDead();++frame) {
                        const auto p=player.position();
                        const glm::vec2 delta(cave.target.x-p.x,cave.target.z-p.z);
                        if(glm::length(delta)<3.f){reached=true;break;}
                        walkingWorld.prepareSpawnTerrain(p);
                        walkingWorld.prepareSpawnTerrain(p+glm::vec3(glm::normalize(delta).x*3.f,0,glm::normalize(delta).y*3.f));
                        const auto ahead=p+glm::vec3(glm::normalize(delta).x*.7f,0,glm::normalize(delta).y*.7f);
                        input.jump=player.isGrounded()&&walkingWorld.aabbIntersectsSolid(ahead+glm::vec3(-.3f,.02f,-.3f),ahead+glm::vec3(.3f,1.8f,.3f));
                        jumps+=input.jump;
                        const auto look=player.lookDirection();const float yaw=std::atan2(look.z,look.x),wanted=std::atan2(delta.y,delta.x);
                        float turn=wanted-yaw;while(turn>3.14159265f)turn-=6.2831853f;while(turn< -3.14159265f)turn+=6.2831853f;
                        player.addMouseMovement(glm::degrees(turn),0);
                        player.update(1.f/fps,input,walkingWorld);
                        maximumDamage=std::max(maximumDamage,player.lastFallDamage());
                    }
                    const auto end=player.position();
                    walks<<20260917U<<','<<cave.mouth.x<<','<<cave.mouth.z<<','<<fps<<','<<frame<<','<<reached<<','<<player.health()<<','<<maximumDamage<<','<<jumps<<','<<end.x<<','<<end.y<<','<<end.z<<std::endl;
                    require(reached && maximumDamage<=.5f,"Survival entrance walk blocked or unsafe");
                }
                ++walked;
            }
        }
        require(walked==12,"Insufficient real entrance walks");
        if(!spawnOnly)writeOptimizationChunkHashes(directory+"/historical-hashes.csv");
        report=spawnOnly?"50 paired V21/V22 spawn safety, respawn and persistence checks passed":
            "V22 geography maps, 35 biomes, 50 paired deterministic spawns, cave bores, climate, heights, concurrency and saves passed";
        return true;
    }catch(const std::exception& e){report=e.what();return false;}
}
