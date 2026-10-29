#include "World.h"
#include "ExplorationLoot.h"
#include "Definitions.h"
#include "Persistence.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <tuple>

namespace {
int divFloor(int n,int d){int q=n/d;return q-(n%d<0);}
std::uint64_t hashBlocks(const std::vector<Block>& blocks) {
    std::uint64_t h=14695981039346656037ULL;for(auto b:blocks){h^=std::uint8_t(b);h*=1099511628211ULL;}return h;
}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
bool World::runUndergroundAudit(const std::string& directory,std::string& report) const {
    namespace fs=std::filesystem;fs::create_directories(directory);
    try {
        std::ofstream distribution(directory+"/structures.csv"),generation(directory+"/generation.csv"),structureTiming(directory+"/structure-generation.csv"),lootStats(directory+"/loot.csv"),hashes(directory+"/historical.csv");
        distribution<<"seed,type,region_x,region_z,x,y,z,biome,rooms,chests,enemies,cells,entrance_x,entrance_y,entrance_z\n";
        generation<<"seed,version,x,z,ms,bytes,hash\n";
        structureTiming<<"type,version,repeat,chunks,ms,bytes\n";
        lootStats<<"depth,table,chests,common,uncommon,rare,diamonds,relics\n";
        hashes<<"seed,version,x,z,hash\n";
        std::array<int,3> counts{}, baselineCounts{};int connectivity=0;
        using P=std::tuple<int,int,int>;
        std::array<std::shared_ptr<const UndergroundPlan>,3> captures{};
        for(unsigned seed:{123456789U,20260917U,42U}) {
            World w(seed,generationVersion_);
            World previous(seed,20);
            std::ofstream map(directory+"/map-"+std::to_string(seed)+"-v"+std::to_string(generationVersion_)+"-8192.bin",std::ios::binary);
            struct Pixel {std::uint8_t biome,reserved;std::int16_t height;float t,h,c,river,mt,mh;};
            for(int z=0;z<512;++z)for(int x=0;x<512;++x) {
                const auto t=w.sampleTerrain(x*16-4096,z*16-4096);
                const Pixel p{std::uint8_t(t.biome),0,std::int16_t(t.height),t.temperature,t.humidity,t.continentalness,t.river,t.macroTemperature,t.macroHumidity};
                map.write(reinterpret_cast<const char*>(&p),sizeof(p));
            }
            for(int rz=-16;rz<16;++rz)for(int rx=-16;rx<16;++rx) {
                if(auto old=previous.undergroundPlan(rx,rz)) ++baselineCounts[int(old->kind)];
                auto p=w.undergroundPlan(rx,rz);if(!p)continue;
                ++counts[int(p->kind)];
                const auto& r=p->rooms.front();
                distribution<<seed<<','<<UndergroundStructures::name(p->kind)<<','<<rx<<','<<rz<<','<<r.x<<','<<r.y<<','<<r.z<<','<<biomeDefinition(p->biome).name<<','<<p->rooms.size()<<','<<p->loot.size()<<','<<p->mobs.size()<<','<<p->cells.size()<<','<<p->entrance.x<<','<<p->entrance.y<<','<<p->entrance.z<<'\n';
                require(p->bounds.minY>=-54&&p->bounds.maxY<=256,"Structure height limits");
                require(divFloor(p->bounds.minX,256)==rx&&divFloor(p->bounds.maxX,256)==rx&&
                        divFloor(p->bounds.minZ,256)==rz&&divFloor(p->bounds.maxZ,256)==rz,"Region spacing violated");
                require(w.dryUndergroundCave(p->entrance.x,p->entrance.y,p->entrance.z),"Cave connection not dry/navigable");
                require(!w.undergroundReserved(p->bounds),"Surface structure overlap");
                std::map<P,Block> cells;for(const auto& c:p->cells)require(cells.emplace(P{c.x,c.y,c.z},c.block).second,"Duplicate planned cell");
                if(generationVersion_>=21) require(std::any_of(p->cells.begin(),p->cells.end(),[](const auto& c){return c.block==Block::Cobweb;}),"Structure without cobwebs");
                const auto block=[&](int x,int y,int z){auto it=cells.find({x,y,z});return it==cells.end()?Block::Stone:it->second;};
                const auto walkable=[&](int x,int y,int z){return isSolid(block(x,y-1,z))&&!isSolid(block(x,y,z))&&!isSolid(block(x,y+1,z));};
                std::queue<P> queue;std::set<P> visited;P start{r.x,r.y+1,r.z};
                require(walkable(r.x,r.y+1,r.z),"Room centre blocked");queue.push(start);visited.insert(start);
                while(!queue.empty()) {
                    auto [x,y,z]=queue.front();queue.pop();
                    constexpr int d[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
                    for(auto& step:d)for(int dy:{0,1,-1}) {
                        const int nx=x+step[0],ny=y+dy,nz=z+step[1];
                        if(nx<p->bounds.minX||nx>p->bounds.maxX||nz<p->bounds.minZ||nz>p->bounds.maxZ||ny<p->bounds.minY||ny>p->bounds.maxY)continue;
                        // A one-block step also needs clearance above the higher head.
                        if(dy>0&&isSolid(block(x,y+2,z)))continue;
                        if(walkable(nx,ny,nz)&&visited.insert({nx,ny,nz}).second)queue.emplace(nx,ny,nz);
                    }
                }
                for(const auto& room:p->rooms)if(!visited.count({room.x,room.y+1,room.z})) {
                    std::ofstream detail(directory+"/failed-layout.csv");detail<<"x,y,z,block\n";
                    for(const auto& cell:p->cells)detail<<cell.x<<','<<cell.y<<','<<cell.z<<','<<int(cell.block)<<'\n';
                    std::ostringstream error;error<<"Disconnected room seed "<<seed<<" region "<<rx<<","<<rz<<" centre "<<room.x<<","<<room.y+1<<","<<room.z<<" walkable "<<walkable(room.x,room.y+1,room.z)<<" visited "<<visited.size();
                    throw std::runtime_error(error.str());
                }
                if(!visited.count({p->entrance.x,p->entrance.y,p->entrance.z})) {
                    std::ofstream detail(directory+"/failed-layout.csv");detail<<"x,y,z,block\n";
                    for(const auto& cell:p->cells)detail<<cell.x<<','<<cell.y<<','<<cell.z<<','<<int(cell.block)<<'\n';
                    std::ostringstream error;error<<"Entrance unreachable seed "<<seed<<" region "<<rx<<","<<rz<<" entrance "<<p->entrance.x<<","<<p->entrance.y<<","<<p->entrance.z;
                    throw std::runtime_error(error.str());
                }
                for(const auto& m:p->loot) {
                    require(block(m.x,m.y,m.z)==Block::Chest,"Loot marker not chest");
                    bool reach=false;for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)reach|=visited.count({m.x+dx,m.y,m.z+dz})>0;
                    require(reach,"Chest inaccessible");
                }
                for(const auto& m:p->mobs)require(walkable(m.x,m.y,m.z),"Encounter clips into terrain");
                ++connectivity;
                // Reassemble chunk-clipped placement; every cell must appear once.
                if(counts[int(p->kind)]<=3) {
                    for(int cz=divFloor(p->bounds.minZ,16);cz<=divFloor(p->bounds.maxZ,16);++cz)
                        for(int cx=divFloor(p->bounds.minX,16);cx<=divFloor(p->bounds.maxX,16);++cx) {
                            GeneratedChunk data{cx,cz,std::vector<Block>(384*256,Block::Stone)};w.applyUnderground(cx,cz,data);
                            for(const auto& cell:p->cells)if(divFloor(cell.x,16)==cx&&divFloor(cell.z,16)==cz)
                                require(data.blocks[w.localIndex(cell.x-cx*16,cell.y,cell.z-cz*16)]==cell.block,"Chunk clipping seam");
                        }
                    const int cx=divFloor(r.x,16),cz=divFloor(r.z,16);auto a=w.generateChunkData(cx,cz),b=w.generateChunkData(cx,cz);
                    require(a.blocks==b.blocks,"Nondeterministic generation");
                    std::array<std::future<std::uint64_t>,4> workers;
                    for(auto& worker:workers)worker=std::async(std::launch::async,[&]{return hashBlocks(w.generateChunkData(cx,cz).blocks);});
                    for(auto& worker:workers)require(worker.get()==hashBlocks(a.blocks),"Worker ordering changed structure");
                }
                if(seed==123456789U&&(!captures[int(p->kind)]||
                   (p->kind==UndergroundKind::Dungeon&&p->rooms.front().y<0&&captures[1]->rooms.front().y>=0)))captures[int(p->kind)]=p;
            }
            // Same coordinates/version workloads isolate generation overhead.
            for(unsigned version:{19U,20U}) {
                World probe(seed,version);
                for(int repeat=0;repeat<3;++repeat)for(int z=-4;z<4;++z)for(int x=-4;x<4;++x) {
                    auto start=std::chrono::steady_clock::now();auto data=probe.generateChunkData(x,z);
                    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                    generation<<seed<<','<<version<<','<<x<<','<<z<<','<<ms<<','<<data.blocks.size()<<','<<hashBlocks(data.blocks)<<'\n';
                }
            }
        }
        for(int c:counts)require(c>=5,"Structure type too rare in sampled area");
        // Exercise the actual chunk integration queue, including negative Y.
        {
            const auto p=captures[1];require(p&&p->rooms.front().y<0,"Missing deep encounter fixture");
            World encounters(123456789U,generationVersion_);
            const auto& r=p->rooms.front();encounters.generate(2,glm::vec3(r.x,r.y+1,r.z));
            encounters.prepareSpawnTerrain(glm::vec3(r.x,r.y+1,r.z));
            require(encounters.takeActiveStructureMobMarkers(false).empty(),"Paused encounter activated");
            const auto active=encounters.takeActiveStructureMobMarkers();
            require(std::any_of(active.begin(),active.end(),[](const auto& m){return m.hostile&&m.y<0;}),"Deep encounter lost during chunk integration");
            require(encounters.takeActiveStructureMobMarkers().empty(),"Encounter queue emitted duplicates");
        }
        for(auto p:captures)if(p) {
            const auto name=std::string(UndergroundStructures::name(p->kind));
            std::ofstream cells(directory+"/layout-"+name+".csv");cells<<"x,y,z,block\n";
            for(const auto& c:p->cells)cells<<c.x<<','<<c.y<<','<<c.z<<','<<int(c.block)<<'\n';
            for(unsigned version:{19U,20U}) {
                World probe(123456789U,version);
                for(int repeat=0;repeat<3;++repeat) {
                    std::size_t bytes=0;int chunks=0;
                    const auto start=std::chrono::steady_clock::now();
                    for(int z=divFloor(p->bounds.minZ,16);z<=divFloor(p->bounds.maxZ,16);++z)
                        for(int x=divFloor(p->bounds.minX,16);x<=divFloor(p->bounds.maxX,16);++x) {
                            bytes+=probe.generateChunkData(x,z).blocks.size();++chunks;
                        }
                    structureTiming<<name<<','<<version<<','<<repeat<<','<<chunks<<','
                        <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<','<<bytes<<'\n';
                }
            }
        }
        for(int depth:{40,0,-40})for(auto table:{StructureLoot::Mineshaft,StructureLoot::Dungeon,StructureLoot::Ancient}) {
            std::array<int,5> c{};
            for(unsigned i=0;i<10000;++i) {
                StructureLootMarker m{int(i),depth,31,table,i};const auto a=rollExplorationLoot(42,m),b=rollExplorationLoot(42,m);
                for(int s=0;s<27;++s) {
                    require(a[s].item==b[s].item&&a[s].count==b[s].count&&a[s].durability==b[s].durability,"Loot determinism");
                    if(a[s].empty())continue;
                    require(a[s].count<=itemDefinition(a[s].item).maxStack,"Loot stack overflow");
                    const auto item=a[s].item;bool rare=item==Item::Diamond||item==Item::SurveyorToken||item==Item::CavernShard||item==Item::AncientSeal;
                    if(rare)++c[2];else if(item==Item::IronIngot||item==Item::GoldIngot||item==Item::IronPickaxe||item==Item::IronSword||item==Item::CookedBeef||item==Item::Book)++c[1];else ++c[0];
                    if(item==Item::Diamond)c[3]+=a[s].count;if(item==Item::SurveyorToken||item==Item::CavernShard||item==Item::AncientSeal)++c[4];
                }
            }
            lootStats<<depth<<','<<int(table)<<",10000";for(int v:c)lootStats<<','<<v;lootStats<<'\n';
        }
        // Compare all historical fixtures supplied from the validated V18 build.
        std::ifstream legacy(directory+"/baseline-historical.csv");require(bool(legacy),"Missing historical baseline fixture");
        std::string line;std::getline(legacy,line);int checked=0;unsigned lastSeed=0,lastVersion=0;std::unique_ptr<World> probe;
        while(std::getline(legacy,line)) {
            std::replace(line.begin(),line.end(),',',' ');std::istringstream in(line);unsigned seed,version;int x,z;std::uint64_t expected;
            if(!(in>>seed>>version>>x>>z>>expected))continue;
            if(version>20)continue;
            // The historical 1–10 fixture changes the version on one probe;
            // reproduce that exact fixture/cache history rather than comparing
            // a fresh probe against its warmed surface-plan cache.
            if(!probe||seed!=lastSeed||(version!=lastVersion&&version>10))probe=std::make_unique<World>(seed,version);
            probe->generationVersion_=version;lastSeed=seed;lastVersion=version;
            auto h=hashBlocks(probe->generateChunkData(x,z).blocks);
            if(h!=expected) {std::ostringstream e;e<<"Historical hash mismatch "<<seed<<" V"<<version<<" "<<x<<","<<z<<" expected "<<expected<<" actual "<<h;throw std::runtime_error(e.str());}
            hashes<<seed<<','<<version<<','<<x<<','<<z<<','<<h<<'\n';++checked;
        }
        require(checked>100,"Insufficient legacy hashes");
        if(Persistence::enabled()) {
            for(unsigned version=1;version<=21;++version) {
                World saved(42,version),restored(42,19);const auto path=directory+"/legacy-save.vxw";glm::vec3 pos(0,100,0);
                require(saved.saveWorld(path,pos)&&restored.loadWorld(path,pos)&&restored.generationVersion()==int(version),"Stored generator/save compatibility");
                require(hashBlocks(saved.generateChunkData(-3,2).blocks)==hashBlocks(restored.generateChunkData(-3,2).blocks),"Unexplored legacy terrain differs after loading");fs::remove(path);
            }
            SurvivalWorld encounters(42);const glm::ivec3 encounterMarker(-3,-30,4);
            encounters.spawnStructureMob(encounterMarker,true);encounters.spawnStructureMob(encounterMarker,true);
            require(encounters.diagnostics(glm::vec3(encounterMarker)).hostile==1,"Duplicate encounter spawn");
            const auto mobPath=directory+"/encounters.vxm";require(encounters.save(mobPath,42),"Encounter save failed");
            SurvivalWorld restoredEncounters(42);require(restoredEncounters.load(mobPath,42),"Encounter load failed");
            restoredEncounters.spawnStructureMob(encounterMarker,true);
            require(restoredEncounters.diagnostics(glm::vec3(encounterMarker)).hostile==1,"Encounter respawned on reload");fs::remove(mobPath);
            World w(123456789U,generationVersion_);auto p=captures[0];require(bool(p),"Missing test mineshaft");
            const auto marker=p->loot.front();const glm::ivec3 pos(marker.x,marker.y,marker.z);
            w.prepareSpawnTerrain(glm::vec3(pos));auto chest=w.chestAt(pos,true);require(chest&&w.getBlock(pos.x,pos.y,pos.z)==Block::Chest,"Generated chest missing");
            for(auto& slot:chest->slots)slot.clear();chest->slots[26]={Item::AncientSeal,1,0};
            auto generated=w.generateChunkData(divFloor(pos.x,16),divFloor(pos.z,16));w.initializeGeneratedLoot(generated);
            require(chest->slots[26].item==Item::AncientSeal,"Loot rerolled on regeneration");
            w.generate(2,glm::vec3(0,150,0)); // Release the actual structure chunks.
            w.generate(2,glm::vec3(pos));w.prepareSpawnTerrain(glm::vec3(pos));
            chest=w.chestAt(pos,false);
            require(chest&&chest->slots[26].item==Item::AncientSeal&&chest->slots[0].empty(),"Chest reset on chunk unload/reload");
            const auto path=directory+"/modified-chest.vxw";glm::vec3 spawn(pos);
            require(w.saveWorld(path,spawn),"Chest save failed");World loaded(123456789U,18);require(loaded.loadWorld(path,spawn)&&loaded.generationVersion()==generationVersion_,"New-world save load failed");
            loaded.prepareSpawnTerrain(glm::vec3(pos));chest=loaded.chestAt(pos,false);
            require(chest&&chest->slots[26].item==Item::AncientSeal&&chest->slots[0].empty(),"Chest contents reset on load");
            loaded.setBlock(pos.x,pos.y,pos.z,Block::Air);require(loaded.saveWorld(path,spawn),"Destroyed chest save failed");
            World destroyed(123456789U,19);require(destroyed.loadWorld(path,spawn),"Destroyed chest load failed");destroyed.prepareSpawnTerrain(glm::vec3(pos));
            require(destroyed.getBlock(pos.x,pos.y,pos.z)==Block::Air&&!destroyed.chestAt(pos,false),"Destroyed chest regenerated");
            fs::remove(path);
        }
        std::ofstream frequency(directory+"/frequency.csv");frequency<<"type,v20,current\n";
        for(int i=0;i<3;++i) frequency<<UndergroundStructures::name(UndergroundKind(i))<<','<<baselineCounts[i]<<','<<counts[i]<<'\n';
        std::array<int,3> wideBefore{},wideAfter{};
        for(unsigned seed:{123456789U,20260917U,42U}) {
            World before(seed,20),after(seed,generationVersion_);
            for(int z=-32;z<32;++z)for(int x=-32;x<32;++x) {
                if(auto p=before.undergroundPlan(x,z)) ++wideBefore[int(p->kind)];
                if(auto p=after.undergroundPlan(x,z)) ++wideAfter[int(p->kind)];
            }
        }
        std::ofstream wide(directory+"/frequency-16384.csv");wide<<"type,v20,current\n";
        for(int i=0;i<3;++i) wide<<UndergroundStructures::name(UndergroundKind(i))<<','<<wideBefore[i]<<','<<wideAfter[i]<<'\n';
        report="Underground audit passed: "+std::to_string(counts[0])+" mineshafts, "+std::to_string(counts[1])+" dungeons, "+std::to_string(counts[2])+" ruins; "+std::to_string(connectivity)+" connected layouts; "+std::to_string(checked)+" legacy hashes; deterministic loot; "+
            (Persistence::enabled()?"chest/encounter persistence.":"demo persistence disabled (save checks skipped).");
        return true;
    }catch(const std::exception& e){report=std::string("Underground audit FAILED: ")+e.what();return false;}
}

