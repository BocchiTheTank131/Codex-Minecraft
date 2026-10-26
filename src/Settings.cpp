#include "Settings.h"
#include "SaveFile.h"
#include "Survival.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <GLFW/glfw3.h>

namespace {
float parseFloat(const std::string& value){std::size_t used=0;const float result=std::stof(value,&used);if(value.find_first_not_of(" \t\r",used)!=std::string::npos)throw std::invalid_argument("float setting");return result;}
int parseInt(const std::string& value){std::size_t used=0;const int result=std::stoi(value,&used);if(value.find_first_not_of(" \t\r",used)!=std::string::npos)throw std::invalid_argument("integer setting");return result;}
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
    uiScale=std::clamp(uiScale,0,3);
    for(auto* list:{&creativeFavorites,&creativeRecent}){
        std::vector<int> valid;for(auto id:*list)if(id>0&&id<int(Item::Count)&&std::find(valid.begin(),valid.end(),id)==valid.end())valid.push_back(id);
        if(list==&creativeRecent&&valid.size()>24)valid.resize(24);*list=std::move(valid);
    }
    terrainLod=std::clamp(terrainLod,0,3);
    occlusionCulling=std::clamp(occlusionCulling,0,2);
    terrainMemoryBudget=std::clamp(terrainMemoryBudget,0,3);
    chunkStreamingQuality=std::clamp(chunkStreamingQuality,0,3);
    weatherQuality = std::clamp(weatherQuality,0,3);
    precipitationDensity = std::isfinite(precipitationDensity) ? std::clamp(precipitationDensity,0.0f,1.0f) : 1.0f;
    lightningFlash = std::isfinite(lightningFlash) ? std::clamp(lightningFlash,0.0f,1.0f) : .65f;
    renderDistance = std::clamp(renderDistance, 2, 64);
    simulationDistance = std::clamp(simulationDistance, 2, 32);
    fov = std::isfinite(fov) ? std::clamp(fov, 55.f, 105.f) : 74.f;
    mouseSensitivity = std::isfinite(mouseSensitivity) ? std::clamp(mouseSensitivity, .03f, .30f) : .1f;
    brightness = std::isfinite(brightness) ? std::clamp(brightness, 0.f, 1.f) : .5f;
    masterVolume = std::isfinite(masterVolume) ? std::clamp(masterVolume, 0.f, 1.f) : 1.f;
    musicVolume = std::isfinite(musicVolume) ? std::clamp(musicVolume, 0.f, 1.f) : .5f;
    sfxVolume = std::isfinite(sfxVolume) ? std::clamp(sfxVolume, 0.f, 1.f) : 1.f;
    passiveMobVolume = std::isfinite(passiveMobVolume) ? std::clamp(passiveMobVolume, 0.f, 1.f) : 1.f;
    hostileMobVolume = std::isfinite(hostileMobVolume) ? std::clamp(hostileMobVolume, 0.f, 1.f) : 1.f;
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
        weatherQuality = 1;
        break;
    case GraphicsPreset::Medium:
        renderDistance = 12;
        simulationDistance = 10;
        antiAliasingSamples = 2;
        entityDistance = 12;
        particlePercent = 70;
        effectQuality = 1;
        weatherQuality = 2;
        break;
    case GraphicsPreset::High:
        renderDistance = 24;
        simulationDistance = 16;
        antiAliasingSamples = 4;
        entityDistance = 24;
        particlePercent = 100;
        effectQuality = 2;
        weatherQuality = 3;
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
            if(key=="terrain_lod") terrainLod=parseInt(value);
            else if(key=="occlusion_culling") occlusionCulling=parseInt(value);
            else if(key=="terrain_memory_budget") terrainMemoryBudget=parseInt(value);
            else if (key == "weather_cycle") weatherCycle = std::stoi(value)!=0;
            else if (key == "chunk_streaming_quality") chunkStreamingQuality = parseInt(value);
            else if (key == "weather_quality") weatherQuality = parseInt(value);
            else if (key == "precipitation_density") precipitationDensity = parseFloat(value);
            else if (key == "snow_accumulation") snowAccumulation = std::stoi(value)!=0;
            else if (key == "lightning_effects") lightningEffects = std::stoi(value)!=0;
            else if (key == "lightning_flash") lightningFlash = parseFloat(value);
            else if (key == "weather_fog") weatherFog = std::stoi(value)!=0;
            else if (key == "weather_wind") weatherWind = std::stoi(value)!=0;
            else if (key == "render_distance")
                renderDistance = parseInt(value);
            else if (key == "simulation_distance")
                simulationDistance = parseInt(value);
            else if (key == "fov")
                fov = parseFloat(value);
            else if (key == "mouse_sensitivity")
                mouseSensitivity = parseFloat(value);
            else if (key == "brightness")
                brightness = parseFloat(value);
            else if (key == "master_volume")
                masterVolume = parseFloat(value);
            else if (key == "music_volume")
                musicVolume = parseFloat(value);
            else if (key == "sfx_volume")
                sfxVolume = parseFloat(value);
            else if (key == "passive_mob_volume")
                passiveMobVolume = parseFloat(value);
            else if (key == "hostile_mob_volume")
                hostileMobVolume = parseFloat(value);
            else if (key == "anti_aliasing_samples")
                antiAliasingSamples = parseInt(value);
            else if (key == "graphics_preset")
                graphicsPreset = static_cast<GraphicsPreset>(std::stoi(value));
            else if (key == "entity_distance")
                entityDistance = parseInt(value);
            else if (key == "particle_percent")
                particlePercent = parseInt(value);
            else if (key == "effect_quality")
                effectQuality = parseInt(value);
            else if (key == "frame_limit")
                frameLimit = parseInt(value);
            else if (key == "fullscreen")
                fullscreen = std::stoi(value) != 0;
            else if (key == "vsync")
                vsync = std::stoi(value) != 0;
            else if(key=="ui_scale")uiScale=parseInt(value);
            else if(key=="interaction_hints")interactionHints=parseInt(value)!=0;
            else if(key=="placement_preview")placementPreview=parseInt(value)!=0;
            else if(key=="creative_favorites"||key=="creative_recent"){
                auto& list=key=="creative_favorites"?creativeFavorites:creativeRecent;list.clear();std::istringstream ids(value);std::string id;
                while(std::getline(ids,id,',')&&list.size()<std::size_t(Item::Count))if(!id.empty())list.push_back(parseInt(id));
            }
            else if (key == "show_fps")
                showFps = std::stoi(value) != 0;
            else if (key == "show_coordinates")
                showCoordinates = std::stoi(value) != 0;
            else if (key.rfind("key_", 0) == 0) {
                const int index = std::stoi(key.substr(4));
                if (index >= 0 && index < ControlActionCount)
                    controls[static_cast<std::size_t>(index)] = parseInt(value);
            }
        } catch (...) {
        }
    }
    clamp();
    return true;
}

