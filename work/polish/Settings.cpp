#include "Settings.h"

#include <algorithm>
#include <fstream>
#include <sstream>

void GameSettings::clamp() {
    renderDistance = std::clamp(renderDistance, 2, 16);
    fov = std::clamp(fov, 55.0f, 105.0f);
    mouseSensitivity = std::clamp(mouseSensitivity, 0.03f, 0.30f);
    masterVolume = std::clamp(masterVolume, 0.0f, 1.0f);
}

bool GameSettings::load(const std::string& path) {
    std::ifstream input(path);
    if (!input) return false;
    std::string line;
    while (std::getline(input, line)) {
        const auto split = line.find('=');
        if (split == std::string::npos) continue;
        const std::string key = line.substr(0, split);
        const std::string value = line.substr(split + 1);
        try {
            if (key == "render_distance") renderDistance = std::stoi(value);
            else if (key == "fov") fov = std::stof(value);
            else if (key == "mouse_sensitivity") mouseSensitivity = std::stof(value);
            else if (key == "master_volume") masterVolume = std::stof(value);
            else if (key == "fullscreen") fullscreen = std::stoi(value) != 0;
            else if (key == "vsync") vsync = std::stoi(value) != 0;
            else if (key == "show_fps") showFps = std::stoi(value) != 0;
            else if (key == "show_coordinates") showCoordinates = std::stoi(value) != 0;
        } catch (...) {}
    }
    clamp();
    return true;
}

bool GameSettings::save(const std::string& path) const {
    std::ofstream output(path, std::ios::trunc);
    if (!output) return false;
    output << "render_distance=" << renderDistance << '\n'
           << "fov=" << fov << '\n'
           << "mouse_sensitivity=" << mouseSensitivity << '\n'
           << "master_volume=" << masterVolume << '\n'
           << "fullscreen=" << fullscreen << '\n'
           << "vsync=" << vsync << '\n'
           << "show_fps=" << showFps << '\n'
           << "show_coordinates=" << showCoordinates << '\n';
    return !!output;
}
