#include "Renderer.h"

#include "World.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {
constexpr const char* WorldVertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aUv;
layout(location=2) in vec3 aNormal;
layout(location=3) in float aSunLight;
layout(location=4) in float aBlockLight;
layout(location=5) in float aAo;
uniform mat4 uView;
uniform mat4 uProjection;
out vec2 vUv;
out vec3 vNormal;
out float vSunLight;
out float vBlockLight;
out float vAo;
out float vDistance;
void main() {
    vec4 viewPosition = uView * vec4(aPosition, 1.0);
    gl_Position = uProjection * viewPosition;
    vUv = aUv; vNormal = aNormal;
    vSunLight = aSunLight; vBlockLight = aBlockLight; vAo = aAo;
    vDistance = length(viewPosition.xyz);
}
)GLSL";

constexpr const char* WorldFragmentShader = R"GLSL(
#version 330 core
in vec2 vUv;
in vec3 vNormal;
in float vSunLight;
in float vBlockLight;
in float vAo;
in float vDistance;
uniform sampler2D uAtlas;
uniform vec3 uSunDirection;
uniform vec3 uSkyColor;
uniform float uDaylight;
uniform bool uWaterPass;
uniform bool uFullbright;
out vec4 fragColor;
void main() {
    vec4 texel = texture(uAtlas, vUv);
    if (texel.a < 0.20) discard;
    float sunDiffuse = max(dot(normalize(vNormal), uSunDirection), 0.0);
    float moonDiffuse = max(dot(normalize(vNormal), -uSunDirection), 0.0);
    float directional = sunDiffuse * (0.18 + 0.48 * uDaylight) + moonDiffuse * 0.10 * (1.0-uDaylight);
    float skyContribution = vSunLight * mix(0.035, 0.46, uDaylight);
    float directionalContribution = vSunLight * directional;
    float emittedContribution = vBlockLight * 0.88;
    float brightness = 0.018 + vAo * (skyContribution + directionalContribution + emittedContribution);
    vec3 lit = uFullbright ? texel.rgb : texel.rgb * brightness + vec3(1.0,0.54,0.18) * vBlockLight * 0.13;
    if (uWaterPass) lit = mix(lit, vec3(0.07,0.26,0.47), 0.28);
    float fogStart = mix(75.0, 105.0, uDaylight);
    float fog = smoothstep(fogStart, fogStart + 65.0, vDistance);
    fragColor = vec4(mix(lit, uSkyColor, fog), uWaterPass ? texel.a * 0.68 : texel.a);
}
)GLSL";

constexpr const char* SkyVertexShader = R"GLSL(
#version 330 core
out vec2 vNdc;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vNdc = p * 2.0 - 1.0;
    gl_Position = vec4(vNdc, 1.0, 1.0);
}
)GLSL";

constexpr const char* SkyFragmentShader = R"GLSL(
#version 330 core
in vec2 vNdc;
uniform mat4 uInverseViewProjection;
uniform vec3 uSunDirection;
uniform float uDaylight;
uniform float uTime;
out vec4 fragColor;
float hash(vec3 p) {
    p = fract(p * 0.1031); p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}
