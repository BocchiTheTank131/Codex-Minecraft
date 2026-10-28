#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Weather.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#include <psapi.h>
#endif

void Game::runStreamingBenchmark() {
    namespace fs=std::filesystem;
    fs::create_directories(streamingBenchmark_);
    std::ofstream csv(fs::path(streamingBenchmark_)/"streaming.csv");
    std::ofstream samples(fs::path(streamingBenchmark_)/"streaming-samples.csv");
    samples<<"generator,rd,phase,seconds,ram_mb,private_mb,loaded,rendered,dirty,generation,mesh,uploads,retired,lighting\n";
    std::ofstream retirement(fs::path(streamingBenchmark_)/"retirement.csv");
    std::ofstream slow(fs::path(streamingBenchmark_)/"slow-frames.csv");
    slow<<"rd,phase,frame,frame_ms,stream_ms,render_ms,retire_ms,unload_ms,allocation_ms,subdata_ms,snapshot_ms,lighting_ms\n";
    retirement<<"phase,frame,retire_ms,unload_ms,fence_ms,ranges_ms,delete_ms,trim_ms,payload_ms,vbo_delete_ms,ebo_delete_ms,vao_delete_ms,delete_calls,page_allocations\n";
    csv<<"generator,rd,phase,fps,mean_ms,p99_ms,worst_ms,cpu_percent,ram_mb,loaded,rendered,dirty,generation,mesh,upload_queue,stream_ms,render_ms,mesh_ms,upload_ms,gen_active,mesh_active,gen_limit,mesh_limit,gen_per_s,mesh_per_s,integrate_ms,light_ms,snapshot_ms,upload_batch_ms,stale,peak_unload_ms,peak_retire_ms,retired,lighting_queue,recovery_seconds,terrain_vertices,terrain_used_bytes,terrain_capacity_bytes,opaque_draws,p95_ms,p999_ms,page_delete_calls,page_allocations\n";
    settings_.vsync=false; settings_.frameLimit=0; settings_.antiAliasingSamples=0;
    settings_.weatherQuality=0; settings_.simulationDistance=8; glfwSwapInterval(0);
    showDebug_=false; weather_->set(WeatherType::Clear,3600);
    player_->setSpectatorMode(true);
    world_->setSimulationDistance(8);
    const auto distances=streamingFixtureDistance_?std::vector<int>{streamingFixtureDistance_}:std::vector<int>{8,16,32,64};
    for(int rd : distances) {
        settings_.renderDistance=rd;
        const glm::vec3 origin(0.5f,150.f,0.5f);
        player_->teleport(origin); world_->generate(rd,origin);
        for(int phase=0;phase<4;++phase) {
            std::vector<double> frames; frames.reserve(20000);
            const double duration=phase==0?20:phase==1?streamingFlightSeconds_:phase==2?streamingRecoverySeconds_:20;
            if(phase==3) player_->teleport({-4096.5f,150.f,4096.5f});
            double streamMs=0,renderMs=0,integrateMs=0,lightMs=0,snapshotMs=0,uploadMs=0;
            double genActive=0,meshActive=0,genRate=0,meshRate=0;
            float peakUnload=0,peakRetire=0;double recovery=-1;std::uint64_t pageDeletes=0,pageAllocations=0;
#ifdef _WIN32
            FILETIME create,exit,kernel0,user0,kernel1,user1;
            GetProcessTimes(GetCurrentProcess(),&create,&exit,&kernel0,&user0);
#endif
            const double start=glfwGetTime();
            double previous=start;
            double nextMemorySample=0;
            while(glfwGetTime()-start<duration) {
                const double t=glfwGetTime(); const float dt=static_cast<float>(t-previous); previous=t;
                glfwPollEvents(); timing_.worldTime=WorldTime::DefaultSeconds;
                // Player spectator base speed 8.5 * maximum multiplier 8.
                if(phase==1) player_->teleport(origin+glm::vec3(float(t-start)*68.f,0,-float(t-start)*17.f));
                world_->setStreamingView(player_->lookDirection(),dt*1000);
                world_->updateStreaming(player_->cameraPosition(),2);
                const auto stages=world_->streamingDiagnostics();
                if(stages.retireMilliseconds>.5f || stages.unloadMilliseconds>.5f)
                    retirement<<phase<<','<<frames.size()<<','<<stages.retireMilliseconds<<','<<stages.unloadMilliseconds<<','<<stages.fencePollMilliseconds<<','<<stages.rangeReleaseMilliseconds<<','<<stages.bufferDeleteMilliseconds<<','<<stages.pageTrimMilliseconds<<','<<stages.payloadReleaseMilliseconds<<','<<stages.pageVboDeleteMilliseconds<<','<<stages.pageEboDeleteMilliseconds<<','<<stages.pageVaoDeleteMilliseconds<<','<<stages.pageDeleteCalls<<','<<stages.pageAllocations<<'\n';
                pageDeletes+=stages.pageDeleteCalls;pageAllocations+=stages.pageAllocations;
                peakUnload=std::max(peakUnload,stages.unloadMilliseconds);peakRetire=std::max(peakRetire,stages.retireMilliseconds);
                if(phase==2 && recovery<0 && !world_->pendingChunkCount() && !world_->queuedMeshRebuildCount() &&
                    !world_->pendingCpuMeshCount() && !world_->completedMeshCount() && !stages.lightingQueued && !stages.retiredChunks) recovery=glfwGetTime()-start;
                integrateMs+=stages.integrateMilliseconds; lightMs+=stages.lightingMilliseconds;
                snapshotMs+=stages.snapshotMilliseconds; uploadMs+=stages.uploadMilliseconds;
                genActive+=stages.generationActive;meshActive+=stages.meshActive;
                genRate+=stages.generatedPerSecond;meshRate+=stages.meshedPerSecond;
                const double after=glfwGetTime(); streamMs+=(after-t)*1000;
                renderFrame(std::min(dt,.05f));const auto render=(glfwGetTime()-after)*1000;renderMs+=render;
                const auto frame=(glfwGetTime()-t)*1000;
                if(frame>8)slow<<rd<<','<<phase<<','<<frames.size()<<','<<frame<<','<<(after-t)*1000<<','<<render<<','<<stages.retireMilliseconds<<','<<stages.unloadMilliseconds<<','<<stages.bufferAllocationMilliseconds<<','<<stages.bufferUploadMilliseconds<<','<<stages.snapshotMilliseconds<<','<<stages.lightingMilliseconds<<'\n';
                frames.push_back(frame);
                // A sparse trace distinguishes growing cleanup debt from a
                // denser destination that legitimately needs larger meshes.
                if(t-start>=nextMemorySample) {
                    double workingSet=0,privateBytes=0;
#ifdef _WIN32
                    PROCESS_MEMORY_COUNTERS_EX memory{};
                    if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))) {
                        workingSet=memory.WorkingSetSize/1048576.0;
                        privateBytes=memory.PrivateUsage/1048576.0;
                    }
#endif
                    samples<<world_->generationVersion()<<','<<rd<<','<<phase<<','<<t-start<<','<<workingSet<<','<<privateBytes<<','
                        <<world_->loadedChunkCount()<<','<<world_->renderedChunkCount()<<','<<world_->queuedMeshRebuildCount()<<','
                        <<world_->pendingChunkCount()<<','<<world_->pendingCpuMeshCount()<<','<<world_->completedMeshCount()<<','
                        <<stages.retiredChunks<<','<<stages.lightingQueued<<'\n';
                    samples.flush();nextMemorySample=t-start+5;
                }
            }
            const double elapsed=glfwGetTime()-start;
            double cpu=0,ram=0;
