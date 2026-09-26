#pragma once

#include <string>

struct GameSettings {
    int renderDistance = 7;
    float fov = 74.0f;
    float mouseSensitivity = 0.10f;
    float masterVolume = 1.0f;
    bool fullscreen = false;
    bool vsync = true;
    bool showFps = true;
    bool showCoordinates = true;

    bool load(const std::string& path);
    bool save(const std::string& path) const;
    void clamp();
};
