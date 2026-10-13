#include "TerrainOcclusion.h"
#include <glm/gtc/type_ptr.hpp>
#include <chrono>
#include <cstring>
#include <stdexcept>

namespace {
bool same(const glm::mat4& a,const glm::mat4& b) {return std::memcmp(glm::value_ptr(a),glm::value_ptr(b),sizeof(glm::mat4))==0;}
bool same(const MeshBounds& a,const MeshBounds& b) {return a.valid && b.valid && a.minimum==b.minimum && a.maximum==b.maximum;}
int region(int chunk) {int q=chunk/4;return q-(chunk%4<0);}
std::int64_t key(const Chunk& chunk) {return std::int64_t((std::uint64_t(std::uint32_t(region(chunk.x)))<<32)|std::uint32_t(region(chunk.z)));}
GLuint shader(GLenum type,const char* code) {
    GLuint id=glCreateShader(type);glShaderSource(id,1,&code,nullptr);glCompileShader(id);
    GLint ok;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){glDeleteShader(id);throw std::runtime_error("Occlusion shader unavailable");}return id;
}
bool safeBox(const MeshBounds& box,const glm::mat4& view,const glm::mat4& projection,const glm::vec3& camera) {
    if(glm::all(glm::greaterThanEqual(camera,box.minimum)) && glm::all(glm::lessThanEqual(camera,box.maximum)))return false;
    // Near-plane intersections can expose geometry without a rasterized box face.
    for(int i=0;i<8;++i) {
        const glm::vec3 p(i&1?box.maximum.x:box.minimum.x,i&2?box.maximum.y:box.minimum.y,i&4?box.maximum.z:box.minimum.z);
        const auto clip=projection*(view*glm::vec4(p,1));
        if(clip.w<=0 || clip.z<=-clip.w) return false;
    }
    return true;
}
}
TerrainOcclusion::TerrainOcclusion() {
    if(!glBeginQuery || !glGetQueryObjectuiv)return;
    try {
        const auto vs=shader(GL_VERTEX_SHADER,"#version 330 core\nlayout(location=0)in vec3 p;uniform mat4 v,m;uniform vec3 lo,hi;void main(){gl_Position=m*(v*vec4(mix(lo,hi,p),1));}");
        const auto fs=shader(GL_FRAGMENT_SHADER,"#version 330 core\nout vec4 c;void main(){c=vec4(1);}");
        program_=glCreateProgram();glAttachShader(program_,vs);glAttachShader(program_,fs);glLinkProgram(program_);glDeleteShader(vs);glDeleteShader(fs);
        GLint ok;glGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok)return;
        view_=glGetUniformLocation(program_,"v");projection_=glGetUniformLocation(program_,"m");
        minimum_=glGetUniformLocation(program_,"lo");maximum_=glGetUniformLocation(program_,"hi");
        std::array<glm::vec3,36> points{};
        constexpr int faces[6][4]={{1,3,7,5},{4,6,2,0},{2,6,7,3},{0,1,5,4},{5,7,6,4},{0,2,3,1}};
        constexpr int indices[]={0,1,2,0,2,3};int n=0;
        for(const auto& face:faces)for(int i:indices){int c=face[i];points[n++]={float(c&1),float((c>>1)&1),float((c>>2)&1)};}
        glGenVertexArrays(1,&vao_);glGenBuffers(1,&vbo_);glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glBufferData(GL_ARRAY_BUFFER,sizeof(points),points.data(),GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(glm::vec3),nullptr);glBindVertexArray(0);
        for(auto& slot:slots_)glGenQueries(1,&slot.query);
        GLint bits=0;glGetQueryiv(GL_TIMESTAMP,GL_QUERY_COUNTER_BITS,&bits);
        gpuTimers_=bits>0 && glGetQueryObjectui64v && glQueryCounter;
        if(gpuTimers_)for(auto& timer:timers_){glGenQueries(1,&timer.start);glGenQueries(1,&timer.query);}
        lookup_.reserve(slots_.size());stats_.supported=true;
    } catch(...) {stats_.supported=false;}
}
TerrainOcclusion::~TerrainOcclusion() {
    for(auto& slot:slots_)if(slot.query)glDeleteQueries(1,&slot.query);
    for(auto& timer:timers_)if(timer.query)glDeleteQueries(1,&timer.query);
    for(auto& timer:timers_)if(timer.start)glDeleteQueries(1,&timer.start);
    if(program_)glDeleteProgram(program_);if(vbo_)glDeleteBuffers(1,&vbo_);if(vao_)glDeleteVertexArrays(1,&vao_);
}
void TerrainOcclusion::draw(const World& world,const World::VisibleChunks& visible,const glm::mat4& view,
                           const glm::mat4& projection,const glm::vec3& camera,GLuint restoreProgram,int mode) {
    ++frame_;stats_.culled=stats_.queries=stats_.groups=0;stats_.savedVertices=0;stats_.queryCpuMs=0;
    if(!mode || !stats_.supported) {identity_=0;stats_.autoSuspended=false;world.drawOpaque(visible);return;}
    if(mode==1 && !gpuTimers_){identity_=0;stats_.autoSuspended=true;world.drawOpaque(visible);return;}
    const auto workStart=std::chrono::steady_clock::now();
    std::array<GLint,4> viewport{};GLint samples=0;
    glGetIntegerv(GL_VIEWPORT,viewport.data());glGetIntegerv(GL_SAMPLES,&samples);
    const bool unchanged=identity_==world.visibilityIdentity() && revision_==world.visibilityRevision() && same(view,previousView_) && same(projection,previousProjection_) && viewport==previousViewport_ && samples==previousSamples_;
    identity_=world.visibilityIdentity();revision_=world.visibilityRevision();previousView_=view;previousProjection_=projection;
    previousViewport_=viewport;previousSamples_=samples;
    if(!unchanged) {++proofEpoch_;baselineSamples_=filteredSamples_=0;baselineGpuMs_=filteredGpuMs_=0;suspendUntil_=0;emptyProofFrames_=0;for(auto& slot:slots_)slot.hidden=false;}
    if(!unchanged) {stats_.autoSuspended=false;world.drawOpaque(visible);return;}
    for(auto& timer:timers_)if(timer.pending) {
        GLuint ready=0;glGetQueryObjectuiv(timer.query,GL_QUERY_RESULT_AVAILABLE,&ready);
        if(ready) {
            GLuint startReady=0;glGetQueryObjectuiv(timer.start,GL_QUERY_RESULT_AVAILABLE,&startReady);
            if(!startReady)continue;
            GLuint64 start=0,end=0;glGetQueryObjectui64v(timer.start,GL_QUERY_RESULT,&start);glGetQueryObjectui64v(timer.query,GL_QUERY_RESULT,&end);timer.pending=false;
            if(timer.epoch==proofEpoch_) {
                auto& average=timer.baseline?baselineGpuMs_:filteredGpuMs_;
                auto& count=timer.baseline?baselineSamples_:filteredSamples_;
                const double ms=double(end-start)/1000000.0;average=count?average*.875+ms*.125:ms;++count;
            }
        }
    }
    const bool suspended=mode==1 && frame_<suspendUntil_;
    if(suspended){stats_.autoSuspended=true;world.drawOpaque(visible);return;}
    if(stats_.autoSuspended){++proofEpoch_;baselineSamples_=filteredSamples_=0;baselineGpuMs_=filteredGpuMs_=0;emptyProofFrames_=0;}
    stats_.autoSuspended=false;
    for(auto& slot:slots_) {
        slot.bounds.valid=false;
        if(slot.pending) {
            GLuint ready=0;glGetQueryObjectuiv(slot.query,GL_QUERY_RESULT_AVAILABLE,&ready);
            if(ready) {
                GLuint passed=1;glGetQueryObjectuiv(slot.query,GL_QUERY_RESULT,&passed);
                slot.pending=false;slot.hidden=passed==0 && unchanged && slot.identity==identity_ && slot.revision==revision_ && same(slot.proofView,view) && same(slot.proofProjection,projection) && slot.viewport==viewport && slot.samples==samples;
            }
        }
    }
    // Only 64 coarse 4x4-chunk groups are tracked. Everything else stays visible.
    for(const auto* chunk:visible.opaque) {
        const auto k=key(*chunk);auto found=lookup_.find(k);
        if(found==lookup_.end()) {
            unsigned index=0;
            for(;index<slots_.size();++index) if(!slots_[index].pending && (!slots_[index].assigned || slots_[index].seen+8<frame_))break;
            if(index==slots_.size())continue;
            auto& slot=slots_[index];if(slot.assigned)lookup_.erase(slot.key);
            slot.assigned=true;slot.key=k;slot.hidden=false;lookup_[k]=index;found=lookup_.find(k);
        }
        auto& slot=slots_[found->second];slot.seen=frame_;
        if(!slot.bounds.valid)slot.bounds=chunk->opaqueBounds;
        else {slot.bounds.minimum=glm::min(slot.bounds.minimum,chunk->opaqueBounds.minimum);slot.bounds.maximum=glm::max(slot.bounds.maximum,chunk->opaqueBounds.maximum);}
    }
    filtered_.opaque.clear();filtered_.opaque.reserve(visible.opaque.size());
    const bool probe=mode==1 && gpuTimers_ && !suspended && (baselineSamples_<4 || frame_%32==0);
    for(const auto* chunk:visible.opaque) {
        const auto found=lookup_.find(key(*chunk));bool hide=false;
        if(unchanged && !suspended && !probe && found!=lookup_.end()) {
            const auto& slot=slots_[found->second];
            hide=slot.hidden && slot.issued+4>=frame_ && slot.identity==identity_ && slot.revision==revision_ &&
                same(slot.proofView,view) && same(slot.proofProjection,projection) && same(slot.bounds,slot.proofBounds);
        }
        if(hide) {++stats_.culled;stats_.savedVertices+=chunk->opaqueVertexCount;}
        else filtered_.opaque.push_back(chunk);
    }
    struct TimerScope {GLuint end=0;~TimerScope(){if(end)glQueryCounter(end,GL_TIMESTAMP);}} timing;
    if(gpuTimers_ && !suspended)for(auto& timer:timers_)if(!timer.pending) {
        timer.pending=true;timer.baseline=probe;timer.epoch=proofEpoch_;
        glQueryCounter(timer.start,GL_TIMESTAMP);timing.end=timer.query;break;
    }
    const float prepareMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-workStart).count();
    world.drawOpaque(filtered_);
    if(suspended || probe)return;
    const auto start=std::chrono::steady_clock::now();
    GLint depthFunction;GLboolean depthMask,colorMask[4];glGetIntegerv(GL_DEPTH_FUNC,&depthFunction);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);
    const bool cull=glIsEnabled(GL_CULL_FACE);glDisable(GL_CULL_FACE);glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glUseProgram(program_);glBindVertexArray(vao_);
    glUniformMatrix4fv(view_,1,GL_FALSE,glm::value_ptr(view));glUniformMatrix4fv(projection_,1,GL_FALSE,glm::value_ptr(projection));
    // Query only after real opaque depth exists. No result is read in this pass.
    for(unsigned offset=0;offset<slots_.size() && stats_.queries<8;++offset) {
        auto& slot=slots_[(unsigned(frame_)*8+offset)%slots_.size()];
        auto queryBounds=slot.bounds;
        const auto magnitude=glm::max(glm::abs(queryBounds.minimum),glm::abs(queryBounds.maximum));
        const float padding=std::max(.05f,std::max({magnitude.x,magnitude.y,magnitude.z})*std::numeric_limits<float>::epsilon()*4);
        queryBounds.minimum-=glm::vec3(padding);queryBounds.maximum+=glm::vec3(padding);
        if(!slot.bounds.valid || slot.pending || !safeBox(queryBounds,view,projection,camera))continue;
        slot.proofBounds=slot.bounds;slot.proofView=view;slot.proofProjection=projection;slot.identity=identity_;slot.revision=revision_;slot.issued=frame_;
        slot.viewport=viewport;slot.samples=samples;
        const auto lo=queryBounds.minimum,hi=queryBounds.maximum;
        glUniform3fv(minimum_,1,glm::value_ptr(lo));glUniform3fv(maximum_,1,glm::value_ptr(hi));
        glBeginQuery(GL_ANY_SAMPLES_PASSED,slot.query);glDrawArrays(GL_TRIANGLES,0,36);glEndQuery(GL_ANY_SAMPLES_PASSED);
        slot.pending=true;++stats_.queries;
    }
    glBindVertexArray(0);glUseProgram(restoreProgram);glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);
    glDepthMask(depthMask);glDepthFunc(depthFunction);if(cull)glEnable(GL_CULL_FACE);
    for(const auto& slot:slots_)stats_.groups+=slot.bounds.valid;
    stats_.queryCpuMs=prepareMs+std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count();
    if(!stats_.culled)++emptyProofFrames_;else emptyProofFrames_=0;
    // Auto bounds admission and backs off when no useful proof is found or CPU
    // query overhead becomes material. On is a diagnostic override, still safe.
    if(mode==1 && (emptyProofFrames_>=48 || stats_.queryCpuMs>.25f)) {suspendUntil_=frame_+240;emptyProofFrames_=0;}
    // Compare the queried/culling pass against periodic full-draw GPU probes.
    // Available results only; noisy/new cameras train again rather than hiding.
    if(mode==1 && baselineSamples_>=4 && filteredSamples_>=16 && filteredGpuMs_>baselineGpuMs_*1.03)
        suspendUntil_=frame_+240;
}
