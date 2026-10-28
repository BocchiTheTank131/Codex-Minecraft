#include "Game.h"
#include "Player.h"
#include "Transportation.h"
#include "Weather.h"
#include "SaveFile.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <stdexcept>

double fixtureWorkingSetMb();
class EngineOptimizationTest {
    using Clock=std::chrono::steady_clock;
    static double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
    static std::string bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
    static void check(bool ok,const char* name){std::cout<<"engine "<<name<<" passed="<<ok<<'\n';if(!ok)throw std::runtime_error(name);}
    static std::unique_ptr<Game> open(bool freezeWorkers=true) {
        auto g=std::make_unique<Game>();char app[]="EngineHarness",seed[]="--seed=20260917",version[]="--generator=23";char* args[]={app,seed,version};
        if(!g->initialize(3,args))throw std::runtime_error("Engine fixture initialization");
        glfwHideWindow(g->window_);g->settings_.renderDistance=2;g->settings_.simulationDistance=2;
        g->playSavedWorld(WorldLibrary::create("Engine fixture",20260917,GameMode::Survival,true));
        check(g->world_!=nullptr,"isolated_world");
        // This fixture isolates capture/codec work. Foreground traversal tests
        // separately keep streaming, physics, rendering and simulation active.
        if(freezeWorkers){auto& world=*g->world_;world.stopping_=true;world.generationCv_.notify_all();world.meshCv_.notify_all();
        for(auto& worker:world.workers_)if(worker.joinable())worker.join();
        for(auto& worker:world.meshWorkers_)if(worker.joinable())worker.join();}
        return g;
    }
    static void populate(Game& g,int count) {
        auto& w=*g.world_;w.edits_.clear();w.furnaces_.clear();w.chests_.clear();w.buildingTexts_.clear();w.buildingDrops_.clear();w.automation.reset();
        for(int i=0;i<count;++i)w.edits_[World::chunkKey(1000+(i/2048)%128,1000+(i/2048)/128)][std::size_t(264*256+i%2048)]=Block::Stone;
        const BlockEntityPosition chest{16000,200,16000},furnace{16002,200,16000},hopper{16004,200,16000};
        const auto edit=[&](BlockEntityPosition p,Block b){w.edits_[World::chunkKey(World::floorDiv(p.x,16),World::floorDiv(p.z,16))][w.localIndex(World::floorMod(p.x,16),p.y,World::floorMod(p.z,16))]=b;};
        edit(chest,Block::Chest);edit(furnace,Block::Furnace);edit(hopper,Block::Hopper);
        w.chests_[chest].slots[0]={Item::IronSword,1,7,0};w.chests_[hopper].slots[0]={Item::IronIngot,12,0,0};
        w.furnaces_[furnace].input={Item::IronOre,3,0,0};w.furnaces_[furnace].fuel={Item::Coal,2,0,0};w.furnaces_[furnace].progress=.25f;
        Automation::Node node;node.type=Block::Hopper;node.remaining=2;w.automation.nodes_[glm::ivec3(hopper.x,hopper.y,hopper.z)]=node;
        w.buildingDrops_.push_back({{16000,201,16000},{Item::Diamond,3,0,0}});
        g.inventory_->clear();g.inventory_->add(Item::IronHelmet);g.inventory_->equipFromSlot(0);g.inventory_->add(Item::IronSword,1,7);
        g.transportation_->maps_.clear();g.transportation_->waypoints_.clear();g.transportation_->boats_.clear();g.transportation_->nextMap_=g.transportation_->nextBoat_=1;
        const auto id=g.transportation_->createMap({.5f,100,.5f});g.inventory_->add(Item::FilledMap,1,0,id);
        g.transportation_->maps_[0].pixels[13]=0xff563412;g.transportation_->maps_[0].pixels[600]=0xff154476;
        g.transportation_->addWaypoint("Negative route",{-200,85,-300});
        g.transportation_->boats_.push_back({1,Item::OakBoat,{2,65,2},{0,0,0},.7f,0});g.transportation_->nextBoat_=2;
        g.player_->teleport({.5f,100,.5f});g.player_->health_=13;g.player_->hunger_=14;g.player_->saturation_=3;
        g.timing_.worldTime=321.5f;g.weather_->set(WeatherType::Rain,3600);
        g.survival_->animals_.clear();g.survival_->drops_.clear();g.survival_->spawnedStructureMarkers_.clear();
        for(int type:{0,6,7,8,9}){SurvivalWorld::Animal a;a.type=SurvivalWorld::AnimalType(type);a.position={float(type),100,4};a.health=12;a.age=20;a.persistent=type==0;g.survival_->animals_.push_back(a);}
        g.survival_->spawnedStructureMarkers_.insert(12345);
    }
    static void exportComponents(Game& g,const std::filesystem::path& out) {
        std::filesystem::create_directories(out);
        check(g.world_->saveWorld((out/"world.vxw").string(),g.player_->position()),"export_world");
        check(g.inventory_->save((out/"inventory.vxi").string(),g.seed_),"export_inventory");
        check(g.player_->save((out/"player.vps").string(),g.seed_),"export_player");
        check(g.survival_->save((out/"mobs.vxm").string(),g.seed_),"export_mobs");
        check(g.transportation_->save((out/"travel.vtn").string(),g.seed_),"export_travel");
    }
public:
    static int run(int argc,char** argv) {
        const auto out=std::filesystem::absolute(argc>2?argv[2]:"engine-evidence");std::filesystem::create_directories(out);
        const std::string mode=argv[1];auto g=open(mode!="--engine-stream-stress");
        if(mode=="--engine-stream-stress") {
            populate(*g,50000);const auto saved=g->activeWorld_;
            g->setGameMode(GameMode::Spectator);g->settings_.renderDistance=8;g->world_->setRenderDistance(8);
            const int cycles=argc>3?std::clamp(std::stoi(argv[3]),1,64):24;
            const double seconds=argc>4?std::clamp(std::stod(argv[4]),.05,2.0):.15;
            if(!std::isfinite(seconds))throw std::invalid_argument("Finite stress duration required");
            std::ofstream trace(out/"resource-cycles.csv");trace<<"cycle,loaded,generation,dirty,retired,terrain_used,terrain_capacity,ram_mb\n";
            for(int cycle=0;cycle<cycles;++cycle){
                const auto position=glm::vec3((cycle%2?-1.f:1.f)*(4096.f+cycle*128.f),150.f,float(cycle*173-2048));
                g->player_->teleport(position);g->inventory_->add(Item::Diamond);g->timing_.worldTime=float(800+cycle);
                check(g->saveAll(true),"streaming_save_queued");
                const auto start=glfwGetTime();
                while(glfwGetTime()-start<seconds){glfwPollEvents();g->pollSave();g->updateSimulation(1.f/120);g->renderFrame(1.f/120);}
                check(g->saveAll(true),"streaming_newest_state_requested");g->pollSave(true);
                check(SaveFile::verifyCheckpoint(saved.directory.string()),"streaming_checkpoint_verified");
                Inventory disk;check(disk.load((saved.directory/"voxel_inventory.vxi").string(),g->seed_)&&disk.count(Item::Diamond)==cycle+1,"streaming_item_conservation");
                const auto memory=g->world_->terrainMemory();const auto state=g->world_->streamingDiagnostics();
                trace<<cycle<<','<<g->world_->loadedChunkCount()<<','<<g->world_->pendingChunkCount()<<','<<g->world_->queuedMeshRebuildCount()<<','<<state.retiredChunks<<','<<memory.usedBytes<<','<<memory.capacityBytes<<','<<fixtureWorkingSetMb()<<'\n';trace.flush();
            }
            g->player_->killByCommand();check(g->saveAll(true),"dead_async_checkpoint_queued");g->pollSave(true);
            Player dead({0,0,0});check(dead.load((saved.directory/"voxel_player.vps").string(),g->seed_)&&dead.health()==0,"dead_async_statistics_saved");
            g->returnToMainMenu();g->playSavedWorld(saved);check(g->world_&&g->inventory_->count(Item::Diamond)==cycles&&g->transportation_->maps_.size()==1,"streaming_world_switch_reload");
        } else if(mode=="--engine-benchmark") {
            std::ofstream csv(out/"saves.csv");csv<<"edits,repeat,capture_ms,total_ms,worker_verify_ms,worker_encode_ms,worker_commit_ms,hash_ms,staging_inclusive_ms,snapshot_bytes\n";
            for(int n:{0,50000,250000}) {populate(*g,n);
                for(int repeat=0;repeat<3;++repeat){const auto start=Clock::now();check(g->saveAll(true),"benchmark_save_queued");const auto capture=elapsed(start);
                    auto status=g->pendingSaveStatus_;g->pollSave(true);const auto total=elapsed(start);
                    check(SaveFile::verifyCheckpoint(g->activeWorld_.directory.string()),"benchmark_complete_checkpoint");
                    csv<<n<<','<<repeat<<','<<capture<<','<<total;
#ifdef ENGINE_BASELINE
                    csv<<",0,0,0,0,0,0\n";
#else
                    csv<<','<<status->verificationMilliseconds<<','<<status->encodingMilliseconds<<','<<status->commitMilliseconds<<','<<status->hashingMilliseconds<<','<<status->stagingMilliseconds<<','<<status->snapshotBytes<<'\n';
#endif
                }
            }
        } else if(mode=="--engine-remesh") {
            std::ofstream csv(out/"mesh-requests.csv");csv<<"edits,queued,dirty,revision_delta,ms\n";
            auto& w=*g->world_;auto& c=*w.chunks_.begin()->second;
            for(int count:{100,1000,10000}){w.dirtyQueue_.clear();w.dirtySet_.clear();w.priorityDirtyKeys_.clear();const auto revision=c.meshRevision;const auto start=Clock::now();
                for(int i=0;i<count;++i)w.markDirty(c.x,c.z,true);
                csv<<count<<','<<w.dirtyQueue_.size()<<','<<w.dirtySet_.size()<<','<<c.meshRevision-revision<<','<<elapsed(start)<<'\n';
                check(c.meshRevision==revision+count,"all_edit_revisions_retained");}
        } else if(mode=="--engine-export") {populate(*g,50000);exportComponents(*g,out);}
#ifndef ENGINE_BASELINE
        else if(mode=="--engine-tests") {
            populate(*g,50000);const auto direct=out/"direct";exportComponents(*g,direct);
            const auto frozen=out/"captured";std::filesystem::create_directories(frozen);
            SaveFile::SnapshotBudget budget;std::vector<SaveFile::SnapshotComponent> components;
            components.push_back(g->world_->captureSaveWorld((frozen/"world.vxw").string(),g->player_->position()));
            components.push_back(g->inventory_->captureSave((frozen/"inventory.vxi").string(),g->seed_));
            components.push_back(g->player_->captureSave((frozen/"player.vps").string(),g->seed_));
            components.push_back(g->survival_->captureSave((frozen/"mobs.vxm").string(),g->seed_));
            components.push_back(g->transportation_->captureSave((frozen/"travel.vtn").string(),g->seed_));
            // Compare live AUT1 encoding with captured active/suspended timer history.
            auto& logic=g->world_->automation;logic.stats_.ticks=14;
            auto& timed=logic.nodes_.begin()->second;timed.due=15;timed.timerAt=10;timed.remaining=4;
            for(std::uint64_t t=11;t<=14;++t)logic.ranges_[t%128]={t,t==12?0:1000,1000,2,true};
            std::ostringstream timerBefore,timerAfter;check(logic.write(timerBefore),"active_timer_reference");
            auto timerSnapshot=logic.captureWrite();logic.stats_.ticks=99;logic.nodes_.clear();
            check(timerSnapshot(timerAfter)&&timerBefore.str()==timerAfter.str(),"active_suspended_timer_snapshot_exact");
            g->world_->edits_.clear();g->world_->chests_.clear();g->world_->buildingDrops_.clear();g->world_->automation.reset();
            g->inventory_->clear();g->player_->heal(7);g->survival_->animals_.clear();g->transportation_->maps_.clear();
            auto status=std::make_shared<SaveFile::CommitStatus>();auto job=SaveFile::commitSnapshotAsync((frozen/"save.checkpoint").string(),std::move(components),status);
            check(job.get()&&SaveFile::verifyCheckpoint(frozen.string()),"immutable_complete_generation");
            for(const auto* name:{"world.vxw","inventory.vxi","player.vps","mobs.vxm","travel.vtn"})check(bytes(direct/name)==bytes(frozen/name),"captured_codec_exact_bytes");
            auto restored=std::make_unique<World>(g->seed_,23);glm::vec3 position;
            check(restored->loadWorld((frozen/"world.vxw").string(),position)&&restored->buildingDrops_.size()==1,"captured_world_load_and_deferred_ownership");
            // AUT1's registry is restored independently of the diagnostics cache.
            check(restored->automation.nodes_.size()==1,"captured_automation_registry");
            Inventory inventory;Player player({0,0,0});Transportation navigation({0,0,0});SurvivalWorld mobs(g->seed_);
            check(inventory.load((frozen/"inventory.vxi").string(),g->seed_)&&inventory.count(Item::FilledMap)==1,"captured_inventory_identity");
            check(player.load((frozen/"player.vps").string(),g->seed_)&&player.health()==13&&player.hunger()==14,"captured_survival_statistics");
            check(navigation.load((frozen/"travel.vtn").string(),g->seed_)&&navigation.maps_.size()==1&&navigation.boats_.size()==1,"captured_navigation_ownership");
            check(mobs.load((frozen/"mobs.vxm").string(),g->seed_)&&mobs.animals_.size()==5,"captured_billboard_records");
            bool rejected=false;{SaveFile::SnapshotBudget tiny(1);try{g->world_->captureSaveWorld((out/"over-budget.vxw").string(),g->player_->position());}catch(const std::length_error&){rejected=true;}}
            check(rejected&&!std::filesystem::exists(out/"over-budget.vxw"),"budget_rejects_before_write");
            // Failed worker encoding cannot replace any verified component.
            const auto previous=bytes(frozen/"checkpoint.manifest");
            std::vector<SaveFile::SnapshotComponent> invalid;
            invalid.push_back([path=(frozen/"inventory.vxi").string()]{return SaveFile::write(path,std::ios::binary,[](std::ofstream& f){f<<"candidate";return bool(f);});});
            invalid.push_back([]{return false;});
            auto invalidStatus=std::make_shared<SaveFile::CommitStatus>();
            check(!SaveFile::commitSnapshotAsync((frozen/"save.checkpoint").string(),std::move(invalid),invalidStatus).get(),"worker_encoding_failure_reported");
            check(bytes(frozen/"checkpoint.manifest")==previous&&SaveFile::verifyCheckpoint(frozen.string())&&bytes(frozen/"inventory.vxi")==bytes(direct/"inventory.vxi"),"failed_snapshot_preserves_verified_checkpoint");
            populate(*g,50000);
            for(int cycle=0;cycle<12;++cycle){
                check(g->saveAll(true),"repeated_snapshot_queued");
                g->inventory_->add(Item::Diamond);g->timing_.worldTime=float(500+cycle);
                check(g->saveAll(true),"latest_state_coalesced");g->saveSettings();g->pollSave(true);
                check(!g->pendingSave_.valid()&&!g->settingsSaveQueued_&&SaveFile::verifyCheckpoint(g->activeWorld_.directory.string()),"coalesced_checkpoint_and_preferences_complete");
                Inventory latest;check(latest.load((g->activeWorld_.directory/"voxel_inventory.vxi").string(),g->seed_)&&latest.count(Item::Diamond)==cycle+1,"repeated_checkpoint_item_conservation");
            }
            populate(*g,0);auto& world=*g->world_;auto& chunk=*world.chunks_.begin()->second;
            world.dirtyQueue_.clear();world.dirtySet_.clear();world.priorityDirtyKeys_.clear();const auto revision=chunk.meshRevision;
            for(int i=0;i<10000;++i)world.markDirty(chunk.x,chunk.z,true);
            check(world.dirtyQueue_.size()==1&&world.dirtySet_.size()==1&&chunk.meshRevision==revision+10000&&chunk.meshRevisionToken->load()==chunk.meshRevision,"priority_requests_coalesce_without_losing_revisions");
            auto retired=std::make_unique<World>(991,23);retired->terrainMemoryBudget_=1;retired->renderDistance_=64;
            std::vector<GLuint> buffers;
            for(int i=0;i<12;++i){auto page=std::make_shared<TerrainBufferPage>(256,2);buffers.push_back(page->vbo);buffers.push_back(page->ebo);retired->terrainPages_.push_back(std::move(page));}
            for(int frame=0;frame<80;++frame){retired->terrainPageTrimmedThisFrame_=false;retired->nextTerrainDeletion_={};retired->streamingStats_.pageDeleteCalls=0;
                retired->releaseEmptyTerrainPages();check(retired->terrainPageDeletions_.size()<=8&&retired->streamingStats_.pageDeleteCalls<=1,"bounded_page_deletion_frame");}
            check(retired->terrainPages_.empty()&&retired->terrainPageDeletions_.empty()&&retired->terrainMemory().capacityBytes==0,"page_cleanup_finishes");
            retired.reset();for(auto handle:buffers)check(glIsBuffer(handle)==GL_FALSE,"gpu_handle_released");
            check(glGetError()==GL_NO_ERROR,"gpu_cleanup_no_gl_error");
        }
#endif
        else throw std::runtime_error("Unknown engine fixture");
        g->shutdown();return 0;
    }
};
int runEngineTests(int argc,char** argv){return EngineOptimizationTest::run(argc,argv);}
