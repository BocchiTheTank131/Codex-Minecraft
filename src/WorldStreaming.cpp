#include "World.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

void World::workerLoop(unsigned workerIndex) {
    while (!stopping_) {
        GenerationRequest request;
        RetiredChunkPayload payload;
        bool releasing=false;
        {
            std::unique_lock<std::mutex> lock(generationMutex_);
            generationCv_.wait(lock, [&] { return stopping_ || (workerIndex < generationWorkerLimit_.load() && (!generationQueue_.empty() || !retiredChunkPayloads_.empty())); });
            if (stopping_)
                return;
            // Retired arrays are detached, immutable and contain no GL handles.
            // Prefer cleanup above the bounded admission watermark so a long
            // flight cannot accumulate an indefinitely growing memory debt.
            if(!retiredChunkPayloads_.empty() && (generationQueue_.empty() || retiredChunkPayloads_.size()>=96)) {
                payload=std::move(retiredChunkPayloads_.front());retiredChunkPayloads_.pop_front();
                ++activePayloadReleases_;releasing=true;
            } else {
                request = generationQueue_.front();generationQueue_.pop_front();
            }
        }
        if(releasing) {
            payload={}; // Heap frees happen outside the queue lock.
            --activePayloadReleases_;
            continue;
        }
        const auto relevant = [&] {
            if(request.epoch!=generationEpoch_.load(std::memory_order_acquire)) return false;
            const std::uint64_t center = static_cast<std::uint64_t>(
                generationCenterKey_.load(std::memory_order_relaxed));
            const int centerX = static_cast<std::int32_t>(center >> 32U);
            const int centerZ = static_cast<std::int32_t>(center);
            const int radius = generationLoadRadius_.load(std::memory_order_relaxed) + 1;
            const std::int64_t dx = static_cast<std::int64_t>(request.x) - centerX;
            const std::int64_t dz = static_cast<std::int64_t>(request.y) - centerZ;
            return std::int64_t(dx) * dx + std::int64_t(dz) * dz <= radius * radius;
        };
        if (!relevant()) {
            std::lock_guard<std::mutex> lock(generationMutex_);
            if(request.epoch==generationEpoch_.load()) pendingKeys_.erase(chunkKey(request.x, request.y));
            continue;
        }
        ++activeGenerationWorkers_;
        const auto generationStart=std::chrono::steady_clock::now();
        GeneratedChunk generated = generateChunkData(request.x, request.y);
        generationMicroseconds_.fetch_add(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-generationStart).count(),std::memory_order_relaxed);
        generated.epoch=request.epoch;
        --activeGenerationWorkers_;
        ++generationCompleted_;
        {
            std::lock_guard<std::mutex> lock(generationMutex_);
            if (relevant())
                completedQueue_.push_back(std::move(generated));
            else if(request.epoch==generationEpoch_.load())
                pendingKeys_.erase(chunkKey(request.x, request.y));
        }
    }
}