float valueNoise(vec2 p) {
    vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
    float a=hash(vec3(i,7.0)), b=hash(vec3(i+vec2(1,0),7.0));
    float c=hash(vec3(i+vec2(0,1),7.0)), d=hash(vec3(i+vec2(1,1),7.0));
    return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
}
float cloudNoise(vec2 p) {
    float value=0.0, amplitude=0.55;
    for(int i=0;i<4;i++){ value+=valueNoise(p)*amplitude; p=p*2.03+17.1; amplitude*=0.5; }
    return value;
}
void main() {
    vec4 farPoint = uInverseViewProjection * vec4(vNdc, 1.0, 1.0);
    vec3 ray = normalize(farPoint.xyz / farPoint.w);
    float horizon = clamp(ray.y * 0.55 + 0.45, 0.0, 1.0);
    vec3 nightHorizon = vec3(0.025,0.035,0.075);
    vec3 nightZenith = vec3(0.005,0.008,0.025);
    vec3 dayHorizon = vec3(0.58,0.76,0.94);
    vec3 dayZenith = vec3(0.22,0.48,0.82);
    vec3 color = mix(mix(nightHorizon, nightZenith, horizon),
                     mix(dayHorizon, dayZenith, horizon), uDaylight);
    float twilight = (1.0-uDaylight) * smoothstep(-0.28,0.05,uSunDirection.y);
    color += vec3(0.42,0.10,0.035) * twilight * pow(1.0-horizon,2.0);
    float twinkle = 0.72 + 0.28*sin(uTime*2.0 + hash(floor(ray*520.0))*40.0);
    float star = step(0.9962, hash(floor(ray * 520.0))) * (1.0-uDaylight) * smoothstep(-0.12,0.18,ray.y) * twinkle;
    color += vec3(star);
    if(ray.y > 0.025) {
        vec2 cloudUv = ray.xz / (ray.y + 0.22) * 1.8 + vec2(uTime*0.012, uTime*0.002);
        float clouds = smoothstep(0.54,0.68,cloudNoise(cloudUv));
        clouds *= smoothstep(0.02,0.16,ray.y) * (0.18 + 0.82*uDaylight);
        color = mix(color, mix(vec3(0.16,0.18,0.24),vec3(0.96,0.97,1.0),uDaylight), clouds*0.72);
    }
    float sun = smoothstep(0.9987, 0.99965, dot(ray, uSunDirection));
    float glow = pow(max(dot(ray, uSunDirection), 0.0), 96.0);
    color += vec3(1.0,0.78,0.38) * (sun * 1.5 + glow * 0.32) * uDaylight;
    float moonDot=dot(ray,-uSunDirection);
    float moon = smoothstep(0.9989,0.99972,moonDot);
    float phaseCut = smoothstep(0.9990,0.99974,dot(normalize(ray+vec3(0.018,0.0,0.0)),-uSunDirection));
    color += vec3(0.70,0.78,0.94) * max(moon-phaseCut*0.46,0.0) * (1.0-uDaylight);
    fragColor = vec4(color,1.0);
}
)GLSL";

constexpr const char* EntityVertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vNormal;
void main(){ gl_Position=uProjection*uView*uModel*vec4(aPosition,1.0); vNormal=mat3(uModel)*aNormal; }
)GLSL";
constexpr const char* EntityFragmentShader = R"GLSL(
#version 330 core
in vec3 vNormal;
uniform vec3 uColor;
uniform vec3 uSunDirection;
uniform float uDaylight;
out vec4 fragColor;
void main(){
    float diffuse=max(dot(normalize(vNormal),uSunDirection),0.0);
    float light=mix(0.16,0.62,uDaylight)+diffuse*0.32*uDaylight;
    fragColor=vec4(uColor*light,1.0);
}
)GLSL";
constexpr const char* UiVertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec2 aPosition;
layout(location=1) in vec4 aColor;
out vec4 vColor;
void main(){ gl_Position=vec4(aPosition,0.0,1.0); vColor=aColor; }
)GLSL";
constexpr const char* UiFragmentShader = R"GLSL(
#version 330 core
in vec4 vColor; out vec4 fragColor;
void main(){ fragColor=vColor; }
)GLSL";

constexpr const char* ParticleVertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aColor;
uniform mat4 uView; uniform mat4 uProjection;
out vec3 vColor;
void main(){
    vec4 viewPos=uView*vec4(aPosition,1.0);
    gl_Position=uProjection*viewPos;
    gl_PointSize=clamp(55.0/max(-viewPos.z,1.0),2.0,7.0);
    vColor=aColor;
}
)GLSL";
constexpr const char* ParticleFragmentShader = R"GLSL(
#version 330 core
in vec3 vColor; out vec4 fragColor;
void main(){ fragColor=vec4(vColor,1.0); }
)GLSL";

struct UiVertex { glm::vec2 position; glm::vec4 color; };
struct ParticleVertex { glm::vec3 position; glm::vec3 color; };
struct EntityVertex { glm::vec3 position; glm::vec3 normal; };

struct CelestialState {
    glm::vec3 sunDirection;
    glm::vec3 skyColor;
    float daylight;
};

CelestialState celestial(float worldTime) {
    constexpr float cycleSeconds = 210.0f;
    const float angle = worldTime / cycleSeconds * glm::two_pi<float>() + 0.35f;
    const glm::vec3 sun = glm::normalize(glm::vec3(std::cos(angle) * 0.72f, std::sin(angle), -0.42f));
    const float daylight = glm::smoothstep(-0.13f, 0.17f, sun.y);
    return {sun, glm::mix(glm::vec3(0.025f,0.035f,0.075f), glm::vec3(0.50f,0.72f,0.93f), daylight), daylight};
}

