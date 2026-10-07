#include "WorldLibrary.h"
#include "Persistence.h"
#include "SaveFile.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <random>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
const std::array<const char*, 5> Files{
    "voxel_world.vxw", "voxel_inventory.vxi", "voxel_player.vps", "voxel_mobs.vxm", "world_seed.txt"};
SavedWorld read(const fs::path& directory) {
    SavedWorld result;
    result.directory = directory;
    result.name = directory.filename().string();
    std::ifstream info(directory / "world.info");
    std::string name;
    if (info >> std::quoted(name) && !name.empty()) result.name = name.substr(0, 48);
    info >> result.lastPlayed;
    std::ifstream seedFile(directory / "world_seed.txt");
    std::string mode;
    seedFile >> result.seed >> mode >> result.time;
    result.mode = mode == "creative" ? GameMode::Creative :
                  mode == "spectator" ? GameMode::Spectator : GameMode::Survival;
    if (!std::isfinite(result.time) || result.time < 0) result.time = 35.0f;
    result.time = std::fmod(result.time, 420.0f);
    return result;
}
bool equalFiles(const fs::path& a, const fs::path& b) {
    if (fs::file_size(a) != fs::file_size(b)) return false;
    std::ifstream first(a, std::ios::binary), second(b, std::ios::binary);
    return std::equal(std::istreambuf_iterator<char>(first), {},
                      std::istreambuf_iterator<char>(second), {});
}
}

namespace WorldLibrary {
bool writeMetadata(const SavedWorld& world) {
    if (!Persistence::enabled()) return false;
    return SaveFile::write((world.directory / "world.info").string(), std::ios::out,
        [&](std::ofstream& output) {
            output << std::quoted(world.name) << '\n' << world.lastPlayed << '\n';
            return static_cast<bool>(output);
        });
}

std::vector<SavedWorld> list() {
    std::vector<SavedWorld> worlds;
    if (!Persistence::enabled() || !fs::exists("saves")) return worlds;
    for (const auto& entry : fs::directory_iterator("saves")) {
        if (entry.is_symlink() || !entry.is_directory() || entry.path().filename().string()[0] == '.') continue;
        if (fs::is_regular_file(entry.path() / "world.info")) worlds.push_back(read(entry.path()));
    }
    std::sort(worlds.begin(), worlds.end(), [](const auto& a, const auto& b) {
        if (a.lastPlayed != b.lastPlayed) return a.lastPlayed > b.lastPlayed;
        return a.directory < b.directory;
    });
    return worlds;
}

SavedWorld create(const std::string& name, std::uint32_t seed, GameMode mode) {
    if (!Persistence::enabled()) throw std::runtime_error("Demo cannot create saved worlds");
    if (name.empty() || name.size() > 48 || name.find_first_not_of(' ') == std::string::npos)
        throw std::runtime_error("Enter a world name (1-48 characters)");
    fs::create_directories("saves");
    SavedWorld result;
    result.name = name;
    result.seed = seed;
    result.mode = mode;
    // The visible name never becomes a path: slashes, reserved names and duplicates are safe.
    std::random_device random;
    for (int attempt = 0; attempt < 32; ++attempt) {
        result.directory = fs::path("saves") / ("world-" + std::to_string(random()) + "-" + std::to_string(random()));
        if (!fs::create_directory(result.directory)) continue;
        if (!writeMetadata(result)) throw std::runtime_error("Cannot write world metadata");
        const auto path = (result.directory / "world_seed.txt").string();
        if (!SaveFile::write(path, std::ios::out, [&](std::ofstream& output) {
            output << seed << '\n' << (mode == GameMode::Creative ? "creative" : "survival") << "\n35\n";
            return static_cast<bool>(output);
        })) throw std::runtime_error("Cannot write world seed");
        return result;
    }
    throw std::runtime_error("Cannot create a unique world folder");
}

void importLegacy() {
    if (!Persistence::enabled() || !fs::is_regular_file("voxel_world.vxw")) return;
    fs::create_directories("saves");
    const fs::path destination = fs::path("saves") / "legacy-world";
    if (fs::is_regular_file(destination / "world.info")) return;
    if (fs::exists(destination)) throw std::runtime_error("Legacy import folder already exists; original saves preserved");
    const fs::path staging = fs::path("saves") / ".import-legacy";
    fs::create_directories(staging);
    for (const char* name : Files) {
        if (!fs::is_regular_file(name)) continue;
        fs::copy_file(name, staging / name, fs::copy_options::overwrite_existing);
        if (!equalFiles(name, staging / name)) throw std::runtime_error("Legacy save copy failed verification");
    }
    auto imported = read(staging);
    imported.name = "Imported World";
    if (!fs::exists(staging / "world_seed.txt")) {
        std::ifstream world("voxel_world.vxw", std::ios::binary);
        world.seekg(12);
        world.read(reinterpret_cast<char*>(&imported.seed), sizeof(imported.seed));
        if (!world) throw std::runtime_error("Cannot read legacy world seed");
        std::ofstream seed(staging / "world_seed.txt");
        seed << imported.seed << "\nsurvival\n35\n";
        if (!seed) throw std::runtime_error("Cannot write imported seed");
    }
    if (!writeMetadata(imported)) throw std::runtime_error("Cannot write import metadata");
    fs::rename(staging, destination); // Publish only after all copies succeeded. Originals remain intact.
}
}
