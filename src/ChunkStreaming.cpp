#include "ChunkStreaming.h"
#include <cmath>
StreamingWorkerPlan streamingWorkerPlan(unsigned hardware, ChunkStreamingQuality quality) {
    hardware=std::max(1U,hardware);
    if(hardware<=3 || quality==ChunkStreamingQuality::Low) return {1,1};
    // Leave headroom for rendering, audio and the OS. Logical CPUs are not
    // treated as independent full-speed cores on hybrid/SMT processors.
    const unsigned budget=std::max(2U,hardware/2);
    // Meshing is slower per chunk than generation in the V11 fixtures. Give
    // small/mid-range CPUs half the worker budget before the four-mesh ceiling.
    unsigned meshes=std::clamp(budget/2,1U,4U);
    unsigned generation=std::clamp(budget-meshes,1U,8U);
    if(quality==ChunkStreamingQuality::Balanced) { meshes=std::min(meshes,2U);generation=std::min(generation,4U); }
    if(quality==ChunkStreamingQuality::Auto) generation=std::min(generation,6U);
    return {generation,meshes};
}
float streamingPriority(int dx,int dz,const glm::vec2& forward,bool edited) {
    const float distance=std::sqrt(float(dx)*dx+float(dz)*dz);
    const float dot=distance>.001f?(dx*forward.x+dz*forward.y)/distance:1.f;
    // Distance dominates. Behind-camera work is deferred, never excluded.
    return (edited?-100000.f:0.f)+distance*(dot>.35f?.75f:dot<-.25f?1.35f:1.f);
}
