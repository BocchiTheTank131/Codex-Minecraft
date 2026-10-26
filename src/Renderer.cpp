#include "Renderer.h"
#include <GLFW/glfw3.h>
#include "Building.h"
#include "Transportation.h"
#include "Player.h"
#include "RenderScratch.h"
#include "Definitions.h"
#include "UIManager.h"
#include "MenuLayout.h"
#include "SpriteManifest.h"

#include "World.h"
#include "TerrainOcclusion.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <chrono>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#ifdef VOXEL_STANDALONE
#include "EmbeddedResources.h"
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#include <wincodec.h>
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

#ifdef _WIN32
GLuint loadSpriteImage(const char* name, int resourceId, int& width, int& height) {
    std::vector<std::uint8_t> ownedBytes;
    const void* source = nullptr;
    std::size_t length = 0;
#ifdef VOXEL_STANDALONE
    const HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10));
    const HGLOBAL loaded = resource ? LoadResource(nullptr, resource) : nullptr;
    source = loaded ? LockResource(loaded) : nullptr;
    length = resource ? SizeofResource(nullptr, resource) : 0;
#else
    (void)resourceId;
    wchar_t executable[MAX_PATH]{};
    const DWORD pathLength = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    const auto spritePath = pathLength > 0 && pathLength < MAX_PATH
        ? std::filesystem::path(executable).parent_path() / "sprites" / name
        : std::filesystem::path("sprites") / name;
    std::ifstream input(spritePath, std::ios::binary);
    if (input)
        ownedBytes.assign(std::istreambuf_iterator<char>(input), {});
    if (ownedBytes.empty()) {
        std::ifstream fallback(std::filesystem::path("sprites") / name, std::ios::binary);
        if (fallback) ownedBytes.assign(std::istreambuf_iterator<char>(fallback), {});
    }
    source = ownedBytes.data();
    length = ownedBytes.size();
#endif
    if (!source || length == 0 || length > UINT32_MAX)
        throw std::runtime_error(std::string("Missing sprite image: ") + name);
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    std::vector<std::uint8_t> pixels;
    UINT imageWidth = 0, imageHeight = 0;
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) result = factory->CreateStream(&stream);
    if (SUCCEEDED(result)) result = stream->InitializeFromMemory(
        const_cast<BYTE*>(static_cast<const BYTE*>(source)), static_cast<DWORD>(length));
    if (SUCCEEDED(result)) result = factory->CreateDecoderFromStream(stream, nullptr,
                                           WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result)) result = frame->GetSize(&imageWidth, &imageHeight);
    if (SUCCEEDED(result) && (imageWidth == 0 || imageHeight == 0 ||
                              imageWidth > 4096 || imageHeight > 4096)) result = E_FAIL;
    if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) result = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(result)) {
        pixels.resize(static_cast<std::size_t>(imageWidth) * imageHeight * 4);
        result = converter->CopyPixels(nullptr, imageWidth * 4,
                                       static_cast<UINT>(pixels.size()), pixels.data());
    }
    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    if (SUCCEEDED(comResult)) CoUninitialize();
    if (FAILED(result))
        throw std::runtime_error(std::string("Cannot decode sprite image: ") + name);
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageWidth, imageHeight, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    width = static_cast<int>(imageWidth);
    height = static_cast<int>(imageHeight);
    return texture;
}
#endif

constexpr const char* WorldVertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aUv;
layout(location=2) in uint aShade;
uniform uint uSunBits[64];
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
    float d=uintBitsToFloat(0x3f3504f3u);
    vec3 normals[10]=vec3[10](vec3(1,0,0),vec3(-1,0,0),vec3(0,1,0),vec3(0,-1,0),vec3(0,0,1),vec3(0,0,-1),
        vec3(-d,0,d),vec3(d,0,-d),vec3(d,0,d),vec3(-d,0,-d));
    vUv=aUv;vNormal=normals[aShade&15u];
    for(uint axis=0u;axis<3u;++axis) if((aShade&(1u<<(23u+axis)))!=0u) vNormal[axis]=uintBitsToFloat(0x80000000u);
    vSunLight=uintBitsToFloat(uSunBits[(aShade>>4u)&63u]);
    vBlockLight=uintBitsToFloat(uSunBits[(aShade>>10u)&15u]);
    vAo=float((aShade>>14u)&127u)/64.0;
    vDistance = length(viewPosition.xyz);
    vWorldPosition = aPosition;
    uint tile = aMaterial & 255u;
    vAtlasTile = tile == 255u ? -1.0 : float(tile);
    vClimateTint = vec3(float((aMaterial >> 8u) & 255u),
                       float((aMaterial >> 16u) & 255u),
                       float((aMaterial >> 24u) & 255u)) / 255.0;
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
in vec3 vClimateTint;
uniform sampler2D uAtlas;
uniform float uAtlasTiles;
uniform vec3 uSunDirection;
uniform vec3 uSkyColor;
uniform vec3 uCameraPosition;
uniform float uDaylight;
uniform float uTime;
uniform float uBrightness;
uniform vec4 uWeather;
uniform float uWetness;
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
        atlasUv = vec2((vAtlasTile + repeatUv.x) / uAtlasTiles, repeatUv.y);
    }
    vec4 texel = texture(uAtlas, atlasUv);
    if (texel.a < 0.20) discard;
    // Grass side pixels retain their brown soil; tint only green vegetation.
    int tile = vAtlasTile >= 0.0 ? int(vAtlasTile) : int(atlasUv.x*uAtlasTiles);
    if(tile != 1 || (texel.g > texel.r && texel.g > texel.b)) texel.rgb *= vClimateTint;
    vec3 normal = normalize(vNormal);
    float day = smoothstep(0.0, 1.0, uDaylight);
    float sunDiffuse = max(dot(normal, uSunDirection), 0.0);
    float moonDiffuse = max(dot(normal, -uSunDirection), 0.0);
    float faceShade = 0.84 + max(normal.y, 0.0) * 0.16 - max(-normal.y, 0.0) * 0.14;
    float weatherLight = 1.0 - .25*uWeather.x - .55*uWeather.y;
    float skyContribution = vSunLight * mix(0.055, 0.48, day) * weatherLight;
    float sunContribution = vSunLight * sunDiffuse * mix(0.0, 0.54, day) * weatherLight;
    float moonContribution = vSunLight * moonDiffuse * (1.0-day) * 0.17;
    float emittedContribution = vBlockLight * 0.90;
    float rawLight = clamp(max(skyContribution + sunContribution + moonContribution,
                               emittedContribution), 0.0, 1.0);
    // A low-light floor preserves texture detail. The curve lifts weak skylight
    // near openings while keeping emitted light much stronger than unlit caves.
    float ambientFloor = mix(0.025, 0.085, uBrightness);
    float response = pow(rawLight, mix(1.0, 0.72, uBrightness));
    float ao = uEffectQuality == 0 ? 1.0 :
        mix(0.90, vAo, smoothstep(0.03, 0.30, rawLight));
    float brightness = pow(clamp(ambientFloor +
        (1.0 - ambientFloor) * response * faceShade * ao, 0.0, 1.0), 0.75);
    vec3 sunTint = mix(vec3(1.0,0.48,0.20), vec3(1.0,0.93,0.76),
                       smoothstep(-0.02,0.42,uSunDirection.y));
    vec3 naturalLight = mix(vec3(0.56,0.66,0.88), sunTint, day);
    vec3 lit = uFullbright ? texel.rgb :
        texel.rgb * brightness * naturalLight +
        vec3(1.0,0.50,0.16) * vBlockLight * 0.14;
    float alpha = texel.a;
    if (uWaterPass) {
        float topSurface = max(normal.y,0.0);
        vec3 waterColor = mix(vec3(0.035,0.17,0.34), vec3(0.08,0.34,0.56), day);
        lit = mix(lit, waterColor * vClimateTint, 0.34);
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
    if(!uFullbright) {
        lit *= 1.0 - uWetness * .10 * max(normal.y,0.0) * smoothstep(.6,1.0,vSunLight);
        lit += texel.rgb * uWeather.z * .55 * vSunLight;
    }
    if(!uUnderwater && !uFullbright) {
        float fog = uWeather.w * (uWeather.x*.35 + uWeather.y*.12) * smoothstep(32.0,256.0,vDistance);
        lit = mix(lit,vec3(.28,.32,.36),fog);
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
uniform vec4 uWeather;
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
        float clouds = smoothstep(mix(.54,.30,uWeather.x),mix(.68,.48,uWeather.x),cloudNoise(cloudUv));
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
    color = mix(color, mix(vec3(.28,.32,.38),vec3(.085,.105,.14),uWeather.y),uWeather.x*.78);
    color += vec3(.7,.75,.85)*uWeather.z;
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
out vec2 vWoodUv;
void main(){ gl_Position=uProjection*uView*uModel*vec4(aPosition,1.0); vNormal=mat3(uModel)*aNormal; vec3 p=aPosition+vec3(.5); vWoodUv=abs(aNormal.y)>.5?p.xz:abs(aNormal.x)>.5?p.zy:p.xy; }
)GLSL";
constexpr const char* EntityFragmentShader = R"GLSL(
#version 330 core
in vec3 vNormal;
in vec2 vWoodUv;
uniform sampler2D uWood;
uniform int uWoodFamily;
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
    vec3 base=uColor;
    if(uWoodFamily>=0){vec2 uv=(floor(clamp(vWoodUv,vec2(0),vec2(.999))*16.0)+.5)/16.0;uv.x=(uv.x+float(uWoodFamily))/6.0;base=texture(uWood,uv).rgb*uColor;}
    fragColor=vec4(base*light*tint,1.0);
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
constexpr const char* PreviewFragmentShader=R"GLSL(
#version 330 core
in vec2 vUv;
uniform sampler2D uItemAtlas;
uniform vec3 uColor;
out vec4 fragColor;
void main(){vec4 pixel=texture(uItemAtlas,vUv);if(pixel.a<.02)discard;fragColor=vec4(pixel.rgb*uColor,.32*pixel.a);}
)GLSL";
constexpr const char* UiFragmentShader = R"GLSL(
#version 330 core
in vec4 vColor; in vec2 vUv; in float vTextured; uniform sampler2D uItemAtlas; out vec4 fragColor;
void main(){ vec4 sprite=texture(uItemAtlas,vUv); fragColor=mix(vColor,sprite*vColor,clamp(vTextured,0.0,1.0)); if(fragColor.a<0.02)discard; }
)GLSL";

constexpr const char* PanoramaFragmentShader = R"GLSL(
#version 330 core
in vec4 vColor; in vec2 vUv; in float vTextured;
uniform sampler2D uItemAtlas; out vec4 fragColor;
void main() {
    if(vTextured < 0.5) { fragColor=vColor; return; }
    vec2 uv=vec2(fract(vUv.x),vUv.y);
    vec4 pixel=texture(uItemAtlas,uv);
    // The supplied artwork is not edge-matched. Blend a narrow wrapping seam
    // at runtime, retaining the original asset and a continuous 360-degree pan.
    float seam=smoothstep(0.0,0.025,min(uv.x,1.0-uv.x));
    vec4 edge=(texture(uItemAtlas,vec2(0.001,uv.y))+
               texture(uItemAtlas,vec2(0.999,uv.y)))*0.5;
    fragColor=mix(edge,pixel,seam)*vColor;
}
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
constexpr const char* BillboardFragmentShader = R"GLSL(
#version 330 core
in vec2 vUv;
uniform sampler2D uSprite;
uniform float uLight;
uniform float uHurt;
uniform float uOpacity;
out vec4 fragColor;
void main(){
    vec4 pixel=texture(uSprite,vUv);
    if(pixel.a<0.10) discard;
    vec3 color=mix(pixel.rgb*uLight,vec3(1.0,0.07,0.05),clamp(uHurt,0.0,1.0)*0.72);
    fragColor=vec4(color,pixel.a*uOpacity);
}
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

    const float angle = WorldTime::angle(worldTime);
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
        {'+', {0, 4, 4, 31, 4, 4, 0}},       {' ', {0, 0, 0, 0, 0, 0, 0}},
        {'_', {0, 0, 0, 0, 0, 0, 31}},       {'~', {0, 0, 9, 22, 0, 0, 0}},
        {'@', {14, 17, 23, 21, 23, 16, 14}}, {'>', {16, 8, 4, 2, 4, 8, 16}},
        {'<', {1, 2, 4, 8, 4, 2, 1}},      {'"', {10, 10, 10, 0, 0, 0, 0}},
        {'%', {25, 26, 2, 4, 8, 11, 19}}, {'=', {0, 31, 0, 31, 0, 0, 0}},
        {'?', {14, 17, 1, 2, 4, 0, 4}}};
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

