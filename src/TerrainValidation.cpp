#include "TerrainVertex.h"
#include "World.h"
#include "Definitions.h"
#include <glm/gtc/type_ptr.hpp>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
constexpr const char* LegacyVertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aUv;
layout(location=2) in vec3 aNormal;
layout(location=3) in float aSunLight;
layout(location=4) in float aBlockLight;
layout(location=5) in float aAo;
layout(location=6) in uint aMaterial;
uniform mat4 uView;
uniform mat4 uProjection;
out vec2 vUv;
out vec3 vNormal;
out float vSunLight;
out float vBlockLight;
out float vAo;
out float vDistance;
out vec3 vWorldPosition;
out float vAtlasTile;
out vec3 vClimateTint;
void main() {
    vec4 viewPosition = uView * vec4(aPosition, 1.0);
    gl_Position = uProjection * viewPosition;
    vUv = aUv; vNormal = aNormal;
    vSunLight = aSunLight; vBlockLight = aBlockLight; vAo = aAo;
    vDistance = length(viewPosition.xyz);
    vWorldPosition = aPosition;
    uint tile = aMaterial & 255u;
    vAtlasTile = tile == 255u ? -1.0 : float(tile);
    vClimateTint = vec3(float((aMaterial >> 8u) & 255u),
                       float((aMaterial >> 16u) & 255u),
                       float((aMaterial >> 24u) & 255u)) / 255.0;
}
)GLSL";