std::uint8_t hashPixel(int x, int y, int seed) {
    std::uint32_t n = static_cast<std::uint32_t>(x * 374761393 + y * 668265263 + seed * 2246822519U);
    n = (n ^ (n >> 13U)) * 1274126177U;
    return static_cast<std::uint8_t>((n ^ (n >> 16U)) & 0xffU);
}

void putPixel(std::vector<std::uint8_t>& pixels, int width, int x, int y,
              int r, int g, int b, int a=255) {
    const std::size_t i=static_cast<std::size_t>((y*width+x)*4);
    pixels[i]=static_cast<std::uint8_t>(std::clamp(r,0,255));
    pixels[i+1]=static_cast<std::uint8_t>(std::clamp(g,0,255));
    pixels[i+2]=static_cast<std::uint8_t>(std::clamp(b,0,255));
    pixels[i+3]=static_cast<std::uint8_t>(std::clamp(a,0,255));
}

void addRect(std::vector<UiVertex>& vertices, float x, float y, float w, float h,
             const glm::vec4& color, int screenWidth, int screenHeight) {
    auto ndc=[&](float px,float py){ return glm::vec2(px/screenWidth*2.0f-1.0f, 1.0f-py/screenHeight*2.0f); };
    const glm::vec2 a=ndc(x,y), b=ndc(x+w,y), c=ndc(x+w,y+h), d=ndc(x,y+h);
    vertices.insert(vertices.end(),{{a,color},{b,color},{c,color},{a,color},{c,color},{d,color}});
}

