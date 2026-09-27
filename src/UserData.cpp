#include "UserData.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;

constexpr std::array<const char*, 6> ProfileFiles = {
    "voxel_world.vxw", "voxel_inventory.vxi", "voxel_player.vps",
    "voxel_mobs.vxm", "voxel_settings.cfg", "world_seed.txt"};

bool hasProfile(const fs::path& directory) {
    for (const char* name : ProfileFiles) {
        if (fs::exists(directory / name)) return true;
    }
    return false;
}

void copyLegacyProfile(const fs::path& source, const fs::path& destination) {
    std::error_code error;
    if (!fs::is_directory(source, error) || fs::equivalent(source, destination, error) ||
        !hasProfile(source)) return;
    for (const char* name : ProfileFiles) {
        const fs::path original = source / name;
        if (fs::is_regular_file(original, error))
            fs::copy_file(original, destination / name, fs::copy_options::skip_existing);
    }
    const fs::path screenshots = source / "screenshots";
    if (fs::is_directory(screenshots, error))
        fs::copy(screenshots, destination / "screenshots",
                 fs::copy_options::recursive | fs::copy_options::skip_existing);
}

fs::path executableDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size())
        throw std::runtime_error("Cannot locate the executable directory");
    buffer.resize(length);
    return fs::path(buffer).parent_path();
}

fs::path defaultDirectory() {
    PWSTR knownFolder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &knownFolder)))
        throw std::runtime_error("Cannot find Local AppData");
    const fs::path result = fs::path(knownFolder) / "VoxelFrontier";
    CoTaskMemFree(knownFolder);
    return result;
}
} // namespace

namespace UserData {
void initialize(bool migrateLegacy) {
    const fs::path previousDirectory = fs::current_path();
    const DWORD requested = GetEnvironmentVariableW(L"VOXEL_FRONTIER_DATA_DIR", nullptr, 0);
    fs::path destination;
    if (requested > 1) {
        std::wstring overridePath(requested, L'\0');
        const DWORD copied = GetEnvironmentVariableW(L"VOXEL_FRONTIER_DATA_DIR",
                                                      overridePath.data(), requested);
        if (copied == 0 || copied >= requested)
            throw std::runtime_error("Invalid VOXEL_FRONTIER_DATA_DIR");
        overridePath.resize(copied);
        destination = overridePath;
    } else {
        destination = defaultDirectory();
    }
    destination = fs::absolute(destination);
    fs::create_directories(destination);
    if (migrateLegacy && !hasProfile(destination)) {
        copyLegacyProfile(previousDirectory, destination);
        if (!hasProfile(destination))
            copyLegacyProfile(executableDirectory(), destination);
    }
    fs::current_path(destination);
}

void initializeLogging() {
    const fs::path logDirectory = fs::current_path() / "logs";
    fs::create_directories(logDirectory);
    const fs::path logFile = logDirectory / "VoxelFrontier.log";
    static auto* stream = new std::ofstream(logFile, std::ios::app);
    if (!stream->is_open())
        throw std::runtime_error("Cannot open Voxel Frontier log file");
    std::cout.rdbuf(stream->rdbuf());
    std::cerr.rdbuf(stream->rdbuf());
    std::cout << "Voxel Frontier started\n" << std::flush;
}
} // namespace UserData