GLuint program(const char* vs,const char* fs) {
    GLuint shaders[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
    const char* sources[2]={vs,fs};
    for(int i=0;i<2;++i) {
        glShaderSource(shaders[i],1,&sources[i],nullptr);glCompileShader(shaders[i]);
        GLint good;glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&good);
        if(!good) {char log[2048]{};glGetShaderInfoLog(shaders[i],2048,nullptr,log);throw std::runtime_error(log);}
    }
    const auto p=glCreateProgram();glAttachShader(p,shaders[0]);glAttachShader(p,shaders[1]);glLinkProgram(p);
    for(auto shader:shaders)glDeleteShader(shader);
    GLint good;glGetProgramiv(p,GL_LINK_STATUS,&good);if(!good)throw std::runtime_error("Terrain comparison link failed");return p;
}
}
bool validateTerrainPackingGpu(const char* vertexShader,const char* fragmentShader,unsigned atlasTexture) {
    GLint oldProgram,oldVao,oldArray,oldFramebuffer,oldTexture,viewport[4];
    glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldArray);glGetIntegerv(GL_FRAMEBUFFER_BINDING,&oldFramebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTexture);glGetIntegerv(GL_VIEWPORT,viewport);
    const bool cull=glIsEnabled(GL_CULL_FACE),blend=glIsEnabled(GL_BLEND),depth=glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);
    GLuint fbo,color,vao[3],vbo[3],p[3],ebo=0;glGenFramebuffers(1,&fbo);glGenTextures(1,&color);
    glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Terrain comparison FBO incomplete");
    glViewport(0,0,256,256);glGenVertexArrays(3,vao);glGenBuffers(3,vbo);glGenBuffers(1,&ebo);
    p[0]=program(LegacyVertexShader,fragmentShader);p[1]=program(vertexShader,fragmentShader);p[2]=program(vertexShader,fragmentShader);
    std::vector<VoxelVertex> raw;std::vector<PackedTerrainVertex> packed;
    const glm::vec3 normals[]={ {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
        glm::normalize(glm::vec3(-1,0,1)),glm::normalize(glm::vec3(1,0,-1)),glm::normalize(glm::vec3(1,0,1)),glm::normalize(glm::vec3(-1,0,-1)) };
    constexpr int corners[]={0,1,2,0,2,3};
    for(int cell=0;cell<256;++cell)for(int corner:corners) {
        const float x=-1.f+(cell%16)*.125f,y=-1.f+(cell/16)*.125f;
        std::uint32_t sunRaw=TerrainPacking::sunBits()[cell%64];float sun;std::memcpy(&sun,&sunRaw,4);
        VoxelVertex v{{x+((corner==1||corner==2)? .123f:0),y+(corner>=2?.123f:0),0},
            {float(corner==1||corner==2)*4,float(corner>=2)*2},normals[cell%10],sun,float(cell%16)/15.f,float(49+cell%16)/64.f,float(cell%BlockAtlasTiles)};
        v.atlasTile.setTint(0x97c8adU);
        if(cell%3==0) {v.atlasTile=VoxelMaterial(-1);v.uv={float(cell%BlockAtlasTiles)/BlockAtlasTiles+.0005f+(corner==1||corner==2?.003f:0),corner>=2?.9995f:.0005f};}
        raw.push_back(v);packed.push_back(TerrainPacking::pack(v));
    }
    std::vector<PackedTerrainVertex> unique;std::vector<std::uint32_t> indices;
    TerrainPacking::indexQuads(raw,unique,indices);
    for(std::size_t i=0;i<raw.size();++i) {
        const auto restored=TerrainPacking::unpack(unique[indices[i]]);
        if(std::memcmp(&restored,&raw[i],sizeof(VoxelVertex)))throw std::runtime_error("Indexed attributes/order mismatch");
    }
    for(int variant=0;variant<3;++variant) {
        glBindVertexArray(vao[variant]);glBindBuffer(GL_ARRAY_BUFFER,vbo[variant]);
        if(variant==0) {
            glBufferData(GL_ARRAY_BUFFER,raw.size()*sizeof(VoxelVertex),raw.data(),GL_STATIC_DRAW);
            const std::size_t offsets[]={offsetof(VoxelVertex,position),offsetof(VoxelVertex,uv),offsetof(VoxelVertex,normal),offsetof(VoxelVertex,sunLight),offsetof(VoxelVertex,blockLight),offsetof(VoxelVertex,ao)};
            const GLint sizes[]={3,2,3,1,1,1};
            for(int i=0;i<6;++i) {glEnableVertexAttribArray(i);glVertexAttribPointer(i,sizes[i],GL_FLOAT,GL_FALSE,sizeof(VoxelVertex),reinterpret_cast<void*>(offsets[i]));}
            glEnableVertexAttribArray(6);glVertexAttribIPointer(6,1,GL_UNSIGNED_INT,sizeof(VoxelVertex),reinterpret_cast<void*>(offsetof(VoxelVertex,atlasTile)));
        } else {
            const auto& data=variant==2?unique:packed;
            glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(PackedTerrainVertex),data.data(),GL_STATIC_DRAW);
            if(variant==2){glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*4,indices.data(),GL_STATIC_DRAW);}
            glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(PackedTerrainVertex),nullptr);
            glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(PackedTerrainVertex),reinterpret_cast<void*>(12));
            glEnableVertexAttribArray(2);glVertexAttribIPointer(2,1,GL_UNSIGNED_INT,sizeof(PackedTerrainVertex),reinterpret_cast<void*>(24));
            glEnableVertexAttribArray(6);glVertexAttribIPointer(6,1,GL_UNSIGNED_INT,sizeof(PackedTerrainVertex),reinterpret_cast<void*>(20));
        }
        glUseProgram(p[variant]);const glm::mat4 identity(1);
        glUniformMatrix4fv(glGetUniformLocation(p[variant],"uView"),1,GL_FALSE,glm::value_ptr(identity));
        glUniformMatrix4fv(glGetUniformLocation(p[variant],"uProjection"),1,GL_FALSE,glm::value_ptr(identity));
        glUniform1uiv(glGetUniformLocation(p[variant],"uSunBits"),64,TerrainPacking::sunBits().data());
        glUniform1f(glGetUniformLocation(p[variant],"uAtlasTiles"),float(BlockAtlasTiles));
        glUniform3f(glGetUniformLocation(p[variant],"uSunDirection"),.3f,.8f,.1f);
        glUniform3f(glGetUniformLocation(p[variant],"uSkyColor"),.5f,.7f,.9f);
        glUniform1f(glGetUniformLocation(p[variant],"uBrightness"),.5f);
        glUniform1i(glGetUniformLocation(p[variant],"uEffectQuality"),2);
    }
    std::array<std::vector<std::uint8_t>,3> pixels;for(auto& data:pixels)data.resize(256*256*4);
    bool equal=true;
    for(int scene=0;scene<8;++scene) {
        for(int variant=0;variant<3;++variant) {
            glBindTexture(GL_TEXTURE_2D,atlasTexture);glUseProgram(p[variant]);glBindVertexArray(vao[variant]);
            glUniform1f(glGetUniformLocation(p[variant],"uDaylight"),scene%2?1.f:0.f);
            glUniform1i(glGetUniformLocation(p[variant],"uWaterPass"),(scene/2)%2);
            glUniform4f(glGetUniformLocation(p[variant],"uWeather"),scene>=4?.7f:0,scene>=4?.5f:0,0,0);
            glUniform1f(glGetUniformLocation(p[variant],"uWetness"),scene>=4?.8f:0);
            glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
            if(variant==2)glDrawElements(GL_TRIANGLES,GLsizei(indices.size()),GL_UNSIGNED_INT,nullptr);
            else glDrawArrays(GL_TRIANGLES,0,GLsizei(raw.size()));
            // Explicit correctness fixture readback only; never in runtime culling.
            glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels[variant].data());
        }
        equal &= pixels[0]==pixels[1] && pixels[0]==pixels[2];
    }
    const bool noError=glGetError()==GL_NO_ERROR;
    glDeleteProgram(p[0]);glDeleteProgram(p[1]);glDeleteProgram(p[2]);glDeleteBuffers(3,vbo);glDeleteBuffers(1,&ebo);glDeleteVertexArrays(3,vao);
    glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);
    glBindFramebuffer(GL_FRAMEBUFFER,oldFramebuffer);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glBindTexture(GL_TEXTURE_2D,oldTexture);glBindVertexArray(oldVao);glBindBuffer(GL_ARRAY_BUFFER,oldArray);glUseProgram(oldProgram);
    if(cull)glEnable(GL_CULL_FACE);if(blend)glEnable(GL_BLEND);if(depth)glEnable(GL_DEPTH_TEST);
    std::cout<<"Terrain packing GPU comparison: "<<(equal&&noError?"8 legacy/packed/indexed scenes pixel-identical":"FAILED")<<"; 48 to 28 bytes\n";
    return equal&&noError;
}