void World::updateStreaming(const glm::vec3& playerPosition, int meshBudget) {
    if(!std::isfinite(playerPosition.x)||!std::isfinite(playerPosition.y)||!std::isfinite(playerPosition.z)||std::abs(double(playerPosition.x))>30000000||std::abs(double(playerPosition.z))>30000000)return;
    using Clock=std::chrono::steady_clock;
    const auto start=Clock::now();
    updateTerrainLod(playerPosition);
    terrainPageTrimmedThisFrame_=false;
    streamingStats_.unloadMilliseconds=0;
    streamingStats_.fencePollMilliseconds=streamingStats_.rangeReleaseMilliseconds=streamingStats_.bufferDeleteMilliseconds=streamingStats_.pageTrimMilliseconds=streamingStats_.payloadReleaseMilliseconds=0;
    // A soft budget: individual sunlight/GL operations remain indivisible.
    // Always make progress even after a slow frame; never cut load distance.
    streamingBudgetMilliseconds_=streamingQuality_==ChunkStreamingQuality::Low ? .5f :
        std::clamp((16.667f-streamingFrameMilliseconds_)*.18f,.5f,2.f);
    const int centerX = floorDiv(static_cast<int>(std::floor(playerPosition.x)), CHUNK_SIZE);
    const int centerZ = floorDiv(static_cast<int>(std::floor(playerPosition.z)), CHUNK_SIZE);
    if (!streamCenterValid_ || requestRadius_<0 || streamCenter_.x != centerX || streamCenter_.y != centerZ) {
        streamCenter_ = {centerX, centerZ};streamCenterValid_=true;
        generationLoadRadius_.store(
            std::max(renderDistance_, simulationDistance_), std::memory_order_relaxed);
        generationCenterKey_.store(chunkKey(centerX, centerZ), std::memory_order_relaxed);
        nextRequestOffset_ = 0;
        discardObsoleteRequests(centerX, centerZ);
        const auto unloadStart=Clock::now();
        unloadDistant(centerX, centerZ);
        streamingStats_.unloadMilliseconds=std::chrono::duration<float,std::milli>(Clock::now()-unloadStart).count();
        lastStreamingPriority_={};
    }
    if(std::chrono::duration<float>(start-lastStreamingPriority_).count()>=.25f) {
        reprioritizeStreaming(); lastStreamingPriority_=start;
    }
    const auto integrateStart=Clock::now();
    releaseRetiredTerrain();
    releaseEmptyTerrainPages();
    streamingStats_.retireMilliseconds=std::chrono::duration<float,std::milli>(Clock::now()-integrateStart).count();
    integrateCompleted(static_cast<int>(workerPlan_.generation*2), centerX, centerZ);
    const auto lightingStart=Clock::now();
    if (!lightingQueue_.empty()) {
        const glm::ivec2 update = lightingQueue_.front();
        lightingQueue_.pop_front();
        lightingSet_.erase(chunkKey(update.x, update.y));
        // An unloaded lighting region has no destination to modify. Prune it
        // before source discovery, without skipping edge regions or clearing.
        bool destination=false;
        for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x) destination|=findChunk(update.x+x,update.y+z)!=nullptr;
        if(destination) rebuildBlockLightingNear(update.x, update.y);
    }
    const auto uploadStart=Clock::now();
    requestChunksAround(centerX, centerZ);
    const int parallelBudget=meshBudget==0 ? 0 : std::max(meshBudget,int(workerPlan_.mesh*2));
    uploadCompletedMeshes(parallelBudget);
    const auto snapshotStart=Clock::now();
    dispatchMeshJobs(parallelBudget);
    const auto end=Clock::now();
    const auto milliseconds=[](auto a,auto b){return std::chrono::duration<float,std::milli>(b-a).count();};
    streamingStats_.integrateMilliseconds=milliseconds(integrateStart,lightingStart);
    streamingStats_.lightingMilliseconds=milliseconds(lightingStart,uploadStart);
    streamingStats_.uploadMilliseconds=milliseconds(uploadStart,snapshotStart);
    streamingStats_.snapshotMilliseconds=milliseconds(snapshotStart,end);
    const float sampleSeconds=std::chrono::duration<float>(end-lastStreamingSample_).count();
    if(sampleSeconds>=.5f) {
        const auto gen=generationCompleted_.load(),mesh=meshCompleted_.load();
        if(lastStreamingSample_!=Clock::time_point{}) {
            streamingStats_.generatedPerSecond=float(gen-sampledGeneration_)/sampleSeconds;
            streamingStats_.meshedPerSecond=float(mesh-sampledMeshes_)/sampleSeconds;
        }
        sampledGeneration_=gen; sampledMeshes_=mesh; lastStreamingSample_=end;
        // When meshes lag, rebalance CPU toward mesh workers. The generation
        // admission queue stays bounded and resumes automatically on recovery.
        generationWorkerLimit_=dirtySet_.size()>workerPlan_.mesh*128 ?
            std::min(workerPlan_.generation,2U):workerPlan_.generation;
        meshWorkerLimit_=std::min(workerPlan_.mesh,std::max(1U,unsigned(dirtySet_.size()+31)/32));
        generationCv_.notify_all(); meshCv_.notify_all();
    }
}

