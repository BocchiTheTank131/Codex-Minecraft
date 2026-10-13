#pragma once
#include "World.h"
#include <array>
#include <unordered_map>

// Render-thread only. Results are proofs for one exact camera and mesh epoch.
// Motion, edits, uploads, unloading and unknown results always draw normally.
class TerrainOcclusion {
public:
    struct Diagnostics {unsigned culled=0,queries=0,groups=0;std::size_t savedVertices=0;float queryCpuMs=0;bool supported=false,autoSuspended=false;};
    TerrainOcclusion();
    ~TerrainOcclusion();
    void draw(const World& world,const World::VisibleChunks& visible,const glm::mat4& view,
              const glm::mat4& projection,const glm::vec3& camera,GLuint restoreProgram,int mode);
    const Diagnostics& diagnostics() const {return stats_;}
private:
    struct Slot {
        GLuint query=0;bool pending=false,hidden=false,assigned=false;
        std::int64_t key=0;std::uint64_t identity=0,revision=0,issued=0,seen=0;
        std::array<GLint,4> viewport{};GLint samples=0;
        MeshBounds bounds{},proofBounds{};
        glm::mat4 proofView{1},proofProjection{1};
    };
    std::array<Slot,64> slots_{};
    struct Timer {GLuint start=0,query=0;bool pending=false,baseline=false;std::uint64_t epoch=0;};
    std::array<Timer,8> timers_{};
    bool gpuTimers_=false;
    std::uint64_t proofEpoch_=0;
    unsigned baselineSamples_=0,filteredSamples_=0;
    double baselineGpuMs_=0,filteredGpuMs_=0;
    std::unordered_map<std::int64_t,unsigned> lookup_;
    World::VisibleChunks filtered_;
    GLuint program_=0,vao_=0,vbo_=0;
    GLint view_=0,projection_=0,minimum_=0,maximum_=0;
    std::uint64_t frame_=0,identity_=0,revision_=0,suspendUntil_=0;
    unsigned emptyProofFrames_=0;
    glm::mat4 previousView_{1},previousProjection_{1};
    std::array<GLint,4> previousViewport_{};GLint previousSamples_=0;
    Diagnostics stats_;
};
