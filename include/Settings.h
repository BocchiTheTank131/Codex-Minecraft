#pragma once
#include "InputManager.h"

#include <string>
#include <vector>

enum class GraphicsPreset : int { Low = 0, Medium = 1, High = 2, Custom = 3 };

struct GameSettings {
    int uiScale=0; // Auto / Small / Normal / Large
    bool interactionHints=true,placementPreview=true;
    std::vector<int> creativeFavorites,creativeRecent;
    int terrainLod=1; // Off / Low / Medium / High (explicit far-detail strength)
    int occlusionCulling=1; // Off / Auto / On
    int terrainMemoryBudget=0; // Auto / Low / Balanced / High
    int chunkStreamingQuality = 0; // Auto / Low / Balanced / High
    int renderDistance = 8;
    int simulationDistance = 8;
    float fov = 74.0f;
    float mouseSensitivity = 0.10f;
    float brightness = 0.5f;
    float masterVolume = 1.0f;
    float musicVolume = 0.5f;
    float sfxVolume = 1.0f;
    float passiveMobVolume = 1.0f;
    float hostileMobVolume = 1.0f;
    bool weatherCycle = true, snowAccumulation = true, lightningEffects = true;
    bool weatherFog = true, weatherWind = true;
    int weatherQuality = 2;
    float precipitationDensity = 1.0f, lightningFlash = .65f;
    int antiAliasingSamples = 0;
    GraphicsPreset graphicsPreset = GraphicsPreset::Custom;
    int entityDistance = 12;
    int particlePercent = 100;
    int effectQuality = 2;
    int frameLimit = 0;
    bool fullscreen = false;
    bool vsync = true;
    bool showFps = true;
    bool showCoordinates = true;
    ControlBindings controls = defaultControlBindings();

    bool bindControl(ControlAction action, int key);

    bool load(const std::string& path);
    bool save(const std::string& path) const;
    void clamp();
    void applyPreset(GraphicsPreset preset);
};
