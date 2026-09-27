#include "Renderer.h"
#include "Definitions.h"

#include "World.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#ifdef VOXEL_STANDALONE
#include "EmbeddedResources.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif

namespace {
struct EntityVisibility {
    std::array<glm::vec4, 6> planes{};
    glm::vec3 camera{0.0f};
    float maximumDistance = 0.0f;

    EntityVisibility(const glm::mat4& view,
                     const glm::mat4& projection,
        float maximumDistance)
        : camera(glm::vec3(glm::inverse(view)[3])),
          maximumDistance(maximumDistance) {
        const glm::mat4 clip = projection * view;
        const glm::vec4 rows[4] = {
            {clip[0][0], clip[1][0], clip[2][0], clip[3][0]},
            {clip[0][1], clip[1][1], clip[2][1], clip[3][1]},
            {clip[0][2], clip[1][2], clip[2][2], clip[3][2]},
            {clip[0][3], clip[1][3], clip[2][3], clip[3][3]}};
        planes = {rows[3] + rows[0], rows[3] - rows[0], rows[3] + rows[1],
                  rows[3] - rows[1], rows[3] + rows[2], rows[3] - rows[2]};
        for (glm::vec4& plane : planes)
            plane /= glm::length(glm::vec3(plane));
    }

    bool visible(const glm::vec3& center, float radius) const {
        const glm::vec3 delta = center - camera;
        const float allowed = maximumDistance + radius;
        if (glm::dot(delta, delta) > allowed * allowed)
            return false;
        for (const glm::vec4& plane : planes)
            if (glm::dot(glm::vec3(plane), center) + plane.w < -radius)
                return false;
        return true;
    }
};

constexpr const char* WorldVertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aUv;
layout(location=2) in vec3 aNormal;
layout(location=3) in float aSunLight;
layout(location=4) in float aBlockLight;
layout(location=5) in float aAo;
layout(location=6) in float aAtlasTile;
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
void main() {
    vec4 viewPosition = uView * vec4(aPosition, 1.0);
    gl_Position = uProjection * viewPosition;
    vUv = aUv; vNormal = aNormal;
    vSunLight = aSunLight; vBlockLight = aBlockLight; vAo = aAo;
    vDistance = length(viewPosition.xyz);
    vWorldPosition = aPosition;
    vAtlasTile = aAtlasTile;
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
in vec3 vWorldPosition;
in float vAtlasTile;
uniform sampler2D uAtlas;
uniform vec3 uSunDirection;
uniform vec3 uSkyColor;
uniform vec3 uCameraPosition;
uniform float uDaylight;
uniform float uTime;
uniform bool uWaterPass;
uniform bool uFullbright;
uniform bool uUnderwater;
uniform bool uSpectatorInsideBlock;
uniform int uEffectQuality;
out vec4 fragColor;
void main() {
    if (uSpectatorInsideBlock &&
        all(equal(ivec3(floor(vWorldPosition - normalize(vNormal) * 0.001)),
                  ivec3(floor(uCameraPosition))))) discard;
    vec2 atlasUv = vUv;
    if (vAtlasTile >= 0.0) {
        vec2 repeatUv = clamp(fract(vUv), vec2(0.03125), vec2(0.96875));
        atlasUv = vec2((vAtlasTile + repeatUv.x) / 45.0, repeatUv.y);
    }
    vec4 texel = texture(uAtlas, atlasUv);
    if (texel.a < 0.20) discard;
    vec3 normal = normalize(vNormal);
    float day = smoothstep(0.0, 1.0, uDaylight);
    float sunDiffuse = max(dot(normal, uSunDirection), 0.0);
    float moonDiffuse = max(dot(normal, -uSunDirection), 0.0);
    float faceShade = 0.84 + max(normal.y, 0.0) * 0.16 - max(-normal.y, 0.0) * 0.14;
    float skyContribution = vSunLight * mix(0.055, 0.48, day);
    float sunContribution = vSunLight * sunDiffuse * mix(0.0, 0.54, day);
    float moonContribution = vSunLight * moonDiffuse * (1.0-day) * 0.17;
    float emittedContribution = vBlockLight * 0.90;
    float brightness = 0.024 + (uEffectQuality == 0 ? 1.0 : vAo) * faceShade *
        (skyContribution + sunContribution + moonContribution + emittedContribution);
    vec3 sunTint = mix(vec3(1.0,0.48,0.20), vec3(1.0,0.93,0.76),
                       smoothstep(-0.02,0.42,uSunDirection.y));
    vec3 naturalLight = mix(vec3(0.56,0.66,0.88), sunTint, day);
    vec3 lit = uFullbright ? texel.rgb : texel.rgb * brightness * naturalLight +
               vec3(1.0,0.50,0.16) * vBlockLight * 0.14;
    float alpha = texel.a;
    if (uWaterPass) {
        float topSurface = max(normal.y,0.0);
        vec3 waterColor = mix(vec3(0.035,0.17,0.34), vec3(0.08,0.34,0.56), day);
        lit = mix(lit, waterColor, 0.34);
        if (uEffectQuality > 0) {
            float ripple = sin(vWorldPosition.x*0.34 + uTime*0.75) *
                           sin(vWorldPosition.z*0.29 - uTime*0.58);
            float highlight = topSurface * max(ripple,0.0) * (0.05 + 0.12*day);
            lit += highlight;
            if (uEffectQuality > 1) {
                vec3 viewDirection = normalize(uCameraPosition-vWorldPosition);
                float fresnel = pow(1.0-max(dot(viewDirection,normal),0.0),3.0);
                lit += uSkyColor * fresnel * topSurface * 0.16;
            }
        }
        alpha *= 0.68;
    }
    if (uUnderwater) {
        float underwaterFog = smoothstep(3.0,24.0,vDistance);
        lit = mix(lit,vec3(0.025,0.18,0.30),underwaterFog);
    }
    fragColor = vec4(lit,alpha);
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
uniform int uEffectQuality;
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
    int octaves = uEffectQuality == 0 ? 1 : (uEffectQuality == 1 ? 2 : 4);
    for(int i=0;i<octaves;i++){ value+=valueNoise(p)*amplitude; p=p*2.03+17.1; amplitude*=0.5; }
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
    float twilight = 1.0-smoothstep(0.02,0.34,abs(uSunDirection.y));
    float horizonBand = exp(-abs(ray.y)*5.5);
    color = mix(color,vec3(0.94,0.29,0.08),twilight*horizonBand*0.42);
    color += vec3(0.34,0.08,0.025) * twilight * pow(1.0-horizon,2.0);
    float twinkle = 0.72 + 0.28*sin(uTime*2.0 + hash(floor(ray*520.0))*40.0);
    float star = step(0.9962, hash(floor(ray * 520.0))) * (1.0-uDaylight) * smoothstep(-0.12,0.18,ray.y) * twinkle;
    color += vec3(star);
    if(ray.y > 0.025 && uEffectQuality > 0) {
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
    vec3 normal=normalize(vNormal);
    float sun=max(dot(normal,uSunDirection),0.0);
    float moon=max(dot(normal,-uSunDirection),0.0);
    float light=mix(0.14,0.58,uDaylight)+sun*0.38*uDaylight+
                moon*0.14*(1.0-uDaylight);
    vec3 tint=mix(vec3(0.62,0.70,0.92),vec3(1.0,0.96,0.84),uDaylight);
    fragColor=vec4(uColor*light*tint,1.0);
}
)GLSL";
constexpr const char* UiVertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec2 aPosition;
layout(location=1) in vec4 aColor;
layout(location=2) in vec2 aUv;
layout(location=3) in float aTextured;
out vec4 vColor; out vec2 vUv; out float vTextured;
void main(){ gl_Position=vec4(aPosition,0.0,1.0); vColor=aColor; vUv=aUv; vTextured=aTextured; }
)GLSL";
constexpr const char* UiFragmentShader = R"GLSL(
#version 330 core
in vec4 vColor; in vec2 vUv; in float vTextured; uniform sampler2D uItemAtlas; out vec4 fragColor;
void main(){ vec4 sprite=texture(uItemAtlas,vUv); fragColor=mix(vColor,sprite*vColor,clamp(vTextured,0.0,1.0)); if(fragColor.a<0.02)discard; }
)GLSL";

constexpr const char* ItemVertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition; layout(location=1) in vec2 aUv;
uniform mat4 uView; uniform mat4 uProjection; out vec2 vUv;
void main(){gl_Position=uProjection*uView*vec4(aPosition,1.0);vUv=aUv;}
)GLSL";
constexpr const char* ItemFragmentShader = R"GLSL(
#version 330 core
in vec2 vUv; uniform sampler2D uItemAtlas; out vec4 fragColor;
void main(){vec4 c=texture(uItemAtlas,vUv);if(c.a<0.12)discard;fragColor=vec4(c.rgb,c.a);}
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