// Store the original normalized vertices, rather than transforming cached
// vertices: this preserves the exact floating-point operations and pixel edges.
struct Renderer::TextGeometryCache {
    struct Entry {
        std::string text;
        float x = 0, y = 0, scale = 0;
        glm::vec4 color{0};
        int width = 0, height = 0;
        std::uint64_t used = 0;
        std::vector<UiVertex> vertices;
    };
    std::array<Entry, 64> entries{};
    std::uint64_t clock = 0;
    std::vector<UiVertex> uploadedHud;
    static constexpr std::size_t MaximumBytes = 8 * 1024 * 1024;

    void appendRun(std::vector<UiVertex>& out, std::string_view text,
                   float x, float y, float scale, const glm::vec4& color,
                   int width, int height) {
        if (text.empty()) return;
        Entry* oldest = &entries.front();
        for (Entry& entry : entries) {
            if (entry.used && entry.x == x && entry.y == y && entry.scale == scale &&
                entry.color == color && entry.width == width && entry.height == height &&
                std::string_view(entry.text) == text) {
                entry.used = ++clock;
                out.insert(out.end(), entry.vertices.begin(), entry.vertices.end());
                return;
            }
            if (entry.used < oldest->used) oldest = &entry;
        }
        oldest->text.assign(text.data(), text.size());
        oldest->x = x; oldest->y = y; oldest->scale = scale; oldest->color = color;
        oldest->width = width; oldest->height = height; oldest->used = ++clock;
        oldest->vertices.clear();
        float px = x;
        for (char raw : text) {
            const auto& rows = glyph(static_cast<char>(std::toupper(static_cast<unsigned char>(raw))));
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (rows[static_cast<std::size_t>(row)] & (1 << (4 - col)))
                        addRect(oldest->vertices, px + col * scale, y + row * scale,
                                scale, scale, color, width, height);
            px += 6.0f * scale;
        }
        out.insert(out.end(), oldest->vertices.begin(), oldest->vertices.end());
        std::size_t retained = 0;
        for (const Entry& entry : entries) retained += entry.vertices.capacity() * sizeof(UiVertex);
        while (retained > MaximumBytes) {
            Entry* victim = nullptr;
            for (Entry& entry : entries)
                if (entry.used && (!victim || entry.used < victim->used)) victim = &entry;
            if (!victim) break;
            retained -= victim->vertices.capacity() * sizeof(UiVertex);
            *victim = Entry{};
        }
    }

    void append(std::vector<UiVertex>& out, const std::string& text,
                float x, float y, float scale, const glm::vec4& color,
                int width, int height) {
        std::string_view remaining(text);
        while (!remaining.empty()) {
            const auto end = remaining.find('\n');
            const auto line = remaining.substr(0, end);
            // Keep fixed labels cached even when the adjacent live number changes.
            const auto number = line.find_first_of("0123456789-");
            const auto split = number == std::string_view::npos ? line.size() : number;
            appendRun(out, line.substr(0, split), x, y, scale, color, width, height);
            float valueX = x;
            for (std::size_t i = 0; i < split; ++i) valueX += 6.0f * scale;
            appendRun(out, line.substr(split), valueX, y, scale, color, width, height);
            if (end == std::string_view::npos) break;
            remaining.remove_prefix(end + 1);
            y += 9.0f * scale;
        }
    }
};

Renderer::ProgramUniforms Renderer::cacheUniforms(GLuint program) {
    ProgramUniforms locations;
    locations.atlas = glGetUniformLocation(program, "uAtlas");
    locations.atlasTiles = glGetUniformLocation(program, "uAtlasTiles");
    locations.brightness = glGetUniformLocation(program, "uBrightness");
    locations.cameraPosition = glGetUniformLocation(program, "uCameraPosition");
    locations.color = glGetUniformLocation(program, "uColor");
    locations.daylight = glGetUniformLocation(program, "uDaylight");
    locations.effectQuality = glGetUniformLocation(program, "uEffectQuality");
    locations.fullbright = glGetUniformLocation(program, "uFullbright");
    locations.hurt = glGetUniformLocation(program, "uHurt");
    locations.inverseViewProjection = glGetUniformLocation(program, "uInverseViewProjection");
    locations.itemAtlas = glGetUniformLocation(program, "uItemAtlas");
    locations.light = glGetUniformLocation(program, "uLight");
    locations.model = glGetUniformLocation(program, "uModel");
    locations.opacity = glGetUniformLocation(program, "uOpacity");
    locations.projection = glGetUniformLocation(program, "uProjection");
    locations.skyColor = glGetUniformLocation(program, "uSkyColor");
    locations.spectatorInsideBlock = glGetUniformLocation(program, "uSpectatorInsideBlock");
    locations.sprite = glGetUniformLocation(program, "uSprite");
    locations.sunDirection = glGetUniformLocation(program, "uSunDirection");
    locations.time = glGetUniformLocation(program, "uTime");
    locations.underwater = glGetUniformLocation(program, "uUnderwater");
    locations.view = glGetUniformLocation(program, "uView");
    locations.weather = glGetUniformLocation(program,"uWeather");
    locations.wetness = glGetUniformLocation(program,"uWetness");
    locations.waterPass = glGetUniformLocation(program, "uWaterPass");
    return locations;
}

Renderer::Renderer() {
    textGeometry_ = std::make_unique<TextGeometryCache>();
    auto makeProgram = [&](const char* vs, const char* fs) {
        GLuint v = compileShader(GL_VERTEX_SHADER, vs), f = compileShader(GL_FRAGMENT_SHADER, fs);
        GLuint p = linkProgram(v, f);
        glDeleteShader(v);
        glDeleteShader(f);
        return p;
    };
    occlusion_=std::make_unique<TerrainOcclusion>();
    worldProgram_ = makeProgram(WorldVertexShader, WorldFragmentShader);
    glUseProgram(worldProgram_);
    glUniform1uiv(glGetUniformLocation(worldProgram_,"uSunBits"),64,TerrainPacking::sunBits().data());
    glUseProgram(0);
    skyProgram_ = makeProgram(SkyVertexShader, SkyFragmentShader);
    uiProgram_ = makeProgram(UiVertexShader, UiFragmentShader);
    panoramaProgram_ = makeProgram(UiVertexShader, PanoramaFragmentShader);
    particleProgram_ = makeProgram(ParticleVertexShader, ParticleFragmentShader);
    entityProgram_ = makeProgram(EntityVertexShader, EntityFragmentShader);
    itemProgram_ = makeProgram(ItemVertexShader, ItemFragmentShader);
    previewProgram_=makeProgram(ItemVertexShader,PreviewFragmentShader);
    previewUniforms_=cacheUniforms(previewProgram_);
    billboardProgram_ = makeProgram(ItemVertexShader, BillboardFragmentShader);
    billboardUniforms_ = cacheUniforms(billboardProgram_);
    entityUniforms_ = cacheUniforms(entityProgram_);
    boatFamilyUniform_=glGetUniformLocation(entityProgram_,"uWoodFamily");
    glUseProgram(entityProgram_);glUniform1i(boatFamilyUniform_,-1);glUniform1i(glGetUniformLocation(entityProgram_,"uWood"),0);
    std::vector<std::uint8_t> wood(96*16*4);const glm::ivec3 tones[]={{155,103,52},{205,191,129},{91,61,34},{172,111,70},{194,91,38},{66,43,28}};
    for(int family=0;family<6;++family)for(int y=0;y<16;++y)for(int x=0;x<16;++x){const int grain=int(hashPixel(x/3,y,family+281)%17)-8;const int joint=y%4==0?-28:0;const auto c=tones[family]+glm::ivec3(grain+joint);putPixel(wood,96,family*16+x,y,c.r,c.g,c.b);}
    glGenTextures(1,&boatTexture_);glBindTexture(GL_TEXTURE_2D,boatTexture_);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,96,16,0,GL_RGBA,GL_UNSIGNED_BYTE,wood.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    itemUniforms_ = cacheUniforms(itemProgram_);
    panoramaUniforms_ = cacheUniforms(panoramaProgram_);
    particleUniforms_ = cacheUniforms(particleProgram_);
    skyUniforms_ = cacheUniforms(skyProgram_);
    uiUniforms_ = cacheUniforms(uiProgram_);
    worldUniforms_ = cacheUniforms(worldProgram_);
    // These terrain-program constants never change between draws. Preserve the
    // caller's program binding; relinking would require initializing them again.
    GLint previousProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glUseProgram(worldProgram_);
    glUniform1i(worldUniforms_.atlas, 0);
    glUniform1f(worldUniforms_.atlasTiles, static_cast<float>(BlockAtlasTiles));
    glUseProgram(static_cast<GLuint>(previousProgram));
    uiItemAtlasUniform_ = uiUniforms_.itemAtlas;
    itemAtlasUniform_ = itemUniforms_.itemAtlas;
    atlasTexture_ = createAtlasTexture();
    itemTexture_ = loadItemTexture("assets/item_icons_expansion.rgba");
#ifdef _WIN32
    for (std::size_t index = 0; index < hostileTextures_.size(); ++index) {
        int imageWidth = 0, imageHeight = 0;
        hostileTextures_[index] = loadSpriteImage(SpriteAssets[index].name,
            SpriteAssets[index].id, imageWidth, imageHeight);
        hostileAspectRatios_[index] = static_cast<float>(imageWidth) / imageHeight;
    }
    int panoramaWidth = 0, panoramaHeight = 0;
    panoramaTexture_ = loadSpriteImage("panorama/panorama.jpg", 102,
                                       panoramaWidth, panoramaHeight);
    panoramaAspectRatio_ = static_cast<float>(panoramaWidth) / panoramaHeight;
    glBindTexture(GL_TEXTURE_2D, panoramaTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
#endif
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
    glGenVertexArrays(1, &hudVao_);
    glGenBuffers(1, &hudVbo_);
    glBindVertexArray(hudVao_);
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo_);
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
    if(mapTexture_) glDeleteTextures(1,&mapTexture_);
    if(boatTexture_) glDeleteTextures(1,&boatTexture_);
    if (hudVbo_) glDeleteBuffers(1, &hudVbo_);
    if (hudVao_) glDeleteVertexArrays(1, &hudVao_);
    if (panoramaProgram_) glDeleteProgram(panoramaProgram_);
    if (panoramaTexture_) glDeleteTextures(1, &panoramaTexture_);
    glDeleteTextures(static_cast<GLsizei>(hostileTextures_.size()), hostileTextures_.data());
    if (billboardProgram_)
        glDeleteProgram(billboardProgram_);
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
    if(previewProgram_)glDeleteProgram(previewProgram_);
    if(previewVbo_)glDeleteBuffers(1,&previewVbo_);
    if(previewVao_)glDeleteVertexArrays(1,&previewVao_);
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
    constexpr int s = 16, count = BlockAtlasTiles, width = s * count;
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
    // Append the original crafting textures without moving legacy tile slots.
#ifdef VOXEL_STANDALONE
    const HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(101), MAKEINTRESOURCEW(10));
    const HGLOBAL loaded = resource ? LoadResource(nullptr, resource) : nullptr;
    const void* bytes = loaded ? LockResource(loaded) : nullptr;
    const DWORD length = resource ? SizeofResource(nullptr, resource) : 0;
    if (!bytes || length == 0) throw std::runtime_error("Missing embedded crafting block atlas");
    std::istringstream input(std::string(static_cast<const char*>(bytes), length),
                             std::ios::in | std::ios::binary);