#ifdef _WIN32
            GetProcessTimes(GetCurrentProcess(),&create,&exit,&kernel1,&user1);
            const auto ticks=[](FILETIME f){return (std::uint64_t(f.dwHighDateTime)<<32)|f.dwLowDateTime;};
            cpu=double(ticks(kernel1)-ticks(kernel0)+ticks(user1)-ticks(user0))/1e7/elapsed/
                std::max(1U,std::thread::hardware_concurrency())*100;
            PROCESS_MEMORY_COUNTERS pm{};
            if(GetProcessMemoryInfo(GetCurrentProcess(),&pm,sizeof(pm))) ram=pm.WorkingSetSize/1048576.0;
#endif
            std::sort(frames.begin(),frames.end()); const auto n=frames.size();
            const auto memory=world_->terrainMemory();
            csv<<world_->generationVersion()<<','<<rd<<','<<std::array<const char*,4>{"warmup","flight8x","recovery","teleport"}[phase]<<','
                <<n/elapsed<<','<<elapsed*1000/n<<','<<frames[(n-1)*99/100]<<','<<frames.back()<<','<<cpu<<','<<ram<<','
                <<world_->loadedChunkCount()<<','<<world_->renderedChunkCount()<<','<<world_->queuedMeshRebuildCount()<<','
                <<world_->pendingChunkCount()<<','<<world_->pendingCpuMeshCount()<<','<<world_->completedMeshCount()<<','
                <<streamMs/n<<','<<renderMs/n<<','<<world_->lastChunkRebuildMilliseconds()<<','<<world_->lastMeshUploadMilliseconds()<<','
                <<genActive/n<<','<<meshActive/n<<','<<world_->streamingDiagnostics().generationLimit<<','
                <<world_->streamingDiagnostics().meshLimit<<','<<genRate/n<<','<<meshRate/n<<','
                <<integrateMs/n<<','<<lightMs/n<<','<<snapshotMs/n<<','<<uploadMs/n<<','<<world_->streamingDiagnostics().staleMeshes<<','<<peakUnload<<','<<peakRetire<<','<<world_->streamingDiagnostics().retiredChunks<<','<<world_->streamingDiagnostics().lightingQueued<<','<<recovery<<','<<memory.vertices<<','<<memory.usedBytes<<','<<memory.capacityBytes<<','<<world_->opaqueSubmissionCount()<<','<<frames[(n-1)*95/100]<<','<<frames[(n-1)*999/1000]<<','<<pageDeletes<<','<<pageAllocations<<'\n';
            csv.flush();
        }
    }
    std::cout<<"Streaming flight benchmark saved to "<<streamingBenchmark_<<'\n';
}
