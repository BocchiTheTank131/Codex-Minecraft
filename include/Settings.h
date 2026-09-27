#pragma once
#include "InputManager.h"

#include <string>

enum class GraphicsPreset : int { Low = 0, Medium = 1, High = 2, Custom = 3 };

struct GameSettings {
    int renderDistance = 7;
    int simulationDistance = 7;
    float fov = 74.0f;
    float mouseSensitivity = 0.10f;
    float brightness = 0.5f;
    float masterVolume = 1.0f;
    float musicVolume = 1.0f;
    float sfxVolume = 1.0f;
    float passiveMobVolume = 1.0f;
    float hostileMobVolume = 1.0f;
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
