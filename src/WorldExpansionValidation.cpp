#include "World.h"
#include "TerrainExpansion.h"
#include "Persistence.h"
#include "Weather.h"
#include "Player.h"
#include "BiomeVegetation.h"
#include "GenerationMetrics.h"
#include "ExplorationLoot.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <future>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>

namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
std::uint64_t blockHash(const std::vector<Block>& blocks) {
    std::uint64_t h=14695981039346656037ULL;
    for(auto block:blocks)for(unsigned shift=0;shift<32;shift+=8) {
        h^=(std::uint32_t(block)>>shift)&255U;h*=1099511628211ULL;
    }return h;
}
struct Pixel {std::uint8_t biome,flags;std::int16_t height;float temperature,humidity,continent,river,erosion,ravine;};
static_assert(sizeof(Pixel)==28,"V23 map schema");
}

bool World::runExpansionAudit(const std::string& directory,std::string& report,const std::string& mode) const {
    try {
        namespace fs=std::filesystem;fs::create_directories(directory);
        unsigned checks=0;
        const auto check=[&](bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);};
        if(mode=="freeze") {
            std::ofstream index(directory+"/chunks.csv"),stages(directory+"/stages.csv");
            index<<"seed,version,x,z,repeat,ms,hash,block_bytes\n";
            stages<<"seed,x,z,repeat,columns,plans,entrances,lattice,density,materials,vegetation,structures,shore,height_queries,height_hits\n";
            for(unsigned seed:{20260917U,123456789U,42U}) {
                auto w=std::make_unique<World>(seed,23);
                std::vector<glm::ivec2> coords;
                for(int i=0;i<32;++i)coords.push_back({(i%8)*173-606,(i/8)*197-295});
                for(int z=-2;z<=2;++z)for(int x=-2;x<=2;++x)coords.push_back({x,z});
                // Native landmark, mountain, lake and entrance districts from V23 validation.
                for(auto p:{glm::ivec2(981,-4054),glm::ivec2(2609,-3889),glm::ivec2(2888,-3907),
                    glm::ivec2(2615,-3775),glm::ivec2(2511,-2650),glm::ivec2(-3008,-3776),
                    glm::ivec2(-3788,-3641),glm::ivec2(-768,-4032),glm::ivec2(-4032,-4032),
                    glm::ivec2(3264,-3648),glm::ivec2(1290,-690),glm::ivec2(-701,-1220)})
                    for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x)coords.push_back({floorDiv(p.x,16)+x,floorDiv(p.y,16)+z});
                std::map<std::pair<int,int>,std::vector<Block>> expected;
                for(int repeat=0;repeat<2;++repeat)for(auto c:coords) {
                    GenerationMetrics::Sample sample;GenerationMetrics::Capture capture(sample);
                    const auto start=Clock::now();auto data=w->generateChunkData(c.x,c.y);const auto elapsed=ms(start);
                    index<<seed<<",23,"<<c.x<<','<<c.y<<','<<repeat<<','<<elapsed<<','<<blockHash(data.blocks)<<','<<data.blocks.size()*sizeof(Block)<<'\n';
                    stages<<seed<<','<<c.x<<','<<c.y<<','<<repeat;
                    for(auto t:sample.milliseconds)stages<<','<<t;
                    stages<<','<<sample.heightQueries<<','<<sample.heightHits<<'\n';
                    if(repeat)check(data.blocks==expected.at({c.x,c.y}),"Warm freeze bytes changed");
                    else expected[{c.x,c.y}]=data.blocks;
                    const auto path=directory+"/"+std::to_string(seed)+"-"+std::to_string(c.x)+"-"+std::to_string(c.y)+".bin";
                    std::ostringstream out(std::ios::binary);
                    const auto put=[&](const auto& value){out.write(reinterpret_cast<const char*>(&value),sizeof(value));};
                    put(seed);put(c.x);put(c.y);put(std::uint32_t(data.blocks.size()));put(std::uint8_t(data.hasStructure));
                    out.write(reinterpret_cast<const char*>(data.blocks.data()),data.blocks.size()*sizeof(Block));
                    put(std::uint32_t(data.loot.size()));
                    for(const auto& m:data.loot){put(m.x);put(m.y);put(m.z);put(std::uint8_t(m.table));put(m.salt);
                        auto loot=rollExplorationLoot(seed,m);addEquipmentLoot(loot,seed,m);
                        for(const auto& s:loot){put(std::uint16_t(s.item));put(s.count);put(s.durability);put(s.identity);}}
                    put(std::uint32_t(data.mobs.size()));
                    for(const auto& m:data.mobs){put(m.x);put(m.y);put(m.z);put(std::uint8_t(m.hostile));}
                    check(bool(out),"Frozen chunk encoding failed");
                    const auto bytes=out.str();
                    if(repeat){std::ifstream file(path,std::ios::binary);const std::string cold{std::istreambuf_iterator<char>(file),{}};
                        check(bytes==cold,"Warm chunk/loot/encounter bytes changed");}
                    else {std::ofstream file(path,std::ios::binary);file.write(bytes.data(),std::streamsize(bytes.size()));check(bool(file),"Frozen chunk export failed");}
                }
            }
        } else if(mode=="maps") {
            std::ofstream counts(directory+"/biomes.csv"),sites(directory+"/sites.csv"),structures(directory+"/structures.csv"),caves(directory+"/entrances.csv");
            counts<<"seed,version,extent,biome,samples\n";
            sites<<"seed,version,kind,x,y,z\n";
            structures<<"seed,version,kind,x,y,z,min_x,max_x,min_z,max_z\n";
            caves<<"seed,version,x,y,z,target_x,target_y,target_z,radius\n";
            std::array<std::size_t,35> coverage{};
            for(unsigned seed:{20260917U,123456789U,42U})for(unsigned version:{22U,23U}) {
                auto w=std::make_unique<World>(seed,version);
                for(int extent:{4096,15360,46080}) {
                    std::ofstream file(directory+"/map-"+std::to_string(seed)+"-v"+std::to_string(version)+"-"+std::to_string(extent)+".bin",std::ios::binary);
                    std::array<std::size_t,35> found{};
                    int peak=-64,peakX=0,peakZ=0;std::set<Biome> located;
                    for(int z=0;z<512;++z)for(int x=0;x<512;++x) {
                        const int wx=-extent/2+x*extent/512,wz=-extent/2+z*extent/512;
                        const auto t=w->sampleTerrain(wx,wz);
                        check(int(t.biome)<35 && t.height<=255 && t.height>=-48,"Map biome/height bounds");
                        ++found[int(t.biome)];if(version==23)++coverage[int(t.biome)];
                        if(t.height>peak){peak=t.height;peakX=wx;peakZ=wz;}
                        if(located.insert(t.biome).second)sites<<seed<<','<<version<<','<<biomeDefinition(t.biome).name<<','<<wx<<','<<t.height<<','<<wz<<'\n';
                        const Pixel p{std::uint8_t(t.biome),0,std::int16_t(t.height),t.temperature,t.humidity,t.continentalness,t.river,t.erosion,t.ravine};
                        file.write(reinterpret_cast<const char*>(&p),sizeof(p));
                    }
                    sites<<seed<<','<<version<<",peak-"<<extent<<','<<peakX<<','<<peak<<','<<peakZ<<'\n';
                    for(int i=0;i<35;++i)counts<<seed<<','<<version<<','<<extent<<','<<biomeDefinition(Biome(i)).name<<','<<found[i]<<'\n';
                    check(bool(file),"Map output failed");
                    std::cout<<"V"<<version<<" seed "<<seed<<" extent "<<extent<<" peak "<<peak<<std::endl;
                }
                const auto terrain=[&](int x,int z){return w->structureTerrainAt(x,z);};
                for(int rz=-32;rz<32;++rz)for(int rx=-32;rx<32;++rx) {
                    const auto plan=w->structures_.smallPlanForRegion(rx,rz,terrain);
                    for(const auto& piece:plan.pieces) {
                        structures<<seed<<','<<version<<','<<int(piece.kind)<<','<<piece.x<<','<<piece.y<<','<<piece.z<<','<<piece.bounds.minX<<','<<piece.bounds.maxX<<','<<piece.bounds.minZ<<','<<piece.bounds.maxZ<<'\n';
                        check(canPlaceStructure(piece.bounds,3,terrain),"Accepted landmark support/slope invalid");
                    }
                }
                std::set<std::pair<int,int>> mouths;
                for(int z=-1536;z<=1536;z+=96)for(int x=-1536;x<=1536;x+=96)
                    for(const auto& c:w->diversityEntrances(floorDiv(x,16),floorDiv(z,16)))
                        if(mouths.emplace(int(c.mouth.x),int(c.mouth.z)).second) {
                            caves<<seed<<','<<version<<','<<c.mouth.x<<','<<c.mouth.y<<','<<c.mouth.z<<','<<c.target.x<<','<<c.target.y<<','<<c.target.z<<','<<c.radius<<'\n';
                            const auto target=glm::ivec3(glm::floor(c.target));
                            check(w->nativeCavityAt(target.x,target.y-1,target.z),"Entrance misses native cave");
                        }
            }
            for(auto n:coverage)check(n>0,"V23 biome missing from multi-seed maps");
        } else if(mode=="spawns") {
            std::ofstream out(directory+"/spawns.csv");
            out<<"seed,version,x,y,z,ms,biomes_1200,nearest_cave,land_samples,longest_biome_ray\n";
            for(int i=0;i<50;++i)for(unsigned version:{22U,23U}) {
                const unsigned seed=i<3?std::array<unsigned,3>{20260917U,123456789U,42U}[i]:
                    TerrainDensity::hash(i,23,-i,0x23a17U);
                auto w=std::make_unique<World>(seed,version);const auto begin=Clock::now();
                const auto p=w->findSurvivalSpawnNear(0,0);const double cost=ms(begin);
                const auto repeat=w->findSurvivalSpawnNear(0,0);check(p==repeat,"Spawn not repeatable");
                std::map<std::int64_t,GeneratedChunk> generated;
                const auto block=[&](int x,int y,int z) {
                    const int cx=floorDiv(x,16),cz=floorDiv(z,16);const auto k=chunkKey(cx,cz);
                    auto it=generated.find(k);if(it==generated.end())it=generated.emplace(k,w->generateChunkData(cx,cz)).first;
                    const auto idx=w->localIndex(floorMod(x,16),y,floorMod(z,16));
                    return idx<it->second.blocks.size()?it->second.blocks[idx]:Block::Air;
                };
                const int px=int(std::floor(p.x)),py=int(std::floor(p.y)),pz=int(std::floor(p.z));
                check(isSolid(block(px,py-1,pz)) && !isWater(block(px,py,pz)) && !isSolid(block(px,py,pz)) &&
                    !isSolid(block(px,py+1,pz)),"Survival spawn lacks safe body/floor");
                std::set<Biome> biomes;int land=0,longest=0;
                for(int dz=-1200;dz<=1200;dz+=150)for(int dx=-1200;dx<=1200;dx+=150) {
                    const auto t=w->sampleTerrain(px+dx,pz+dz);if(t.height>66){++land;biomes.insert(t.biome);}
                }
                for(auto d:{glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)}) {
                    const auto b=w->sampleTerrain(px,pz).biome;int distance=0;
                    for(;distance<=1500;distance+=16)if(w->sampleTerrain(px+d.x*distance,pz+d.y*distance).biome!=b)break;
                    longest=std::max(longest,distance);
                }
                float nearest=2000;
                for(int dz=-600;dz<=600;dz+=120)for(int dx=-600;dx<=600;dx+=120)
                    for(const auto& c:w->diversityEntrances(floorDiv(px+dx,16),floorDiv(pz+dz,16)))
                        nearest=std::min(nearest,std::hypot(c.mouth.x-p.x,c.mouth.z-p.z));
                out<<seed<<','<<version<<','<<p.x<<','<<p.y<<','<<p.z<<','<<cost<<','<<biomes.size()<<','<<nearest<<','<<land<<','<<longest<<std::endl;
                if(Persistence::enabled()) {
                    const auto save=directory+"/spawn-roundtrip.vxw";
                    check(w->saveWorld(save,p),"Spawn save failed");
                    auto loaded=std::make_unique<World>(seed);glm::vec3 restored;
                    check(loaded->loadWorld(save,restored)&&loaded->generationVersion()==version&&restored==p,"Saved world generator changed");
                    check(loaded->generateChunkData(-19,27).blocks==w->generateChunkData(-19,27).blocks,"Unexplored saved terrain changed");
                }
                std::cout<<"Spawn "<<i<<" V"<<version<<" safe; cave "<<nearest<<std::endl;
            }
        } else if(mode=="ecosystems") {
            std::ofstream caves(directory+"/caves.csv"),landmarks(directory+"/landmarks.csv"),entrances(directory+"/entrances.csv"),walks(directory+"/walks.csv"),nativeSites(directory+"/native-sites.csv");
            caves<<"seed,version,x,z,air,water,largest_component,components,largest_touches_boundary\n";
            landmarks<<"seed,kind,x,y,z,chests,cells,rotation,mirror\n";
            entrances<<"seed,x,y,z,target_x,target_y,target_z,radius\n";
            walks<<"seed,x,z,fps,reached,health,maximum_fall_damage,frames,end_x,end_y,end_z\n";
            nativeSites<<"seed,version,kind,x,y,z,height\n";
            for(unsigned seed:{20260917U,123456789U,42U})for(unsigned version:{22U,23U}) {
                auto w=std::make_unique<World>(seed,version);
                glm::ivec2 center{};std::size_t most=0;
                for(int i=0;i<32;++i) {
                    const int cx=(i%8)*173-606,cz=(i/8)*197-295;
                    const auto data=w->generateChunkData(cx,cz);std::size_t n=0;
                    for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
                        const int top=std::min(95,w->terrainHeight(cx*16+x,cz*16+z)-8);
                        for(int y=-48;y<=top;++y)if(data.blocks[w->localIndex(x,y,z)]==Block::Air)++n;
                    }
                    if(n>most){most=n;center={cx,cz};}
                }
                constexpr int span=48,levels=144,bottom=-48;
                const auto index=[](int x,int y,int z){return (y*span+z)*span+x;};
                std::vector<std::uint8_t> volume(span*span*levels);
                std::size_t air=0,water=0;
                for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
                    const auto data=w->generateChunkData(center.x+dx,center.y+dz);
                    for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
                        const int top=w->terrainHeight((center.x+dx)*16+x,(center.y+dz)*16+z)-8;
                        for(int y=0;y<levels && bottom+y<=top;++y) {
                            const auto idx=w->localIndex(x,bottom+y,z);
                            const auto b=idx<data.blocks.size()?data.blocks[idx]:Block::Air;
                            if(b==Block::Air){volume[index((dx+1)*16+x,y,(dz+1)*16+z)]=1;++air;}
                            if(isWater(b))++water;
                        }
                    }
                }
                std::size_t largest=0,components=0;bool connected=false;
                std::vector<int> queue;queue.reserve(volume.size()/2);
                for(int i=0;i<int(volume.size());++i)if(volume[i]==1) {
                    ++components;queue.clear();queue.push_back(i);volume[i]=2;bool boundary=false;
                    for(std::size_t p=0;p<queue.size();++p) {
                        const int id=queue[p],x=id%span,z=id/span%span,y=id/(span*span);
                        boundary|=x==0||x==span-1||z==0||z==span-1||y==0||y==levels-1;
                        for(const auto d:{glm::ivec3(1,0,0),glm::ivec3(-1,0,0),glm::ivec3(0,1,0),glm::ivec3(0,-1,0),glm::ivec3(0,0,1),glm::ivec3(0,0,-1)}) {
                            const int xx=x+d.x,yy=y+d.y,zz=z+d.z;
                            if(xx<0||xx>=span||yy<0||yy>=levels||zz<0||zz>=span)continue;
                            const int next=index(xx,yy,zz);if(volume[next]==1){volume[next]=2;queue.push_back(next);}
                        }
                    }
                    if(queue.size()>largest){largest=queue.size();connected=boundary;}
                }
                caves<<seed<<','<<version<<','<<center.x*16<<','<<center.y*16<<','<<air<<','<<water<<','<<largest<<','<<components<<','<<connected<<std::endl;
                check(largest>1000,"No meaningful connected underground volume in survey");
                int longest=0;
                for(int z=4;z<span-4;++z)for(int x=4;x<span-4;++x) {
                    int start=0;
                    for(int y=1;y<levels;++y) {
                        if(!volume[index(x,y,z)]){start=y+1;continue;}
                        if(start>0 && y-start+1>longest) {
                            longest=y-start+1;
                            if(longest>=16)nativeSites<<seed<<','<<version<<",cavern,"<<(center.x-1)*16+x<<','<<bottom+start+2<<','<<(center.y-1)*16+z<<','<<longest<<'\n';
                        }
                    }
                }
                if(version==22)continue;
                bool lake=false;
                for(int z=-4096;z<=4096 && !lake;z+=64)for(int x=-4096;x<=4096 && !lake;x+=64) {
                    const float aquifer=w->aquiferNoise_.fractal2D(x*.0018f+113.7f,z*.0018f-67.3f,2,2.f,.3f);
                    if(aquifer<.36f)continue;
                    const int level=int(std::floor(TerrainExpansion::aquiferLevel(aquifer)));
                    if(w->terrainHeight(x,z)<level+24 || !w->nativeCavityAt(x,level,z) || !w->nativeCavityAt(x,level+3,z))continue;
                    const auto data=w->generateChunkData(floorDiv(x,16),floorDiv(z,16));
                    if(!isWater(data.blocks[w->localIndex(floorMod(x,16),level,floorMod(z,16))]))continue;
                    check(!isWater(data.blocks[w->localIndex(floorMod(x,16),level+1,floorMod(z,16))]),"Aquifer extends above declared datum");
                    nativeSites<<seed<<','<<version<<",lake,"<<x<<','<<level+3<<','<<z<<','<<level<<std::endl;lake=true;
                }
                check(lake,"No natural underground lake found in bounded survey");
                const auto terrain=[&](int x,int z){return w->structureTerrainAt(x,z);};
                std::set<StructurePieceKind> kinds;
                for(int rz=-32;rz<32 && kinds.size()<5;++rz)for(int rx=-32;rx<32 && kinds.size()<5;++rx) {
                    const auto plan=w->structures_.smallPlanForRegion(rx,rz,terrain);
                    for(const auto& piece:plan.pieces) {
                        if(piece.kind<StructurePieceKind::WildernessCamp || !kinds.insert(piece.kind).second)continue;
                        const auto& model=structureTemplate(piece.kind);
                        for(const auto& entry:model.connectors) {
                            const auto approach=rotateLocalPosition(entry.x,entry.z,piece.rotation,piece.mirror);
                            const int ground=terrain(piece.x+approach[0],piece.z+approach[1]).height;
                            const int step=piece.y+entry.y-ground;
                            if(step>1)std::cout<<"Inaccessible landmark "<<int(piece.kind)<<" at "<<piece.x<<','<<piece.y<<','<<piece.z<<" entry step "<<step<<std::endl;
                            check(step<=1,"Landmark approach needs an impossible multi-block jump");
                        }
                        std::map<std::int64_t,GeneratedChunk> chunks;
                        for(int cz=floorDiv(piece.bounds.minZ-2,16);cz<=floorDiv(piece.bounds.maxZ+2,16);++cz)
                            for(int cx=floorDiv(piece.bounds.minX-2,16);cx<=floorDiv(piece.bounds.maxX+2,16);++cx)
                                chunks.emplace(chunkKey(cx,cz),w->generateChunkData(cx,cz));
                        for(const auto& entry:chunks) {
                            const auto& c=entry.second;
                            for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
                                const int y=w->terrainHeight(c.x*16+x,c.z*16+z);
                                const auto base=w->localIndex(x,y,z),root=w->localIndex(x,y+1,z);
                                if(root<c.blocks.size()&&isLog(c.blocks[root]))
                                    check(isSolid(c.blocks[base])&&!isWater(c.blocks[base]),"Unsupported generated root");
                            }
                        }
                        const auto get=[&](int x,int y,int z) {
                            const auto& c=chunks.at(chunkKey(floorDiv(x,16),floorDiv(z,16)));
                            const auto i=w->localIndex(floorMod(x,16),y,floorMod(z,16));
                            return i<c.blocks.size()?c.blocks[i]:Block::Air;
                        };
                        for(const auto& entry:model.connectors) {
                            const auto p=rotateLocalPosition(entry.x,entry.z,piece.rotation,piece.mirror);
                            const int x=piece.x+p[0],z=piece.z+p[1],y=terrain(x,z).height;
                            const auto feet=get(x,y+1,z),head=get(x,y+2,z);
                            check((!isSolid(feet)||blockGeometry(feet).maxY<=.125f)&&!isSolid(head)&&
                                !isWater(feet)&&!isLava(feet),"Landmark approach obstructed after decoration");
                        }
                        for(const auto& cell:model.cells)if(isTorch(cell.block)) {
                            const auto p=rotateLocalPosition(cell.x,cell.z,piece.rotation,piece.mirror);
                            check(isSolid(get(piece.x+p[0],piece.y+cell.y-1,piece.z+p[1])),
                                "Generated landmark torch lacks solid support");
                        }
                        unsigned chests=0;
                        for(const auto& marker:model.loot) {
                            const auto p=rotateLocalPosition(marker.x,marker.z,piece.rotation,piece.mirror);
                            const glm::ivec3 location(piece.x+p[0],piece.y+marker.y,piece.z+p[1]);
                            check(blockType(get(location.x,location.y,location.z))==Block::Chest,"Landmark chest cut at chunk boundary");
                            check(!isSolid(get(location.x,location.y+1,location.z)),"Landmark chest blocked");++chests;
                            w->prepareSpawnTerrain(glm::vec3(location));auto* chest=w->chestAt(location);
                            check(chest!=nullptr,"Landmark loot not initialized");
                            if(Persistence::enabled()) {
                                chest->slots.fill({});chest->slots[0]={Item::DiamondPickaxe,1,37};
                                const auto path=directory+"/landmark.vxw";glm::vec3 player(location),restored;
                                check(w->saveWorld(path,player),"Landmark save failed");
                                auto loaded=std::make_unique<World>(seed);
                                check(loaded->loadWorld(path,restored)&&loaded->generationVersion()==23,"Landmark generator roundtrip failed");
                                loaded->prepareSpawnTerrain(player);const auto* saved=loaded->chestAt(location);
                                check(saved&&saved->slots[0].item==Item::DiamondPickaxe&&saved->slots[0].durability==37,"Landmark chest rerolled");
                                check(loaded->setBlockInternal(location.x,location.y,location.z,Block::Air,true,false),"Landmark chest break failed");
                                check(loaded->saveWorld(path,player),"Broken landmark save failed");
                                auto destroyed=std::make_unique<World>(seed);check(destroyed->loadWorld(path,restored),"Broken landmark reload failed");
                                destroyed->prepareSpawnTerrain(player);
                                check(destroyed->getBlock(location.x,location.y,location.z)==Block::Air,"Broken landmark chest regenerated");
                            }
                        }
                        landmarks<<seed<<','<<int(piece.kind)<<','<<piece.x<<','<<piece.y<<','<<piece.z<<','<<chests<<','<<model.cells.size()<<','<<piece.rotation<<','<<piece.mirror<<std::endl;
                    }
                }
                check(kinds.size()==5,"Missing one of five landmark types");
                // Exact generated bore clearance, after vegetation and structures.
                std::set<std::pair<int,int>> inspected;
                for(int z=-1200;z<=1200 && inspected.size()<8;z+=96)
                    for(int x=-1200;x<=1200 && inspected.size()<8;x+=96)
                        for(const auto& c:w->diversityEntrances(floorDiv(x,16),floorDiv(z,16))) {
                            const auto mouth=glm::ivec3(glm::floor(c.mouth));
                            if(!inspected.emplace(mouth.x,mouth.z).second)continue;
                            entrances<<seed<<','<<c.mouth.x<<','<<c.mouth.y<<','<<c.mouth.z<<','<<c.target.x<<','<<c.target.y<<','<<c.target.z<<','<<c.radius<<std::endl;
                            std::map<std::int64_t,GeneratedChunk> chunks;
                            for(int step=0;step<=24;++step) {
                                const auto p=glm::mix(c.mouth,c.target,step/24.f);const auto cell=glm::ivec3(glm::floor(p));
                                const int cx=floorDiv(cell.x,16),cz=floorDiv(cell.z,16);const auto key=chunkKey(cx,cz);
                                auto it=chunks.find(key);if(it==chunks.end())it=chunks.emplace(key,w->generateChunkData(cx,cz)).first;
                                for(int dy=0;dy<=1;++dy) {
                                    const auto idx=w->localIndex(floorMod(cell.x,16),cell.y+dy,floorMod(cell.z,16));
                                    const auto b=idx<it->second.blocks.size()?it->second.blocks[idx]:Block::Air;
                                    check(!isSolid(b)&&!isWater(b)&&!isLava(b),"Generated entrance sealed or flooded");
                                }
                            }
                            if(inspected.size()<=4)for(int fps:{30,60}) {
                                Player player(c.mouth-glm::vec3(0,.9f,0));player.setMouseSensitivity(1.f);
                                PlayerInput input;input.enabled=true;input.moveForward=true;
                                bool reached=false;float damage=0;int frame=0;
                                for(;frame<fps*25 && !player.isDead();++frame) {
                                    const auto p=player.position();const glm::vec2 delta(c.target.x-p.x,c.target.z-p.z);
                                    if(glm::length(delta)<3.f){reached=true;break;}
                                    const auto direction=glm::normalize(delta);
                                    w->prepareSpawnTerrain(p);w->prepareSpawnTerrain(p+glm::vec3(direction.x*3.f,0,direction.y*3.f));
                                    const auto ahead=p+glm::vec3(direction.x*.7f,0,direction.y*.7f);
                                    input.jump=player.isGrounded()&&w->aabbIntersectsSolid(ahead+glm::vec3(-.3f,.02f,-.3f),ahead+glm::vec3(.3f,1.8f,.3f));
                                    const auto look=player.lookDirection();float turn=std::atan2(delta.y,delta.x)-std::atan2(look.z,look.x);
                                    while(turn>3.14159265f)turn-=6.2831853f;while(turn< -3.14159265f)turn+=6.2831853f;
                                    player.addMouseMovement(glm::degrees(turn),0);player.update(1.f/fps,input,*w);
                                    damage=std::max(damage,player.lastFallDamage());
                                }
                                const auto end=player.position();
                                walks<<seed<<','<<c.mouth.x<<','<<c.mouth.z<<','<<fps<<','<<reached<<','<<player.health()<<','<<damage<<','<<frame<<','<<end.x<<','<<end.y<<','<<end.z<<std::endl;
                                check(reached&&damage<=.5f,"V23 Survival entrance walk blocked or unsafe");
                            }
                        }
                std::cout<<"Ecosystems seed "<<seed<<" validated "<<kinds.size()<<" landmark types and "<<inspected.size()<<" entrances"<<std::endl;
            }
        } else {
            check(World().generationVersion()==LatestGeneratorVersion,"New-world default is not latest");
            for(int cz=-4;cz<4;++cz)for(int cx=-4;cx<4;++cx) {
                std::vector<Block> blocks(256*321,Block::Air);
                for(int z=0;z<16;++z)for(int x=0;x<16;++x)blocks[localIndex(x,80,z)]=Block::Mycelium;
                decorateBiomeChunk(cx,cz,991,blocks,[](int,int){return BiomeTerrain{80,Biome::MushroomFields,0};},
                    [](int,int){return false;},[](int,int){return true;},-64,257,64,23);
                check(std::none_of(blocks.begin(),blocks.end(),[](Block b){return isLog(b)||isLeaf(b);}),
                    "Mushroom habitat received ordinary woodland vegetation");
            }
            if(Persistence::enabled())for(unsigned version=1;version<=23;++version) {
                auto saved=std::make_unique<World>(991,version);const auto path=directory+"/generator-roundtrip.vxw";
                const glm::vec3 position(1.5f,100.f,-1.5f);glm::vec3 restored;
                check(saved->saveWorld(path,position),"Versioned world save failed");
                auto loaded=std::make_unique<World>(991);
                check(loaded->loadWorld(path,restored)&&loaded->generationVersion()==version&&restored==position,"Historical generator migrated on load");
                check(loaded->generateChunkData(-17,31).blocks==saved->generateChunkData(-17,31).blocks,"Historical unexplored chunk changed after upgrade");
                std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);
                file.seekp(36);const unsigned invalid=LatestGeneratorVersion+1;file.write(reinterpret_cast<const char*>(&invalid),4);file.close();
                const auto old=restored;
                check(!loaded->loadWorld(path,restored)&&restored==old&&loaded->generationVersion()==version,"Invalid generator partially mutated active world");
            }
            std::ofstream generation(directory+"/generation.csv"),vegetation(directory+"/vegetation.csv");
            generation<<"seed,version,x,z,repeat,ms,hash,bytes\n";
            vegetation<<"seed,version,x,z,log_voxels,leaf_voxels,plant_voxels,forest_columns,covered_forest_columns,roots\n";
            for(unsigned seed:{20260917U,123456789U,42U})for(unsigned version:{22U,23U}) {
                auto w=std::make_unique<World>(seed,version);
                std::vector<glm::ivec2> coordinates;
                for(int i=0;i<32;++i)coordinates.push_back({(i%8)*173-606,(i/8)*197-295});
                std::vector<std::vector<Block>> expected;
                for(auto c:coordinates) {
                    const auto start=Clock::now();auto data=w->generateChunkData(c.x,c.y);
                    generation<<seed<<','<<version<<','<<c.x<<','<<c.y<<",0,"<<ms(start)<<','<<blockHash(data.blocks)<<','<<data.blocks.size()*sizeof(Block)<<'\n';
                    std::size_t logs=0,leaves=0,plants=0,forest=0,covered=0,roots=0;
                    for(auto b:data.blocks){logs+=isLog(b);leaves+=isLeaf(b);plants+=isPlant(b);}
                    for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
                        const auto t=w->sampleTerrain(c.x*16+x,c.y*16+z);
                        const bool wooded=t.biome==Biome::Forest||t.biome==Biome::BirchForest||t.biome==Biome::FlowerForest||
                            t.biome==Biome::DarkForest||t.biome==Biome::Taiga||t.biome==Biome::OldGrowthTaiga||
                            t.biome==Biome::SnowyTaiga||t.biome==Biome::Jungle||t.biome==Biome::SparseJungle;
                        if(!wooded)continue;
                        ++forest;bool canopy=false;
                        for(int y=t.height+1;y<=std::min(t.height+24,256);++y) {
                            const auto idx=w->localIndex(x,y,z);if(idx>=data.blocks.size())break;
                            canopy|=isLeaf(data.blocks[idx])||isLog(data.blocks[idx]);
                            if(y==t.height+1&&isLog(data.blocks[idx]))++roots;
                        }
                        covered+=canopy;
                    }
                    vegetation<<seed<<','<<version<<','<<c.x<<','<<c.y<<','<<logs<<','<<leaves<<','<<plants<<','<<forest<<','<<covered<<','<<roots<<'\n';
                    for(int z=0;z<16;z+=3)for(int x=0;x<16;x+=3) {
                        const int wx=c.x*16+x,wz=c.y*16+z;const auto t=w->sampleTerrain(wx,wz);
                        if(t.biome!=Biome::Forest&&t.biome!=Biome::Plains)continue;
                        if(w->explorationStructureReservedAt(wx,wz)||t.ravine>.01f)continue;
                        int slope=0;
                        for(const auto d:{glm::ivec2(4,0),glm::ivec2(-4,0),glm::ivec2(0,4),glm::ivec2(0,-4)})
                            slope=std::max(slope,std::abs(w->terrainHeight(wx+d.x,wz+d.y)-t.height));
                        if(slope>5)continue;
                        const auto b=data.blocks[w->localIndex(x,t.height,z)];
                        check(b==Block::Grass||b==Block::Air,"Soil overwrote canonical grass surface");
                    }
                    for(std::size_t i=0;i<data.blocks.size();++i)
                        if(i>=std::size_t(257+64)*256)check(data.blocks[i]==Block::Air,"Natural content exceeds Y256");
                    expected.push_back(std::move(data.blocks));
                }
                for(int repeat=1;repeat<=2;++repeat)for(int i=31;i>=0;--i) {
                    const auto c=coordinates[i];const auto start=Clock::now();const auto data=w->generateChunkData(c.x,c.y);
                    check(data.blocks==expected[i],"Chunk order/cache changed generated bytes");
                    generation<<seed<<','<<version<<','<<c.x<<','<<c.y<<','<<repeat<<','<<ms(start)<<','<<blockHash(data.blocks)<<','<<data.blocks.size()*sizeof(Block)<<'\n';
                }
                for(int workers:{1,2,4,8}) {
                    std::vector<std::future<void>> jobs;
                    for(int worker=0;worker<workers;++worker)jobs.push_back(std::async(std::launch::async,[&,worker,workers] {
                        for(int i=worker;i<32;i+=workers)
                            if(w->generateChunkData(coordinates[i].x,coordinates[i].y).blocks!=expected[i])
                                throw std::runtime_error("Worker count changed chunk bytes");
                    }));
                    for(auto& job:jobs)job.get();check(true,"Concurrent deterministic generation");
                }
                // Cross-version cache pollution, especially seed-equal entrances.
                auto other=std::make_unique<World>(seed,version==22?23:22);
                other->generateChunkData(coordinates[0].x,coordinates[0].y);
                check(w->generateChunkData(coordinates[0].x,coordinates[0].y).blocks==expected[0],"Cross-version cache contamination");
                for(int c:{-29999000,-1000000,-17,-1,0,15,16,1000000,29999000}) {
                    const auto t=w->sampleTerrain(c,-c);
                    check(std::isfinite(t.temperature)&&std::isfinite(t.humidity)&&t.height>=-48&&t.height<=255,"Distant terrain invalid");
                }
                // The exact density lattice's shared halo must agree with sparse
                // entrance/structure probes on both sides of +/- chunk borders.
                for(int cx:{-2,-1,0,1}) {
                    TerrainDensity::Grid grid(cx,-1,w->ridgeNoise_,w->cheeseNoise_,w->spaghettiNoise_,w->noodleNoise_,w->caveNoise_,144);
                    for(int x=cx*16-1;x<=cx*16+16;++x)for(int y=-48;y<128;y+=7) {
                        const auto a=grid.at(x,y,-8),b=TerrainDensity::sampleAt(x,y,-8,w->ridgeNoise_,w->cheeseNoise_,w->spaghettiNoise_,w->noodleNoise_,w->caveNoise_);
                        check(std::abs(a.chamber-b.chamber)<1e-6f && std::abs(a.surface-b.surface)<1e-6f,"Density halo seam");
                        const auto cached=TerrainDensity::sampleAtCached(x,y,-8,seed,version,w->ridgeNoise_,w->cheeseNoise_,w->spaghettiNoise_,w->noodleNoise_,w->caveNoise_);
                        check(cached.surface==b.surface&&cached.chamber==b.chamber&&cached.tunnelA==b.tunnelA&&
                            cached.tunnelB==b.tunnelB&&cached.pillar==b.pillar&&cached.vein==b.vein,"Sparse lattice cache changed fields");
                    }
                }
                std::cout<<"Generation seed "<<seed<<" V"<<version<<" deterministic"<<std::endl;
            }
        }
        report="V23 "+mode+": "+std::to_string(checks)+" checks passed";return true;
    }catch(const std::exception& e){report=std::string("V23 ")+mode+" failed: "+e.what();return false;}
}