struct UiVertex {
    glm::vec2 position;
    glm::vec4 color;
    glm::vec2 uv;
    float textured;
};
struct ParticleVertex {
    glm::vec3 position;
    glm::vec3 color;
};
struct EntityVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct CelestialState {
    glm::vec3 sunDirection;
    glm::vec3 skyColor;
    float daylight;
};

CelestialState celestial(float worldTime) {

    const float angle = worldTime / DayNightCycleSeconds * glm::two_pi<float>() + 0.35f;
    const glm::vec3 sun =
        glm::normalize(glm::vec3(std::cos(angle) * 0.72f, std::sin(angle), -0.42f));
    const float daylight = glm::smoothstep(-0.18f, 0.22f, sun.y);
    return {sun,
            glm::mix(glm::vec3(0.025f, 0.035f, 0.075f), glm::vec3(0.50f, 0.72f, 0.93f), daylight),
            daylight};
}

std::uint8_t hashPixel(int x, int y, int seed) {
    std::uint32_t n =
        static_cast<std::uint32_t>(x * 374761393 + y * 668265263 + seed * 2246822519U);
    n = (n ^ (n >> 13U)) * 1274126177U;
    return static_cast<std::uint8_t>((n ^ (n >> 16U)) & 0xffU);
}

void putPixel(
    std::vector<std::uint8_t>& pixels, int width, int x, int y, int r, int g, int b, int a = 255) {
    const std::size_t i = static_cast<std::size_t>((y * width + x) * 4);
    pixels[i] = static_cast<std::uint8_t>(std::clamp(r, 0, 255));
    pixels[i + 1] = static_cast<std::uint8_t>(std::clamp(g, 0, 255));
    pixels[i + 2] = static_cast<std::uint8_t>(std::clamp(b, 0, 255));
    pixels[i + 3] = static_cast<std::uint8_t>(std::clamp(a, 0, 255));
}

void addRect(std::vector<UiVertex>& vertices,
             float x,
             float y,
             float w,
             float h,
             const glm::vec4& color,
             int screenWidth,
             int screenHeight) {
    auto ndc = [&](float px, float py) {
        return glm::vec2(px / screenWidth * 2.0f - 1.0f, 1.0f - py / screenHeight * 2.0f);
    };
    const glm::vec2 a = ndc(x, y), b = ndc(x + w, y), c = ndc(x + w, y + h), d = ndc(x, y + h);
    vertices.insert(vertices.end(),
                    {{a, color, {0, 0}, 0},
                     {b, color, {0, 0}, 0},
                     {c, color, {0, 0}, 0},
                     {a, color, {0, 0}, 0},
                     {c, color, {0, 0}, 0},
                     {d, color, {0, 0}, 0}});
}

