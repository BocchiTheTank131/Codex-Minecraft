#include "WeatherRenderer.h"
#include "World.h"
#include "Settings.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
std::uint32_t hash(std::uint32_t n) { n ^= n>>16; n*=0x7feb352dU; n^=n>>15; n*=0x846ca68bU; return n^(n>>16); }
float unit(std::uint32_t n) { return (hash(n)&0xffff)/65536.0f; }
GLuint shader(GLenum kind,const char* source) {
    GLuint s=glCreateShader(kind); glShaderSource(s,1,&source,nullptr); glCompileShader(s);
    GLint success=0; glGetShaderiv(s,GL_COMPILE_STATUS,&success);
    if (!success) { char log[1024]{}; glGetShaderInfoLog(s,1024,nullptr,log); glDeleteShader(s); throw std::runtime_error(log); }
    return s;
}
}
WeatherRenderer::WeatherRenderer() {
    const char* vs="#version 330 core\nlayout(location=0) in vec3 p; layout(location=1) in vec4 c; uniform mat4 vp; out vec4 color; void main(){gl_Position=vp*vec4(p,1);color=c;}";
    const char* fs="#version 330 core\nin vec4 color; out vec4 fragColor; void main(){fragColor=color;}";
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);
    program_=glCreateProgram(); glAttachShader(program_,v); glAttachShader(program_,f); glLinkProgram(program_);
    glDeleteShader(v); glDeleteShader(f); GLint ok=0; glGetProgramiv(program_,GL_LINK_STATUS,&ok);
    if (!ok) throw std::runtime_error("Weather shader link failed");
    vp_=glGetUniformLocation(program_,"vp");
    glGenVertexArrays(1,&vao_); glGenBuffers(1,&vbo_); glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER,vbo_);
    // Fixed upper bound: 4096 columns, five precipitation quads plus a splash.
    vertices_.reserve(150000);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(sizeof(glm::vec3))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}
WeatherRenderer::~WeatherRenderer() { glDeleteBuffers(1,&vbo_); glDeleteVertexArrays(1,&vao_); glDeleteProgram(program_); }
void WeatherRenderer::clear() { for(auto& c:columns_) c.valid=false; vertices_.clear(); particleCount_=0; }
void WeatherRenderer::quad(const glm::vec3& p,const glm::vec3& r,const glm::vec3& u,const glm::vec4& c) {
    vertices_.insert(vertices_.end(),{{p-r-u,c},{p+r-u,c},{p+r+u,c},{p-r-u,c},{p+r+u,c},{p-r+u,c}});
}
void WeatherRenderer::render(const Weather& weather,const World& world,const GameSettings& s,
                            const glm::vec3& camera,const glm::mat4& view,const glm::mat4& projection,
                            double time,float daylight) {
    vertices_.clear(); particleCount_=0;
    if (s.weatherQuality==0) return; // Zero exposure/biome scans, uploads or draws.
    const glm::mat4 inverse=glm::inverse(view);
    const glm::vec3 right=glm::normalize(glm::vec3(inverse[0]));
    const glm::vec3 up=glm::normalize(glm::vec3(inverse[1]));
    const int radius=s.weatherQuality==1 ? 16 : s.weatherQuality==2 ? 24 : 30;
    const int stride=s.weatherQuality==1 ? 2 : 1;
    const int cx=static_cast<int>(std::floor(camera.x)),cz=static_cast<int>(std::floor(camera.z));
    const float t=static_cast<float>(std::fmod(time,3600.0));
    const float density=s.precipitationDensity*weather.intensity();
    int biomeBudget=128;
    if (density>.005f) for (int z=cz-radius;z<=cz+radius;z+=stride) for (int x=cx-radius;x<=cx+radius;x+=stride) {
        const float d=static_cast<float>((x-cx)*(x-cx)+(z-cz)*(z-cz));
        if(d>radius*radius) continue;
        const auto key=static_cast<std::uint32_t>(x)*73856093U ^ static_cast<std::uint32_t>(z)*19349663U;
        auto& column=columns_[static_cast<std::size_t>((z&63)*64+(x&63))];
        if(!column.valid || column.x!=x || column.z!=z) {
            if(biomeBudget--<=0) continue;
            column={x,z,true,Weather::precipitation(world,x,z)};
        }
        if(column.kind==Precipitation::None) continue;
        const float floor=world.precipitationHeight(x,z);
        if(floor<0 || floor>camera.y+15) continue;
        const bool snow=column.kind==Precipitation::Snow;
        const int lanes=s.weatherQuality==1 ? 1 : s.weatherQuality==2 ? 2 : 4;
        const float alpha=(1-d/(radius*radius))*(snow ? .72f : .35f)*(.35f+.65f*daylight);
        for(int i=0;i<lanes;++i) {
            const auto seed=key+static_cast<std::uint32_t>(i)*7277U;
            if(unit(seed+133U)>density) continue;
            const float bottom=std::max(floor,camera.y-10);
            const float y=bottom+.06f+26.0f-std::fmod(unit(seed)*26+t*(snow ? 1.3f : 15.0f),26.0f);
            const glm::vec3 p(x+.15f+unit(seed+2)*.7f,y,z+.15f+unit(seed+3)*.7f);
            const glm::vec3 wind=s.weatherWind ? right*(snow ? std::sin(t+unit(seed)*6)*.02f : .08f) : glm::vec3(0);
            if(snow) {
                glm::vec3 center=p+wind*6.0f;
                center.x=std::clamp(center.x,x+.04f,x+.96f);
                center.z=std::clamp(center.z,z+.04f,z+.96f);
                quad(center,right*.035f,up*.035f,{.85f,.90f,1.0f,alpha});
            }
            else {
                // Clip the bottom of each streak exactly at the roof/surface.
                const float half=std::min(.45f,(y-floor)*.5f);
                quad(p,right*.014f,glm::vec3(0,half,0)+wind,{.56f,.70f,.85f,alpha});
            }
            ++particleCount_;
        }
        if(!snow && s.weatherQuality>=2 && std::abs(floor-camera.y)<10 &&
           std::fmod(t+unit(key)*7,1.0f)<.14f && unit(key+4)<density) {
            const float size=.025f+.11f*std::fmod(t+unit(key)*7,1.0f)/.14f;
            const glm::vec3 p(x+.5f,floor+.015f,z+.5f);
            quad(p,{size,0,0},{0,0,size},{.7f,.83f,.92f,alpha*.7f}); ++particleCount_;
        }
    }
    if(s.lightningEffects) for(const auto& bolt:weather.bolts()) {
        glm::vec3 previous=bolt.position;
        for(int i=1;i<=12;++i) {
            glm::vec3 next=bolt.position+glm::vec3((unit(bolt.shape+i)*2-1)*2,i*5.0f,(unit(bolt.shape+i+50)*2-1)*2);
            const glm::vec3 center=(previous+next)*.5f;
            quad(center,right*.065f,(next-previous)*.5f,{.80f,.86f,1,1-bolt.age/.45f});
            quad(center,right*.22f,(next-previous)*.5f,{.55f,.65f,1,(1-bolt.age/.45f)*.25f});
            previous=next;
        }
    }
    if(vertices_.empty()) return;
    glUseProgram(program_); const glm::mat4 vp=projection*view; glUniformMatrix4fv(vp_,1,GL_FALSE,glm::value_ptr(vp));
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER,vbo_);
    // Replace the used stream range, allowing GL 3.3 drivers to orphan an
    // in-flight allocation instead of retaining/shadowing a large fixed buffer.
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices_.size()*sizeof(Vertex)),vertices_.data(),GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0); glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_CULL_FACE);
}