void World::releaseRetiredTerrain(bool drain) {
    const auto start=std::chrono::steady_clock::now();
    unsigned count=0;
    while(!retiredTerrain_.empty()) {
        const auto& fence=retiredTerrain_.front().fence;
        if(!drain && fence && !fence->complete) {
            const auto pollStart=std::chrono::steady_clock::now();
            const GLenum state=glClientWaitSync(fence->handle,0,0);
            streamingStats_.fencePollMilliseconds+=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-pollStart).count();
            if(state==GL_WAIT_FAILED) throw std::runtime_error("GPU retirement fence wait failed");
            if(state!=GL_ALREADY_SIGNALED && state!=GL_CONDITION_SATISFIED) break;
            fence->complete=true;
        }
        auto mesh=std::move(retiredTerrain_.front());retiredTerrain_.pop_front();
        const auto rangeStart=std::chrono::steady_clock::now();
        if(mesh.page) mesh.page->release(mesh.first,mesh.count);
        if(mesh.waterPage)mesh.waterPage->release(mesh.waterFirst,mesh.waterCount);
        const auto deleteStart=std::chrono::steady_clock::now();
        streamingStats_.rangeReleaseMilliseconds+=std::chrono::duration<float,std::milli>(deleteStart-rangeStart).count();
        if(mesh.vbo) glDeleteBuffers(1,&mesh.vbo);
        if(mesh.vao) glDeleteVertexArrays(1,&mesh.vao);
        const auto payloadStart=std::chrono::steady_clock::now();
        streamingStats_.bufferDeleteMilliseconds+=std::chrono::duration<float,std::milli>(payloadStart-deleteStart).count();
        if(!drain && !stopping_) {
            std::lock_guard<std::mutex> lock(generationMutex_);
            retiredChunkPayloads_.push_back({std::move(mesh.blocks),std::move(mesh.light),std::move(mesh.emitters)});
        }
        streamingStats_.payloadReleaseMilliseconds+=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-payloadStart).count();
        ++count;
        if(!drain && count>=4 && std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count()>=.5f) break;
    }
    if(count) releaseEmptyTerrainPages();
    if(count && !drain) generationCv_.notify_all();
}

void World::setStreamingQuality(ChunkStreamingQuality quality) {
    streamingQuality_=quality;
    workerPlan_=streamingWorkerPlan(std::thread::hardware_concurrency(),quality);
    workerPlan_.generation=std::min(workerPlan_.generation,unsigned(workers_.size()));
    workerPlan_.mesh=std::min(workerPlan_.mesh,unsigned(meshWorkers_.size()));
    generationWorkerLimit_=workerPlan_.generation; meshWorkerLimit_=workerPlan_.mesh;
    generationCv_.notify_all(); meshCv_.notify_all();
}

void World::setStreamingView(const glm::vec3& direction,float frameMilliseconds) {
    const glm::vec2 forward(direction.x,direction.z);
    if(glm::length(forward)>.001f) streamingForward_=glm::normalize(forward);
    streamingFrameMilliseconds_=std::isfinite(frameMilliseconds)?std::max(0.f,frameMilliseconds):16.f;
}

ChunkStreamingDiagnostics World::streamingDiagnostics() const {
    auto stats=streamingStats_;
    { std::lock_guard<std::mutex> lock(generationMutex_);
      stats.generationQueued=int(generationQueue_.size());stats.generationReady=int(completedQueue_.size());
      stats.retiredChunks=int(retiredChunkPayloads_.size())+int(activePayloadReleases_.load()); }
    { std::lock_guard<std::mutex> lock(meshMutex_);
      stats.meshQueued=int(meshQueue_.size());stats.meshReady=int(completedMeshes_.size());stats.meshActive=int(meshWorkersBusy_); }
    stats.generationActive=int(activeGenerationWorkers_.load()+activePayloadReleases_.load());
    stats.generationLimit=generationWorkerLimit_; stats.meshLimit=meshWorkerLimit_;
    stats.retiredChunks+=int(retiredTerrain_.size());stats.lightingQueued=int(lightingQueue_.size());
    stats.generated=generationCompleted_;stats.meshed=meshCompleted_;stats.staleMeshes=staleMeshCount_;
    stats.generationMilliseconds=stats.generated?float(generationMicroseconds_.load())/float(stats.generated)/1000.f:0;
    return stats;
}