const std::array<std::uint8_t, 7>& glyph(char c) {
    static const std::array<std::uint8_t, 7> blank{};
    static const std::unordered_map<char, std::array<std::uint8_t, 7>> font = {
        {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
        {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
        {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
        {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
        {'I', {14, 4, 4, 4, 4, 4, 14}},      {'J', {7, 2, 2, 2, 18, 18, 12}},
        {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
        {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
        {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
        {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
        {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
        {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
        {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
        {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
        {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
        {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
        {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
        {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
        {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
        {'-', {0, 0, 0, 31, 0, 0, 0}},       {'.', {0, 0, 0, 0, 0, 12, 12}},
        {':', {0, 12, 12, 0, 12, 12, 0}},    {'/', {1, 2, 2, 4, 8, 8, 16}},
        {'+', {0, 4, 4, 31, 4, 4, 0}},       {' ', {0, 0, 0, 0, 0, 0, 0}}};
    const auto found = font.find(c);
    return found == font.end() ? blank : found->second;
}

void addSprite(std::vector<UiVertex>& v,
               Item item,
               float x,
               float y,
               float w,
               float h,
               const glm::vec4& color,
               int W,
               int H) {
    ItemSpriteUv uv;
    if (!itemSpriteUv(item, uv))
        return;
    auto ndc = [&](float px, float py) {
        return glm::vec2(px / W * 2.f - 1.f, 1.f - py / H * 2.f);
    };
    glm::vec2 a = ndc(x, y), b = ndc(x + w, y), c = ndc(x + w, y + h), d = ndc(x, y + h);
    v.insert(v.end(),
             {{a, color, {uv.u0, uv.v0}, 1},
              {b, color, {uv.u1, uv.v0}, 1},
              {c, color, {uv.u1, uv.v1}, 1},
              {a, color, {uv.u0, uv.v0}, 1},
              {c, color, {uv.u1, uv.v1}, 1},
              {d, color, {uv.u0, uv.v1}, 1}});
}
glm::vec3 blockColor(Block block) {
    if (isWater(block))
        return {0.10f, 0.38f, 0.72f};

    switch (block) {
    case Block::Grass:
        return {0.30f, 0.67f, 0.20f};
    case Block::Dirt:
        return {0.48f, 0.31f, 0.17f};
    case Block::Stone:
        return {0.50f, 0.50f, 0.52f};
    case Block::Sand:
        return {0.82f, 0.75f, 0.47f};
    case Block::Log:
        return {0.42f, 0.26f, 0.12f};
    case Block::Leaves:
        return {0.16f, 0.48f, 0.13f};
    case Block::Water:
        return {0.10f, 0.38f, 0.72f};
    case Block::CoalOre:
        return {0.18f, 0.18f, 0.20f};
    case Block::IronOre:
        return {0.72f, 0.44f, 0.27f};
    case Block::GoldOre:
        return {0.92f, 0.70f, 0.12f};
    case Block::CopperOre:
        return {0.76f, 0.38f, 0.20f};
    case Block::DiamondOre:
        return {0.15f, 0.90f, 0.90f};
    case Block::Cobblestone:
    case Block::StoneBricks:
    case Block::Furnace:
    case Block::StoneSlab:
        return {0.48f, 0.49f, 0.51f};
    case Block::Bricks:
        return {0.66f, 0.27f, 0.18f};
    case Block::Glass:
        return {0.66f, 0.88f, 0.94f};
    case Block::Gravel:
        return {0.43f, 0.42f, 0.42f};
    case Block::Clay:
        return {0.48f, 0.53f, 0.63f};
    case Block::Snow:
    case Block::SnowBlock:
        return {0.93f, 0.97f, 1.0f};
    case Block::BirchPlanks:
        return {0.78f, 0.68f, 0.45f};
    case Block::BirchLog:
        return {0.82f, 0.80f, 0.68f};
    case Block::BirchLeaves:
        return {0.32f, 0.68f, 0.24f};
    case Block::Cactus:
        return {0.18f, 0.52f, 0.16f};
    case Block::Bookshelf:
        return {0.55f, 0.31f, 0.16f};
    case Block::WoodenDoor:
    case Block::WoodenSlab:
        return {0.64f, 0.43f, 0.20f};
    default:
        return {0.8f, 0.8f, 0.8f};
    }
}
} // namespace

const std::array<std::uint8_t, 7>& uiGlyph(char character) {
    return glyph(character);
}

Renderer::Renderer() {
    auto makeProgram = [&](const char* vs, const char* fs) {
        GLuint v = compileShader(GL_VERTEX_SHADER, vs), f = compileShader(GL_FRAGMENT_SHADER, fs);
        GLuint p = linkProgram(v, f);
        glDeleteShader(v);
        glDeleteShader(f);
        return p;
    };
    worldProgram_ = makeProgram(WorldVertexShader, WorldFragmentShader);
    skyProgram_ = makeProgram(SkyVertexShader, SkyFragmentShader);
    uiProgram_ = makeProgram(UiVertexShader, UiFragmentShader);
    particleProgram_ = makeProgram(ParticleVertexShader, ParticleFragmentShader);
    entityProgram_ = makeProgram(EntityVertexShader, EntityFragmentShader);
    itemProgram_ = makeProgram(ItemVertexShader, ItemFragmentShader);
    uiItemAtlasUniform_ = glGetUniformLocation(uiProgram_, "uItemAtlas");
    itemAtlasUniform_ = glGetUniformLocation(itemProgram_, "uItemAtlas");
    atlasTexture_ = createAtlasTexture();
    itemTexture_ = loadItemTexture("assets/item_icons_expansion.rgba");
    glGenVertexArrays(1, &skyVao_);
    glGenVertexArrays(1, &uiVao_);
    glGenBuffers(1, &uiVbo_);
    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(UiVertex),
                          reinterpret_cast<void*>(offsetof(UiVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(UiVertex),
                          reinterpret_cast<void*>(offsetof(UiVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(UiVertex),
                          reinterpret_cast<void*>(offsetof(UiVertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,
                          1,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(UiVertex),
                          reinterpret_cast<void*>(offsetof(UiVertex, textured)));
    glGenVertexArrays(1, &particleVao_);
    glGenBuffers(1, &particleVbo_);
    glBindVertexArray(particleVao_);
    glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(ParticleVertex),
                          reinterpret_cast<void*>(offsetof(ParticleVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(ParticleVertex),
                          reinterpret_cast<void*>(offsetof(ParticleVertex, color)));
    const glm::vec3 normals[6] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    const glm::vec3 corners[6][4] = {
        {{.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}, {.5f, -.5f, .5f}},
        {{-.5f, -.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f, -.5f}, {-.5f, -.5f, -.5f}},
        {{-.5f, .5f, .5f}, {.5f, .5f, .5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}},
        {{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, -.5f, .5f}, {-.5f, -.5f, .5f}},
        {{.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}, {-.5f, -.5f, .5f}},
        {{-.5f, -.5f, -.5f}, {-.5f, .5f, -.5f}, {.5f, .5f, -.5f}, {.5f, -.5f, -.5f}}};
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    std::vector<EntityVertex> cube;
    cube.reserve(36);
    for (int face = 0; face < 6; ++face)
        for (int index : indices)
            cube.push_back({corners[face][index], normals[face]});
    glGenVertexArrays(1, &entityVao_);
    glGenBuffers(1, &entityVbo_);
    glBindVertexArray(entityVao_);
    glBindBuffer(GL_ARRAY_BUFFER, entityVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(cube.size() * sizeof(EntityVertex)),
                 cube.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(EntityVertex),
                          reinterpret_cast<void*>(offsetof(EntityVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(EntityVertex),
                          reinterpret_cast<void*>(offsetof(EntityVertex, normal)));
    glBindVertexArray(0);
    glGenVertexArrays(1, &itemVao_);
    glGenBuffers(1, &itemVbo_);
    glBindVertexArray(itemVao_);
    glBindBuffer(GL_ARRAY_BUFFER, itemVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

Renderer::~Renderer() {
    if (itemVbo_)
        glDeleteBuffers(1, &itemVbo_);
    if (itemVao_)
        glDeleteVertexArrays(1, &itemVao_);
    if (entityVbo_)
        glDeleteBuffers(1, &entityVbo_);
    if (entityVao_)
        glDeleteVertexArrays(1, &entityVao_);
    if (particleVbo_)
        glDeleteBuffers(1, &particleVbo_);
    if (particleVao_)
        glDeleteVertexArrays(1, &particleVao_);
    if (uiVbo_)
        glDeleteBuffers(1, &uiVbo_);
    if (uiVao_)
        glDeleteVertexArrays(1, &uiVao_);
    if (skyVao_)
        glDeleteVertexArrays(1, &skyVao_);
    if (itemTexture_)
        glDeleteTextures(1, &itemTexture_);
    if (atlasTexture_)
        glDeleteTextures(1, &atlasTexture_);
    if (itemProgram_)
        glDeleteProgram(itemProgram_);
    if (entityProgram_)
        glDeleteProgram(entityProgram_);
    if (particleProgram_)
        glDeleteProgram(particleProgram_);
    if (uiProgram_)
        glDeleteProgram(uiProgram_);
    if (skyProgram_)
        glDeleteProgram(skyProgram_);
    if (worldProgram_)
        glDeleteProgram(worldProgram_);
}

GLuint Renderer::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint n = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &n);
        std::string log(static_cast<std::size_t>(n), '\0');
        glGetShaderInfoLog(shader, n, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed: " + log);
    }
    return shader;
}
GLuint Renderer::linkProgram(GLuint v, GLuint f) {
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint n = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &n);
        std::string log(static_cast<std::size_t>(n), '\0');
        glGetProgramInfoLog(p, n, nullptr, log.data());
        glDeleteProgram(p);
        throw std::runtime_error("Shader linking failed: " + log);
    }
    return p;
}

GLuint Renderer::createAtlasTexture() {
    constexpr int s = 16, count = 45, width = s * count;
    std::vector<std::uint8_t> p(static_cast<std::size_t>(width * s * 4));
    for (int y = 0; y < s; ++y)
        for (int x = 0; x < s; ++x) {
            int g = static_cast<int>(hashPixel(x, y, 11) % 31) - 15,
                d = static_cast<int>(hashPixel(x, y, 29) % 35) - 17,
                st = static_cast<int>(hashPixel(x, y, 53) % 39) - 19;
            putPixel(p,
                     width,
                     x,
                     y,
                     static_cast<std::uint8_t>(78 + g / 3),
                     static_cast<std::uint8_t>(150 + g),
                     static_cast<std::uint8_t>(57 + g / 3));
            if (y < 4)
                putPixel(p, width, s + x, y, 72 + g / 3, 143 + g, 52 + g / 3);
            else
                putPixel(p, width, s + x, y, 126 + d, 86 + d / 2, 52 + d / 3);
            putPixel(p, width, 2 * s + x, y, 126 + d, 86 + d / 2, 52 + d / 3);
            putPixel(p, width, 3 * s + x, y, 126 + st, 126 + st, 128 + st);
            int sn = static_cast<int>(hashPixel(x, y, 71) % 25) - 12;
            putPixel(p, width, 4 * s + x, y, 211 + sn, 194 + sn, 126 + sn / 2);
            int wn = static_cast<int>(hashPixel(x, y, 83) % 25) - 12;
            putPixel(p, width, 5 * s + x, y, 111 + wn, 72 + wn / 2, 35 + wn / 3);
            int dx = x - 7, dy = y - 7,
                r = static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dy * dy)) * 7);
            putPixel(p, width, 6 * s + x, y, 139 - r, 96 - r, 48 - r / 2);
            int ln = static_cast<int>(hashPixel(x, y, 97) % 35) - 17;
            bool hole = hashPixel(x, y, 101) > 242;
            putPixel(p, width, 7 * s + x, y, 45 + ln / 3, 121 + ln, 38 + ln / 3, hole ? 0 : 255);
            int wave = ((x + y / 2) % 5 == 0) ? 25 : 0;
            putPixel(p, width, 8 * s + x, y, 35 + wave, 105 + wave, 190 + wave, 170);
            bool coalFleck = hashPixel(x, y, 131) > 210;
            putPixel(p,
                     width,
                     9 * s + x,
                     y,
                     coalFleck ? 38 : 126 + st,
                     coalFleck ? 38 : 126 + st,
                     coalFleck ? 42 : 128 + st);
            bool ironFleck = hashPixel(x, y, 149) > 216;
            putPixel(p,
                     width,
                     10 * s + x,
                     y,
                     ironFleck ? 188 : 126 + st,
                     ironFleck ? 125 : 126 + st,
                     ironFleck ? 82 : 128 + st);
            bool goldFleck = hashPixel(x, y, 167) > 222;
            putPixel(p,
                     width,
                     11 * s + x,
                     y,
                     goldFleck ? 238 : 126 + st,
                     goldFleck ? 190 : 126 + st,
                     goldFleck ? 42 : 128 + st);
            bool copperFleck = hashPixel(x, y, 179) > 216;
            putPixel(p,
                     width,
                     12 * s + x,
                     y,
                     copperFleck ? 198 : 126 + st,
                     copperFleck ? 91 : 126 + st,
                     copperFleck ? 43 : 128 + st);
            bool diamondFleck = hashPixel(x, y, 191) > 226;
            putPixel(p,
                     width,
                     13 * s + x,
                     y,
                     diamondFleck ? 54 : 126 + st,
                     diamondFleck ? 222 : 126 + st,
                     diamondFleck ? 226 : 128 + st);
            int plankLine = (y % 5 == 0) ? -24 : 0;
            putPixel(p,
                     width,
                     14 * s + x,
                     y,
                     174 + plankLine + d / 4,
                     116 + plankLine + d / 5,
                     55 + plankLine / 2);
            bool tableGrid = (x % 8 == 0 || y % 8 == 0);
            putPixel(p,
                     width,
                     15 * s + x,
                     y,
                     tableGrid ? 72 : 145 + d / 5,
                     tableGrid ? 42 : 86 + d / 6,
                     tableGrid ? 20 : 39);
            bool flame = (y < 6 && x > 4 && x < 11);
            putPixel(p,
                     width,
                     16 * s + x,
                     y,
                     flame ? 255 : 105 + wn / 3,
                     flame ? 178 : 67 + wn / 4,
                     flame ? 34 : 28);

            const bool cobbleMortar = (x % 5 == 0) || ((y + (x / 5) * 2) % 5 == 0);
            putPixel(p,
                     width,
                     17 * s + x,
                     y,
                     cobbleMortar ? 73 : 119 + st / 3,
                     cobbleMortar ? 74 : 120 + st / 3,
                     cobbleMortar ? 76 : 122 + st / 3);
            const bool stoneBrickMortar = y % 5 == 0 || (x + ((y / 5) % 2) * 4) % 8 == 0;
            putPixel(p,
                     width,
                     18 * s + x,
                     y,
                     stoneBrickMortar ? 70 : 132 + st / 4,
                     stoneBrickMortar ? 72 : 134 + st / 4,
                     stoneBrickMortar ? 75 : 138 + st / 4);
            const bool redBrickMortar = y % 4 == 0 || (x + ((y / 4) % 2) * 4) % 8 == 0;
            putPixel(p,
                     width,
                     19 * s + x,
                     y,
                     redBrickMortar ? 174 : 145 + d / 3,
                     redBrickMortar ? 160 : 61 + d / 5,
                     redBrickMortar ? 143 : 42 + d / 7);
            const bool glassEdge = x == 0 || y == 0 || x == 15 || y == 15 ||
                                   x == y || x + y == 15;
            putPixel(p,
                     width,
                     20 * s + x,
                     y,
                     glassEdge ? 187 : 128,
                     glassEdge ? 231 : 202,
                     glassEdge ? 244 : 224,
                     glassEdge ? 190 : 76);
            const int gravelNoise = static_cast<int>(hashPixel(x, y, 337) % 66) - 33;
            putPixel(p,
                     width,
                     21 * s + x,
                     y,
                     111 + gravelNoise,
                     108 + gravelNoise,
                     107 + gravelNoise);
            putPixel(p, width, 22 * s + x, y, 119 + sn / 3, 133 + sn / 3, 158 + sn / 2);
            putPixel(p, width, 23 * s + x, y, 232 + sn / 5, 241 + sn / 6, 246 + sn / 7);
            const int birchPlankLine = y % 4 == 0 ? -30 : 0;
            putPixel(p,
                     width,
                     24 * s + x,
                     y,
                     205 + birchPlankLine + d / 8,
                     184 + birchPlankLine + d / 9,
                     128 + birchPlankLine / 2 + d / 10);
            const bool birchMark = hashPixel(x / 2, y, 353) > 228;
            putPixel(p,
                     width,
                     25 * s + x,
                     y,
                     birchMark ? 48 : 215 + sn / 3,
                     birchMark ? 48 : 211 + sn / 3,
                     birchMark ? 44 : 188 + sn / 4);
            const int birchRadius =
                static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dy * dy)) * 6);
            putPixel(p,
                     width,
                     26 * s + x,
                     y,
                     220 - birchRadius / 2,
                     190 - birchRadius / 3,
                     126 - birchRadius / 4);
            const bool birchLeafHole = hashPixel(x, y, 367) > 239;
            putPixel(p,
                     width,
                     27 * s + x,
                     y,
                     62 + ln / 3,
                     148 + ln,
                     45 + ln / 4,
                     birchLeafHole ? 0 : 255);
            const bool cactusRib = x % 5 == 0;
            putPixel(p,
                     width,
                     28 * s + x,
                     y,
                     cactusRib ? 31 : 47 + g / 5,
                     cactusRib ? 111 : 142 + g / 2,
                     cactusRib ? 35 : 50 + g / 6);
            putPixel(p,
                     width,
                     29 * s + x,
                     y,
                     56 + g / 4,
                     141 + g / 2,
                     50 + g / 5);
            const bool furnaceBorder = x < 2 || x > 13 || y < 2 || y > 13;
            const bool furnaceMouth = x >= 4 && x <= 11 && y >= 9 && y <= 13;
            putPixel(p,
                     width,
                     30 * s + x,
                     y,
                     furnaceMouth ? 220 : (furnaceBorder ? 72 : 112 + st / 4),
                     furnaceMouth ? 91 : (furnaceBorder ? 73 : 113 + st / 4),
                     furnaceMouth ? 22 : (furnaceBorder ? 76 : 116 + st / 4));
            const bool shelf = y % 6 == 0;
            const int bookBand = (x / 2 + y / 6) % 4;
            putPixel(p,
                     width,
                     31 * s + x,
                     y,
                     shelf ? 86 : (bookBand == 0 ? 176 : (bookBand == 1 ? 55 : 42)),
                     shelf ? 50 : (bookBand == 2 ? 151 : 62),
                     shelf ? 24 : (bookBand == 3 ? 174 : 55));
            const bool doorFrame = x < 2 || x > 13 || y < 2 || y > 13 || y == 8;
            const bool doorKnob = (x == 11 || x == 12) && (y == 8 || y == 9);
            putPixel(p,
                     width,
                     32 * s + x,
                     y,
                     doorKnob ? 226 : (doorFrame ? 105 : 157 + d / 6),
                     doorKnob ? 178 : (doorFrame ? 62 : 100 + d / 8),
                     doorKnob ? 57 : (doorFrame ? 27 : 43 + d / 10));
            const bool graniteFleck = hashPixel(x, y, 401) > 178;
            putPixel(p, width, 33 * s + x, y,
                     graniteFleck ? 174 : 137 + d / 4,
                     graniteFleck ? 126 : 101 + d / 5,
                     graniteFleck ? 116 : 96 + d / 6);
            const bool dioriteFleck = hashPixel(x, y, 409) > 190;
            putPixel(p, width, 34 * s + x, y,
                     dioriteFleck ? 112 : 205 + sn / 4,
                     dioriteFleck ? 114 : 203 + sn / 4,
                     dioriteFleck ? 116 : 198 + sn / 4);
            const int andesiteNoise = static_cast<int>(hashPixel(x, y, 419) % 39) - 19;
            putPixel(p, width, 35 * s + x, y,
                     112 + andesiteNoise, 116 + andesiteNoise, 119 + andesiteNoise);
            const bool mossPatch = hashPixel(x, y, 431) > 194;
            putPixel(p, width, 36 * s + x, y,
                     mossPatch ? 62 : (cobbleMortar ? 73 : 119 + st / 3),
                     mossPatch ? 116 : (cobbleMortar ? 74 : 120 + st / 3),
                     mossPatch ? 45 : (cobbleMortar ? 76 : 122 + st / 3));
            const bool brickMoss = hashPixel(x, y, 439) > 204;
            putPixel(p, width, 37 * s + x, y,
                     brickMoss ? 57 : (stoneBrickMortar ? 70 : 132 + st / 4),
                     brickMoss ? 112 : (stoneBrickMortar ? 72 : 134 + st / 4),
                     brickMoss ? 43 : (stoneBrickMortar ? 75 : 138 + st / 4));
            const bool iceLine = x == y || x + y == 15 || (x + y * 3) % 17 == 0;
            putPixel(p, width, 38 * s + x, y,
                     iceLine ? 210 : 137, iceLine ? 242 : 207, 250, iceLine ? 210 : 120);
            const int mudNoise = static_cast<int>(hashPixel(x, y, 449) % 27) - 13;
            putPixel(p, width, 39 * s + x, y,
                     76 + mudNoise, 53 + mudNoise / 2, 34 + mudNoise / 3);
            const bool grassPixel = y > 3 && std::abs(x - (7 + (y % 3) - y / 5)) < 2;
            putPixel(p, width, 40 * s + x, y,
                     grassPixel ? 54 : 0, grassPixel ? 154 : 0, grassPixel ? 32 : 0,
                     grassPixel ? 255 : 0);
            const bool redStem = y > 7 && (x == 7 || x == 8);
            const bool redPetal = y >= 3 && y <= 8 && std::abs(x - 7) + std::abs(y - 5) < 5;
            putPixel(p, width, 41 * s + x, y,
                     redPetal ? 218 : (redStem ? 42 : 0),
                     redPetal ? 45 : (redStem ? 126 : 0),
                     redPetal ? 34 : (redStem ? 31 : 0),
                     (redPetal || redStem) ? 255 : 0);
            const bool yellowStem = y > 7 && (x == 7 || x == 8);
            const bool yellowPetal = y >= 3 && y <= 8 && std::abs(x - 7) + std::abs(y - 5) < 5;
            putPixel(p, width, 42 * s + x, y,
                     yellowPetal ? 244 : (yellowStem ? 42 : 0),
                     yellowPetal ? 201 : (yellowStem ? 126 : 0),
                     yellowPetal ? 35 : (yellowStem ? 31 : 0),
                     (yellowPetal || yellowStem) ? 255 : 0);
            const bool ladderRail = x == 3 || x == 4 || x == 11 || x == 12;
            const bool ladderRung = y == 3 || y == 7 || y == 11 || y == 14;
            putPixel(p, width, 43 * s + x, y,
                     (ladderRail || ladderRung) ? 164 : 0,
                     (ladderRail || ladderRung) ? 105 : 0,
                     (ladderRail || ladderRung) ? 45 : 0,
                     (ladderRail || ladderRung) ? 255 : 0);
            const bool chestBand = y == 7 || y == 8 || x == 0 || x == 15 || y == 0 || y == 15;
            const bool chestLatch = x >= 7 && x <= 9 && y >= 6 && y <= 10;
            putPixel(p, width, 44 * s + x, y,
                     chestLatch ? 196 : (chestBand ? 75 : 151 + d / 6),
                     chestLatch ? 171 : (chestBand ? 46 : 91 + d / 8),
                     chestLatch ? 72 : (chestBand ? 22 : 36 + d / 10));
        }
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, s, 0, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

GLuint Renderer::loadItemTexture(const std::string& path) {
#ifdef VOXEL_STANDALONE
    const HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(EmbeddedItemAtlasId),
                                        MAKEINTRESOURCEW(10));
    if (!resource)
        throw std::runtime_error("Missing embedded item atlas");
    const HGLOBAL loaded = LoadResource(nullptr, resource);
    const void* bytes = loaded ? LockResource(loaded) : nullptr;
    const DWORD length = SizeofResource(nullptr, resource);
    if (!bytes || length == 0)
        throw std::runtime_error("Cannot load embedded item atlas");
    std::istringstream input(std::string(static_cast<const char*>(bytes), length),
                             std::ios::in | std::ios::binary);
#else
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Missing item atlas: " + path);
#endif
    std::int32_t width = 0, height = 0;
    input.read(reinterpret_cast<char*>(&width), 4);
    input.read(reinterpret_cast<char*>(&height), 4);
    if (!input || width <= 0 || height <= 0 || width > 4096 || height > 4096)
        throw std::runtime_error("Invalid item atlas");
    constexpr int ItemTilePixels = 64;
    if (!setItemAtlasDimensions(width, height, ItemTilePixels))
        throw std::runtime_error("Item atlas dimensions must be multiples of 64");
#ifndef NDEBUG
    const ItemAtlasLayout& layout = itemAtlasLayout();
    std::clog << "Item atlas: " << width << 'x' << height << ", "
              << layout.columns << 'x' << layout.rows << " cells of "
              << layout.tilePixels << " pixels\n";
#endif
    for (int value = 1; value < static_cast<int>(Item::Count); ++value) {
        const Item item = static_cast<Item>(value);
        ItemSpriteUv uv;
        if (!itemSpriteUv(item, uv) || uv.u0 < 0.0f || uv.v0 < 0.0f ||
            uv.u1 > 1.0f || uv.v1 > 1.0f ||
            uv.u0 >= uv.u1 || uv.v0 >= uv.v1)
            throw std::runtime_error(
                "Item sprite is outside atlas: " + itemDefinition(item).displayName);
    }
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
    input.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (!input)
        throw std::runtime_error("Truncated item atlas");
    std::size_t visiblePixelCount = 0;
    for (std::size_t alpha = 3; alpha < pixels.size(); alpha += 4) {
        if (pixels[alpha] != 0)
            ++visiblePixelCount;
    }
    if (visiblePixelCount == 0)
        throw std::runtime_error("Item atlas contains no visible pixels: " + path);
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}
void Renderer::renderSky(const glm::mat4& view,
                         const glm::mat4& projection,
                         float worldTime) const {
    const CelestialState state = celestial(worldTime);
    glm::mat4 rotationView = glm::mat4(glm::mat3(view));
    glm::mat4 inverse = glm::inverse(projection * rotationView);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(skyProgram_);
    glUniformMatrix4fv(glGetUniformLocation(skyProgram_, "uInverseViewProjection"),
                       1,
                       GL_FALSE,
                       glm::value_ptr(inverse));
    glUniform3fv(
        glGetUniformLocation(skyProgram_, "uSunDirection"), 1, glm::value_ptr(state.sunDirection));
    glUniform1f(glGetUniformLocation(skyProgram_, "uDaylight"), state.daylight);
    glUniform1f(glGetUniformLocation(skyProgram_, "uTime"), worldTime);
    glUniform1i(glGetUniformLocation(skyProgram_, "uEffectQuality"), effectQuality_);
    glBindVertexArray(skyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderWorld(const World& world,
                           const glm::mat4& view,
                           const glm::mat4& projection,
                           const glm::vec3& camera,
                           float worldTime,
                           bool fullbright,
                           bool underwater,
                           bool spectatorInsideBlock) const {
    const CelestialState state = celestial(worldTime);
    const glm::mat4 vp = projection * view;
    glUseProgram(worldProgram_);
    glUniformMatrix4fv(
        glGetUniformLocation(worldProgram_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(worldProgram_, "uProjection"),
                       1,
                       GL_FALSE,
                       glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(worldProgram_, "uSunDirection"),
                 1,
                 glm::value_ptr(state.sunDirection));
    glUniform3fv(
        glGetUniformLocation(worldProgram_, "uSkyColor"), 1, glm::value_ptr(state.skyColor));
    glUniform3fv(glGetUniformLocation(worldProgram_, "uCameraPosition"),
                 1,
                 glm::value_ptr(camera));
    glUniform1f(glGetUniformLocation(worldProgram_, "uDaylight"), state.daylight);
    glUniform1f(glGetUniformLocation(worldProgram_, "uTime"), worldTime);
    glUniform1i(glGetUniformLocation(worldProgram_, "uEffectQuality"), effectQuality_);
    glUniform1i(glGetUniformLocation(worldProgram_, "uFullbright"),
                fullbright ? GL_TRUE : GL_FALSE);
    glUniform1i(glGetUniformLocation(worldProgram_, "uUnderwater"),
                underwater ? GL_TRUE : GL_FALSE);
    glUniform1i(glGetUniformLocation(worldProgram_, "uSpectatorInsideBlock"),
                spectatorInsideBlock ? GL_TRUE : GL_FALSE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture_);
    glUniform1i(glGetUniformLocation(worldProgram_, "uAtlas"), 0);
    glUniform1i(glGetUniformLocation(worldProgram_, "uWaterPass"), GL_FALSE);
    world.drawOpaque(vp);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUniform1i(glGetUniformLocation(worldProgram_, "uWaterPass"), GL_TRUE);
    world.drawWater(vp, camera);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::spawnBreakParticles(const glm::ivec3& blockPosition, Block block) {
    static std::mt19937 rng(424242);
    std::uniform_real_distribution<float> random(-1.0f, 1.0f);
    const glm::vec3 color = blockColor(block);
    for (int i = 0; i < 18 * particlePercent_ / 100; ++i) {
        glm::vec3 velocity(
            random(rng) * 2.2f, 1.4f + std::abs(random(rng)) * 2.2f, random(rng) * 2.2f);
        particles_.push_back({glm::vec3(blockPosition) + glm::vec3(0.5f) +
                                  glm::vec3(random(rng), random(rng), random(rng)) * 0.32f,
                              velocity,
                              color,
                              0.65f + std::abs(random(rng)) * 0.35f});
    }
}
void Renderer::spawnHitParticles(const glm::vec3& position, bool critical) {
    static std::mt19937 rng(99173);
    std::uniform_real_distribution<float> random(-1.0f, 1.0f);
    const glm::vec3 color = critical ? glm::vec3(1.0f, .78f, .12f) : glm::vec3(.92f, .12f, .10f);
    for (int i = 0; i < (critical ? 20 : 12) * particlePercent_ / 100; ++i) {
        glm::vec3 velocity(
            random(rng) * 2.5f, 1.0f + std::abs(random(rng)) * 2.8f, random(rng) * 2.5f);
        particles_.push_back({position + glm::vec3(random(rng), random(rng), random(rng)) * .22f,
                              velocity,
                              color,
                              .48f + std::abs(random(rng)) * .28f});
    }
}
void Renderer::updateParticles(float dt) {
    for (Particle& p : particles_) {
        p.life -= dt;
        p.velocity.y -= 10.0f * dt;
        p.position += p.velocity * dt;
    }
    particles_.erase(std::remove_if(particles_.begin(),
                                    particles_.end(),
                                    [](const Particle& p) { return p.life <= 0.0f; }),
                     particles_.end());
}
void Renderer::renderParticles(const glm::mat4& view,
                               const glm::mat4& projection,
                               float maximumDistance) const {
    if (particles_.empty())
        return;
    const EntityVisibility visibility(view, projection, maximumDistance);
    std::vector<ParticleVertex> vertices;
    vertices.reserve(particles_.size());
    for (const Particle& p : particles_)
        if (visibility.visible(p.position, 0.2f))
            vertices.push_back({p.position, p.color * std::min(1.0f, p.life * 2.0f)});
    if (vertices.empty())
        return;
    glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(ParticleVertex)),
                 vertices.data(),
                 GL_STREAM_DRAW);
    glUseProgram(particleProgram_);
    glUniformMatrix4fv(
        glGetUniformLocation(particleProgram_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(particleProgram_, "uProjection"),
                       1,
                       GL_FALSE,
                       glm::value_ptr(projection));
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBindVertexArray(particleVao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
}

void Renderer::renderEntities(const std::vector<RenderCuboid>& cuboids,
                              const glm::mat4& view,
                              const glm::mat4& projection,
                              float worldTime,
                              float maximumDistance) const {
    visibleEntityCount_ = 0;
    if (cuboids.empty())
        return;
    const EntityVisibility visibility(view, projection, maximumDistance);
    const CelestialState state = celestial(worldTime);
    glUseProgram(entityProgram_);
    glUniformMatrix4fv(
        glGetUniformLocation(entityProgram_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(entityProgram_, "uProjection"),
                       1,
                       GL_FALSE,
                       glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(entityProgram_, "uSunDirection"),
                 1,
                 glm::value_ptr(state.sunDirection));
    glUniform1f(glGetUniformLocation(entityProgram_, "uDaylight"), state.daylight);
    glBindVertexArray(entityVao_);
    for (const RenderCuboid& cuboid : cuboids) {
        if (!visibility.visible(cuboid.center, glm::length(cuboid.size) * 0.5f))
            continue;
        ++visibleEntityCount_;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), cuboid.center);
        model = glm::scale(model, cuboid.size);
        glUniformMatrix4fv(
            glGetUniformLocation(entityProgram_, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(
            glGetUniformLocation(entityProgram_, "uColor"), 1, glm::value_ptr(cuboid.color));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);
}
void Renderer::renderHud(int width,
                         int height,
                         const Inventory& inventory,
                         float health,
                         float hurtFlash,
                         float breakProgress,
                         const std::string& debugText,
                         const std::string& craftingText,
                         bool spectator) const {
    std::vector<UiVertex> vertices;
    auto drawText = [&](const std::string& text,
                        float originX,
                        float originY,
                        float scale,
                        const glm::vec4& color) {
        float px = originX, py = originY;
        for (char raw : text) {
            char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
            if (c == '\n') {
                px = originX;
                py += 9.0f * scale;
                continue;
            }
            const auto& rows = glyph(c);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (rows[static_cast<std::size_t>(row)] & (1 << (4 - col)))
                        addRect(vertices,
                                px + col * scale,
                                py + row * scale,
                                scale,
                                scale,
                                color,
                                width,
                                height);
            px += 6.0f * scale;
        }
    };
    const float slot = 44.0f, gap = 4.0f, total = 9 * slot + 8 * gap,
                start = (width - total) * 0.5f, y = height - 58.0f;
    if (!spectator) {
    if (hurtFlash > 0.0f)
        addRect(vertices,
                0,
                0,
                static_cast<float>(width),
                static_cast<float>(height),
                {0.72f, 0.02f, 0.01f, std::min(0.24f, hurtFlash * 0.65f)},
                width,
                height);
    for (int i = 0; i < 9; ++i) {
        const float x = start + i * (slot + gap);
        const ItemStack& stack = inventory.slot(i);
        const Item item = stack.item;
        const int count = stack.count;
        if (i == inventory.selectedSlot())
            addRect(vertices,
                    x - 3,
                    y - 3,
                    slot + 6,
                    slot + 6,
                    {0.95f, 0.95f, 0.95f, 0.98f},
                    width,
                    height);
        addRect(vertices, x, y, slot, slot, {0.06f, 0.06f, 0.08f, 0.86f}, width, height);
        addRect(vertices,
                x + 3,
                y + 3,
                slot - 6,
                slot - 6,
                {0.22f, 0.22f, 0.25f, 0.90f},
                width,
                height);
        if (count > 0)
            addSprite(vertices, item, x + 4, y + 4, 36, 36, {1, 1, 1, 1}, width, height);
        drawText(std::to_string(i + 1), x + 4, y + 4, 1.0f, {0.78f, 0.80f, 0.84f, 1});
        if (count > 0)
            drawText(std::to_string(count),
                     x + slot - 4 - static_cast<float>(std::to_string(count).size()) * 6.0f,
                     y + slot - 10,
                     1.0f,
                     {1, 1, 1, 1});
    }
    const float healthX = start, healthY = y - 20.0f;
    for (int i = 0; i < 10; ++i) {
        const float filled = std::clamp(health - static_cast<float>(i * 2), 0.0f, 2.0f) / 2.0f;
        addRect(vertices,
                healthX + i * 18,
                healthY,
                14,
                11,
                {0.16f, 0.03f, 0.04f, 0.90f},
                width,
                height);
        if (filled > 0)
            addRect(vertices,
                    healthX + i * 18 + 2,
                    healthY + 2,
                    10 * filled,
                    7,
                    {0.86f, 0.08f, 0.10f, 1},
                    width,
                    height);
    }
    addRect(vertices, width * 0.5f - 10, height * 0.5f - 1, 20, 2, {1, 1, 1, 0.95f}, width, height);
    addRect(vertices, width * 0.5f - 1, height * 0.5f - 10, 2, 20, {1, 1, 1, 0.95f}, width, height);
    if (breakProgress > 0.0f) {
        addRect(vertices,
                width * 0.5f - 37,
                height * 0.5f + 18,
                74,
                8,
                {0, 0, 0, 0.72f},
                width,
                height);
        addRect(vertices,
                width * 0.5f - 35,
                height * 0.5f + 20,
                70 * std::clamp(breakProgress, 0.0f, 1.0f),
                4,
                {0.92f, 0.78f, 0.30f, 1},
                width,
                height);
    }
    }
    if (!debugText.empty()) {
        int lines = 1, maxChars = 0, current = 0;
        for (char c : debugText) {
            if (c == '\n') {
                ++lines;
                maxChars = std::max(maxChars, current);
                current = 0;
            } else
                ++current;
        }
        maxChars = std::max(maxChars, current);
        addRect(vertices,
                8,
                8,
                static_cast<float>(maxChars * 12 + 12),
                static_cast<float>(lines * 18 + 10),
                {0, 0, 0, 0.62f},
                width,
                height);
        drawText(debugText, 14, 14, 2.0f, {0.92f, 0.96f, 1.0f, 1});
    }
    if (!craftingText.empty()) {
        const float panelW = std::min(680.0f, static_cast<float>(width) - 32.0f),
                    panelX = (width - panelW) * 0.5f, panelY = y - 72.0f;
        addRect(vertices, panelX, panelY, panelW, 54, {0.04f, 0.03f, 0.025f, 0.88f}, width, height);
        drawText(craftingText, panelX + 12, panelY + 9, 1.5f, {0.98f, 0.86f, 0.58f, 1});
    }
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)),
                 vertices.data(),
                 GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, itemTexture_);
    glUniform1i(uiItemAtlasUniform_, 0);
    glBindVertexArray(uiVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
void Renderer::renderItemSprites(const std::vector<RenderItemSprite>& sprites,
                                 const glm::mat4& view,
                                 const glm::mat4& projection,
                                 float time,
                                 float maximumDistance) const {
    if (sprites.empty())
        return;
    const EntityVisibility visibility(view, projection, maximumDistance);
    glm::mat4 inv = glm::inverse(view);
    glm::vec3 right = glm::normalize(glm::vec3(inv[0])), up = glm::normalize(glm::vec3(inv[1]));
    std::vector<float> vertices;
    vertices.reserve(sprites.size() * 30);
    auto push = [&](glm::vec3 p, float u, float v) {
        vertices.insert(vertices.end(), {p.x, p.y, p.z, u, v});
    };
    for (const auto& s : sprites) {
        if (!visibility.visible(s.center, s.size))
            continue;
        ItemSpriteUv uv;
        if (!itemSpriteUv(s.item, uv))
            continue;
        ++visibleEntityCount_;
        glm::vec3 c = s.center + glm::vec3(0, .12f + std::sin(time * 2.7f + s.center.x) * .05f, 0);
        float h = s.size * .5f;
        glm::vec3 a = c - right * h + up * h, b = c - right * h - up * h,
                  d = c + right * h + up * h, e = c + right * h - up * h;
        push(a, uv.u0, uv.v0);
        push(b, uv.u0, uv.v1);
        push(e, uv.u1, uv.v1);
        push(a, uv.u0, uv.v0);
        push(e, uv.u1, uv.v1);
        push(d, uv.u1, uv.v0);
    }
    if (vertices.empty())
        return;
    glBindBuffer(GL_ARRAY_BUFFER, itemVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(),
                 GL_STREAM_DRAW);
    glUseProgram(itemProgram_);
    glUniformMatrix4fv(
        glGetUniformLocation(itemProgram_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(
        glGetUniformLocation(itemProgram_, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, itemTexture_);
    glUniform1i(itemAtlasUniform_, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(itemVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 5));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}
void Renderer::renderSelectionOutline(const glm::ivec3& block, Block type,
                                      const glm::mat4& view,
                                      const glm::mat4& projection) const {
    glUseProgram(entityProgram_);
    const BlockGeometryProperties bounds = blockGeometry(type);
    const glm::vec3 minimum(bounds.minX, bounds.minY, bounds.minZ);
    const glm::vec3 maximum(bounds.maxX, bounds.maxY, bounds.maxZ);
    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(block) +
                                                        (minimum + maximum) * .5f);
    model = glm::scale(model, (maximum - minimum) + glm::vec3(.006f));
    glUniformMatrix4fv(
        glGetUniformLocation(entityProgram_, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(
        glGetUniformLocation(entityProgram_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(entityProgram_, "uProjection"),
                       1,
                       GL_FALSE,
                       glm::value_ptr(projection));
    glm::vec3 color(.03f);
    glm::vec3 sun(0, 1, 0);
    glUniform3fv(glGetUniformLocation(entityProgram_, "uColor"), 1, glm::value_ptr(color));
    glUniform3fv(glGetUniformLocation(entityProgram_, "uSunDirection"), 1, glm::value_ptr(sun));
    glUniform1f(glGetUniformLocation(entityProgram_, "uDaylight"), 1);
    glBindVertexArray(entityVao_);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
}
void Renderer::renderMobOutline(const RenderCuboid& bounds,
                                const glm::mat4& view,
                                const glm::mat4& projection) const {
    glUseProgram(entityProgram_);
    glm::mat4 model = glm::translate(glm::mat4(1), bounds.center);
    model = glm::scale(model, bounds.size * 1.018f);
    glUniformMatrix4fv(
        glGetUniformLocation(entityProgram_, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(
        glGetUniformLocation(entityProgram_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(entityProgram_, "uProjection"),
                       1,
                       GL_FALSE,
                       glm::value_ptr(projection));
    glm::vec3 color(1.0f, .88f, .28f), sun(0, 1, 0);
    glUniform3fv(glGetUniformLocation(entityProgram_, "uColor"), 1, glm::value_ptr(color));
    glUniform3fv(glGetUniformLocation(entityProgram_, "uSunDirection"), 1, glm::value_ptr(sun));
    glUniform1f(glGetUniformLocation(entityProgram_, "uDaylight"), 1);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(entityVao_);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.5f);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}
void Renderer::renderMenu(int width,
                          int height,
                          bool settingsPage,
                          int hovered,
                          const GameSettings& settings,
                          GameMode mode,
                          bool standaloneDemo,
                          bool showSaveWarning) const {
    std::vector<UiVertex> vertices;
    auto drawText = [&](std::string value, float x, float y, float scale, glm::vec4 color) {
        for (char raw : value) {
            const auto& rows = glyph(
                static_cast<char>(std::toupper(static_cast<unsigned char>(raw))));
            for (int row = 0; row < 7; ++row)
                for (int column = 0; column < 5; ++column)
                    if (rows[row] & (1 << (4 - column)))
                        addRect(vertices,
                                x + column * scale,
                                y + row * scale,
                                scale,
                                scale,
                                color,
                                width,
                                height);
            x += 6 * scale;
        }
    };
    addRect(vertices,
            0,
            0,
            static_cast<float>(width),
            static_cast<float>(height),
            {0, 0, 0, .62f},
            width,
            height);
    const float panelX = width * .5f - 220.0f;
    const float panelY = height * .5f - (settingsPage ? 350.0f : 300.0f);
    const float panelHeight = settingsPage ? 700.0f : 600.0f;
    addRect(vertices, panelX, panelY, 440, panelHeight, {.075f, .075f, .085f, .98f}, width, height);
    drawText(settingsPage ? "SETTINGS" : "GAME PAUSED",
             panelX + (settingsPage ? 114.0f : 92.0f),
             panelY + 28.0f,
             3.0f,
             {1, .92f, .68f, 1});

    if (!settingsPage) {
        const std::array<std::string, 7> labels{
            "RESUME GAME",
            "SETTINGS",
            std::string("MODE  ") + gameModeName(mode),
            "RESET WORLD",
            "SAVE WORLD",
            standaloneDemo ? "QUIT TO DESKTOP" : "SAVE & QUIT",
            "EXIT GAME"};
        for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
            const float y = panelY + 82.0f + index * 61.0f;
            addRect(vertices,
                    panelX + 45.0f,
                    y,
                    350.0f,
                    44.0f,
                    index == hovered ? glm::vec4(.38f, .48f, .30f, 1)
                                     : glm::vec4(.20f, .20f, .23f, 1),
                    width,
                    height);
            drawText(labels[static_cast<std::size_t>(index)],
                     panelX + 65.0f,
                     y + 14.0f,
                     1.8f,
                     {1, 1, 1, 1});
        }
        if (standaloneDemo) {
            drawText("STANDALONE DEMO  SAVING DISABLED", panelX + 38.0f,
                     panelY + 527.0f, 1.35f, {1, .85f, .52f, 1});
        }
        if (showSaveWarning) {
            drawText("SAVING NOT AVAILABLE IN STANDALONE DEMO", panelX + 22.0f,
                     panelY + 554.0f, 1.15f, {1, .67f, .56f, 1});
        }
    } else {
        const std::string antiAliasing = settings.antiAliasingSamples == 0
                                             ? "OFF"
                                             : std::to_string(settings.antiAliasingSamples) + "X MSAA";
        const std::string preset = settings.graphicsPreset == GraphicsPreset::Low
                                       ? "LOW"
                                       : settings.graphicsPreset == GraphicsPreset::Medium
                                             ? "MEDIUM"
                                             : settings.graphicsPreset == GraphicsPreset::High
                                                   ? "HIGH"
                                                   : "CUSTOM";
        const std::string frameLimit = settings.frameLimit == 0
                                           ? "UNLIMITED"
                                           : std::to_string(settings.frameLimit);
        const std::array<std::string, 13> labels{
            "RENDER DISTANCE",
            "SIMULATION DISTANCE",
            "FOV",
            "MOUSE SENSITIVITY",
            "MASTER VOLUME",
            std::string("ANTI ALIASING  ") + antiAliasing,
            std::string("FULLSCREEN  ") + (settings.fullscreen ? "ON" : "OFF"),
            std::string("VSYNC  ") + (settings.vsync ? "ON" : "OFF"),
            std::string("SHOW FPS  ") + (settings.showFps ? "ON" : "OFF"),
            std::string("SHOW COORDINATES  ") +
                (settings.showCoordinates ? "ON" : "OFF"),
            std::string("GRAPHICS PRESET  ") + preset,
            "ENTITY DISTANCE",
            std::string("FRAME LIMIT  ") + frameLimit};
        const std::array<std::string, 6> sliderValues{
            std::to_string(settings.renderDistance),
            std::to_string(settings.simulationDistance),
            std::to_string(static_cast<int>(std::round(settings.fov))),
            std::to_string(static_cast<int>(std::round(settings.mouseSensitivity * 100.0f))),
            std::to_string(static_cast<int>(std::round(settings.masterVolume * 100.0f))),
            std::to_string(settings.entityDistance)};
        const std::array<float, 6> sliderPositions{
            (settings.renderDistance - 2.0f) / 62.0f,
            (settings.simulationDistance - 2.0f) / 30.0f,
            (settings.fov - 55.0f) / 50.0f,
            (settings.mouseSensitivity - 0.03f) / 0.27f,
            settings.masterVolume,
            (settings.entityDistance - 2.0f) / 62.0f};
        for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
            const float y = panelY + 65.0f + index * 40.0f;
            addRect(vertices,
                    panelX + 24.0f,
                    y,
                    392.0f,
                    38.0f,
                    index == hovered ? glm::vec4(.35f, .44f, .29f, 1)
                                     : glm::vec4(.19f, .19f, .22f, 1),
                    width,
                    height);
            drawText(labels[static_cast<std::size_t>(index)],
                     panelX + 38.0f,
                     y + (index < 5 || index == 11 ? 7.0f : 12.0f),
                     index < 5 || index == 11 ? 1.15f : 1.4f,
                     {1, 1, 1, 1});
            if (index < 5 || index == 11) {
                const std::size_t sliderIndex = index == 11 ? 5 : static_cast<std::size_t>(index);
                constexpr float sliderXOffset = 190.0f;
                constexpr float sliderWidth = 150.0f;
                addRect(vertices,
                        panelX + sliderXOffset,
                        y + 22.0f,
                        sliderWidth,
                        5.0f,
                        {.10f, .11f, .12f, 1.0f},
                        width,
                        height);
                const float normalized =
                    std::clamp(sliderPositions[sliderIndex], 0.0f, 1.0f);
                addRect(vertices,
                        panelX + sliderXOffset,
                        y + 22.0f,
                        sliderWidth * normalized,
                        5.0f,
                        {.48f, .70f, .34f, 1.0f},
                        width,
                        height);
                addRect(vertices,
                        panelX + sliderXOffset + sliderWidth * normalized - 4.0f,
                        y + 15.0f,
                        8.0f,
                        19.0f,
                        {.88f, .92f, .78f, 1.0f},
                        width,
                        height);
                drawText(sliderValues[sliderIndex],
                         panelX + 354.0f,
                         y + 17.0f,
                         1.25f,
                         {1.0f, .93f, .70f, 1.0f});
            }
        }
        const float controlsY = panelY + 585.0f;
        addRect(vertices, panelX + 95.0f, controlsY, 250.0f, 42.0f,
                hovered == 13 ? glm::vec4(.38f, .48f, .30f, 1)
                              : glm::vec4(.20f, .20f, .23f, 1), width, height);
        drawText("CONTROLS", panelX + 155.0f, controlsY + 13.0f,
                 2.0f, {1, 1, 1, 1});
        const float y = panelY + 635.0f;
        addRect(vertices,
                panelX + 95.0f,
                y,
                250.0f,
                42.0f,
                hovered == 14 ? glm::vec4(.38f, .48f, .30f, 1)
                             : glm::vec4(.20f, .20f, .23f, 1),
                width,
                height);
        drawText("BACK", panelX + 180.0f, y + 13.0f, 2.0f, {1, 1, 1, 1});
    }

    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)),
                 vertices.data(),
                 GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, itemTexture_);
    glUniform1i(uiItemAtlasUniform_, 0);
    glBindVertexArray(uiVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderControlsMenu(int width, int height, int hovered,
                                  const GameSettings& settings, int activeBinding) const {
    std::vector<UiVertex> vertices;
    vertices.reserve(14000);
    auto drawText = [&](const std::string& value, float x, float y, float scale,
                        const glm::vec4& color) {
        for (char raw : value) {
            const auto& rows = glyph(
                static_cast<char>(std::toupper(static_cast<unsigned char>(raw))));
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 5; ++column) {
                    if (rows[row] & (1 << (4 - column))) {
                        addRect(vertices, x + column * scale, y + row * scale,
                                scale, scale, color, width, height);
                    }
                }
            }
            x += 6 * scale;
        }
    };
    const float panelX = width * 0.5f - 220.0f;
    const float panelY = height * 0.5f - 350.0f;
    addRect(vertices, 0, 0, static_cast<float>(width), static_cast<float>(height),
            {0, 0, 0, .62f}, width, height);
    addRect(vertices, panelX, panelY, 440.0f, 700.0f,
            {.075f, .075f, .085f, .98f}, width, height);
    drawText("CONTROLS", panelX + 105.0f, panelY + 28.0f,
             3.0f, {1, .92f, .68f, 1});
    for (int index = 0; index < ControlActionCount; ++index) {
        const float y = panelY + 75.0f + index * 32.0f;
        addRect(vertices, panelX + 35.0f, y, 370.0f, 29.0f,
                index == hovered ? glm::vec4(.35f, .44f, .29f, 1)
                                 : glm::vec4(.19f, .19f, .22f, 1), width, height);
        drawText(controlActionName(static_cast<ControlAction>(index)),
                 panelX + 48.0f, y + 8.0f, 1.35f, {1, 1, 1, 1});
        const std::string key = index == activeBinding ? "PRESS KEY" :
            controlKeyName(settings.controls[static_cast<std::size_t>(index)]);
        drawText(key, panelX + 242.0f, y + 8.0f, 1.25f,
                 index == activeBinding ? glm::vec4(1, .8f, .3f, 1)
                                        : glm::vec4(.78f, .93f, .75f, 1));
    }
    for (int index = 0; index < 2; ++index) {
        const float y = panelY + (index == 0 ? 552.0f : 604.0f);
        addRect(vertices, panelX + 45.0f, y, 350.0f, 40.0f,
                hovered == ControlActionCount + index
                    ? glm::vec4(.38f, .48f, .30f, 1)
                    : glm::vec4(.20f, .20f, .23f, 1), width, height);
        drawText(index == 0 ? "RESET TO DEFAULTS" : "BACK",
                 panelX + (index == 0 ? 85.0f : 175.0f), y + 12.0f,
                 1.8f, {1, 1, 1, 1});
    }
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)),
                 vertices.data(), GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, itemTexture_);
    glUniform1i(uiItemAtlasUniform_, 0);
    glBindVertexArray(uiVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderResetMenu(int width,
                               int height,
                               int hovered,
                               const std::string& seedText,
                               GameMode mode) const {
    std::vector<UiVertex> vertices;
    auto drawText = [&](std::string value, float x, float y, float scale, glm::vec4 color) {
        for (char raw : value) {
            const auto& rows = glyph(
                static_cast<char>(std::toupper(static_cast<unsigned char>(raw))));
            for (int row = 0; row < 7; ++row)
                for (int column = 0; column < 5; ++column)
                    if (rows[row] & (1 << (4 - column)))
                        addRect(vertices,
                                x + column * scale,
                                y + row * scale,
                                scale,
                                scale,
                                color,
                                width,
                                height);
            x += 6 * scale;
        }
    };
    addRect(vertices, 0, 0, static_cast<float>(width), static_cast<float>(height),
            {0, 0, 0, .72f}, width, height);
    const float panelX = width * .5f - 220.0f;
    const float panelY = height * .5f - 300.0f;
    addRect(vertices, panelX, panelY, 440, 600, {.075f, .075f, .085f, .99f}, width, height);
    drawText("RESET WORLD", panelX + 92.0f, panelY + 30.0f, 3.0f, {1, .72f, .40f, 1});
    drawText("THIS DELETES CURRENT WORLD DATA",
             panelX + 42.0f,
             panelY + 92.0f,
             1.35f,
             {1, .62f, .48f, 1});
    drawText("TYPE SEED", panelX + 45.0f, panelY + 145.0f, 1.6f, {1, 1, 1, 1});
    addRect(vertices, panelX + 45.0f, panelY + 175.0f, 350.0f, 42.0f,
            {.12f, .12f, .14f, 1}, width, height);
    drawText(seedText.empty() ? "0" : seedText,
             panelX + 60.0f,
             panelY + 188.0f,
             1.8f,
             {.92f, .92f, .96f, 1});

    const std::array<std::string, 3> labels{
        std::string("MODE  ") + gameModeName(mode),
        "CONFIRM RESET",
        "CANCEL"};
    const std::array<float, 3> positions{panelY + 230.0f, panelY + 330.0f, panelY + 400.0f};
    for (int index = 0; index < 3; ++index) {
        addRect(vertices,
                panelX + 45.0f,
                positions[static_cast<std::size_t>(index)],
                350.0f,
                48.0f,
                index == hovered
                    ? glm::vec4(index == 1 ? .58f : .38f, index == 1 ? .24f : .48f, .24f, 1)
                    : glm::vec4(.20f, .20f, .23f, 1),
                width,
                height);
        drawText(labels[static_cast<std::size_t>(index)],
                 panelX + 75.0f,
                 positions[static_cast<std::size_t>(index)] + 15.0f,
                 2.0f,
                 {1, 1, 1, 1});
    }
    drawText("BACKSPACE EDITS SEED", panelX + 88.0f, panelY + 500.0f, 1.4f,
             {.75f, .77f, .82f, 1});

    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)),
                 vertices.data(),
                 GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, itemTexture_);
    glUniform1i(uiItemAtlasUniform_, 0);
    glBindVertexArray(uiVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
