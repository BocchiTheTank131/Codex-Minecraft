#include "World.h"
#include <algorithm>
#include <array>

void World::queueFluidUpdate(int x, int y, int z) {
    if (y < minimumY() || y >= maximumY())
        return;
    if (!findChunk(floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE)))
        return;

    const FluidCell cell{x, y, z};
    if (fluidQueued_.insert(cell).second)
        fluidQueue_.push_back(cell);
}

void World::queueFluidNeighborhood(int x, int y, int z) {
    queueFluidUpdate(x, y, z);
    queueFluidUpdate(x, y - 1, z);
    queueFluidUpdate(x, y + 1, z);
    queueFluidUpdate(x - 1, y, z);
    queueFluidUpdate(x + 1, y, z);
    queueFluidUpdate(x, y, z - 1);
    queueFluidUpdate(x, y, z + 1);
}

void World::updateFluidCell(const FluidCell& cell) {
    if(!findChunk(floorDiv(cell.x,CHUNK_SIZE),floorDiv(cell.z,CHUNK_SIZE))) {fluidLavaDue_.erase(cell);return;}
    if (!simulationActiveAt(float(cell.x),float(cell.z))) {
        queueFluidUpdate(cell.x,cell.y,cell.z); return;
    }
    const Block current=getBlock(cell.x,cell.y,cell.z);
    if(current!=Block::Air && !isWater(current) && !isLava(current)) {fluidLavaDue_.erase(cell);return;}
    constexpr std::array<glm::ivec3,4> directions{{{1,0,0},{-1,0,0},{0,0,1},{0,0,-1}}};
    bool lava=isLava(current);
    if(current==Block::Air) {
        const auto above=getBlock(cell.x,cell.y+1,cell.z);
        lava=isLava(above);
        if(!isWater(above)) for(const auto& d:directions)
            lava |= isLava(getBlock(cell.x+d.x,cell.y,cell.z+d.z));
    }
    // Per-job deadlines cannot phase-lock with the bounded queue's traversal.
    if(lava) {
        auto due=fluidLavaDue_.try_emplace(cell,fluidTick_+3).first;
        if(fluidTick_<due->second) {queueFluidUpdate(cell.x,cell.y,cell.z);return;}
    }
    fluidLavaDue_.erase(cell);
    if(isWaterSource(current)||isLavaSource(current)) {
        // A loaded/source-only notification must activate dependent empty cells,
        // even if their original notification was consumed before integration.
        const auto below=getBlock(cell.x,cell.y-1,cell.z);
        if(below==Block::Air || (lava ? isLava(below)&&!isLavaSource(below)&&below!=Block::LavaFalling :
                                           isWater(below)&&!isWaterSource(below)&&below!=Block::WaterFalling))
            queueFluidUpdate(cell.x,cell.y-1,cell.z);
        if(isSolid(below)) for(const auto& d:directions) {
            const auto b=getBlock(cell.x+d.x,cell.y,cell.z+d.z);
            if(b==Block::Air || (lava?isLava(b)&&!isLavaSource(b)&&lavaLevel(b)>1:
                                          isWater(b)&&!isWaterSource(b)&&waterLevel(b)>1))
                queueFluidUpdate(cell.x+d.x,cell.y,cell.z+d.z);
        }
        return;
    }
    const auto same=[&](Block b){return lava?isLava(b):isWater(b);};
    const auto source=[&](Block b){return lava?isLavaSource(b):isWaterSource(b);};
    Block desired=Block::Air;
    if(same(getBlock(cell.x,cell.y+1,cell.z))) desired=lava?Block::LavaFalling:Block::WaterFalling;
    else {
        int best=lava?5:8;bool unknown=false;
        for(const auto& d:directions) {
            const int x=cell.x+d.x,z=cell.z+d.z;
            if(!findChunk(floorDiv(x,CHUNK_SIZE),floorDiv(z,CHUNK_SIZE))) {unknown=true;continue;}
            const Block provider=getBlock(x,cell.y,z);
            if(!same(provider)||!isSolid(getBlock(x,cell.y-1,z))) continue;
            const int candidate=source(provider)||provider==(lava?Block::LavaFalling:Block::WaterFalling)?1:
                (lava?lavaLevel(provider):waterLevel(provider))+1;
            best=std::min(best,candidate);
        }
        desired=lava?flowingLavaBlock(best):flowingWaterBlock(best);
        if(unknown && desired==Block::Air && same(current)) return; // Reload schedules the boundary again.
    }
    if(desired!=current) setBlockInternal(cell.x,cell.y,cell.z,desired,false);
}

void World::updateFluids(float deltaTime, int updateBudget) {
    constexpr float FluidTickSeconds = 0.10f;
    fluidUpdateAccumulator_ += std::max(0.0f, deltaTime);
    int ticks = std::min(4, static_cast<int>(fluidUpdateAccumulator_ / FluidTickSeconds));
    if (ticks <= 0)
        return;
    fluidUpdateAccumulator_ -= static_cast<float>(ticks) * FluidTickSeconds;

    updateBudget = std::max(1, updateBudget);
    for (int tick = 0; tick < ticks; ++tick) {
        ++fluidTick_;
        const int cellsThisTick =
            std::min(updateBudget, static_cast<int>(fluidQueue_.size()));
        for (int i = 0; i < cellsThisTick; ++i) {
            const FluidCell cell = fluidQueue_.front();
            fluidQueue_.pop_front();
            fluidQueued_.erase(cell);
            updateFluidCell(cell);
        }
    }
}

void World::scheduleFluidBoundaryUpdates(int chunkX, int chunkZ) {
    constexpr std::array<glm::ivec2, 4> Directions{
        {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
    for (const glm::ivec2& direction : Directions) {
        if (!findChunk(chunkX + direction.x, chunkZ + direction.y))
            continue;

        for (int y = minimumY(); y < maximumY(); ++y) {
            for (int offset = 0; offset < CHUNK_SIZE; ++offset) {
                const int localX = direction.x > 0   ? CHUNK_SIZE - 1
                                   : direction.x < 0 ? 0
                                                     : offset;
                const int localZ = direction.y > 0   ? CHUNK_SIZE - 1
                                   : direction.y < 0 ? 0
                                                     : offset;
                const int worldX = chunkX * CHUNK_SIZE + localX;
                const int worldZ = chunkZ * CHUNK_SIZE + localZ;
                const int neighborX = worldX + direction.x;
                const int neighborZ = worldZ + direction.y;
                const Block block = getBlock(worldX, y, worldZ);
                const Block neighbor = getBlock(neighborX, y, neighborZ);
                if ((isWater(block) || isWater(neighbor) || isLava(block) || isLava(neighbor)) && block != neighbor) {
                    queueFluidUpdate(worldX, y, worldZ);
                    queueFluidUpdate(neighborX, y, neighborZ);
                }
            }
        }
    }
}
