#include "Settings.h"
#include "SaveFile.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <GLFW/glfw3.h>

namespace {
bool reservedControlKey(int key) {
    return key == GLFW_KEY_ESCAPE ||
           (key >= GLFW_KEY_1 && key <= GLFW_KEY_9);
}
}

bool GameSettings::bindControl(ControlAction action, int key) {
    const int index = static_cast<int>(action);
    if (index < 0 || index >= ControlActionCount || key < 0 ||
        key > GLFW_KEY_LAST || reservedControlKey(key))
        return false;
    const int previous = controls[static_cast<std::size_t>(index)];
    for (int other = 0; other < ControlActionCount; ++other) {
        if (other != index && controls[static_cast<std::size_t>(other)] == key) {
            controls[static_cast<std::size_t>(other)] = previous;
            break;
        }
    }
    controls[static_cast<std::size_t>(index)] = key;
    return true;
}

void GameSettings::clamp() {
    renderDistance = std::clamp(renderDistance, 2, 64);
    simulationDistance = std::clamp(simulationDistance, 2, 32);
    fov = std::clamp(fov, 55.0f, 105.0f);
    mouseSensitivity = std::clamp(mouseSensitivity, 0.03f, 0.30f);
    masterVolume = std::clamp(masterVolume, 0.0f, 1.0f);
    if (antiAliasingSamples != 2 && antiAliasingSamples != 4)
        antiAliasingSamples = 0;
    entityDistance = std::clamp(entityDistance, 2, 64);
    particlePercent = std::clamp(particlePercent, 0, 100);
    effectQuality = std::clamp(effectQuality, 0, 2);
    if (frameLimit != 30 && frameLimit != 60 && frameLimit != 120)
        frameLimit = 0;
    if (static_cast<int>(graphicsPreset) < 0 || static_cast<int>(graphicsPreset) > 3)
        graphicsPreset = GraphicsPreset::Custom;
    std::array<bool, GLFW_KEY_LAST + 1> used{};
    const ControlBindings defaults = defaultControlBindings();
    for (int index = 0; index < ControlActionCount; ++index) {
        int& key = controls[static_cast<std::size_t>(index)];
        if (key < 0 || key > GLFW_KEY_LAST || reservedControlKey(key) ||
            used[static_cast<std::size_t>(key)]) {
            key = defaults[static_cast<std::size_t>(index)];
            if (used[static_cast<std::size_t>(key)]) {
                for (int candidate = GLFW_KEY_A; candidate <= GLFW_KEY_Z; ++candidate) {
                    if (!used[static_cast<std::size_t>(candidate)]) {
                        key = candidate;
                        break;
                    }
                }
            }
        }
        used[static_cast<std::size_t>(key)] = true;
    }
}

void GameSettings::applyPreset(GraphicsPreset preset) {
    graphicsPreset = preset;
    switch (preset) {
    case GraphicsPreset::Low:
        renderDistance = 7;
        simulationDistance = 5;
        antiAliasingSamples = 0;
        entityDistance = 6;
        particlePercent = 35;
        effectQuality = 0;
        break;
    case GraphicsPreset::Medium:
        renderDistance = 12;
        simulationDistance = 10;
        antiAliasingSamples = 2;
        entityDistance = 12;
        particlePercent = 70;
        effectQuality = 1;
        break;
    case GraphicsPreset::High:
        renderDistance = 24;
        simulationDistance = 16;
        antiAliasingSamples = 4;
        entityDistance = 24;
        particlePercent = 100;
        effectQuality = 2;
        break;
    case GraphicsPreset::Custom:
        break;
    }
    clamp();
}

bool GameSettings::load(const std::string& path) {
    std::ifstream input(path);
    if (!input)
        return false;
    std::string line;
    while (std::getline(input, line)) {
        const auto split = line.find('=');
        if (split == std::string::npos)
            continue;
        const std::string key = line.substr(0, split);
        const std::string value = line.substr(split + 1);
        try {
            if (key == "render_distance")
                renderDistance = std::stoi(value);
            else if (key == "simulation_distance")
                simulationDistance = std::stoi(value);
            else if (key == "fov")
                fov = std::stof(value);
            else if (key == "mouse_sensitivity")
                mouseSensitivity = std::stof(value);
            else if (key == "master_volume")
                masterVolume = std::stof(value);
            else if (key == "anti_aliasing_samples")
                antiAliasingSamples = std::stoi(value);
            else if (key == "graphics_preset")
                graphicsPreset = static_cast<GraphicsPreset>(std::stoi(value));
            else if (key == "entity_distance")
                entityDistance = std::stoi(value);
            else if (key == "particle_percent")
                particlePercent = std::stoi(value);
            else if (key == "effect_quality")
                effectQuality = std::stoi(value);
            else if (key == "frame_limit")
                frameLimit = std::stoi(value);
            else if (key == "fullscreen")
                fullscreen = std::stoi(value) != 0;
            else if (key == "vsync")
                vsync = std::stoi(value) != 0;
            else if (key == "show_fps")
                showFps = std::stoi(value) != 0;
            else if (key == "show_coordinates")
                showCoordinates = std::stoi(value) != 0;
            else if (key.rfind("key_", 0) == 0) {
                const int index = std::stoi(key.substr(4));
                if (index >= 0 && index < ControlActionCount)
                    controls[static_cast<std::size_t>(index)] = std::stoi(value);
            }
        } catch (...) {
        }
    }
    clamp();
    return true;
}

bool GameSettings::save(const std::string& path) const {
    return SaveFile::write(path, std::ios::out, [&](std::ofstream& output) {
    output << "render_distance=" << renderDistance << '\n'
           << "simulation_distance=" << simulationDistance << '\n'
           << "fov=" << fov << '\n'
           << "mouse_sensitivity=" << mouseSensitivity << '\n'
           << "master_volume=" << masterVolume << '\n'
           << "anti_aliasing_samples=" << antiAliasingSamples << '\n'
           << "graphics_preset=" << static_cast<int>(graphicsPreset) << '\n'
           << "entity_distance=" << entityDistance << '\n'
           << "particle_percent=" << particlePercent << '\n'
           << "effect_quality=" << effectQuality << '\n'
           << "frame_limit=" << frameLimit << '\n'
           << "fullscreen=" << fullscreen << '\n'
           << "vsync=" << vsync << '\n'
           << "show_fps=" << showFps << '\n'
           << "show_coordinates=" << showCoordinates << '\n';
    for (int index = 0; index < ControlActionCount; ++index)
        output << "key_" << index << '=' << controls[static_cast<std::size_t>(index)] << '\n';
    return !!output;
    });
}
