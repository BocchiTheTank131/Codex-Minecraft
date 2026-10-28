#include "Game.h"
#include "Transportation.h"
#include "World.h"
#include "Renderer.h"
#include "Player.h"
#include "Weather.h"
#include "Sound.h"
#include "Survival.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#include <psapi.h>
#endif

std::array<glm::vec3,6> World::optimizationScenes() const {
    std::array<glm::vec3,6> scenes{};std::array<bool,4> found{};
    for(int z=-8192;z<=8192;++z) for(int x=-8192;x<=8192;x+=256) {
        if(z%256)continue;
        const auto column=sampleTerrain(x,z);
        const int scene=column.biome==Biome::Plains?0:column.biome==Biome::Forest?1:
            column.height>=150?2:(column.biome==Biome::Ocean || column.biome==Biome::ColdOcean || column.biome==Biome::WarmOcean)?3:-1;
        if(scene>=0 && !found[scene]) {scenes[scene==3?4:scene]={float(x)+.5f,float(column.height+2),float(z)+.5f};found[scene]=true;}
        if(found[0]&&found[1]&&found[2]&&found[3])break;
    }
    for(int i=0;i<3;++i) if(!found[i]) throw std::runtime_error("Missing benchmark terrain scene");
    if(!found[3]) throw std::runtime_error("Missing benchmark ocean scene");
    bool cave=false,ruin=false;
    for(int z=-16;z<16 && (!cave||!ruin);++z)for(int x=-16;x<16;++x)if(auto p=undergroundPlan(x,z)) {
        if(!cave){scenes[3]={float(p->entrance.x)+.5f,float(p->entrance.y),float(p->entrance.z)+.5f};cave=true;}
        if(!ruin && p->kind==UndergroundKind::AncientRuins){const auto& r=p->rooms.front();scenes[5]={float(r.x)+.5f,float(r.y+1),float(r.z)+.5f};ruin=true;}
        if(cave&&ruin)break;
    }
    if(!cave||!ruin)throw std::runtime_error("Missing underground benchmark scene");
    scenes[4].y=float(seaLevel())+.5f;
    return scenes;
}