#else
    std::ifstream input("assets/crafting_blocks.rgba", std::ios::binary);
#endif
    std::int32_t extraWidth = 0, extraHeight = 0;
    input.read(reinterpret_cast<char*>(&extraWidth), 4);
    input.read(reinterpret_cast<char*>(&extraHeight), 4);
    if (!input || extraWidth != (count - 45) * s || extraHeight != s)
        throw std::runtime_error("Invalid crafting block atlas dimensions");
    std::vector<std::uint8_t> extra(static_cast<std::size_t>(extraWidth * s * 4));
    input.read(reinterpret_cast<char*>(extra.data()), static_cast<std::streamsize>(extra.size()));
    if (!input) throw std::runtime_error("Truncated crafting block atlas");
    for (int row = 0; row < s; ++row)
        std::copy_n(extra.data() + row * extraWidth * 4, extraWidth * 4,
                    p.data() + (row * width + 45 * s) * 4);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
    glUniformMatrix4fv(skyUniforms_.inverseViewProjection,
                       1,
                       GL_FALSE,
                       glm::value_ptr(inverse));
    glUniform3fv(
        skyUniforms_.sunDirection, 1, glm::value_ptr(state.sunDirection));
    glUniform1f(skyUniforms_.daylight, state.daylight);
    glUniform4fv(skyUniforms_.weather,1,glm::value_ptr(weather_));
    glUniform1f(skyUniforms_.time, worldTime);
    glUniform1i(skyUniforms_.effectQuality, effectQuality_);
    glBindVertexArray(skyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderWorld(const World& world,
                           const glm::mat4& view,
                           const glm::mat4& projection,
                           const glm::vec3& camera,
                           float worldTime,
                           float brightness,
                           bool fullbright,
                           bool underwater,
                           bool spectatorInsideBlock) const {
    const auto terrainStart=std::chrono::steady_clock::now();
    const CelestialState state = celestial(worldTime);
    const glm::mat4 vp = projection * view;
    glUseProgram(worldProgram_);
    glUniformMatrix4fv(
        worldUniforms_.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(worldUniforms_.projection,
                       1,
                       GL_FALSE,
                       glm::value_ptr(projection));
    glUniform3fv(worldUniforms_.sunDirection,
                 1,
                 glm::value_ptr(state.sunDirection));
    glUniform3fv(
        worldUniforms_.skyColor, 1, glm::value_ptr(state.skyColor));
    glUniform3fv(worldUniforms_.cameraPosition,
                 1,
                 glm::value_ptr(camera));
    glUniform1f(worldUniforms_.daylight, state.daylight);
    glUniform4fv(worldUniforms_.weather,1,glm::value_ptr(weather_));
    glUniform1f(worldUniforms_.wetness,weatherWetness_);
    glUniform1f(worldUniforms_.time, worldTime);
    glUniform1f(worldUniforms_.brightness, brightness);
    glUniform1i(worldUniforms_.effectQuality, effectQuality_);
    glUniform1i(worldUniforms_.fullbright,
                fullbright ? GL_TRUE : GL_FALSE);
    glUniform1i(worldUniforms_.underwater,
                underwater ? GL_TRUE : GL_FALSE);
    glUniform1i(worldUniforms_.spectatorInsideBlock,
                spectatorInsideBlock ? GL_TRUE : GL_FALSE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture_);
    glUniform1i(worldUniforms_.waterPass, GL_FALSE);
    const auto& visible = world.collectVisibleChunks(vp, camera);
    occlusion_->draw(world,visible,view,projection,camera,worldProgram_,spectatorInsideBlock?0:occlusionMode_);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!visible.water.empty()) {
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
    }
    glUniform1i(worldUniforms_.waterPass, GL_TRUE);
    world.drawWater(visible);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    terrainRenderMilliseconds_=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-terrainStart).count();
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
void Renderer::spawnBurningParticles(const glm::vec3& position) {
    if(particlePercent_==0 || particles_.size()>4096) return;
    static std::mt19937 rng(2121);std::uniform_real_distribution<float> r(-1.f,1.f);
    for(int i=0;i<3;++i) particles_.push_back({position+glm::vec3(r(rng)*.3f,r(rng)*.4f,r(rng)*.3f),
        glm::vec3(r(rng)*.2f,2.f,r(rng)*.2f),i==0?glm::vec3(.4f):glm::vec3(1.f,.4f,.06f),.28f});
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
void Renderer::spawnExplosionParticles(const glm::vec3& position) {
    static std::mt19937 rng(81721);
    std::uniform_real_distribution<float> random(-1.0f, 1.0f);
    const int count = 45 * particlePercent_ / 100;
    for (int i = 0; i < count; ++i) {
        glm::vec3 direction(random(rng), random(rng) * .8f + .3f, random(rng));
        if (glm::dot(direction, direction) < .01f) direction.y = 1.0f;
        direction = glm::normalize(direction);
        const float speed = 2.0f + static_cast<float>(i % 5) * .6f;
        const glm::vec3 color = i % 3 == 0 ? glm::vec3(.9f, .46f, .16f) :
                                i % 3 == 1 ? glm::vec3(.44f, .42f, .39f) :
                                             glm::vec3(.25f, .24f, .23f);
        particles_.push_back({position, direction * speed, color, .6f +
                              static_cast<float>(i % 4) * .13f});
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
    thread_local std::vector<ParticleVertex> verticesStorage;
    RenderScratch<ParticleVertex> verticesScratch(verticesStorage, 524288);
    auto& vertices = verticesScratch.get();
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
        particleUniforms_.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(particleUniforms_.projection,
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
        entityUniforms_.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(entityUniforms_.projection,
                       1,
                       GL_FALSE,
                       glm::value_ptr(projection));
    glUniform3fv(entityUniforms_.sunDirection,
                 1,
                 glm::value_ptr(state.sunDirection));
    glUniform1f(entityUniforms_.daylight, state.daylight*(1-.25f*weather_.x-.55f*weather_.y));
    glBindVertexArray(entityVao_);
    int currentWood=-1;
    for (const RenderCuboid& cuboid : cuboids) {
        if (!visibility.visible(cuboid.center, glm::length(cuboid.size) * 0.5f))
            continue;
        ++visibleEntityCount_;
        if(cuboid.woodTexture!=currentWood){currentWood=cuboid.woodTexture;glUniform1i(boatFamilyUniform_,currentWood);if(currentWood>=0){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,boatTexture_);}}
        glm::mat4 model = glm::translate(glm::mat4(1.0f), cuboid.center);
        if(cuboid.yaw!=0) model=glm::rotate(model,-cuboid.yaw,glm::vec3(0,1,0));
        if (glm::dot(cuboid.direction, cuboid.direction) > .0001f) {
            const glm::vec3 direction = glm::normalize(cuboid.direction);
            const glm::vec3 up(0, 1, 0);
            const float angle = std::acos(std::clamp(glm::dot(up, direction), -1.0f, 1.0f));
            glm::vec3 axis = glm::cross(up, direction);
            if (glm::dot(axis, axis) < .0001f) axis = glm::vec3(1, 0, 0);
            model = glm::rotate(model, angle, glm::normalize(axis));
        }
        model = glm::scale(model, cuboid.size);
        glUniformMatrix4fv(
            entityUniforms_.model, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(
            entityUniforms_.color, 1, glm::value_ptr(cuboid.color));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glUniform1i(boatFamilyUniform_,-1);
    glBindVertexArray(0);
}
void Renderer::renderBillboards(const std::vector<RenderBillboard>& billboards,
                                const World& world, const glm::mat4& view,
                                const glm::mat4& projection, float worldTime,
                                float brightness, bool fullbright,
                                float maximumDistance) const {
    if (billboards.empty()) return;
    const EntityVisibility visibility(view, projection, maximumDistance);
    const glm::mat4 inverseView = glm::inverse(view);
    const glm::vec3 cameraRight = glm::normalize(glm::vec3(
        inverseView[0][0], 0.0f, inverseView[0][2]));
    const float daylight = celestial(worldTime).daylight*(1-.25f*weather_.x-.55f*weather_.y);
    thread_local std::vector<const RenderBillboard*> visibleStorage;
    RenderScratch<const RenderBillboard*> visibleScratch(visibleStorage, 65536);
    auto& visible = visibleScratch.get();
    visible.reserve(billboards.size());
    for (const RenderBillboard& sprite : billboards)
        if (visibility.visible(sprite.feet + glm::vec3(0, BillboardMobHeight * .5f, 0),
                               BillboardMobHeight * .55f))
            visible.push_back(&sprite);
    std::sort(visible.begin(), visible.end(), [&](const auto* left, const auto* right) {
        const glm::vec3 leftDelta = left->feet - visibility.camera;
        const glm::vec3 rightDelta = right->feet - visibility.camera;
        return glm::dot(leftDelta, leftDelta) > glm::dot(rightDelta, rightDelta);
    });
    glUseProgram(billboardProgram_);
    glUniformMatrix4fv(billboardUniforms_.view, 1,
                       GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(billboardUniforms_.projection, 1,
                       GL_FALSE, glm::value_ptr(projection));
    glUniform1i(billboardUniforms_.sprite, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(itemVao_);
    for (const RenderBillboard* sprite : visible) {
        const std::size_t variant = sprite->variant % hostileTextures_.size();
        if (!hostileTextures_[variant]) continue;
        const float halfWidth = BillboardMobHeight * .5f * hostileAspectRatios_[variant];
        const glm::vec3 left = sprite->feet - cameraRight * halfWidth;
        const glm::vec3 right = sprite->feet + cameraRight * halfWidth;
        const glm::vec3 up(0.0f, BillboardMobHeight, 0.0f);
        const glm::vec3 positions[6] = {left + up, left, right,
                                         left + up, right, right + up};
        const glm::vec2 uv[6] = {{0,0},{0,1},{1,1},{0,0},{1,1},{1,0}};
        float vertices[30]{};
        for (int i = 0; i < 6; ++i) {
            vertices[i*5+0] = positions[i].x;
            vertices[i*5+1] = positions[i].y;
            vertices[i*5+2] = positions[i].z;
            vertices[i*5+3] = uv[i].x;
            vertices[i*5+4] = uv[i].y;
        }
        glBindBuffer(GL_ARRAY_BUFFER, itemVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
        const int x = static_cast<int>(std::floor(sprite->feet.x));
        const int y = static_cast<int>(std::floor(sprite->feet.y + BillboardMobHeight * .5f));
        const int z = static_cast<int>(std::floor(sprite->feet.z));
        const float sky = world.sunlightAt(x, y, z) / 15.0f * daylight;
        const float block = world.blockLightAt(x, y, z) / 15.0f;
        const float ambient = 0.16f + std::max(sky * .70f, block * .75f);
        const float adjusted = ambient + (brightness - .5f) * .20f;
        glUniform1f(billboardUniforms_.light,
                    fullbright ? 1.0f : std::clamp(adjusted, .08f, 1.0f));
        glUniform1f(billboardUniforms_.hurt,
                    std::clamp(sprite->hurt * 4.5f, 0.0f, 1.0f));
        glUniform1f(billboardUniforms_.opacity, sprite->opacity);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hostileTextures_[variant]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        ++visibleEntityCount_;
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}
void Renderer::renderChat(int width, int height, const ChatUI& chat, double now) const {
    if (!chat.isOpen() && chat.messages().empty()) return;
    thread_local std::vector<UiVertex> verticesStorage;
    RenderScratch<UiVertex> verticesScratch(verticesStorage, 2097152);
    auto& vertices = verticesScratch.get();
    const float x = 12.0f, scale = 1.5f;
    const float panelWidth = std::min(620.0f, static_cast<float>(width) - 24.0f);
    const float inputY = static_cast<float>(height) - 93.0f;
    auto drawText = [&](const std::string& value, float tx, float ty, glm::vec4 color) {
        for (char raw : value) {
            const auto& rows = glyph(static_cast<char>(std::toupper(static_cast<unsigned char>(raw))));
            for (int row = 0; row < 7; ++row)
                for (int column = 0; column < 5; ++column)
                    if (rows[row] & (1 << (4 - column)))
                        addRect(vertices, tx + column * scale, ty + row * scale,
                                scale, scale, color, width, height);
            tx += 6.0f * scale;
        }
    };
    if (chat.isOpen()) {
        addRect(vertices, x, inputY, panelWidth, 27.0f,
                {0.02f, 0.02f, 0.025f, 0.78f}, width, height);
        const std::size_t visibleChars = std::max<std::size_t>(1,
            static_cast<std::size_t>((panelWidth - 20.0f) / (6.0f * scale)));
        const std::size_t firstChar = chat.caret() >= visibleChars
            ? chat.caret() - visibleChars + 1 : 0;
        drawText(chat.input().substr(firstChar, visibleChars),
                 x + 7, inputY + 8, {1, 1, 1, 1});
        if (std::fmod(now, 1.0) < 0.5)
            addRect(vertices, x + 7 + static_cast<float>(chat.caret() - firstChar) * 6.0f * scale,
                    inputY + 7, 2.0f, 13.0f, {1, 1, 1, 1}, width, height);
    }
    int shown = 0;
    for (auto it = chat.messages().rbegin(); it != chat.messages().rend(); ++it) {
        if (shown >= (chat.isOpen() ? 8 : 4)) break;
        if (!chat.isOpen() && now - it->time > 7.0) break;
        const float y = inputY - 19.0f * (++shown);
        addRect(vertices, x, y - 3, panelWidth, 18,
                {0.02f, 0.02f, 0.025f, chat.isOpen() ? 0.67f : 0.49f}, width, height);
        glm::vec4 color{0.87f, 0.87f, 0.87f, 1};
        if (it->tone == ChatTone::Success) color = {0.55f, 0.98f, 0.57f, 1};
        if (it->tone == ChatTone::Warning) color = {1.0f, 0.85f, 0.36f, 1};
        if (it->tone == ChatTone::Error) color = {1.0f, 0.43f, 0.43f, 1};
        drawText(it->text.substr(0, 65), x + 6, y, color);
    }
    if (chat.isOpen()) {
        int i = 0;
        for (const std::string& suggestion : chat.suggestions()) {
            if (i >= 5) break;
            const float y = inputY - 20.0f - 16.0f * i++;
            addRect(vertices, x + panelWidth * .5f, y - 2, panelWidth * .5f, 16,
                    {0.02f, 0.02f, 0.025f, 0.72f}, width, height);
            drawText(suggestion, x + panelWidth * .5f + 6, y, {0.74f, 0.84f, 1, 1});
        }
    }
    if (vertices.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)),
                 vertices.data(), GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiProgram_);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, itemTexture_);
    glUniform1i(uiItemAtlasUniform_, 0);
    glBindVertexArray(uiVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0); glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
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
    thread_local std::vector<UiVertex> verticesStorage;
    RenderScratch<UiVertex> verticesScratch(verticesStorage, 8 * 1024 * 1024);
    auto& vertices = verticesScratch.get();
    auto drawText = [&](const std::string& text,
                        float originX,
                        float originY,
                        float scale,
                        const glm::vec4& color) {
        textGeometry_->append(vertices, text, originX, originY, scale, color, width, height);
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
        if(inventory.hotbarLocked(i))addRect(vertices,x,y,slot,3,{1,.75f,.2f,1},width,height);
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
        const float debugScale=std::clamp(std::min((height-130.f)/(lines*9.f),
            (width-28.f)/std::max(1.f,maxChars*6.f)),1.f,2.f);
        addRect(vertices,
                8,
                8,
                maxChars*6.f*debugScale+12,
                lines*9.f*debugScale+10,
                {0, 0, 0, 0.62f},
                width,
                height);
        drawText(debugText, 14, 14, debugScale, {0.92f, 0.96f, 1.0f, 1});
    }
    if (!craftingText.empty()) {
        const float panelW = std::min(680.0f, static_cast<float>(width) - 32.0f),
                    panelX = (width - panelW) * 0.5f, panelY = y - 72.0f;
        addRect(vertices, panelX, panelY, panelW, 54, {0.04f, 0.03f, 0.025f, 0.88f}, width, height);
        drawText(craftingText, panelX + 12, panelY + 9, 1.5f, {0.98f, 0.86f, 0.58f, 1});
    }
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo_);
    const std::size_t bytes = vertices.size() * sizeof(UiVertex);
    auto& uploaded = textGeometry_->uploadedHud;
    if (uploaded.size() != vertices.size() ||
        (bytes && std::memcmp(uploaded.data(), vertices.data(), bytes) != 0)) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes),
                     vertices.data(), GL_STREAM_DRAW);
        if (bytes <= TextGeometryCache::MaximumBytes) uploaded = vertices;
        else std::vector<UiVertex>().swap(uploaded);
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, itemTexture_);
    glUniform1i(uiItemAtlasUniform_, 0);
    glBindVertexArray(hudVao_);
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
    thread_local std::vector<float> verticesStorage;
    RenderScratch<float> verticesScratch(verticesStorage, 524288);
    auto& vertices = verticesScratch.get();
    vertices.reserve(sprites.size() * 30);
    auto push = [&](glm::vec3 p, float u, float v) {
        vertices.insert(vertices.end(), {p.x, p.y, p.z, u, v});
    };
    for (const auto& s : sprites) {
        if (s.worldPlant || !visibility.visible(s.center, s.size))
            continue;
        ItemSpriteUv uv;
        if (!itemSpriteUv(s.item, uv))
            continue;
        ++visibleEntityCount_;
        glm::vec3 c = s.center;
        glm::vec3 spriteRight=right,spriteUp=up;
        if(s.fixed){spriteRight=glm::normalize(glm::cross(glm::vec3(0,1,0),s.normal));spriteUp={0,1,0};for(int i=0;i<s.rotation;++i){auto old=spriteRight;spriteRight=spriteUp;spriteUp=-old;}}
        else c+=glm::vec3(0,.12f+std::sin(time*2.7f+s.center.x)*.05f,0);
        float h = s.size * .5f;
        glm::vec3 a = c - spriteRight * h + spriteUp * h, b = c - spriteRight * h - spriteUp * h,
                  d = c + spriteRight * h + spriteUp * h, e = c + spriteRight * h - spriteUp * h;
        push(a, uv.u0, uv.v0);
        push(b, uv.u0, uv.v1);
        push(e, uv.u1, uv.v1);
        push(a, uv.u0, uv.v0);
        push(e, uv.u1, uv.v1);
        push(d, uv.u1, uv.v0);
    }
    const auto itemVertexCount=static_cast<GLsizei>(vertices.size()/5);
    // Crossed, double-sided planes use the same plant tile as the ground mesh.
    // Keep them in one extra batch, sharing the item shader and streaming buffer.
    for(const auto& s:sprites){
        if(!s.worldPlant || !visibility.visible(s.center,s.size))continue;
        const Block plant=itemToBlock(s.item);
        if(plant==Block::Air)continue;
        ++visibleEntityCount_;
        constexpr float inset=.0005f;
        const int tile=blockTexture(plant,0);
        const float u0=float(tile)/BlockAtlasTiles+inset,u1=float(tile+1)/BlockAtlasTiles-inset;
        const float h=s.size*.5f;
        for(const glm::vec3 diagonal:{glm::vec3(.70710678f,0,.70710678f),glm::vec3(.70710678f,0,-.70710678f)}){
            const auto a=s.center-diagonal*h+glm::vec3(0,h,0),b=a-glm::vec3(0,s.size,0);
            const auto d=s.center+diagonal*h+glm::vec3(0,h,0),e=d-glm::vec3(0,s.size,0);
            push(a,u0,inset);push(b,u0,1-inset);push(e,u1,1-inset);
            push(a,u0,inset);push(e,u1,1-inset);push(d,u1,inset);
            push(a,u0,inset);push(e,u1,1-inset);push(b,u0,1-inset);
            push(a,u0,inset);push(d,u1,inset);push(e,u1,1-inset);
        }
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
        itemUniforms_.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(
        itemUniforms_.projection, 1, GL_FALSE, glm::value_ptr(projection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, itemTexture_);
    glUniform1i(itemAtlasUniform_, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(itemVao_);
    if(itemVertexCount)glDrawArrays(GL_TRIANGLES,0,itemVertexCount);
    const auto plantVertexCount=static_cast<GLsizei>(vertices.size()/5)-itemVertexCount;
    if(plantVertexCount){glBindTexture(GL_TEXTURE_2D,atlasTexture_);glDrawArrays(GL_TRIANGLES,itemVertexCount,plantVertexCount);}
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}
void Renderer::renderSelectionOutline(const glm::ivec3& block, Block type,
                                      const glm::mat4& view,
                                      const glm::mat4& projection) const {
    glUseProgram(entityProgram_);
    BuildingBoxes boxes;
    if(isBuilding(type))boxes=buildingBoxes(type);
    else {auto g=blockGeometry(type);boxes.add(g.minX,g.minY,g.minZ,g.maxX,g.maxY,g.maxZ);}
    for(int part=0;part<boxes.count;++part){
    const auto b=boxes.boxes[part];BlockGeometryProperties bounds;bounds.minX=b.minimum.x;bounds.minY=b.minimum.y;bounds.minZ=b.minimum.z;bounds.maxX=b.maximum.x;bounds.maxY=b.maximum.y;bounds.maxZ=b.maximum.z;
    const glm::vec3 minimum(bounds.minX, bounds.minY, bounds.minZ);
    const glm::vec3 maximum(bounds.maxX, bounds.maxY, bounds.maxZ);
    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(block) +
                                                        (minimum + maximum) * .5f);
    model = glm::scale(model, (maximum - minimum) + glm::vec3(.006f));
    glUniformMatrix4fv(
        entityUniforms_.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(
        entityUniforms_.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(entityUniforms_.projection,
                       1,
                       GL_FALSE,
                       glm::value_ptr(projection));
    glm::vec3 color(.03f);
    glm::vec3 sun(0, 1, 0);
    glUniform3fv(entityUniforms_.color, 1, glm::value_ptr(color));
    glUniform3fv(entityUniforms_.sunDirection, 1, glm::value_ptr(sun));
    glUniform1f(entityUniforms_.daylight, 1);
    glBindVertexArray(entityVao_);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
    }
}
void Renderer::renderMobOutline(const RenderCuboid& bounds,
                                const glm::mat4& view,
                                const glm::mat4& projection) const {
    glUseProgram(entityProgram_);
    glm::mat4 model = glm::translate(glm::mat4(1), bounds.center);
    model = glm::scale(model, bounds.size * 1.018f);
    glUniformMatrix4fv(
        entityUniforms_.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(
        entityUniforms_.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(entityUniforms_.projection,
                       1,
                       GL_FALSE,
                       glm::value_ptr(projection));
    glm::vec3 color(1.0f, .88f, .28f), sun(0, 1, 0);
    glUniform3fv(entityUniforms_.color, 1, glm::value_ptr(color));
    glUniform3fv(entityUniforms_.sunDirection, 1, glm::value_ptr(sun));
    glUniform1f(entityUniforms_.daylight, 1);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(entityVao_);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.5f);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}
void Renderer::renderMainMenu(int width, int height, int hovered, bool pressed,
                              bool buttonsVisible, bool standaloneDemo, double now) const {
    thread_local std::vector<UiVertex> verticesStorage;
    RenderScratch<UiVertex> verticesScratch(verticesStorage, 2097152);
    auto& vertices = verticesScratch.get();
    const float windowAspect = static_cast<float>(width) / height;
    // Cover the window: crop the longer image axis, never distort the image.
    const float uSpan = std::min(1.0f, windowAspect / panoramaAspectRatio_);
    const float vSpan = std::min(1.0f, panoramaAspectRatio_ / windowAspect);
    const float rotation = static_cast<float>(std::fmod(now / 180.0, 1.0));
    const float u0 = (1.0f - uSpan) * .5f + rotation, u1 = u0 + uSpan;
    const float v0 = (1.0f - vSpan) * .5f, v1 = v0 + vSpan;
    if (panoramaTexture_) {
        const glm::vec4 white(1.0f);
        vertices = {{{-1, 1}, white, {u0, v0}, 1},
                    {{1, 1}, white, {u1, v0}, 1},
                    {{1, -1}, white, {u1, v1}, 1},
                    {{-1, 1}, white, {u0, v0}, 1},
                    {{1, -1}, white, {u1, v1}, 1},
                    {{-1, -1}, white, {u0, v1}, 1}};
    }
    addRect(vertices, 0, 0, static_cast<float>(width), static_cast<float>(height),
            {0.02f, .025f, .035f, panoramaTexture_ ? .30f : 1.0f}, width, height);
    auto text = [&](const std::string& value, float x, float y, float scale, glm::vec4 color) {
        for (char raw : value) {
            const auto& rows = glyph(static_cast<char>(std::toupper(static_cast<unsigned char>(raw))));
            for (int row = 0; row < 7; ++row)
                for (int column = 0; column < 5; ++column)
                    if (rows[row] & (1 << (4 - column)))
                        addRect(vertices, x + column * scale, y + row * scale,
                                scale, scale, color, width, height);
            x += 6 * scale;
        }
    };
    if (buttonsVisible) {
        const float blend = 1.0f - std::exp(-10.0f * static_cast<float>(std::clamp(now-lastMenuDraw_, 0.0, .1)));
        lastMenuDraw_ = now;
        const float scale = std::min({1.0f, width / 520.0f, height / 440.0f});
        const std::string title = "VOXEL FRONTIER";
        const float titleScale = 4.0f * scale;
        const float titleX = (width - (title.size() * 6 - 1) * titleScale) * .5f;
        const float titleY = height * .5f - 135.0f * scale;
        text(title, titleX + 3 * scale, titleY + 3 * scale, titleScale, {0, 0, 0, .8f});
        text(title, titleX, titleY, titleScale, {1, .94f, .75f, 1});
        const std::array<std::string, 3> labels{"PLAY", "SETTINGS", "QUIT"};
        for (int index = 0; index < 3; ++index) {
            const UiRect r = UIManager::mainMenuButton(index, width, height);
            const bool down = hovered == index && pressed;
            menuHover_[index] = glm::mix(menuHover_[index], hovered == index ? 1.0f : 0.0f, blend);
            addRect(vertices, r.x + 4*scale, r.y + 5*scale, r.width, r.height,
                    {0,0,0,.35f}, width,height);
            addRect(vertices, r.x, r.y, r.width, r.height, {.09f, .10f, .10f, 1}, width, height);
            const glm::vec4 fill = down ? glm::vec4(.22f, .30f, .18f, 1)
                : glm::mix(glm::vec4(.18f, .20f, .22f, .96f),
                           glm::vec4(.38f, .48f, .30f, 1), menuHover_[index]);
            addRect(vertices, r.x + 2 * scale, r.y + 2 * scale, r.width - 4 * scale,
                    r.height - 4 * scale, fill, width, height);
            addRect(vertices, r.x + 2 * scale, r.y + 2 * scale, r.width - 4 * scale,
                    2 * scale, down ? glm::vec4(.15f, .20f, .13f, 1)
                                   : glm::vec4(.52f, .56f, .46f, 1), width, height);
            const float fontScale = 2.4f * scale;
            const auto& label = labels[static_cast<std::size_t>(index)];
            text(label, r.x + (r.width - (label.size() * 6 - 1) * fontScale) * .5f,
                 r.y + (r.height - 7 * fontScale) * .5f + (down ? 2 * scale : 0),
                 fontScale, {1, 1, 1, 1});
        }
        text(std::string("V") + VOXELFRONTIER_VERSION, 16, height - 25.0f, 1.5f, {.9f, .9f, .9f, 1});
        if (standaloneDemo) {
            const std::string demo = "STANDALONE DEMO";
            text(demo, width - demo.size() * 9.0f - 16, height - 25.0f, 1.5f,
                 {1, .85f, .52f, 1});
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)),
                 vertices.data(), GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(panoramaProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, panoramaTexture_);
    glUniform1i(panoramaUniforms_.itemAtlas, 0);
    glBindVertexArray(uiVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderMenu(int width,
                          int height,
                          bool settingsPage,
                          int hovered,
                          const GameSettings& settings,
                          GameMode /*mode*/,
                          bool standaloneDemo,
                          bool showSaveWarning,
                          bool audioPage) const {
    thread_local std::vector<UiVertex> verticesStorage;
    RenderScratch<UiVertex> verticesScratch(verticesStorage, 2097152);
    auto& vertices = verticesScratch.get();
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
    const float panelY = height * .5f - (audioPage ? 220.0f : settingsPage ? 350.0f : 300.0f);
    const float panelHeight = audioPage ? 440.0f : settingsPage ? 700.0f : 600.0f;
    addRect(vertices, panelX, panelY, 440, panelHeight, {.075f, .075f, .085f, .98f}, width, height);
    drawText(audioPage ? "AUDIO" : settingsPage ? "SETTINGS" : "GAME PAUSED",
             panelX + (audioPage ? 175.0f : settingsPage ? 114.0f : 92.0f),
             panelY + 28.0f,
             3.0f,
             {1, .92f, .68f, 1});

    if (audioPage) {
        const std::array<std::string, 5> labels{
            "MASTER VOLUME", "MUSIC VOLUME", "SFX VOLUME",
            "PASSIVE MOBS", "HOSTILE MOBS"};
        const std::array<float, 5> values{
            settings.masterVolume, settings.musicVolume, settings.sfxVolume,
            settings.passiveMobVolume, settings.hostileMobVolume};
        for (int index = 0; index < 5; ++index) {
            const float y = panelY + 70.0f + index * 58.0f;
            addRect(vertices, panelX + 24.0f, y, 392.0f, 46.0f,
                    index == hovered ? glm::vec4(.35f, .44f, .29f, 1)
                                     : glm::vec4(.19f, .19f, .22f, 1), width, height);
            drawText(labels[static_cast<std::size_t>(index)], panelX + 38.0f,
                     y + 9.0f, 1.25f, {1, 1, 1, 1});
            const float sliderX = panelX + 190.0f;
            addRect(vertices, sliderX, y + 29.0f, 150.0f, 5.0f,
                    {.10f, .11f, .12f, 1}, width, height);
            addRect(vertices, sliderX, y + 29.0f, 150.0f * values[index], 5.0f,
                    {.48f, .70f, .34f, 1}, width, height);
            addRect(vertices, sliderX + 150.0f * values[index] - 4.0f,
                    y + 22.0f, 8.0f, 19.0f, {.88f, .92f, .78f, 1}, width, height);
            drawText(std::to_string(static_cast<int>(std::round(values[index] * 100.0f))) + "%",
                     panelX + 354.0f, y + 25.0f, 1.25f, {1, .93f, .70f, 1});
        }
        addRect(vertices, panelX + 95.0f, panelY + 372.0f, 250.0f, 44.0f,
                hovered == 5 ? glm::vec4(.38f, .48f, .30f, 1)
                             : glm::vec4(.20f, .20f, .23f, 1), width, height);
        drawText("BACK", panelX + 180.0f, panelY + 385.0f, 2.0f, {1, 1, 1, 1});
    } else if (!settingsPage) {
        const std::array<std::string, 6> labels{
            "RESUME GAME",
            "SETTINGS",
            "RESET WORLD",
            "SAVE WORLD",
            standaloneDemo ? "RETURN TO MENU" : "SAVE & RETURN TO MENU",
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
        const std::array<std::string, 14> labels{
            "RENDER DISTANCE",
            "SIMULATION DISTANCE",
            "FOV",
            "MOUSE SENSITIVITY",
            "AUDIO SETTINGS",
            "BRIGHTNESS",
            std::string("ANTI ALIASING  ") + antiAliasing,
            std::string("FULLSCREEN  ") + (settings.fullscreen ? "ON" : "OFF"),
            std::string("VSYNC  ") + (settings.vsync ? "ON" : "OFF"),
            std::string("SHOW FPS  ") + (settings.showFps ? "ON" : "OFF"),
            std::string("SHOW COORDINATES  ") +
                (settings.showCoordinates ? "ON" : "OFF"),
            std::string("GRAPHICS PRESET  ") + preset,
            "ENTITY DISTANCE",
            std::string("FRAME LIMIT  ") + frameLimit};
        const std::array<std::string, 7> sliderValues{
            std::to_string(settings.renderDistance),
            std::to_string(settings.simulationDistance),
            std::to_string(static_cast<int>(std::round(settings.fov))),
            std::to_string(static_cast<int>(std::round(settings.mouseSensitivity * 100.0f))),
            std::to_string(static_cast<int>(std::round(settings.brightness * 100.0f))) + "%",
            std::to_string(static_cast<int>(std::round(settings.masterVolume * 100.0f))),
            std::to_string(settings.entityDistance)};
        const std::array<float, 7> sliderPositions{
            (settings.renderDistance - 2.0f) / 62.0f,
            (settings.simulationDistance - 2.0f) / 30.0f,
            (settings.fov - 55.0f) / 50.0f,
            (settings.mouseSensitivity - 0.03f) / 0.27f,
            settings.brightness,
            settings.masterVolume,
            (settings.entityDistance - 2.0f) / 62.0f};
        for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
            const float y = panelY + 65.0f + index * 37.0f;
            addRect(vertices,
                    panelX + 24.0f,
                    y,
                    392.0f,
                    35.0f,
                    index == hovered ? glm::vec4(.35f, .44f, .29f, 1)
                                     : glm::vec4(.19f, .19f, .22f, 1),
                    width,
                    height);
            drawText(labels[static_cast<std::size_t>(index)],
                     panelX + 38.0f,
                     y + (index < 4 || index == 5 || index == 12 ? 7.0f : 10.0f),
                     index < 4 || index == 5 || index == 12 ? 1.15f : 1.4f,
                     {1, 1, 1, 1});
            if (index < 4 || index == 5 || index == 12) {
                const std::size_t sliderIndex = index == 12 ? 6 : index == 5 ? 4
                                                   : static_cast<std::size_t>(index);
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
                hovered == 14 ? glm::vec4(.38f, .48f, .30f, 1)
                              : glm::vec4(.20f, .20f, .23f, 1), width, height);
        drawText("CONTROLS", panelX + 155.0f, controlsY + 13.0f,
                 2.0f, {1, 1, 1, 1});
        const float y = panelY + 635.0f;
        addRect(vertices,
                panelX + 95.0f,
                y,
                250.0f,
                42.0f,
                hovered == 15 ? glm::vec4(.38f, .48f, .30f, 1)
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
    thread_local std::vector<UiVertex> verticesStorage;
    RenderScratch<UiVertex> verticesScratch(verticesStorage, 2097152);
    auto& vertices = verticesScratch.get();
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
        const float y = panelY + 75.0f + index * 29.0f;
        addRect(vertices, panelX + 35.0f, y, 370.0f, 26.0f,
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
    const auto sensitivity = MenuLayout::sensitivity(width,height);
    addRect(vertices,sensitivity.x,sensitivity.y,sensitivity.width,sensitivity.height,
            hovered==ControlActionCount+2 ? glm::vec4(.3f,.4f,.25f,1) : glm::vec4(.19f,.19f,.22f,1),width,height);
    drawText("MOUSE SENSITIVITY",sensitivity.x+10,sensitivity.y+12,1.2f,{1,1,1,1});
    const float value=std::clamp((settings.mouseSensitivity-.03f)/.27f,0.0f,1.0f);
    addRect(vertices,sensitivity.x+190,sensitivity.y+18,150,5,{.1f,.11f,.12f,1},width,height);
    addRect(vertices,sensitivity.x+190+150*value-4,sensitivity.y+10,8,19,{.88f,.92f,.78f,1},width,height);
    drawText(std::to_string(static_cast<int>(std::round(settings.mouseSensitivity*100))),sensitivity.x+345,sensitivity.y+12,1.2f,{1,.93f,.70f,1});
    for (int index = 0; index < 2; ++index) {
        const float y = panelY + (index == 0 ? 590.0f : 644.0f);
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

namespace {
void menuText(std::vector<UiVertex>& vertices, const std::string& value,
              float x, float y, float scale, glm::vec4 color, int width, int height) {
    for (unsigned char raw : value) {
        const auto& rows = glyph(static_cast<char>(std::toupper(raw)));
        for (int row=0; row<7; ++row)
            for (int column=0; column<5; ++column)
                if (rows[row] & (1 << (4-column)))
                    addRect(vertices,x+column*scale,y+row*scale,scale,scale,color,width,height);
        x += 6*scale;
    }
}
void menuButton(std::vector<UiVertex>& vertices, const UiRect& r, const std::string& label,
                bool highlighted, int width, int height, float font=1.7f) {
    addRect(vertices,r.x,r.y,r.width,r.height,{.07f,.08f,.09f,1},width,height);
    addRect(vertices,r.x+2,r.y+2,r.width-4,r.height-4,
            highlighted ? glm::vec4(.34f,.43f,.28f,1) : glm::vec4(.18f,.20f,.23f,1),width,height);
    menuText(vertices,label,r.x+(r.width-(label.size()*6-1)*font)*.5f,
             r.y+(r.height-7*font)*.5f,font,{1,1,1,1},width,height);
}
void menuPanel(std::vector<UiVertex>& vertices, const std::string& title, int width, int height) {
    const auto p=MenuLayout::panel(width,height);
    addRect(vertices,0,0,static_cast<float>(width),static_cast<float>(height),{0,0,0,.35f},width,height);
    addRect(vertices,p.x,p.y,p.width,p.height,{.055f,.065f,.075f,.96f},width,height);
    menuText(vertices,title,p.x+(p.width-(title.size()*6-1)*3)*.5f,p.y+24,3,
             {1,.94f,.75f,1},width,height);
}
}

void Renderer::drawMenuVertices(const void* data, std::size_t count) const {
    glBindBuffer(GL_ARRAY_BUFFER,uiVbo_);
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(count*sizeof(UiVertex)),data,GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiProgram_);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,itemTexture_);
    glUniform1i(uiItemAtlasUniform_,0);
    glBindVertexArray(uiVao_); glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(count));
    glBindVertexArray(0); glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
}

void Renderer::renderNowPlaying(int width, int height, const std::string& songName) const {
    if (songName.empty() || width < 80 || height < 40) return;
    // Rebuild glyph geometry only on song changes or window resizing.
    struct Cache {
        std::string song;
        int width = 0, height = 0;
        std::vector<UiVertex> vertices;
    };
    thread_local Cache cache;
    if (cache.song != songName || cache.width != width || cache.height != height) {
        cache.song = songName;
        cache.width = width;
        cache.height = height;
        cache.vertices.clear();
        constexpr float scale = 1.25f, padding = 5.0f, margin = 12.0f;
        std::string label = "NOW PLAYING: " + songName;
        const auto maxChars = static_cast<std::size_t>((width - 2 * (margin + padding)) / (6 * scale));
        if (label.size() > maxChars) label = label.substr(0, maxChars - 3) + "...";
        const float textWidth = (label.size() * 6 - 1) * scale;
        const float x = width - margin - padding - textWidth;
        addRect(cache.vertices, x - padding, margin, textWidth + 2 * padding,
                7 * scale + 2 * padding, {0.02f, 0.025f, 0.03f, 0.48f}, width, height);
        menuText(cache.vertices, label, x + 1, margin + padding + 1, scale,
                 {0, 0, 0, .8f}, width, height);
        menuText(cache.vertices, label, x, margin + padding, scale,
                 {.88f, .91f, .94f, 1}, width, height);
    }
    drawMenuVertices(cache.vertices.data(), cache.vertices.size());
}

void Renderer::renderSettingsCategories(int width, int height, int hovered,
                                        const GameSettings& settings, bool video) const {
    thread_local std::vector<UiVertex> verticesStorage;
    RenderScratch<UiVertex> verticesScratch(verticesStorage, 2097152);
    auto& vertices = verticesScratch.get();
    menuPanel(vertices,video ? "VIDEO SETTINGS" : "SETTINGS",width,height);
    const std::array<std::string,3> categories{"VIDEO","AUDIO","CONTROLS"};
    if (!video) {
        const std::array<std::string,5> hub{"VIDEO","AUDIO","CONTROLS","WEATHER","BACK"};
        for (int index=0; index<5; ++index) {
            const int action=index==3 ? 34 : index==4 ? 33 : 30+index;
            menuButton(vertices,MenuLayout::hubRow(index,width,height),hub[index],hovered==action,width,height,2.2f);
        }
    } else {
        for (int index=0; index<int(categories.size()); ++index)
            menuButton(vertices,MenuLayout::tab(index,width,height),categories[index],index==0 || hovered==30+index,width,height);
        const std::array<std::string,19> labels{"RENDER DISTANCE","SIMULATION DISTANCE","FOV","BRIGHTNESS",
            "ANTI ALIASING","FULLSCREEN","VSYNC","FPS LIMIT","GRAPHICS PRESET","ENTITY DISTANCE","SHOW FPS","SHOW COORDINATES","CHUNK STREAMING","TERRAIN LOD","OCCLUSION","TERRAIN MEMORY TARGET","UI SCALE","INTERACTION HINTS","PLACEMENT PREVIEW"};
        const std::array<std::string,19> values{
            std::to_string(settings.renderDistance),std::to_string(settings.simulationDistance),
            std::to_string(static_cast<int>(std::round(settings.fov))),
            std::to_string(static_cast<int>(std::round(settings.brightness*100)))+"%",
            settings.antiAliasingSamples ? std::to_string(settings.antiAliasingSamples)+"X" : "OFF",
            settings.fullscreen ? "ON" : "OFF",settings.vsync ? "ON" : "OFF",
            settings.frameLimit ? std::to_string(settings.frameLimit) : "UNLIMITED",
            settings.graphicsPreset==GraphicsPreset::Low ? "LOW" : settings.graphicsPreset==GraphicsPreset::Medium ? "MEDIUM" : settings.graphicsPreset==GraphicsPreset::High ? "HIGH" : "CUSTOM",
            std::to_string(settings.entityDistance),settings.showFps ? "ON" : "OFF",settings.showCoordinates ? "ON" : "OFF",
            std::array<const char*,4>{"AUTO","LOW","BALANCED","HIGH"}[settings.chunkStreamingQuality],
            std::array<const char*,4>{"OFF","LOW","MEDIUM","HIGH"}[settings.terrainLod],
            std::array<const char*,3>{"OFF","AUTO","ON"}[settings.occlusionCulling],
            std::array<const char*,4>{"AUTO","LOW","BALANCED","HIGH"}[settings.terrainMemoryBudget],
            std::array<const char*,4>{"AUTO","SMALL","NORMAL","LARGE"}[settings.uiScale],settings.interactionHints?"ON":"OFF",settings.placementPreview?"ON":"OFF"};
        const std::array<float,19> positions{(settings.renderDistance-2)/62.0f,(settings.simulationDistance-2)/30.0f,
            (settings.fov-55)/50.0f,settings.brightness,0,0,0,0,0,(settings.entityDistance-2)/62.0f,0,0};
        for (int index=0; index<int(MenuLayout::VideoActions.size()); ++index) {
            const auto r=MenuLayout::videoRow(index,width,height);
            addRect(vertices,r.x,r.y,r.width,r.height,hovered==MenuLayout::VideoActions[index] ? glm::vec4(.27f,.32f,.25f,1) : glm::vec4(.15f,.17f,.19f,1),width,height);
            menuText(vertices,labels[index],r.x+12,r.y+8,1.25f,{1,1,1,1},width,height);
            if (index<4 || index==9) {
                const float x=MenuLayout::panel(width,height).x+340;
                const float normalized=std::clamp(positions[index],0.0f,1.0f);
                addRect(vertices,x,r.y+12,170,5,{.07f,.08f,.09f,1},width,height);
                addRect(vertices,x,r.y+12,170*normalized,5,{.48f,.70f,.34f,1},width,height);
                addRect(vertices,x+170*normalized-4,r.y+5,8,18,{.88f,.92f,.78f,1},width,height);
                menuText(vertices,values[index],x+185,r.y+11,1.2f,{1,.93f,.70f,1},width,height);
            } else menuText(vertices,values[index],r.x+350,r.y+11,1.3f,{.85f,.93f,.78f,1},width,height);
        }
        menuButton(vertices,MenuLayout::back(width,height),"BACK",hovered==15,width,height);
    }
    drawMenuVertices(vertices.data(),vertices.size());
}

void Renderer::renderWorldMenu(int width, int height, int hovered, bool creating,
                               const std::vector<SavedWorld>& worlds, int page, int selected,
                               const std::string& name, const std::string& seed, GameMode mode, bool cheats,
                               int field, const std::string& message, double now) const {
    thread_local std::vector<UiVertex> verticesStorage;
    RenderScratch<UiVertex> verticesScratch(verticesStorage, 2097152);
    auto& vertices = verticesScratch.get();
    menuPanel(vertices,creating ? "CREATE NEW WORLD" : "SELECT WORLD",width,height);
    const auto p=MenuLayout::panel(width,height);
    if (creating) {
        const std::array<std::string,4> labels{"WORLD NAME","SEED (OPTIONAL)","GAME MODE","CHEATS"};
        const std::array<std::string,4> values{name,seed.empty()?"RANDOM":seed,gameModeName(mode),cheats?"ON":"OFF"};
        for (int index=0; index<4; ++index) {
            const auto r=MenuLayout::creationField(index,width,height);
            menuText(vertices,labels[index],r.x,r.y-24,1.6f,{.85f,.9f,.8f,1},width,height);
            addRect(vertices,r.x,r.y,r.width,r.height,field==index || hovered==index ? glm::vec4(.3f,.4f,.25f,1) : glm::vec4(.16f,.18f,.20f,1),width,height);
            auto value=values[index];
            if (value.size()>39) value=value.substr(value.size()-39);
            if (index==field && std::fmod(now,1.0)<.5) value+="_";
            menuText(vertices,value,r.x+14,r.y+18,2,{1,1,1,1},width,height);
        }
        menuText(vertices,"TAB SWITCH FIELD   CTRL+A CLEAR",p.x+40,p.y+470,1.3f,{.65f,.72f,.75f,1},width,height);
        menuButton(vertices,MenuLayout::creationButton(0,width,height),"CREATE WORLD",hovered==4,width,height);
        menuButton(vertices,MenuLayout::creationButton(1,width,height),"BACK",hovered==5,width,height);
    } else {
        for (int row=0; row<6; ++row) {
            const int index=page*6+row;
            if (index>=static_cast<int>(worlds.size())) break;
            const auto& world=worlds[index]; const auto r=MenuLayout::worldRow(row,width,height);
            addRect(vertices,r.x,r.y,r.width,r.height,index==selected ? glm::vec4(.29f,.39f,.24f,1) : hovered==row ? glm::vec4(.24f,.28f,.24f,1) : glm::vec4(.15f,.17f,.19f,1),width,height);
            menuText(vertices,world.name.substr(0,43),r.x+12,r.y+9,1.8f,{1,1,1,1},width,height);
            std::string details=std::string(gameModeName(world.mode))+"  SEED "+std::to_string(world.seed);
            if (world.lastPlayed>0) {
                const auto stamp=static_cast<std::time_t>(world.lastPlayed); std::tm date{};
#ifdef _WIN32
                localtime_s(&date,&stamp);
#else
                localtime_r(&stamp,&date);
#endif
                std::ostringstream text; text<<std::put_time(&date,"%Y-%m-%d %H:%M"); details+="  "+text.str();
            }
            menuText(vertices,details,r.x+12,r.y+35,1.2f,{.72f,.80f,.68f,1},width,height);
        }
        if(selected>=0 && selected<int(worlds.size())) menuButton(vertices,{width*.5f-100,height*.5f+168,200,32},worlds[selected].cheats?"CHEATS: ON":"CHEATS: OFF",hovered==15,width,height,1.3f);
        if (worlds.empty()) menuText(vertices,"NO WORLDS YET. CREATE YOUR FIRST WORLD.",p.x+45,p.y+190,1.8f,{.85f,.90f,.82f,1},width,height);
        menuButton(vertices,MenuLayout::pageButton(0,width,height),"PREV",hovered==13,width,height,1.3f);
        menuButton(vertices,MenuLayout::pageButton(1,width,height),"NEXT",hovered==14,width,height,1.3f);
        menuText(vertices,"PAGE "+std::to_string(page+1)+" / "+std::to_string(std::max(1,(static_cast<int>(worlds.size())+5)/6)),p.x+245,p.y+490,1.3f,{.8f,.85f,.75f,1},width,height);
        const std::array<std::string,3> actions{"PLAY","CREATE NEW WORLD","BACK"};
        for (int i=0; i<3; ++i) menuButton(vertices,MenuLayout::worldAction(i,width,height),actions[i],hovered==10+i,width,height,1.35f);
    }
    if (!message.empty()) menuText(vertices,message.substr(0,70),p.x+24,p.y+515,1.2f,{1,.65f,.5f,1},width,height);
    drawMenuVertices(vertices.data(),vertices.size());
}

void Renderer::renderResetMenu(int width,
                               int height,
                               int hovered,
                               const std::string& seedText,
                               GameMode mode) const {
    thread_local std::vector<UiVertex> verticesStorage;
    RenderScratch<UiVertex> verticesScratch(verticesStorage, 2097152);
    auto& vertices = verticesScratch.get();
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

void Renderer::renderWeatherSettings(int width,int height,int hovered,const GameSettings& s) const {
    thread_local std::vector<UiVertex> storage;
    RenderScratch<UiVertex> scratch(storage,2097152); auto& vertices=scratch.get();
    menuPanel(vertices,"WEATHER SETTINGS",width,height);
    const std::array<std::string,8> labels{"WEATHER CYCLE","WEATHER QUALITY","PRECIPITATION DENSITY","SNOW ACCUMULATION","LIGHTNING EFFECTS","FLASH INTENSITY","WEATHER FOG","WIND EFFECTS"};
    const auto on=[](bool value){return value ? "ON" : "OFF";};
    const std::array<std::string,8> values{on(s.weatherCycle),std::array<std::string,4>{"OFF","LOW","MEDIUM","HIGH"}[s.weatherQuality],
        std::to_string(static_cast<int>(std::round(s.precipitationDensity*100)))+"%",on(s.snowAccumulation),on(s.lightningEffects),
        std::to_string(static_cast<int>(std::round(s.lightningFlash*100)))+"%",on(s.weatherFog),on(s.weatherWind)};
    for(int i=0;i<8;++i) {
        const auto r=MenuLayout::videoRow(i,width,height);
        addRect(vertices,r.x,r.y,r.width,r.height,hovered==40+i ? glm::vec4(.27f,.32f,.25f,1) : glm::vec4(.15f,.17f,.19f,1),width,height);
        menuText(vertices,labels[i],r.x+12,r.y+11,1.25f,{1,1,1,1},width,height);
        const float x=MenuLayout::panel(width,height).x+340;
        if(i==2 || i==5) {
            const float value=i==2 ? s.precipitationDensity : s.lightningFlash;
            addRect(vertices,x,r.y+15,170,5,{.07f,.08f,.09f,1},width,height);
            addRect(vertices,x,r.y+15,170*value,5,{.48f,.70f,.34f,1},width,height);
            addRect(vertices,x+170*value-4,r.y+8,8,19,{.88f,.92f,.78f,1},width,height);
            menuText(vertices,values[i],x+185,r.y+11,1.15f,{1,.93f,.70f,1},width,height);
        } else menuText(vertices,values[i],x,r.y+11,1.3f,{.85f,.93f,.78f,1},width,height);
    }
    menuText(vertices,"GRAPHICS OPTIONS KEEP WEATHER GAMEPLAY ACTIVE",MenuLayout::panel(width,height).x+28,MenuLayout::panel(width,height).y+430,1.15f,{.8f,.85f,.8f,1},width,height);
    menuButton(vertices,MenuLayout::back(width,height),"BACK",hovered==58,width,height);
    drawMenuVertices(vertices.data(),vertices.size());
}

bool Renderer::validateTerrainPacking() const {return validateTerrainPackingGpu(WorldVertexShader,WorldFragmentShader,atlasTexture_);}

unsigned Renderer::occlusionCulledCount() const {return occlusion_->diagnostics().culled;}
unsigned Renderer::occlusionQueryCount() const {return occlusion_->diagnostics().queries;}

void Renderer::renderNavigation(int width,int height,const Transportation& travel,const Player& player,const Inventory& inventory,bool open,bool large,std::uint32_t viewed,const std::string& name,const std::string& coords,bool focus,const std::string& message) const {
    const auto held=inventory.selectedStack(),off=inventory.equipment(4);
    const bool compass=held.item==Item::Compass||off.item==Item::Compass;
    const auto* map=(held.item==Item::FilledMap||off.item==Item::FilledMap)?travel.map(viewed):nullptr;
    if(!open&&!compass&&!map&&!travel.mounted()&&travel.selectedWaypoint<0)return;
    thread_local std::vector<UiVertex> navigationVertices;
    RenderScratch<UiVertex> navigationScratch(navigationVertices,2097152);
    auto& v=navigationScratch.get();v.reserve(12000);
    auto rect=[&](float x,float y,float w,float h,glm::vec4 c){addRect(v,x,y,w,h,c,width,height);};
    auto text=[&](std::string s,float x,float y,float size=1.4f,glm::vec4 color=glm::vec4(1)){textGeometry_->appendRun(v,s,x,y,size,color,width,height);};
    auto needle=[&](float x,float y,float angle,float length,glm::vec4 color){for(int n=0;n<int(length);++n)rect(x+std::sin(angle)*n,y-std::cos(angle)*n,3,3,color);};
    if(travel.mounted()){rect(width*.5f-115,height-145,230,20,{0,0,0,.65f});text("BOAT  WASD / SNEAK TO EXIT",width*.5f-105,height-139,1.2f);}
    if(compass){rect(width-95,80,68,68,{.09f,.12f,.16f,.85f});text("FWD",width-72,82,1.1f);needle(width-61,114,travel.compassAngle,24,{.95f,.2f,.16f,1});needle(width-61,114,travel.compassAngle+3.14159f,19,{.8f,.83f,.9f,1});text("SPAWN",width-83,151,1.1f);}
    if(travel.selectedWaypoint>=0){const auto& wp=travel.waypoints()[travel.selectedWaypoint];const auto d=wp.position-player.position(),f=player.lookDirection();const float a=std::atan2(d.z,d.x)-std::atan2(f.z,f.x);
        rect(width*.5f-140,25,280,30,{0,0,0,.65f});text(wp.name+"  "+std::to_string(int(glm::length(glm::vec2(d.x,d.z))))+" BLOCKS",width*.5f-125,34,1.2f);needle(width*.5f+123,42,a,10,{1,.8f,.2f,1});}
    float mapX=16,mapY=float(height)-270,mapSize=200;
    if(large){mapSize=std::min(width*.6f,height*.58f);mapX=(width-mapSize)*.5f;mapY=(height-mapSize)*.5f-35;}
    if(open){
        rect(0,0,float(width),float(height),{0,0,0,.65f});const float x=width*.5f-300,y=height*.5f-245;
        rect(x,y,600,490,{.1f,.13f,.15f,.98f});text("NAVIGATION / WAYPOINTS",x+20,y+20,2);
        text("NAME: "+name+(!focus?"_":""),x+20,y+62,1.7f);text("X Y Z: "+coords+(focus?"_":""),x+20,y+88,1.5f);
        text("EMPTY COORDINATES = CURRENT POSITION",x+20,y+111,1.2f);
        text("TAB FIELD / ENTER ADD / CTRL+ENTER RENAME",x+20,y+141,1.15f);
        text("UP/DOWN SELECT / DELETE CONFIRM / HOME SPAWN / ESC",x+20,y+164,1.2f);
        const int start=std::max(0,travel.selectedWaypoint-4);int row=0;
        for(int i=start;i<int(travel.waypoints().size())&&row<8;++i,++row){const auto& wp=travel.waypoints()[i];const float yy=y+200+row*25;
            if(i==travel.selectedWaypoint)rect(x+14,yy-4,570,24,{.24f,.34f,.28f,1});
            text(wp.name+"  "+std::to_string(int(wp.position.x))+" "+std::to_string(int(wp.position.y))+" "+std::to_string(int(wp.position.z)),x+24,yy,1.35f);}
        const auto spawn=travel.originalSpawn();text("ORIGINAL SPAWN "+std::to_string(int(spawn.x))+" "+std::to_string(int(spawn.z)),x+20,y+420,1.25f);
        text(message,x+20,y+447,1.05f,{1,.8f,.45f,1});
    }else if(map){rect(mapX-8,mapY-8,mapSize+16,mapSize+16,{.7f,.58f,.35f,1});rect(mapX-8,mapY-(large?49.f:33.f),mapSize+16,large?39.f:23.f,{.04f,.06f,.07f,.9f});text("MAP #"+std::to_string(map->id)+"  1:"+std::to_string(1<<map->scale),mapX,mapY-27,1.4f);if(large){rect(mapX-8,mapY+mapSize+9,mapSize+16,42,{.04f,.06f,.07f,.9f});text("M CLOSE / SNEAK+RMB: ZOOM (-8 PAPER)",mapX,mapY+mapSize+20,1.1f);text(travel.refreshing(map->id)?"REFRESHING SURVEYED TERRAIN...":"F6: REFRESH SURVEYED TERRAIN (IF UNBOUND)",mapX,mapY+mapSize+38,1.1f);text(std::to_string(128<<map->scale)+" x "+std::to_string(128<<map->scale)+" BLOCKS / NORTH UP",mapX,mapY-43,1.1f);}}
    drawMenuVertices(v.data(),v.size());
    if(!map||open)return;
    if(!mapTexture_){glGenTextures(1,&mapTexture_);glBindTexture(GL_TEXTURE_2D,mapTexture_);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,mapTexture_);
    if(mapTextureId_!=map->id || mapTextureRevision_!=map->revision){
        int x0=128,y0=128,x1=-1,y1=-1;
        for(int i=0;i<16384;++i){const auto c=map->pixels[i]?map->pixels[i]:0xff24303bU;if(mapTextureId_!=map->id || mapPixels_[i]!=c){mapPixels_[i]=c;x0=std::min(x0,i%128);x1=std::max(x1,i%128);y0=std::min(y0,i/128);y1=std::max(y1,i/128);}}
        if(x1>=x0){glPixelStorei(GL_UNPACK_ROW_LENGTH,128);glTexSubImage2D(GL_TEXTURE_2D,0,x0,y0,x1-x0+1,y1-y0+1,GL_RGBA,GL_UNSIGNED_BYTE,mapPixels_.data()+y0*128+x0);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);}
        mapTextureId_=map->id;mapTextureRevision_=map->revision;
    }
    v.clear();auto ndc=[&](float x,float y){return glm::vec2(x/width*2-1,1-y/height*2);};const auto a=ndc(mapX,mapY),b=ndc(mapX+mapSize,mapY),c=ndc(mapX+mapSize,mapY+mapSize),d=ndc(mapX,mapY+mapSize);
    v={ {a,{1,1,1,1},{0,0},1},{b,{1,1,1,1},{1,0},1},{c,{1,1,1,1},{1,1},1},{a,{1,1,1,1},{0,0},1},{c,{1,1,1,1},{1,1},1},{d,{1,1,1,1},{0,1},1} };
    glUseProgram(uiProgram_);glBindBuffer(GL_ARRAY_BUFFER,uiVbo_);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(UiVertex),v.data(),GL_STREAM_DRAW);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glBindVertexArray(uiVao_);glDrawArrays(GL_TRIANGLES,0,6);glBindVertexArray(0);glEnable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);
    v.clear();const float span=float(128<<map->scale),px=(player.position().x-map->originX)/span,pz=(player.position().z-map->originZ)/span;
    if(px>=0&&px<1&&pz>=0&&pz<1){
        const float x=mapX+px*mapSize,y=mapY+pz*mapSize;
        rect(x-5,y-5,11,11,{.04f,.04f,.04f,1});rect(x-3,y-3,7,7,{1,1,1,1});
        const auto facing=player.lookDirection();const float heading=std::atan2(facing.x,-facing.z);
        const glm::vec2 direction(std::sin(heading),-std::cos(heading));
        // Two strokes replace 27 overlapping rectangles while retaining contrast.
        auto stroke=[&](float length,float thickness,glm::vec4 color){
            const glm::vec2 start(x,y),end=start+direction*length,side=glm::vec2(-direction.y,direction.x)*(thickness*.5f);
            const std::array<glm::vec2,4> points{start+side,start-side,end-side,end+side};
            for(int index:{0,1,2,0,2,3})v.push_back({ndc(points[index].x,points[index].y),color,{0,0},0});
        };
        stroke(15,5,{.03f,.03f,.03f,1});stroke(12,3,{1,.25f,.1f,1});
    }drawMenuVertices(v.data(),v.size());
}

void Renderer::renderBuildingOverlay(int width,int height,const World& world,const Player& player,const glm::mat4& view,const glm::mat4& projection,bool editor,const std::string& draft,const glm::ivec3& target,const SignEditor* editing) const {
 thread_local std::vector<UiVertex> data;data.clear();
 if(editor){
  const float x=width*.5f-230,y=height*.5f-125;addRect(data,x-24,y-16,508,250,{.12f,.075f,.04f,.96f},width,height);
  textGeometry_->append(data,"EDIT SIGN - 4 LINES / 24 CHARACTERS",x,y,1.5f,{.98f,.91f,.73f,1},width,height);
  const auto lo=editing?std::min(editing->caret,editing->anchor):draft.size(),hi=editing?std::max(editing->caret,editing->anchor):draft.size();
  int row=0,column=0;float caretX=x,caretY=y+36;
  for(std::size_t i=0;i<=draft.size();++i){if(editing&&i==editing->caret){caretX=x+column*12;caretY=y+36+row*24;}if(i==draft.size())break;
   if(i>=lo&&i<hi)addRect(data,x+column*12,y+34+row*24,draft[i]=='\n'?5.f:12.f,20,{.3f,.4f,.65f,.8f},width,height);
   if(draft[i]=='\n'){++row;column=0;continue;}
   textGeometry_->appendRun(data,std::string(1,draft[i]),x+column*12,y+36+row*24,2,{.98f,.91f,.73f,1},width,height);++column;
  }
  if(std::fmod(glfwGetTime(),1.)<.5)addRect(data,caretX,caretY-2,2,20,{1,1,.8f,1},width,height);
  textGeometry_->appendRun(data,"ENTER: NEW LINE / CTRL+ENTER: SAVE / ESC: CANCEL",x,y+141,1.2f,{.8f,.85f,.9f,1},width,height);
  textGeometry_->appendRun(data,"ARROWS / HOME / END / SHIFT SELECT / CTRL+A,C,V",x,y+158,1.1f,{.8f,.85f,.9f,1},width,height);
  for(bool save:{false,true}){auto r=UiLayout::signButton(save,width,height);addRect(data,r.x,r.y,r.width,r.height,{.28f,.34f,.24f,1},width,height);textGeometry_->appendRun(data,save?"SAVE":"CANCEL",r.x+24,r.y+10,1.4f,{1,1,1,1},width,height);}
 }
 else {
  thread_local std::vector<glm::ivec3> signs;world.nearbyBuildingSigns(player.cameraPosition(),signs);
  const auto vp=projection*view;
  for(auto p:signs){auto block=world.getBlock(p.x,p.y,p.z);auto props=blockProperties(block);auto normal=glm::vec3(buildingFacing(props));const auto center=glm::vec3(p)+glm::vec3(.5f,.7f,.5f);if(glm::dot(player.cameraPosition()-center,normal)<=0)continue;
   const auto delta=center-player.cameraPosition();RayHit hit;if(world.raycast(player.cameraPosition(),glm::normalize(delta),glm::length(delta)+.1f,hit)&&hit.block!=p)continue;
   const auto start=data.size();textGeometry_->append(data,world.buildingText(p),0,0,1,{.10f,.065f,.035f,1},144,40);bool visible=true;
   for(std::size_t i=start;i<data.size();++i){auto& v=data[i];float u=(v.position.x+1)*.5f,y=(1-v.position.y)*.5f;glm::vec3 point(.94f-u*.88f,.9f-y*.45f,props&16?.873f:.435f);for(int r=0;r<(props&3);++r){float x=point.x;point.x=1-point.z;point.z=x;}auto clip=vp*glm::vec4(glm::vec3(p)+point,1);if(clip.w<=0){visible=false;break;}v.position=glm::vec2(clip)/clip.w;}
   if(!visible)data.resize(start);
  }
 }
 if(data.empty())return;
 glUseProgram(uiProgram_);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,itemTexture_);glUniform1i(uiItemAtlasUniform_,0);glBindVertexArray(uiVao_);glBindBuffer(GL_ARRAY_BUFFER,uiVbo_);glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(UiVertex),data.data(),GL_STREAM_DRAW);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDrawArrays(GL_TRIANGLES,0,GLsizei(data.size()));glBindVertexArray(0);glEnable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);
}

void Renderer::renderPlacementPreview(const PlacementPlan& plan,const glm::mat4& view,const glm::mat4& projection)const {
 if(plan.blocks.empty())return;
 bool changed=plan.blocks.size()!=previewBlocks_.size();if(!changed)for(std::size_t i=0;i<plan.blocks.size();++i)if(plan.blocks[i].position!=previewBlocks_[i].position||plan.blocks[i].block!=previewBlocks_[i].block)changed=true;
 if(!previewVao_){glGenVertexArrays(1,&previewVao_);glGenBuffers(1,&previewVbo_);glBindVertexArray(previewVao_);glBindBuffer(GL_ARRAY_BUFFER,previewVbo_);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),nullptr);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),reinterpret_cast<void*>(3*sizeof(float)));changed=true;}
 if(changed){
  std::vector<float> data;data.reserve(2*24*36*5);
  auto vertex=[&](glm::vec3 p,glm::vec2 uv,int tile){uv=glm::clamp(uv,glm::vec2(.002f),glm::vec2(.998f));data.insert(data.end(),{p.x,p.y,p.z,(tile+uv.x)/BlockAtlasTiles,uv.y});};
  for(auto part:plan.blocks){
   if(isPlant(part.block)){
    const float height=(isCrop(part.block)||isBerryBush(part.block))?blockGeometry(part.block).maxY:.9f;
    const glm::vec3 origin(part.position);const glm::vec2 uvs[]={{0,1},{0,0},{1,0},{1,1}};
    for(int diagonal=0;diagonal<2;++diagonal){const float x0=diagonal?.92f:.08f,x1=diagonal?.08f:.92f;const glm::vec3 points[]={{x0,0,.08f},{x0,height,.08f},{x1,height,.92f},{x1,0,.92f}};for(int i:{0,1,2,0,2,3})vertex(origin+points[i],uvs[i],blockTexture(part.block,0));}
    continue;
   }
   auto boxes=buildingBoxes(part.block);const auto g=blockGeometry(part.block);if(!isBuilding(part.block))boxes.add(g.minX,g.minY,g.minZ,g.maxX,g.maxY,g.maxZ);
   for(int box=0;box<boxes.count;++box){const auto a=boxes.boxes[box].minimum,b=boxes.boxes[box].maximum;
    for(int face=0;face<6;++face){const int axis=face<2?0:face<4?1:2;const bool positive=face%2==0;const int A=(axis+1)%3,B=(axis+2)%3;
     std::array<glm::vec3,4> points;for(auto& p:points)p[axis]=positive?b[axis]:a[axis];points[0][A]=points[3][A]=a[A];points[1][A]=points[2][A]=b[A];points[0][B]=points[1][B]=a[B];points[2][B]=points[3][B]=b[B];if(!positive)std::swap(points[1],points[3]);
     for(int i:{0,1,2,0,2,3}){const auto p=points[i];glm::vec2 uv(axis==0?p.z:p.x,axis==1?p.z:1-p.y);if(blockType(part.block)==Block::Bed&&face==2)for(int r=0;r<int(blockProperties(part.block)&3);++r)uv={uv.y,1-uv.x};if(blockType(part.block)==Block::Painting&&(blockProperties(part.block)&32)){if((blockProperties(part.block)&3)>=2)uv.x=1-uv.x;uv.x=(uv.x+((blockProperties(part.block)&16)?1.f:0.f))*.5f;}vertex(glm::vec3(part.position)+p,uv,blockTexture(part.block,face));}
    }
   }
  }
  previewBlocks_=plan.blocks;previewVertexCount_=int(data.size()/5);glBindBuffer(GL_ARRAY_BUFFER,previewVbo_);glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(float),data.data(),GL_DYNAMIC_DRAW);
 }
 glUseProgram(previewProgram_);glUniformMatrix4fv(previewUniforms_.view,1,GL_FALSE,glm::value_ptr(view));glUniformMatrix4fv(previewUniforms_.projection,1,GL_FALSE,glm::value_ptr(projection));glUniform3f(previewUniforms_.color,plan.valid?.6f:1.f,plan.valid?1.f:.25f,plan.valid?.6f:.25f);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlasTexture_);glUniform1i(previewUniforms_.itemAtlas,0);glBindVertexArray(previewVao_);glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-1,-1);glDrawArrays(GL_TRIANGLES,0,previewVertexCount_);glDisable(GL_POLYGON_OFFSET_FILL);glDisable(GL_BLEND);glDepthMask(GL_TRUE);glEnable(GL_CULL_FACE);glBindVertexArray(0);
}
void Renderer::renderQolFeedback(int width,int height,const std::string& hint,const std::string& save,bool pause)const {
 thread_local std::vector<UiVertex> v;v.clear();
 if(!hint.empty()){const float scale=1.25f,w=std::min(float(width)-16,float(hint.size())*6*scale+16),x=(width-w)*.5f,y=height*.5f+36;addRect(v,x,y,w,24,{0,0,0,.65f},width,height);textGeometry_->appendRun(v,hint,x+8,y+8,scale,{.93f,.95f,.85f,1},width,height);}
 if(!save.empty()){const float w=std::min(float(width)-16,float(save.size())*6+16),x=pause?(width-w)*.5f:width-w-10,y=pause?height*.5f+244:height-18.f;addRect(v,x,y,w,18,{0,0,0,.72f},width,height);textGeometry_->appendRun(v,save,x+8,y+5,1,{.85f,.93f,.85f,1},width,height);}
 if(!v.empty())drawMenuVertices(v.data(),v.size());
}