void World::reprioritizeStreaming() {
    std::vector<std::int64_t> ready;
    ready.reserve(dirtySet_.size());
    {
        std::lock_guard<std::mutex> lock(meshMutex_);
        for(const auto key:dirtySet_) if(chunks_.find(key)!=chunks_.end() && pendingMeshKeys_.find(key)==pendingMeshKeys_.end()) ready.push_back(key);
    }
    const auto score=[&](std::int64_t key) {
        const int x=static_cast<std::int32_t>(std::uint64_t(key)>>32),z=static_cast<std::int32_t>(key);
        return streamingPriority(x-streamCenter_.x,z-streamCenter_.y,streamingForward_,priorityDirtyKeys_.count(key)!=0);
    };
    std::sort(ready.begin(),ready.end(),[&](auto a,auto b) {const auto sa=score(a),sb=score(b);return sa==sb?a<b:sa<sb;});
    dirtyQueue_.assign(ready.begin(),ready.end());
    {
        std::lock_guard<std::mutex> lock(meshMutex_);
        const auto compare=[&](const auto& a,const auto& b){return score(chunkKey(a.x,a.z))<score(chunkKey(b.x,b.z));};
        std::stable_sort(meshQueue_.begin(),meshQueue_.end(),compare);
        std::stable_sort(completedMeshes_.begin(),completedMeshes_.end(),compare);
    }
    std::lock_guard<std::mutex> lock(generationMutex_);
    std::stable_sort(generationQueue_.begin(),generationQueue_.end(),[&](auto a,auto b){
        return streamingPriority(a.x-streamCenter_.x,a.y-streamCenter_.y,streamingForward_)<
               streamingPriority(b.x-streamCenter_.x,b.y-streamCenter_.y,streamingForward_);
    });
    // Integrating nearby results before distant ones makes their halos complete
    // earlier. Completed generation is capped by the existing admission limit.
    std::stable_sort(completedQueue_.begin(),completedQueue_.end(),[&](const auto& a,const auto& b){
        return streamingPriority(a.x-streamCenter_.x,a.z-streamCenter_.y,streamingForward_)<
               streamingPriority(b.x-streamCenter_.x,b.z-streamCenter_.y,streamingForward_);
    });
}

int World::pendingCpuMeshCount() const {
    std::lock_guard<std::mutex> lock(meshMutex_);
    return static_cast<int>(meshQueue_.size()) + static_cast<int>(meshWorkersBusy_);
}

int World::completedMeshCount() const {
    std::lock_guard<std::mutex> lock(meshMutex_);
    return static_cast<int>(completedMeshes_.size());
}

void World::dispatchMeshJobs(int budget) {
    const std::size_t MaximumMeshJobs = workerPlan_.mesh * 8 + 4;
    const std::size_t MaximumBackgroundJobs = workerPlan_.mesh * 8;
    const auto dispatchStart=std::chrono::steady_clock::now();
    // Priority inserts can leave obsolete queue entries behind. Bound the
    // number examined so a burst of edits cannot drain them in one frame.
    int dispatched = 0;
    int examined = 0;
    const int maximumExamined = std::max(128, budget * 16);
    while (dispatched < budget && examined < maximumExamined &&
           !dirtyQueue_.empty()) {
        ++examined;
        const std::int64_t key = dirtyQueue_.front();
        dirtyQueue_.pop_front();
        if (dirtySet_.find(key) == dirtySet_.end())
            continue;
        const auto found = chunks_.find(key);
        if (found == chunks_.end()) {
            dirtySet_.erase(key);
            priorityDirtyKeys_.erase(key);
            continue;
        }
        const bool priority = priorityDirtyKeys_.find(key) != priorityDirtyKeys_.end();
        {
            std::lock_guard<std::mutex> lock(meshMutex_);
            if (pendingMeshKeys_.find(key) != pendingMeshKeys_.end())
                continue;
            const std::size_t limit = priority ? MaximumMeshJobs : MaximumBackgroundJobs;
            if (pendingMeshKeys_.size() >= limit) {
                dirtyQueue_.push_front(key);
                break;
            }
        }
        // Coalesce arrivals: an absent required halo neighbor would make
        // this halo obsolete as soon as generation integrates it.
        bool waiting=false;
        const int radius=std::max(renderDistance_,simulationDistance_);
        for(int nz=-1;nz<=1;++nz)for(int nx=-1;nx<=1;++nx) {
            const int x=found->second->x+nx,z=found->second->z+nz;
            const int dx=x-streamCenter_.x,dz=z-streamCenter_.y;
            if(std::int64_t(dx)*dx+std::int64_t(dz)*dz<=radius*radius && !findChunk(x,z)) waiting=true;
        }
        if(waiting && !priority) { dirtyQueue_.push_back(key); continue; }
        MeshInput input = captureMeshInput(found->second->x, found->second->z);
        {
            std::lock_guard<std::mutex> lock(meshMutex_);
            pendingMeshKeys_.emplace(key, input.identity);
            if (priority)
                meshQueue_.push_front(std::move(input));
            else
                meshQueue_.push_back(std::move(input));
        }
        priorityDirtyKeys_.erase(key);
        ++dispatched;
        if(std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-dispatchStart).count()>=streamingBudgetMilliseconds_) break;
    }
    if (dispatched > 0) meshCv_.notify_all();
}