void Game::runOptimizationBenchmark() {
    namespace fs=std::filesystem;fs::create_directories(optimizationBenchmark_);
    std::ofstream csv(fs::path(optimizationBenchmark_)/"frames.csv"),locations(fs::path(optimizationBenchmark_)/"scenes.csv");
    csv<<"rd,scene,phase,lod,occlusion,fps,mean_ms,p95_ms,p99_ms,worst_ms,cpu_percent,gpu_ms,gpu_samples,focused_percent,visible_percent,ram_mb,private_mb,loaded,rendered,vertices,triangles,opaque_draws,water_draws,terrain_used,terrain_capacity,generation_per_s,mesh_per_s,mesh_ms,upload_ms,render_call_wall_ms,simulation_ms,terrain_vbo_used,terrain_ebo_used,terrain_vbo_capacity,terrain_ebo_capacity,lod0,lod1,lod2,lod3,occlusion_culled,occlusion_queries,frustum_culled,travel_boats,map_main_average_ms,map_main_worst_ms,map_batches,boat_average_ms,boat_worst_ms,boat_steps_max,boat_candidates_max,p999_ms\n";
    locations<<"scene,x,y,z,biome\n";
    settings_.vsync=false;settings_.frameLimit=0;settings_.antiAliasingSamples=0;settings_.simulationDistance=8;
    settings_.renderDistance=optimizationDistance_;settings_.showFps=false;settings_.showCoordinates=false;settings_.mouseSensitivity=.1f;
    settings_.entityDistance=12;settings_.particlePercent=100;settings_.effectQuality=2;showDebug_=false;
    glfwSwapInterval(0);glfwShowWindow(window_);glfwFocusWindow(window_);
#ifdef VOXEL_RENDER_OPTIMIZATION
    settings_.terrainLod=optimizationLod_;settings_.occlusionCulling=optimizationOcclusion_;world_->setTerrainLod(settings_.terrainLod);
#endif
    world_->setSimulationDistance(8);renderer_->setParticlePercent(100);renderer_->setEffectQuality(2);
    const auto scenes=world_->optimizationScenes();
    constexpr const char* names[]={"plains","forest","mountains","cave","ocean","structures","gameplay"};
    struct Timer {GLuint id=0;bool pending=false;int phase=0;};std::array<Timer,8> timers{};
    GLint bits=0;glGetQueryiv(GL_TIME_ELAPSED,GL_QUERY_COUNTER_BITS,&bits);
    const bool gpu=bits>0 && glGetQueryObjectui64v;
    if(gpu)for(auto& q:timers)glGenQueries(1,&q.id);
    int tag=0;
    for(int scene=0;scene<7;++scene) {
        if(optimizationScene_!="all" && optimizationScene_!=names[scene])continue;
        const glm::vec3 origin=scenes[scene==6?0:scene];
        weather_->set(WeatherType::Clear,3600);settings_.weatherQuality=0;
        inventory_->clear();survival_=std::make_unique<SurvivalWorld>(seed_);survival_->setSoundSystem(sounds_.get());survival_->setWeather(weather_.get());
        player_=std::make_unique<Player>(origin);player_->setEquipment(inventory_.get());player_->setSpectatorMode(true);spectatorMode_=true;creativeMode_=false;
        world_->generate(optimizationDistance_,origin);world_->prepareSpawnTerrain(origin);
        const double warm=glfwGetTime(),maxWarm=optimizationDistance_>=64?30:optimizationDistance_>=32?20:10;
        do {
            glfwPollEvents();timing_.previousFrame=glfwGetTime();timing_.worldTime=WorldTime::DefaultSeconds;
            world_->setStreamingView(player_->lookDirection(),5);world_->updateStreaming(origin,8);renderFrame(.016f);
        } while(glfwGetTime()-warm<1 || ((world_->pendingChunkCount() || world_->queuedMeshRebuildCount() || world_->pendingCpuMeshCount() || world_->completedMeshCount()) && glfwGetTime()-warm<maxWarm));
        glm::vec3 position=origin;
        if(scene!=3 && scene!=4 && scene!=5) position=world_->findSafeSpawnNear(int(origin.x),int(origin.z));
        player_=std::make_unique<Player>(position);player_->setEquipment(inventory_.get());creativeMode_=spectatorMode_=false;
        if(scene==4){creativeMode_=true;player_->setCreativeMode(true);player_->setFlying(true);}
        for(int i=0;i<30;++i){player_->update(1.f/60,PlayerInput{},*world_);world_->updateStreaming(player_->cameraPosition(),8);}
        if(scene==6) {
            for(int piece=0;piece<4;++piece){inventory_->add(static_cast<Item>(int(Item::DiamondHelmet)+piece));inventory_->equipFromSlot(0);}
            inventory_->add(Item::Shield);inventory_->equipFromSlot(0);inventory_->add(Item::IronSword);
            survival_->spawnBillboardPreview(player_->position(),player_->lookDirection());
            weather_->set(WeatherType::Rain,3600);settings_.weatherQuality=2;
        }
        if(transportBenchmark_){
            transportation_=std::make_unique<Transportation>(world_->findSurvivalSpawnNear(0,0));
            const auto id=transportation_->createMap(player_->position(),0);inventory_->add(Item::FilledMap,1,0,id);inventory_->selectSlot(0);viewedMap_=id;
            for(int i=0;i<6;++i)transportation_->placeBoat(*world_,Item(int(Item::OakBoat)+i),player_->position()+glm::vec3((i%3)*2,0,-4-(i/3)*2),0);
        }
        const auto metadata=world_->generationDebugAt(int(position.x),int(position.z));
        locations<<names[scene]<<','<<position.x<<','<<position.y<<','<<position.z<<','<<metadata.biome<<'\n';locations.flush();
        for(int phase=0;phase<3;++phase) {
            glfwFocusWindow(window_);
            transportation_->mapBatches=0;transportation_->mapTotalMilliseconds=transportation_->mapWorstMilliseconds=0;
            ++tag;std::vector<double> frames;frames.reserve(20000);
            double boatTotal=0,boatWorst=0;unsigned boatSteps=0;std::uint64_t boatCandidates=0;
            double gpuTotal=0,renderTotal=0,simulationTotal=0;unsigned gpuSamples=0,focused=0,visible=0;
            const double start=glfwGetTime();double previous=start;int edit=-1;
            const auto target=glm::ivec3(glm::floor(player_->position()+glm::vec3(2,1,-3)));
            const Block original=world_->getBlock(target.x,target.y,target.z);
#ifdef _WIN32
            FILETIME create,exit,k0,u0,k1,u1;GetProcessTimes(GetCurrentProcess(),&create,&exit,&k0,&u0);
#endif
            while(glfwGetTime()-start<3) {
                const double now=glfwGetTime();const float dt=float(std::min(.05,now-previous));previous=now;
                timing_.previousFrame=now;timing_.frameMilliseconds=dt*1000;
                glfwPollEvents();timing_.worldTime=WorldTime::DefaultSeconds;
                if(phase==1)player_->addMouseMovement(double(dt)*220,0);
                if(phase==2 && int((now-start)*2)!=edit) {
                    edit=int((now-start)*2);world_->setBlock(target.x,target.y,target.z,edit%2?original:Block::Stone);
                }
                const auto simStart=glfwGetTime();sounds_->update(dt);updateSimulation(dt);simulationTotal+=(glfwGetTime()-simStart)*1000;
                boatTotal+=transportation_->boatMilliseconds;boatWorst=std::max(boatWorst,double(transportation_->boatMilliseconds));boatSteps=std::max(boatSteps,transportation_->boatSteps);boatCandidates=std::max(boatCandidates,transportation_->boatCollisionCandidates);
                Timer* active=nullptr;
                if(gpu)for(auto& q:timers) {
                    if(q.pending) {
                        GLuint ready=0;glGetQueryObjectuiv(q.id,GL_QUERY_RESULT_AVAILABLE,&ready);
                        if(ready){GLuint64 ns=0;glGetQueryObjectui64v(q.id,GL_QUERY_RESULT,&ns);q.pending=false;if(q.phase==tag){gpuTotal+=double(ns)/1e6;++gpuSamples;}}
                    }
                    if(!active && !q.pending)active=&q;
                }
                if(active){active->pending=true;active->phase=tag;optimizationGpuQueryActive_=true;glBeginQuery(GL_TIME_ELAPSED,active->id);}
                const auto renderStart=glfwGetTime();renderFrame(dt);renderTotal+=(glfwGetTime()-renderStart)*1000;
                if(optimizationGpuQueryActive_){glEndQuery(GL_TIME_ELAPSED);optimizationGpuQueryActive_=false;}
                focused+=glfwGetWindowAttrib(window_,GLFW_FOCUSED)!=0;
                visible+=glfwGetWindowAttrib(window_,GLFW_VISIBLE)!=0 && !glfwGetWindowAttrib(window_,GLFW_ICONIFIED);
                frames.push_back((glfwGetTime()-now)*1000);
            }
            const double elapsed=glfwGetTime()-start;const auto n=frames.size();std::sort(frames.begin(),frames.end());
            double cpu=0,ram=0,privateRam=0;
#ifdef _WIN32
            GetProcessTimes(GetCurrentProcess(),&create,&exit,&k1,&u1);
            const auto ticks=[](FILETIME f){return (std::uint64_t(f.dwHighDateTime)<<32)|f.dwLowDateTime;};
            cpu=double(ticks(k1)-ticks(k0)+ticks(u1)-ticks(u0))/1e7/elapsed/std::max(1U,std::thread::hardware_concurrency())*100;
            PROCESS_MEMORY_COUNTERS_EX processMemory{};GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&processMemory),sizeof(processMemory));
            ram=processMemory.WorkingSetSize/1048576.0;privateRam=processMemory.PrivateUsage/1048576.0;
#endif
            const auto stages=world_->streamingDiagnostics();const auto memory=world_->terrainMemory();
            std::size_t renderedVertices=0,renderedTriangles=0;int waterDraws=0;
#ifdef VOXEL_RENDER_OPTIMIZATION
            renderedVertices=world_->renderedVertexCount();renderedTriangles=world_->renderedTriangleCount();waterDraws=world_->waterSubmissionCount();
#else
            const auto& visibleChunks=world_->collectVisibleChunks(glm::perspective(glm::radians(settings_.fov),1280.f/720.f,.1f,float((optimizationDistance_+3)*16)*1.45f)*player_->viewMatrix(),player_->cameraPosition());
            for(const auto* c:visibleChunks.opaque)renderedVertices+=c->opaqueVertexCount;
            for(const auto* c:visibleChunks.water){renderedVertices+=c->waterVertexCount;++waterDraws;}
            renderedTriangles=renderedVertices/3;
#endif
            csv<<optimizationDistance_<<','<<names[scene]<<','<<std::array<const char*,3>{"stationary","rotation","editing"}[phase]<<','<<optimizationLod_<<','<<optimizationOcclusion_<<','
                <<n/elapsed<<','<<elapsed*1000/n<<','<<frames[(n-1)*95/100]<<','<<frames[(n-1)*99/100]<<','<<frames.back()<<','<<cpu<<','
                <<(gpuSamples?gpuTotal/gpuSamples:-1)<<','<<gpuSamples<<','<<100.0*focused/n<<','<<100.0*visible/n<<','<<ram<<','<<privateRam<<','
                <<world_->loadedChunkCount()<<','<<world_->renderedChunkCount()<<','<<renderedVertices<<','<<renderedTriangles<<','<<world_->opaqueSubmissionCount()<<','<<waterDraws<<','
                <<memory.usedBytes<<','<<memory.capacityBytes<<','<<stages.generatedPerSecond<<','<<stages.meshedPerSecond<<','<<world_->lastChunkRebuildMilliseconds()<<','<<world_->lastMeshUploadMilliseconds()<<','<<renderTotal/n<<','<<simulationTotal/n;
#ifdef VOXEL_RENDER_OPTIMIZATION
            const auto lodCounts=world_->renderedLodCounts();
            csv<<','<<memory.vertexBytes<<','<<memory.indexBytes<<','<<memory.vertexCapacityBytes<<','<<memory.indexCapacityBytes;
            for(auto count:lodCounts)csv<<','<<count;
            csv<<','<<renderer_->occlusionCulledCount()<<','<<renderer_->occlusionQueryCount()<<','<<world_->frustumCulledCount();
#else
            csv<<','<<memory.usedBytes<<",0,"<<memory.capacityBytes<<",0,"<<world_->renderedChunkCount()<<",0,0,0,0,0,-1";
#endif
            csv<<','<<transportation_->boats().size()<<','<<(transportation_->mapBatches?transportation_->mapTotalMilliseconds/transportation_->mapBatches:0)<<','<<transportation_->mapWorstMilliseconds<<','<<transportation_->mapBatches<<','<<boatTotal/n<<','<<boatWorst<<','<<boatSteps<<','<<boatCandidates<<','<<frames[(n-1)*999/1000]<<'\n';csv.flush();
            if(phase==2)world_->setBlock(target.x,target.y,target.z,original);
            if(phase==0){screenshotRequested_=true;renderFrame(0);}
        }
        std::cout<<"Foreground fixture "<<names[scene]<<" RD"<<optimizationDistance_<<" recorded\n";
    }
    for(auto& q:timers)if(q.id)glDeleteQueries(1,&q.id);
    std::cout<<"Optimization benchmark complete: "<<optimizationBenchmark_<<'\n';
}