bool GameSettings::save(const std::string& path) const {
    return SaveFile::write(path, std::ios::out, [&](std::ofstream& output) {
    output << "terrain_lod=" << terrainLod << '\n'
           << "occlusion_culling=" << occlusionCulling << '\n'
           << "terrain_memory_budget=" << terrainMemoryBudget << '\n'
           << "weather_cycle=" << weatherCycle << '\n'
           << "chunk_streaming_quality=" << chunkStreamingQuality << '\n'
           << "weather_quality=" << weatherQuality << '\n'
           << "precipitation_density=" << precipitationDensity << '\n'
           << "snow_accumulation=" << snowAccumulation << '\n'
           << "lightning_effects=" << lightningEffects << '\n'
           << "lightning_flash=" << lightningFlash << '\n'
           << "weather_fog=" << weatherFog << '\n'
           << "weather_wind=" << weatherWind << '\n'
           << "render_distance=" << renderDistance << '\n'
           << "simulation_distance=" << simulationDistance << '\n'
           << "fov=" << fov << '\n'
           << "mouse_sensitivity=" << mouseSensitivity << '\n'
           << "brightness=" << brightness << '\n'
           << "master_volume=" << masterVolume << '\n'
           << "music_volume=" << musicVolume << '\n'
           << "sfx_volume=" << sfxVolume << '\n'
           << "passive_mob_volume=" << passiveMobVolume << '\n'
           << "hostile_mob_volume=" << hostileMobVolume << '\n'
           << "anti_aliasing_samples=" << antiAliasingSamples << '\n'
           << "graphics_preset=" << static_cast<int>(graphicsPreset) << '\n'
           << "entity_distance=" << entityDistance << '\n'
           << "particle_percent=" << particlePercent << '\n'
           << "effect_quality=" << effectQuality << '\n'
           << "frame_limit=" << frameLimit << '\n'
           << "fullscreen=" << fullscreen << '\n'
           << "vsync=" << vsync << '\n'
           << "ui_scale=" << uiScale << '\n' << "interaction_hints=" << interactionHints << '\n' << "placement_preview=" << placementPreview << '\n'
           << "show_fps=" << showFps << '\n'
           << "show_coordinates=" << showCoordinates << '\n';
    for (int index = 0; index < ControlActionCount; ++index)
        output << "key_" << index << '=' << controls[static_cast<std::size_t>(index)] << '\n';
    output<<"creative_favorites=";for(auto id:creativeFavorites)output<<id<<',';output<<'\n';
    output<<"creative_recent=";for(auto id:creativeRecent)output<<id<<',';output<<'\n';
    return !!output;
    });
}