const std::array<std::uint8_t,7>& glyph(char c) {
    static const std::array<std::uint8_t,7> blank{};
    static const std::unordered_map<char,std::array<std::uint8_t,7>> font={
        {'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},
        {'C',{14,17,16,16,16,17,14}},{'D',{30,17,17,17,17,17,30}},
        {'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
        {'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},
        {'I',{14,4,4,4,4,4,14}},{'J',{7,2,2,2,18,18,12}},
        {'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
        {'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},
        {'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},
        {'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
        {'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},
        {'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},
        {'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
        {'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
        {'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},
        {'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},
        {'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
        {'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},
        {'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,1,14}},
        {'-',{0,0,0,31,0,0,0}},{'.',{0,0,0,0,0,12,12}},{':',{0,12,12,0,12,12,0}},
        {'/',{1,2,2,4,8,8,16}},{'+',{0,4,4,31,4,4,0}},{' ',{0,0,0,0,0,0,0}}
    };
    const auto found=font.find(c); return found==font.end()?blank:found->second;
}

glm::vec3 blockColor(Block block) {
    switch(block){
        case Block::Grass:return {0.30f,0.67f,0.20f}; case Block::Dirt:return {0.48f,0.31f,0.17f};
        case Block::Stone:return {0.50f,0.50f,0.52f}; case Block::Sand:return {0.82f,0.75f,0.47f};
        case Block::Log:return {0.42f,0.26f,0.12f}; case Block::Leaves:return {0.16f,0.48f,0.13f};
        case Block::Water:return {0.10f,0.38f,0.72f}; case Block::CoalOre:return {0.18f,0.18f,0.20f};
        case Block::IronOre:return {0.72f,0.44f,0.27f}; case Block::GoldOre:return {0.92f,0.70f,0.12f}; default:return {0.8f,0.8f,0.8f};
    }
}
}

Renderer::Renderer() {
    auto makeProgram=[&](const char* vs,const char* fs){ GLuint v=compileShader(GL_VERTEX_SHADER,vs),f=compileShader(GL_FRAGMENT_SHADER,fs); GLuint p=linkProgram(v,f); glDeleteShader(v);glDeleteShader(f);return p;};
    worldProgram_=makeProgram(WorldVertexShader,WorldFragmentShader);
    skyProgram_=makeProgram(SkyVertexShader,SkyFragmentShader);
    uiProgram_=makeProgram(UiVertexShader,UiFragmentShader);
    particleProgram_=makeProgram(ParticleVertexShader,ParticleFragmentShader);
    entityProgram_=makeProgram(EntityVertexShader,EntityFragmentShader);
    atlasTexture_=createAtlasTexture();
    glGenVertexArrays(1,&skyVao_);
    glGenVertexArrays(1,&uiVao_); glGenBuffers(1,&uiVbo_);
    glBindVertexArray(uiVao_); glBindBuffer(GL_ARRAY_BUFFER,uiVbo_);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(UiVertex),reinterpret_cast<void*>(offsetof(UiVertex,position)));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(UiVertex),reinterpret_cast<void*>(offsetof(UiVertex,color)));
    glGenVertexArrays(1,&particleVao_); glGenBuffers(1,&particleVbo_);
    glBindVertexArray(particleVao_); glBindBuffer(GL_ARRAY_BUFFER,particleVbo_);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(ParticleVertex),reinterpret_cast<void*>(offsetof(ParticleVertex,position)));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(ParticleVertex),reinterpret_cast<void*>(offsetof(ParticleVertex,color)));
    const glm::vec3 normals[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const glm::vec3 corners[6][4]={
        {{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{.5f,.5f,.5f},{.5f,-.5f,.5f}},
        {{-.5f,-.5f,.5f},{-.5f,.5f,.5f},{-.5f,.5f,-.5f},{-.5f,-.5f,-.5f}},
        {{-.5f,.5f,.5f},{.5f,.5f,.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f}},
        {{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,-.5f,.5f},{-.5f,-.5f,.5f}},
        {{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f},{-.5f,-.5f,.5f}},
        {{-.5f,-.5f,-.5f},{-.5f,.5f,-.5f},{.5f,.5f,-.5f},{.5f,-.5f,-.5f}}};
    const int indices[6]={0,1,2,0,2,3};std::vector<EntityVertex> cube;cube.reserve(36);
    for(int face=0;face<6;++face)for(int index:indices)cube.push_back({corners[face][index],normals[face]});
    glGenVertexArrays(1,&entityVao_);glGenBuffers(1,&entityVbo_);glBindVertexArray(entityVao_);glBindBuffer(GL_ARRAY_BUFFER,entityVbo_);
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(cube.size()*sizeof(EntityVertex)),cube.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(EntityVertex),reinterpret_cast<void*>(offsetof(EntityVertex,position)));
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(EntityVertex),reinterpret_cast<void*>(offsetof(EntityVertex,normal)));
    glBindVertexArray(0);
}

Renderer::~Renderer(){
    if(entityVbo_)glDeleteBuffers(1,&entityVbo_);if(entityVao_)glDeleteVertexArrays(1,&entityVao_);
    if(particleVbo_)glDeleteBuffers(1,&particleVbo_);if(particleVao_)glDeleteVertexArrays(1,&particleVao_);
    if(uiVbo_)glDeleteBuffers(1,&uiVbo_);if(uiVao_)glDeleteVertexArrays(1,&uiVao_);if(skyVao_)glDeleteVertexArrays(1,&skyVao_);
    if(atlasTexture_)glDeleteTextures(1,&atlasTexture_);if(entityProgram_)glDeleteProgram(entityProgram_);if(particleProgram_)glDeleteProgram(particleProgram_);
    if(uiProgram_)glDeleteProgram(uiProgram_);if(skyProgram_)glDeleteProgram(skyProgram_);if(worldProgram_)glDeleteProgram(worldProgram_);
}

GLuint Renderer::compileShader(GLenum type,const char* source){
    GLuint shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);GLint ok=GL_FALSE;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){GLint n=0;glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&n);std::string log(static_cast<std::size_t>(n),'\0');glGetShaderInfoLog(shader,n,nullptr,log.data());glDeleteShader(shader);throw std::runtime_error("Shader compilation failed: "+log);}return shader;
}
GLuint Renderer::linkProgram(GLuint v,GLuint f){GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok=GL_FALSE;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){GLint n=0;glGetProgramiv(p,GL_INFO_LOG_LENGTH,&n);std::string log(static_cast<std::size_t>(n),'\0');glGetProgramInfoLog(p,n,nullptr,log.data());glDeleteProgram(p);throw std::runtime_error("Shader linking failed: "+log);}return p;}

GLuint Renderer::createAtlasTexture(){
    constexpr int s=16,count=17,width=s*count;std::vector<std::uint8_t> p(static_cast<std::size_t>(width*s*4));
    for(int y=0;y<s;++y)for(int x=0;x<s;++x){
        int g=static_cast<int>(hashPixel(x,y,11)%31)-15,d=static_cast<int>(hashPixel(x,y,29)%35)-17,st=static_cast<int>(hashPixel(x,y,53)%39)-19;
        putPixel(p,width,x,y,static_cast<std::uint8_t>(78+g/3),static_cast<std::uint8_t>(150+g),static_cast<std::uint8_t>(57+g/3));
        if(y<4)putPixel(p,width,s+x,y,72+g/3,143+g,52+g/3);else putPixel(p,width,s+x,y,126+d,86+d/2,52+d/3);
        putPixel(p,width,2*s+x,y,126+d,86+d/2,52+d/3);putPixel(p,width,3*s+x,y,126+st,126+st,128+st);
        int sn=static_cast<int>(hashPixel(x,y,71)%25)-12;putPixel(p,width,4*s+x,y,211+sn,194+sn,126+sn/2);
        int wn=static_cast<int>(hashPixel(x,y,83)%25)-12;putPixel(p,width,5*s+x,y,111+wn,72+wn/2,35+wn/3);
        int dx=x-7,dy=y-7,r=static_cast<int>(std::sqrt(static_cast<float>(dx*dx+dy*dy))*7);putPixel(p,width,6*s+x,y,139-r,96-r,48-r/2);
        int ln=static_cast<int>(hashPixel(x,y,97)%35)-17;bool hole=hashPixel(x,y,101)>242;putPixel(p,width,7*s+x,y,45+ln/3,121+ln,38+ln/3,hole?0:255);
        int wave=((x+y/2)%5==0)?25:0;putPixel(p,width,8*s+x,y,35+wave,105+wave,190+wave,170);
        bool coalFleck=hashPixel(x,y,131)>210;putPixel(p,width,9*s+x,y,coalFleck?38:126+st,coalFleck?38:126+st,coalFleck?42:128+st);
        bool ironFleck=hashPixel(x,y,149)>216;putPixel(p,width,10*s+x,y,ironFleck?188:126+st,ironFleck?125:126+st,ironFleck?82:128+st);
        bool goldFleck=hashPixel(x,y,167)>222;putPixel(p,width,11*s+x,y,goldFleck?238:126+st,goldFleck?190:126+st,goldFleck?42:128+st);
        bool copperFleck=hashPixel(x,y,179)>216;putPixel(p,width,12*s+x,y,copperFleck?198:126+st,copperFleck?91:126+st,copperFleck?43:128+st);
        bool diamondFleck=hashPixel(x,y,191)>226;putPixel(p,width,13*s+x,y,diamondFleck?54:126+st,diamondFleck?222:126+st,diamondFleck?226:128+st);
        int plankLine=(y%5==0)?-24:0;putPixel(p,width,14*s+x,y,174+plankLine+d/4,116+plankLine+d/5,55+plankLine/2);
        bool tableGrid=(x%8==0||y%8==0);putPixel(p,width,15*s+x,y,tableGrid?72:145+d/5,tableGrid?42:86+d/6,tableGrid?20:39);
        bool flame=(y<6&&x>4&&x<11);putPixel(p,width,16*s+x,y,flame?255:105+wn/3,flame?178:67+wn/4,flame?34:28);
    }
    GLuint t=0;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,s,0,GL_RGBA,GL_UNSIGNED_BYTE,p.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);return t;
}

void Renderer::renderSky(const glm::mat4& view,const glm::mat4& projection,float worldTime)const{
    const CelestialState state=celestial(worldTime);glm::mat4 rotationView=glm::mat4(glm::mat3(view));glm::mat4 inverse=glm::inverse(projection*rotationView);
    glDisable(GL_DEPTH_TEST);glUseProgram(skyProgram_);glUniformMatrix4fv(glGetUniformLocation(skyProgram_,"uInverseViewProjection"),1,GL_FALSE,glm::value_ptr(inverse));
    glUniform3fv(glGetUniformLocation(skyProgram_,"uSunDirection"),1,glm::value_ptr(state.sunDirection));glUniform1f(glGetUniformLocation(skyProgram_,"uDaylight"),state.daylight);glUniform1f(glGetUniformLocation(skyProgram_,"uTime"),worldTime);glBindVertexArray(skyVao_);glDrawArrays(GL_TRIANGLES,0,3);glEnable(GL_DEPTH_TEST);
}

void Renderer::renderWorld(const World& world,const glm::mat4& view,const glm::mat4& projection,const glm::vec3& camera,float worldTime,bool fullbright)const{
    const CelestialState state=celestial(worldTime);const glm::mat4 vp=projection*view;glUseProgram(worldProgram_);
    glUniformMatrix4fv(glGetUniformLocation(worldProgram_,"uView"),1,GL_FALSE,glm::value_ptr(view));glUniformMatrix4fv(glGetUniformLocation(worldProgram_,"uProjection"),1,GL_FALSE,glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(worldProgram_,"uSunDirection"),1,glm::value_ptr(state.sunDirection));glUniform3fv(glGetUniformLocation(worldProgram_,"uSkyColor"),1,glm::value_ptr(state.skyColor));glUniform1f(glGetUniformLocation(worldProgram_,"uDaylight"),state.daylight);glUniform1i(glGetUniformLocation(worldProgram_,"uFullbright"),fullbright?GL_TRUE:GL_FALSE);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlasTexture_);glUniform1i(glGetUniformLocation(worldProgram_,"uAtlas"),0);glUniform1i(glGetUniformLocation(worldProgram_,"uWaterPass"),GL_FALSE);world.drawOpaque(vp);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glUniform1i(glGetUniformLocation(worldProgram_,"uWaterPass"),GL_TRUE);world.drawWater(vp,camera);glDepthMask(GL_TRUE);glDisable(GL_BLEND);
}

void Renderer::spawnBreakParticles(const glm::ivec3& blockPosition,Block block){
    static std::mt19937 rng(424242);std::uniform_real_distribution<float> random(-1.0f,1.0f);const glm::vec3 color=blockColor(block);
    for(int i=0;i<18;++i){glm::vec3 velocity(random(rng)*2.2f,1.4f+std::abs(random(rng))*2.2f,random(rng)*2.2f);particles_.push_back({glm::vec3(blockPosition)+glm::vec3(0.5f)+glm::vec3(random(rng),random(rng),random(rng))*0.32f,velocity,color,0.65f+std::abs(random(rng))*0.35f});}
}
void Renderer::updateParticles(float dt){for(Particle& p:particles_){p.life-=dt;p.velocity.y-=10.0f*dt;p.position+=p.velocity*dt;}particles_.erase(std::remove_if(particles_.begin(),particles_.end(),[](const Particle& p){return p.life<=0.0f;}),particles_.end());}
void Renderer::renderParticles(const glm::mat4& view,const glm::mat4& projection)const{
    if(particles_.empty())return;std::vector<ParticleVertex> vertices;vertices.reserve(particles_.size());for(const Particle& p:particles_)vertices.push_back({p.position,p.color*std::min(1.0f,p.life*2.0f)});
    glBindBuffer(GL_ARRAY_BUFFER,particleVbo_);glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices.size()*sizeof(ParticleVertex)),vertices.data(),GL_STREAM_DRAW);glUseProgram(particleProgram_);glUniformMatrix4fv(glGetUniformLocation(particleProgram_,"uView"),1,GL_FALSE,glm::value_ptr(view));glUniformMatrix4fv(glGetUniformLocation(particleProgram_,"uProjection"),1,GL_FALSE,glm::value_ptr(projection));glEnable(GL_PROGRAM_POINT_SIZE);glBindVertexArray(particleVao_);glDrawArrays(GL_POINTS,0,static_cast<GLsizei>(vertices.size()));glBindVertexArray(0);
}

void Renderer::renderEntities(const std::vector<RenderCuboid>& cuboids,const glm::mat4& view,
                              const glm::mat4& projection,float worldTime)const{
    if(cuboids.empty())return;const CelestialState state=celestial(worldTime);glUseProgram(entityProgram_);
    glUniformMatrix4fv(glGetUniformLocation(entityProgram_,"uView"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(entityProgram_,"uProjection"),1,GL_FALSE,glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(entityProgram_,"uSunDirection"),1,glm::value_ptr(state.sunDirection));
    glUniform1f(glGetUniformLocation(entityProgram_,"uDaylight"),state.daylight);glBindVertexArray(entityVao_);
    for(const RenderCuboid& cuboid:cuboids){glm::mat4 model=glm::translate(glm::mat4(1.0f),cuboid.center);model=glm::scale(model,cuboid.size);
        glUniformMatrix4fv(glGetUniformLocation(entityProgram_,"uModel"),1,GL_FALSE,glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(entityProgram_,"uColor"),1,glm::value_ptr(cuboid.color));glDrawArrays(GL_TRIANGLES,0,36);}
    glBindVertexArray(0);
}
void Renderer::renderHud(int width,int height,const Inventory& inventory,float health,float hurtFlash,
                         float breakProgress,const std::string& debugText,const std::string& craftingText)const{
    std::vector<UiVertex> vertices;
    auto drawText=[&](const std::string& text,float originX,float originY,float scale,const glm::vec4& color){
        float px=originX,py=originY;for(char raw:text){char c=static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
            if(c=='\n'){px=originX;py+=9.0f*scale;continue;}const auto& rows=glyph(c);
            for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[static_cast<std::size_t>(row)]&(1<<(4-col)))
                addRect(vertices,px+col*scale,py+row*scale,scale,scale,color,width,height);px+=6.0f*scale;}
    };
    if(hurtFlash>0.0f)addRect(vertices,0,0,static_cast<float>(width),static_cast<float>(height),{0.72f,0.02f,0.01f,std::min(0.24f,hurtFlash*0.65f)},width,height);
    const float slot=44.0f,gap=4.0f,total=9*slot+8*gap,start=(width-total)*0.5f,y=height-58.0f;
    for(int i=0;i<9;++i){const float x=start+i*(slot+gap);const ItemStack& stack=inventory.slot(i);const Item item=stack.item;const int count=stack.count;
        if(i==inventory.selectedSlot())addRect(vertices,x-3,y-3,slot+6,slot+6,{0.95f,0.95f,0.95f,0.98f},width,height);
        addRect(vertices,x,y,slot,slot,{0.06f,0.06f,0.08f,0.86f},width,height);addRect(vertices,x+3,y+3,slot-6,slot-6,{0.22f,0.22f,0.25f,0.90f},width,height);
        glm::vec3 color=itemColor(item);if(count<=0)color*=0.22f;
        if(isTool(item)){addRect(vertices,x+20,y+13,5,23,{0.42f,0.25f,0.10f,1},width,height);addRect(vertices,x+11,y+10,24,7,{color,1},width,height);}
        else addRect(vertices,x+11,y+11,22,22,{color,1.0f},width,height);
        drawText(std::to_string(i+1),x+4,y+4,1.0f,{0.78f,0.80f,0.84f,1});
        if(count>0)drawText(std::to_string(count),x+slot-4-static_cast<float>(std::to_string(count).size())*6.0f,y+slot-10,1.0f,{1,1,1,1});
    }
    const float healthX=start,healthY=y-20.0f;for(int i=0;i<10;++i){const float filled=std::clamp(health-static_cast<float>(i*2),0.0f,2.0f)/2.0f;
        addRect(vertices,healthX+i*18,healthY,14,11,{0.16f,0.03f,0.04f,0.90f},width,height);if(filled>0)addRect(vertices,healthX+i*18+2,healthY+2,10*filled,7,{0.86f,0.08f,0.10f,1},width,height);}
    addRect(vertices,width*0.5f-10,height*0.5f-1,20,2,{1,1,1,0.95f},width,height);addRect(vertices,width*0.5f-1,height*0.5f-10,2,20,{1,1,1,0.95f},width,height);
    if(breakProgress>0.0f){addRect(vertices,width*0.5f-37,height*0.5f+18,74,8,{0,0,0,0.72f},width,height);addRect(vertices,width*0.5f-35,height*0.5f+20,70*std::clamp(breakProgress,0.0f,1.0f),4,{0.92f,0.78f,0.30f,1},width,height);}
    if(!debugText.empty()){int lines=1,maxChars=0,current=0;for(char c:debugText){if(c=='\n'){++lines;maxChars=std::max(maxChars,current);current=0;}else ++current;}maxChars=std::max(maxChars,current);
        addRect(vertices,8,8,static_cast<float>(maxChars*12+12),static_cast<float>(lines*18+10),{0,0,0,0.62f},width,height);drawText(debugText,14,14,2.0f,{0.92f,0.96f,1.0f,1});}
    if(!craftingText.empty()){const float panelW=std::min(680.0f,static_cast<float>(width)-32.0f),panelX=(width-panelW)*0.5f,panelY=y-72.0f;
        addRect(vertices,panelX,panelY,panelW,54,{0.04f,0.03f,0.025f,0.88f},width,height);drawText(craftingText,panelX+12,panelY+9,1.5f,{0.98f,0.86f,0.58f,1});}
    glBindBuffer(GL_ARRAY_BUFFER,uiVbo_);glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices.size()*sizeof(UiVertex)),vertices.data(),GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUseProgram(uiProgram_);glBindVertexArray(uiVao_);
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices.size()));glBindVertexArray(0);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);
}