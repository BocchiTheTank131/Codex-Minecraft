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
#include <iostream>
#include <random>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
const std::array<const char*, 7> Files{
    "voxel_world.vxw", "voxel_inventory.vxi", "voxel_player.vps", "voxel_mobs.vxm", "world_seed.txt", "voxel_travel.vtn", "checkpoint.manifest"};
SavedWorld read(const fs::path& directory,bool reserved=false) {
    std::unique_ptr<SaveFile::Session> session;
    if(!reserved)session=std::make_unique<SaveFile::Session>(directory.string());
    if(session && !session->acquired())throw std::runtime_error("World in use or access denied: "+directory.string());
    if(!SaveFile::recover((directory/"save.checkpoint").string()))throw std::runtime_error("Interrupted world checkpoint needs recovery: "+directory.string());
    std::string reason;
    if(!SaveFile::verifyCheckpoint(directory.string(),&reason))throw std::runtime_error("Invalid checkpoint: "+reason+"; recover a verified copy: "+directory.string());
    for(const char* filename:{"world.info","world_seed.txt"})if(fs::exists(directory/filename) && fs::file_size(directory/filename)>4096)throw std::runtime_error("Oversized world metadata: "+directory.string());
    SavedWorld result;
    result.directory = directory;
    result.name = directory.filename().string();
    std::ifstream info(directory / "world.info");
    std::string name;
    if(fs::exists(directory/"world.info") && (!(info>>std::quoted(name)>>result.lastPlayed)||name.empty()||name.size()>48||result.lastPlayed<0))throw std::runtime_error("Invalid world metadata: "+directory.string());
    if(!name.empty())result.name=name;
    std::string permission; int enabled=1;
    if(info){
        info>>std::ws;
        if(!info.eof()){
            if(!(info>>permission>>enabled) || permission!="CHEATS1" || (enabled!=0 && enabled!=1))throw std::runtime_error("Invalid command permissions: "+directory.string());
            result.cheats=enabled!=0;info>>std::ws;
            if(!info.eof())throw std::runtime_error("Invalid world metadata length: "+directory.string());
        }
    }
    std::ifstream seedFile(directory / "world_seed.txt");
    std::string mode;
    float storedTime = 35.f;
    if(fs::exists(directory/"world_seed.txt")){
        std::uint64_t seed=0;
        if(!(seedFile>>seed>>mode>>storedTime)||seed>UINT32_MAX || (mode!="creative" && mode!="spectator" && mode!="survival") || !std::isfinite(storedTime)||storedTime<0)throw std::runtime_error("Invalid world seed/time metadata: "+directory.string());
        result.seed=std::uint32_t(seed);
    }
    seedFile.clear();
    result.time = WorldTime::restore(storedTime, seedFile);
    result.mode = mode == "creative" ? GameMode::Creative :
                  mode == "spectator" ? GameMode::Spectator : GameMode::Survival;

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
SavedWorld inspectReserved(const fs::path& directory){return read(directory,true);}
bool writeMetadata(const SavedWorld& world) {
    if (!Persistence::enabled()) return false;
    if(!SaveFile::stagingCheckpoint()){
        SaveFile::Session session(world.directory.string());if(!session.acquired())return false;
        SaveFile::Checkpoint checkpoint((world.directory/"save.checkpoint").string());
        return checkpoint.ready()&&writeMetadata(world)&&checkpoint.commit();
    }
    return SaveFile::write((world.directory / "world.info").string(), std::ios::out,
        [&](std::ofstream& output) {
            output << std::quoted(world.name) << '\n' << world.lastPlayed << '\n' << "CHEATS1 " << (world.cheats?1:0) << '\n';
            return static_cast<bool>(output);
        });
}

bool setCheats(const SavedWorld& selected,bool enabled) {
    if(!Persistence::enabled() || !selected.loadError.empty())return false;
    try {
        SaveFile::Session session(selected.directory.string());if(!session.acquired())return false;
        auto current=read(selected.directory,true);
        current.cheats=enabled;
        SaveFile::Checkpoint checkpoint((current.directory/"save.checkpoint").string());
        return checkpoint.ready() && writeMetadata(current) && checkpoint.commit();
    } catch(const std::exception& error){std::cerr<<"Cheat permission transaction refused: "<<error.what()<<'\n';return false;}
}

std::vector<SavedWorld> list() {
    std::vector<SavedWorld> worlds;
    if (!Persistence::enabled() || !fs::exists("saves")) return worlds;
    for (const auto& entry : fs::directory_iterator("saves")) {
        try {
            if (entry.is_symlink() || !entry.is_directory() || entry.path().filename().string()[0] == '.') continue;
            if (fs::is_regular_file(entry.path() / "world.info"))worlds.push_back(read(entry.path()));
        } catch(const std::exception& error) {
            SavedWorld unavailable;unavailable.directory=entry.path();
            unavailable.name=entry.path().filename().string()+" [Unavailable]";
            unavailable.loadError=error.what();worlds.push_back(std::move(unavailable));
        }
    }
    std::sort(worlds.begin(), worlds.end(), [](const auto& a, const auto& b) {
        if (a.lastPlayed != b.lastPlayed) return a.lastPlayed > b.lastPlayed;
        return a.directory < b.directory;
    });
    return worlds;
}
SavedWorld recoverPrevious(const SavedWorld& world){
    SaveFile::Session session(world.directory.string());
    if(!session.acquired())throw std::runtime_error("Cannot recover an open world");
    const auto destination=world.directory.parent_path()/("world-recovered-"+std::to_string(std::random_device{}()));
    if(!SaveFile::copyPrevious(world.directory.string(),destination.string()))throw std::runtime_error("No complete verified previous checkpoint; originals preserved. See validation recovery instructions.");
    auto recovered=read(destination);recovered.name="Recovered "+recovered.name;recovered.name.resize(std::min<std::size_t>(recovered.name.size(),48));
    if(!writeMetadata(recovered))throw std::runtime_error("Recovered copy retained, but renaming failed");
    return recovered;
}

SavedWorld create(const std::string& name, std::uint32_t seed, GameMode mode, bool cheats) {
    if (!Persistence::enabled()) throw std::runtime_error("Demo cannot create saved worlds");
    if (name.empty() || name.size() > 48 || name.find_first_not_of(' ') == std::string::npos)
        throw std::runtime_error("Enter a world name (1-48 characters)");
    fs::create_directories("saves");
    SavedWorld result;
    result.name = name;
    result.seed = seed;
    result.mode = mode;
    result.cheats = cheats;
    // The visible name never becomes a path: slashes, reserved names and duplicates are safe.
    std::random_device random;
    for (int attempt = 0; attempt < 32; ++attempt) {
        result.directory = fs::path("saves") / ("world-" + std::to_string(random()) + "-" + std::to_string(random()));
        if (!fs::create_directory(result.directory)) continue;
        SaveFile::Session session(result.directory.string());
        if(!session.acquired())throw std::runtime_error("Cannot reserve new world");
        SaveFile::Checkpoint checkpoint((result.directory/"save.checkpoint").string());
        if(!checkpoint.ready())throw std::runtime_error("Cannot initialize world checkpoint");
        if (!writeMetadata(result)) throw std::runtime_error("Cannot write world metadata");
        const auto path = (result.directory / "world_seed.txt").string();
        if (!SaveFile::write(path, std::ios::out, [&](std::ofstream& output) {
            output << seed << '\n' << (mode == GameMode::Spectator ? "spectator" : mode == GameMode::Creative ? "creative" : "survival") << "\n" << WorldTime::DefaultSeconds << "\nTIME_CYCLE1 " << WorldTime::CycleSeconds << '\n';
            return static_cast<bool>(output);
        })) throw std::runtime_error("Cannot write world seed");
        if(!checkpoint.commit())throw std::runtime_error("Cannot commit new world metadata");
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
    {
      SaveFile::Session session(staging.string());if(!session.acquired())throw std::runtime_error("Cannot reserve imported copy");
      SaveFile::Checkpoint checkpoint((staging/"save.checkpoint").string());if(!checkpoint.ready())throw std::runtime_error("Cannot checkpoint imported copy");
      if (!fs::exists(staging / "world_seed.txt")) {
        std::ifstream world("voxel_world.vxw", std::ios::binary);
        world.seekg(12);
        world.read(reinterpret_cast<char*>(&imported.seed), sizeof(imported.seed));
        if (!world) throw std::runtime_error("Cannot read legacy world seed");
        if(!SaveFile::write((staging/"world_seed.txt").string(),std::ios::out,[&](std::ofstream& seed){seed<<imported.seed<<"\nsurvival\n35\n";return bool(seed);}))throw std::runtime_error("Cannot write imported seed");
      }
      if (!writeMetadata(imported) || !checkpoint.commit()) throw std::runtime_error("Cannot commit import metadata");
    }
    fs::rename(staging, destination); // Publish only after all copies succeeded. Originals remain intact.
}
}
