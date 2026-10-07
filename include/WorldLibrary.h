#pragma once
#include "GameMode.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct SavedWorld {
    std::filesystem::path directory;
    std::string name;
    std::uint32_t seed = 20260917;
    GameMode mode = GameMode::Survival;
    float time = 35.0f;
    std::int64_t lastPlayed = 0;
};

// World storage only; settings and the working directory remain global.
namespace WorldLibrary {
std::vector<SavedWorld> list();
SavedWorld create(const std::string& name, std::uint32_t seed, GameMode mode);
void importLegacy();
bool writeMetadata(const SavedWorld& world);
}