void World::packMeshOutput(MeshOutput& result) {
    TerrainPacking::indexQuads(result.opaque,result.packedOpaque,result.opaqueIndices);
    TerrainPacking::indexQuads(result.water,result.packedWater,result.waterIndices);
}
void World::meshWorkerLoop(unsigned workerIndex) {
    MeshOutput scratch; // Private full-precision build storage, never queued/shared.
    while (!stopping_) {
        MeshInput input;
        MeshOutput reusable;
        {
            std::unique_lock<std::mutex> lock(meshMutex_);
            meshCv_.wait(lock, [&] { return stopping_ || (workerIndex < meshWorkerLimit_.load() && !meshQueue_.empty()); });
            if (stopping_)
                return;
            input = std::move(meshQueue_.front());
            meshQueue_.pop_front();
            ++meshWorkersBusy_;
            if (!recycledMeshes_.empty()) {
                reusable = std::move(recycledMeshes_.front());
                recycledMeshes_.pop_front();
            }
        }
        MeshOutput result;
        if(input.revisionToken && input.revisionToken->load(std::memory_order_acquire)!=input.revision) {
            result.x=input.x; result.z=input.z; result.identity=input.identity;
            result.revision=input.revision; result.epoch=input.epoch;
        } else {
            scratch.packedOpaque=std::move(reusable.packedOpaque);
            scratch.packedWater=std::move(reusable.packedWater);
            scratch.opaqueIndices=std::move(reusable.opaqueIndices);scratch.waterIndices=std::move(reusable.waterIndices);
            result=buildMesh(input,std::move(scratch));
            const auto packingStart=std::chrono::steady_clock::now();
            packMeshOutput(result);
            result.buildMilliseconds+=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-packingStart).count();
            scratch.opaque=std::move(result.opaque);scratch.water=std::move(result.water);
            if(scratch.opaque.capacity()>2U*1024U*1024U/sizeof(VoxelVertex)) std::vector<VoxelVertex>().swap(scratch.opaque);
            if(scratch.water.capacity()>1024U*1024U/sizeof(VoxelVertex)) std::vector<VoxelVertex>().swap(scratch.water);
            ++meshCompleted_;
        }
        {
            std::lock_guard<std::mutex> lock(meshMutex_);
            --meshWorkersBusy_;
            if (!stopping_)
                completedMeshes_.push_back(std::move(result));
            input.revisionToken.reset();
            // Immutable inputs remain worker-owned until the build finishes.
            // At most two per worker, each within the fixed 384-height halo.
            if(input.blocks.capacity()<=384U*22U*22U && input.packedLight.capacity()<=384U*22U*22U &&
               recycledMeshInputs_.size()<meshWorkers_.size()*2 && !stopping_)
                recycledMeshInputs_.push_back(std::move(input));
        }
    }
}

void World::recycleMeshOutput(MeshOutput result) {
    // At most two returned outputs per worker, at most 6 MiB per worker.
    if (result.opaque.capacity() > (2 * 1024 * 1024) / sizeof(VoxelVertex))
        std::vector<VoxelVertex>().swap(result.opaque);
    if (result.water.capacity() > (1024 * 1024) / sizeof(VoxelVertex))
        std::vector<VoxelVertex>().swap(result.water);
    result.opaque.clear();
    result.water.clear();
    result.packedOpaque.clear();result.packedWater.clear();
    result.opaqueIndices.clear();result.waterIndices.clear();
    if(result.opaqueIndices.capacity()>1024U*1024U/4)std::vector<std::uint32_t>().swap(result.opaqueIndices);
    if(result.waterIndices.capacity()>512U*1024U/4)std::vector<std::uint32_t>().swap(result.waterIndices);
    if(result.packedOpaque.capacity()>2U*1024U*1024U/sizeof(PackedTerrainVertex)) std::vector<PackedTerrainVertex>().swap(result.packedOpaque);
    if(result.packedWater.capacity()>1024U*1024U/sizeof(PackedTerrainVertex)) std::vector<PackedTerrainVertex>().swap(result.packedWater);
    std::lock_guard<std::mutex> lock(meshMutex_);
    if (recycledMeshes_.size() < meshWorkers_.size()*2 && !stopping_)
        recycledMeshes_.push_back(std::move(result));
}
