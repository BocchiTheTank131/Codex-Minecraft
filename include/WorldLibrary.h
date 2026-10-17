#pragma once
#include "GameMode.h"
#include "WorldTime.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct SavedWorld {
    std::filesystem::path directory;
    std::string name;
    std::uint32_t seed = 20260917;
    GameMode mode = GameMode::Survival;
    float time = WorldTime::DefaultSeconds;
    std::int64_t lastPlayed = 0;
    bool cheats = true; // Missing legacy field preserves previous command access.
    std::string loadError; // Unavailable worlds remain visible without permitting writes.
};

// World storage only; settings and the working directory remain global.
namespace WorldLibrary {
std::vector<SavedWorld> list();
// Caller holds a SaveFile::Session for this directory.
SavedWorld inspectReserved(const std::filesystem::path& directory);
SavedWorld create(const std::string& name, std::uint32_t seed, GameMode mode, bool cheats = false);
void importLegacy();
bool writeMetadata(const SavedWorld& world);
SavedWorld recoverPrevious(const SavedWorld& world);
}
