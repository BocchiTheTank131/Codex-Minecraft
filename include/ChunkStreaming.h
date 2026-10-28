#pragma once
#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
enum class ChunkStreamingQuality { Auto, Low, Balanced, High };
struct StreamingWorkerPlan { unsigned generation=1, mesh=1; };
StreamingWorkerPlan streamingWorkerPlan(unsigned hardware, ChunkStreamingQuality quality);
unsigned availableStreamingProcessors();
float streamingPriority(int dx,int dz,const glm::vec2& forward,bool edited=false);
struct ChunkStreamingDiagnostics {
    int generationQueued=0,generationReady=0,generationActive=0,meshQueued=0,meshReady=0,meshActive=0;
    unsigned generationLimit=1,meshLimit=1;
    float generatedPerSecond=0,meshedPerSecond=0;
    float generationMilliseconds=0;
    float integrateMilliseconds=0,lightingMilliseconds=0,snapshotMilliseconds=0,uploadMilliseconds=0;
    float unloadMilliseconds=0,retireMilliseconds=0;
    float fencePollMilliseconds=0,rangeReleaseMilliseconds=0,bufferDeleteMilliseconds=0,pageTrimMilliseconds=0,payloadReleaseMilliseconds=0;
    float pageVboDeleteMilliseconds=0,pageEboDeleteMilliseconds=0,pageVaoDeleteMilliseconds=0;
    unsigned pageDeleteCalls=0,pageAllocations=0;
    float bufferAllocationMilliseconds=0,bufferUploadMilliseconds=0;
    int retiredChunks=0,lightingQueued=0;
    std::uint64_t generated=0,meshed=0,staleMeshes=0;
};
