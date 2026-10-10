#include "Game.h"
#include "Weather.h"
#include "WeatherRenderer.h"
#include "MenuLayout.h"

#include "Definitions.h"
#include "AmbientOcclusion.h"
#include "Farming.h"
#include "Player.h"
#include "Persistence.h"
#include "Renderer.h"
#include "SaveFile.h"
#include "Screenshot.h"
#include "RenderScratch.h"
#include "Sound.h"
#include "UiLayout.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace {

constexpr const char* WorldSavePath = "voxel_world.vxw";
constexpr const char* InventorySavePath = "voxel_inventory.vxi";
constexpr const char* PlayerSavePath = "voxel_player.vps";
constexpr const char* MobSavePath = "voxel_mobs.vxm";
constexpr const char* SettingsPath = "voxel_settings.cfg";
constexpr const char* SeedPath = "world_seed.txt";

constexpr float NormalHitRecoilDegrees = 0.75f;
constexpr float CriticalHitRecoilDegrees = 1.15f;
constexpr float MaximumHitRecoilDegrees = 1.25f;

float clampedHitRecoilDegrees(float value) {
    return std::isfinite(value) ? std::clamp(value, 0.0f, MaximumHitRecoilDegrees) : 0.0f;
}

float decayHitRecoilDegrees(float value, float deltaTime) {
    const float remaining = clampedHitRecoilDegrees(value) *
                            std::exp(-30.0f * std::max(0.0f, deltaTime));
    return remaining < 0.001f ? 0.0f : remaining;
}

glm::mat4 applyHitRecoil(const glm::mat4& baseView, float recoilDegrees) {
    const float radians = glm::radians(clampedHitRecoilDegrees(recoilDegrees));
    // Left multiplication rotates in camera space, around the eye at (0,0,0).
    // Right multiplication would orbit the camera around the world origin.
    return glm::rotate(glm::mat4(1.0f), radians, glm::vec3(1.0f, 0.0f, 0.0f)) *
           baseView;
}

float daylight(float worldTime) {
    const float angle = worldTime / DayNightCycleSeconds * glm::two_pi<float>() + 0.35f;
    return glm::smoothstep(-0.13f, 0.17f, std::sin(angle));
}

int chunkCoordinate(float position) {
    return static_cast<int>(std::floor(position / CHUNK_SIZE));
}

bool isOre(Block block) {
    return block == Block::CoalOre || block == Block::CopperOre || block == Block::IronOre ||
           block == Block::GoldOre || block == Block::DiamondOre;
}

int oreExperience(Block block) {
    if (block == Block::DiamondOre) {
        return 7;
    }
    if (block == Block::GoldOre) {
        return 5;
    }
    if (block == Block::IronOre) {
        return 3;
    }
    return 2;
}

void waitForFrameLimit(double remainingSeconds) {
    if (remainingSeconds <= 0.0)
        return;
#ifdef _WIN32
    struct FrameTimer {
        HANDLE handle = CreateWaitableTimerExW(
            nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        ~FrameTimer() {
            if (handle)
                CloseHandle(handle);
        }
    };
    static FrameTimer timer;
    if (timer.handle) {
        LARGE_INTEGER relativeTime{};
        relativeTime.QuadPart = -std::max<LONGLONG>(
            1, static_cast<LONGLONG>(remainingSeconds * 10000000.0));
        if (SetWaitableTimer(timer.handle, &relativeTime, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(timer.handle, INFINITE);
            return;
        }
    }
#endif
    std::this_thread::sleep_for(std::chrono::duration<double>(remainingSeconds));
}

std::string facingDirection(const glm::vec3& direction) {
    if (std::abs(direction.x) > std::abs(direction.z)) {
        return direction.x > 0.0f ? "EAST" : "WEST";
    }
    return direction.z > 0.0f ? "SOUTH" : "NORTH";
}

} // namespace

Game::Game() = default;

Game::~Game() {
    shutdown();
}

bool Game::initialize(int argc, char** argv) {
    if (argc > 0 && argv[0]) executablePath_ = argv[0];
    if (Persistence::enabled())
        settings_.load(SettingsPath);
    parseArguments(argc, argv);

    if (!glfwInit()) {
        return false;
    }
    glfwInitialized_ = true;

    if (!createWindow()) {
        shutdown();
        return false;
    }

    try {
        createPresentationSystems();
        // Developer previews/tests still enter their fixtures directly.
        if (!saveOnExit_) {
            createWorldAndSystems();
            ui_.resumeGame();
        } else {
            ui_.openMainMenu();
        }
        synchronizeCursorCapture();
        initialized_ = true;
        if (settings_.fullscreen) {
            applyFullscreenSetting();
        }
        if (smokeTest_.commandVisual) {
            chat_.open();
            synchronizeCursorCapture();
            screenshotRequested_ = true;
            smokeTest_.startTime = glfwGetTime();
        }
        if (smokeTest_.craftingPreview) {
            smokeTest_.startTime = glfwGetTime();
            updateCraftingPreview(smokeTest_.startTime);
        }
        if (smokeTest_.menuEnabled) {
            runMainMenuSmokeTest();
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }

        std::cout << "WASD move, Ctrl sprint, Shift sneak/swim down, Space jump/swim, "
                     "E inventory, Q drop, RMB use/place, LMB attack/mine, G fullbright, "
                     "hold C zoom, / commands, F3 debug, 1-9 hotbar\n";

        if (!geographyAudit_.empty()) {
            std::string report;
            if (!world_->runGeographyAudit(geographyAudit_, report)) throw std::runtime_error(report);
            std::cout << report << std::endl;
            glfwSetWindowShouldClose(window_, true);
        } else if (!biomeAudit_.empty()) {
            std::string report;
            if (!world_->runBiomeAudit(biomeAudit_, report)) throw std::runtime_error(report);
            std::cout << report << std::endl;
            glfwSetWindowShouldClose(window_, true);
        } else if (weatherSmoke_) {
            runWeatherSmokeTest();
            glfwSetWindowShouldClose(window_,GLFW_TRUE);
        } else if (smokeTest_.patch272) {
            runPatch272SmokeTest();
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        } else if (smokeTest_.resetEnabled) {
            runResetSmokeTest();
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        } else if (smokeTest_.survivalEnabled) {
            runSurvivalSmokeTest();
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        } else if (smokeTest_.commandEnabled) {
            runCommandSmokeTest();
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        } else if (smokeTest_.spectatorEnabled) {
            runSpectatorSmokeTest();
            saveOnExit_ = false;
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        } else if (smokeTest_.worldgenEnabled) {
            const WorldgenSurvey survey = world_->runWorldgenSurvey();
            std::cout << "Worldgen preview seed " << seed_ << ": surface openings "
                      << survey.exposedColumns << '/' << survey.surfaceColumns
                      << ", dry/flooded cave blocks " << survey.dryCaveBlocks
                      << '/' << survey.floodedCaveBlocks << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Initialization failed: " << error.what() << '\n';
        shutdown();
        return false;
    }

    return true;
}

int Game::run() {
    if (!initialized_) {
        return EXIT_FAILURE;
    }

    int result = EXIT_SUCCESS;
    try {
        while (!glfwWindowShouldClose(window_)) {
            const float deltaTime = beginFrame();
            handleGlobalInput();

            if (ui_.simulationPaused()) {
                updatePauseInterface();
            }

            sounds_->update(deltaTime);
            updateSimulation(deltaTime);
            renderFrame(deltaTime);
            if (settings_.frameLimit > 0) {
                const double frameEnd = timing_.previousFrame + 1.0 / settings_.frameLimit;
                const double remaining = frameEnd - glfwGetTime();
                waitForFrameLimit(remaining);
            }
        }

        if (Persistence::enabled() && saveOnExit_) {
            saveAll();
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal: " << error.what() << '\n';
        result = EXIT_FAILURE;
    }
    return result;
}

void Game::shutdown() {
    if (!glfwInitialized_) {
        return;
    }

    weather_.reset();
    if (weatherRenderer_) weatherRenderer_->clear();
    if (sounds_) sounds_->setRainAmbience(0,0);
    farming_.reset();
    survival_.reset();
    inventory_.reset();
    player_.reset();
    world_.reset();
    weatherRenderer_.reset();
    sounds_.reset();
    renderer_.reset();
    destroyRenderTarget();

    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
    glfwInitialized_ = false;
    initialized_ = false;
}

void Game::parseArguments(int argc, char** argv) {
    if (Persistence::enabled()) {
        std::ifstream seedFile(worldSavePath(SeedPath));
        std::uint64_t storedSeed = 0;
        if (seedFile >> storedSeed) {
            seed_ = static_cast<std::uint32_t>(storedSeed);
            std::string storedMode;
            if (seedFile >> storedMode) {
                creativeMode_ = storedMode == "creative";
                spectatorMode_ = storedMode == "spectator";
            }
            float savedWorldTime = 35.0f;
            if (seedFile >> savedWorldTime && std::isfinite(savedWorldTime) &&
                savedWorldTime >= 0.0f)
                timing_.worldTime = std::fmod(savedWorldTime, DayNightCycleSeconds);
        }
    }

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        try {
            if (argument.rfind("--seed=", 0) == 0) {
                seed_ = static_cast<std::uint32_t>(std::stoull(argument.substr(7)));
            } else if (argument == "--seed" && index + 1 < argc) {
                seed_ = static_cast<std::uint32_t>(std::stoull(argv[++index]));
            } else if (argument == "--creative") {
                creativeMode_ = true;
                spectatorMode_ = false;
            } else if (argument == "--survival") {
                creativeMode_ = false;
                spectatorMode_ = false;
            } else if (argument == "--spectator") {
                creativeMode_ = false;
                spectatorMode_ = true;
            } else if (argument == "--ui-smoke") {
                smokeTest_.uiEnabled = true;
            } else if (argument.rfind("--geography-audit=",0)==0) {
                geographyAudit_=argument.substr(18); saveOnExit_=false;
            } else if (argument.rfind("--biome-audit=",0)==0) {
                biomeAudit_=argument.substr(14); saveOnExit_=false;
            } else if (argument == "--weather-smoke") {
                weatherSmoke_=true; saveOnExit_=false;
            } else if (argument.rfind("--weather-benchmark=",0)==0) {
                weatherSmoke_=true; saveOnExit_=false; weatherBenchmark_=argument.substr(20);
            } else if (argument == "--survival-smoke") {
                smokeTest_.survivalEnabled = true;
            } else if (argument == "--command-smoke") {
                smokeTest_.commandEnabled = true;
            } else if (argument == "--command-visual-smoke") {
                smokeTest_.commandVisual = true;
            } else if (argument == "--reset-smoke") {
                smokeTest_.resetEnabled = true;
            } else if (argument == "--worldgen-test") {
                smokeTest_.worldgenEnabled = true;
            } else if (argument == "--spectator-smoke") {
                smokeTest_.spectatorEnabled = true;
            } else if (argument == "--billboard-preview") {
                smokeTest_.billboardPreview = true;
            } else if (argument == "--patch272-smoke") {
                smokeTest_.patch272 = true;
            } else if (argument == "--menu-smoke") {
                smokeTest_.menuEnabled = true;
            } else if (argument.rfind("--preview-x=",0)==0) {
                smokeTest_.previewX = std::stof(argument.substr(12));
            } else if (argument.rfind("--preview-z=",0)==0) {
                smokeTest_.previewZ = std::stof(argument.substr(12));
            } else if (argument == "--crafting-preview") {
                smokeTest_.craftingPreview = true;
            } else if (argument.rfind("--preview-brightness=", 0) == 0) {
                smokeTest_.previewBrightness = std::clamp(
                    std::stof(argument.substr(21)) / 100.0f, 0.0f, 1.0f);
            }
        } catch (...) {
            // Preserve the previous valid seed when a command-line value is malformed.
        }
    }

    if (smokeTest_.uiEnabled || smokeTest_.survivalEnabled || smokeTest_.commandEnabled ||
        smokeTest_.commandVisual || smokeTest_.resetEnabled ||
        smokeTest_.worldgenEnabled || smokeTest_.spectatorEnabled || smokeTest_.billboardPreview || smokeTest_.craftingPreview || smokeTest_.patch272)
        saveOnExit_ = false;

}

bool Game::createWindow() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(1280, 720, "Voxel Frontier", nullptr, nullptr);
    if (window_ == nullptr) {
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(settings_.vsync ? 1 : 0);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    return true;
}

void Game::createPresentationSystems() {
    renderer_ = std::make_unique<Renderer>();
    renderer_->setParticlePercent(settings_.particlePercent);
    renderer_->setEffectQuality(settings_.effectQuality);
    sounds_ = std::make_unique<SoundSystem>(executablePath_);
    sounds_->setMasterVolume(settings_.masterVolume);
    sounds_->setCategoryVolumes(settings_.musicVolume, settings_.sfxVolume,
                                settings_.passiveMobVolume, settings_.hostileMobVolume);
    sounds_->startMusic();

    input_.attach(window_);
    input_.setBindings(settings_.controls);
    if (Persistence::enabled() && saveOnExit_) {
        try { WorldLibrary::importLegacy(); }
        catch (const std::exception& error) { menuMessage_ = error.what(); std::cerr << error.what() << '\n'; }
    }
    currentFov_ = settings_.fov;
    timing_.previousFrame = timing_.fpsSampleStart = glfwGetTime();
}

void Game::createWorldAndSystems() {
    world_ = std::make_unique<World>(seed_);
    world_->setSimulationDistance(settings_.simulationDistance);
    glm::vec3 spawnPosition(0.5f, 0.0f, 0.5f);
    const bool loadedWorld = Persistence::enabled() && !smokeTest_.worldgenEnabled &&
                             world_->loadWorld(worldSavePath(WorldSavePath), spawnPosition);
    if (!loadedWorld && Persistence::enabled() && !activeWorld_.directory.empty() &&
        std::filesystem::exists(worldSavePath(WorldSavePath))) {
        world_.reset();
        throw std::runtime_error("Cannot load this world. Original save files were preserved.");
    }
    if (!loadedWorld)
        timing_.worldTime = 35.0f;
    const glm::vec3 safeSpawn = world_->findSafeSpawnNear(0, 0);
    const float spawnDeltaX = spawnPosition.x - safeSpawn.x;
    const float spawnDeltaZ = spawnPosition.z - safeSpawn.z;
    const bool legacyBadSpawn = loadedWorld &&
                                spawnDeltaX * spawnDeltaX + spawnDeltaZ * spawnDeltaZ < 256.0f &&
                                spawnPosition.y < safeSpawn.y - 1.0f;
    if (!loadedWorld || legacyBadSpawn) {
        spawnPosition = safeSpawn;
    }
    if (smokeTest_.worldgenEnabled) {
        spawnPosition = {smokeTest_.previewX + .5f,
            static_cast<float>(world_->terrainHeight(static_cast<int>(smokeTest_.previewX),
                                                     static_cast<int>(smokeTest_.previewZ))),
            smokeTest_.previewZ + .5f};
        spawnPosition.y += 85.0f;
        creativeMode_ = true;
        spectatorMode_ = false;
        saveOnExit_ = false;
    }

    world_->generate(settings_.renderDistance, spawnPosition);
    player_ = std::make_unique<Player>(spawnPosition);
    player_->setMouseSensitivity(settings_.mouseSensitivity);
    if (Persistence::enabled() && !smokeTest_.worldgenEnabled)
        player_->load(worldSavePath(PlayerSavePath), seed_);
    player_->setCreativeMode(creativeMode_);
    if (spectatorMode_)
        player_->setSpectatorMode(true);
    if (smokeTest_.worldgenEnabled) {
        player_->setFlying(true);
        player_->addMouseMovement(0.0,
            -30.0 / std::max(0.01f, settings_.mouseSensitivity));
    }

    inventory_ = std::make_unique<Inventory>();
    if (Persistence::enabled() && !smokeTest_.worldgenEnabled)
        inventory_->load(worldSavePath(InventorySavePath), seed_);
    survival_ = std::make_unique<SurvivalWorld>(seed_);
    if (Persistence::enabled() && !smokeTest_.worldgenEnabled)
        survival_->load(worldSavePath(MobSavePath), seed_);
    survival_->setSoundSystem(sounds_.get());
    if (smokeTest_.billboardPreview) {
        saveOnExit_ = false;
        creativeMode_ = true;
        player_->setCreativeMode(true);
        timing_.worldTime = DayNightCycleSeconds * .75f;
        settings_.brightness = smokeTest_.previewBrightness;
        survival_->spawnBillboardPreview(player_->position(), player_->lookDirection());
    }
    weather_ = std::make_unique<Weather>(seed_);
    if (loadedWorld && Persistence::enabled()) {
        std::ifstream metadata(worldSavePath(SeedPath));
        std::string line;
        for (int i=0;i<3 && std::getline(metadata,line);++i) {}
        weather_->read(metadata); // Absent/invalid old metadata retains clear defaults.
    }
    if (!weatherRenderer_) weatherRenderer_ = std::make_unique<WeatherRenderer>();
    weatherRenderer_->clear();
    survival_->setWeather(weather_.get());
    farming_ = std::make_unique<FarmingSystem>();

    input_.setCursorCaptured(true);
    currentFov_ = settings_.fov;

    const double now = glfwGetTime();
    timing_.previousFrame = now;
    timing_.fpsSampleStart = now;
    timing_.lastSave = now;
    smokeTest_.startTime = now;
}

void Game::synchronizeCursorCapture() {
    input_.setCursorCaptured(ui_.cursorShouldBeCaptured() && !chat_.isOpen());
}

void Game::applyFullscreenSetting() {
    if (settings_.fullscreen) {
        glfwGetWindowPos(window_, &windowState_.windowedX, &windowState_.windowedY);
        glfwGetWindowSize(window_, &windowState_.windowedWidth, &windowState_.windowedHeight);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        return;
    }

    glfwSetWindowMonitor(window_,
                         nullptr,
                         windowState_.windowedX,
                         windowState_.windowedY,
                         windowState_.windowedWidth,
                         windowState_.windowedHeight,
                         0);
}

bool Game::saveAll() {
    if (!Persistence::enabled() || !world_) return true;
    bool saved = true;
    if (!player_->isDead()) {
        saved &= world_->saveWorld(worldSavePath(WorldSavePath), player_->position());
    }
    saved &= inventory_->save(worldSavePath(InventorySavePath), seed_);
    saved &= player_->save(worldSavePath(PlayerSavePath), seed_);
    saved &= survival_->save(worldSavePath(MobSavePath), seed_);
    saved &= settings_.save(SettingsPath);
    if (saved) saved &= saveWorldMetadata();
    timing_.lastSave = glfwGetTime();
    if (!saved) std::cerr << "World save failed; current session retained\n" << std::flush;
    return saved;
}

void Game::saveSettings() {
    if (Persistence::enabled())
        settings_.save(SettingsPath);
}

bool Game::saveWorldMetadata() const {
    if (!Persistence::enabled()) return true;
    bool saved = SaveFile::write(worldSavePath(SeedPath), std::ios::out, [&](std::ofstream& output) {
        const char* mode = spectatorMode_ ? "spectator"
                           : creativeMode_ ? "creative" : "survival";
        output << seed_ << '\n' << mode << '\n'
               << std::setprecision(std::numeric_limits<float>::max_digits10)
               << std::fmod(std::max(0.0f, timing_.worldTime), DayNightCycleSeconds) << '\n';
        if (weather_) weather_->write(output);
        return static_cast<bool>(output);
    });
    if (!activeWorld_.directory.empty()) {
        auto info = activeWorld_;
        info.lastPlayed = static_cast<std::int64_t>(std::time(nullptr));
        saved &= WorldLibrary::writeMetadata(info);
    }
    return saved;
}

std::string Game::worldSavePath(const char* filename) const {
    return activeWorld_.directory.empty() ? filename : (activeWorld_.directory / filename).string();
}

void Game::refreshWorlds() {
    try {
        savedWorlds_ = WorldLibrary::list();
        selectedWorld_ = -1;
        worldPage_ = 0;
        lastWorldClickIndex_ = -1;
    } catch (const std::exception& error) { menuMessage_ = error.what(); }
}

void Game::playSavedWorld(const SavedWorld& selected) {
    if (!Persistence::enabled()) return;
    activeWorld_ = selected;
    seed_ = selected.seed;
    timing_.worldTime = selected.time;
    creativeMode_ = selected.mode == GameMode::Creative;
    spectatorMode_ = selected.mode == GameMode::Spectator;
    try {
        createWorldAndSystems();
        menuMessage_.clear();
        ui_.resumeGame();
        synchronizeCursorCapture();
    } catch (const std::exception& error) {
        weather_.reset();
        if (weatherRenderer_) weatherRenderer_->clear();
        if (sounds_) sounds_->setRainAmbience(0,0);
        farming_.reset(); survival_.reset(); inventory_.reset(); player_.reset(); world_.reset();
        activeWorld_ = {};
        menuMessage_ = error.what();
        std::cerr << menuMessage_ << '\n' << std::flush;
        ui_.openWorldSelection();
        synchronizeCursorCapture();
    }
}

void Game::createNamedWorld() {
    if (!Persistence::enabled()) return;
    try {
        std::uint32_t newSeed = 0;
        if (newWorldSeed_.empty()) newSeed = std::random_device{}();
        else {
            if (!std::all_of(newWorldSeed_.begin(),newWorldSeed_.end(),[](unsigned char c) { return c>='0' && c<='9'; }))
                throw std::runtime_error("Seed must contain digits only");
            const auto value = std::stoull(newWorldSeed_);
            if (value > UINT32_MAX) throw std::runtime_error("Seed must be between 0 and 4294967295");
            newSeed = static_cast<std::uint32_t>(value);
        }
        const auto created = WorldLibrary::create(newWorldName_, newSeed, newWorldMode_);
        playSavedWorld(created);
        if (world_) saveAll();
    } catch (const std::exception& error) { menuMessage_ = error.what(); }
}

GameMode Game::gameMode() const {
    return spectatorMode_ ? GameMode::Spectator
           : creativeMode_ ? GameMode::Creative : GameMode::Survival;
}

glm::vec3 Game::safeExitFromSpectator() const {
    const glm::vec3 origin = player_->position();
    const glm::ivec3 center(glm::floor(origin));
    glm::vec3 best(0.0f);
    float bestDistanceSquared = std::numeric_limits<float>::max();
    for (int radius = 0; radius <= 8; ++radius) {
        if (static_cast<float>(radius * radius) > bestDistanceSquared)
            break;
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dz = -radius; dz <= radius; ++dz) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max({std::abs(dx), std::abs(dy), std::abs(dz)}) != radius)
                        continue;
                    const glm::vec3 candidate(
                        static_cast<float>(center.x + dx) + 0.5f,
                        static_cast<float>(center.y + dy) + 0.01f,
                        static_cast<float>(center.z + dz) + 0.5f);
                    if (candidate.y < 1.0f || candidate.y > WORLD_HEIGHT - Player::Height - 1.0f)
                        continue;
                    const glm::vec3 minimum = candidate +
                        glm::vec3(-Player::Width * 0.5f, 0.0f, -Player::Width * 0.5f);
                    const glm::vec3 maximum = candidate +
                        glm::vec3(Player::Width * 0.5f, Player::Height,
                                  Player::Width * 0.5f);
                    if (world_->aabbIntersectsSolid(minimum, maximum) ||
                        !world_->aabbIntersectsSolid(
                            minimum - glm::vec3(0.0f, 0.16f, 0.0f),
                            maximum - glm::vec3(0.0f, Player::Height - 0.02f, 0.0f)))
                        continue;
                    const Block feet = world_->getBlock(
                        center.x + dx, center.y + dy, center.z + dz);
                    if (isWater(feet))
                        continue;
                    const float distanceSquared = glm::dot(candidate - origin,
                                                            candidate - origin);
                    if (distanceSquared < bestDistanceSquared) {
                        bestDistanceSquared = distanceSquared;
                        best = candidate;
                    }
                }
            }
        }
    }
    if (bestDistanceSquared < std::numeric_limits<float>::max())
        return best;
    return world_->findSafeSpawnNear(center.x, center.z);
}

void Game::setGameMode(GameMode mode) {
    if (mode == gameMode())
        return;
    const bool leavingSpectator = spectatorMode_ && mode != GameMode::Spectator;
    if (leavingSpectator && player_)
        player_->teleport(safeExitFromSpectator());
    creativeMode_ = mode == GameMode::Creative;
    spectatorMode_ = mode == GameMode::Spectator;
    if (player_) {
        if (spectatorMode_)
            player_->setSpectatorMode(true);
        else if (leavingSpectator) {
            player_->setSpectatorMode(false);
            if (creativeMode_)
                player_->setCreativeMode(true);
        } else {
            player_->setCreativeMode(creativeMode_);
        }
    }
    interaction_.hasBlockTarget = false;
    interaction_.hasMiningTarget = false;
    interaction_.mobTarget = {};
    interaction_.breakProgress = 0.0f;
    interaction_.heldItemSwing = 0.0f;
    interaction_.cameraKickDegrees = 0.0f;
    saveWorldMetadata();
}

void Game::resetWorld(std::uint32_t newSeed, GameMode mode) {
    ui_.resumeGame();
    weather_.reset();
    if (weatherRenderer_) weatherRenderer_->clear();
    if (sounds_) sounds_->setRainAmbience(0,0);
    farming_.reset();
    survival_.reset();
    inventory_.reset();
    player_.reset();
    world_.reset();

    std::error_code error;
    if (Persistence::enabled())
        for (const char* path : {WorldSavePath, InventorySavePath, PlayerSavePath, MobSavePath}) {
            error.clear();
            std::filesystem::remove(worldSavePath(path), error);
        }

    seed_ = newSeed;
    creativeMode_ = mode == GameMode::Creative;
    spectatorMode_ = mode == GameMode::Spectator;
    timing_.worldTime = 35.0f;
    saveWorldMetadata();

    world_ = std::make_unique<World>(seed_);
    world_->setSimulationDistance(settings_.simulationDistance);
    const glm::vec3 spawnPosition = world_->findSafeSpawnNear(0, 0);
    world_->generate(settings_.renderDistance, spawnPosition);
    player_ = std::make_unique<Player>(spawnPosition);
    player_->setMouseSensitivity(settings_.mouseSensitivity);
    player_->setCreativeMode(creativeMode_);
    if (spectatorMode_)
        player_->setSpectatorMode(true);

    inventory_ = std::make_unique<Inventory>();
    if (creativeMode_ || spectatorMode_)
        inventory_->clear();
    survival_ = std::make_unique<SurvivalWorld>(seed_);
    survival_->setSoundSystem(sounds_.get());
    weather_ = std::make_unique<Weather>(seed_);
    if (!weatherRenderer_) weatherRenderer_ = std::make_unique<WeatherRenderer>();
    weatherRenderer_->clear();
    survival_->setWeather(weather_.get());
    farming_ = std::make_unique<FarmingSystem>();

    timing_.worldTime = 35.0f;
    timing_.lastSave = glfwGetTime();
    interaction_ = {};
    wasInWater_ = false;
    currentFov_ = settings_.fov;
    saveAll();
    synchronizeCursorCapture();
}
float Game::beginFrame() {
    const double now = glfwGetTime();
    const float deltaTime = std::min(static_cast<float>(now - timing_.previousFrame), 0.05f);
    timing_.previousFrame = now;
    timing_.frameMilliseconds = deltaTime * 1000.0f;

    ++timing_.framesInSample;
    if (now - timing_.fpsSampleStart >= 0.5) {
        timing_.fps = static_cast<float>(timing_.framesInSample / (now - timing_.fpsSampleStart));
        timing_.framesInSample = 0;
        timing_.fpsSampleStart = now;
    }
#ifdef _WIN32
    if (showDebug_ && (timing_.lastMemorySample < 0.0 ||
                       now - timing_.lastMemorySample >= 0.5)) {
        PROCESS_MEMORY_COUNTERS counters{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
            timing_.workingSetBytes = counters.WorkingSetSize;
            timing_.peakWorkingSetBytes = counters.PeakWorkingSetSize;
            timing_.memorySampleAvailable = true;
        }
        timing_.lastMemorySample = now;
    }
#endif

    input_.pollEvents();
    return deltaTime;
}

void Game::handleGlobalInput() {
    if (!world_) {
        if (ui_.state() != GameState::Controls && input_.actionPressed(ControlAction::Screenshot))
            screenshotRequested_ = true;
        if (input_.keyPressed(GLFW_KEY_ESCAPE)) {
            if (ui_.state() == GameState::Controls && activeControlBinding_ >= 0)
                activeControlBinding_ = -1;
            else {
                if (ui_.state() != GameState::MainMenu) saveSettings();
                activeSettingsSlider_ = -1;
                ui_.handleEscape(nullptr);
            }
        }
        synchronizeCursorCapture();
        return;
    }
    if (chat_.isOpen()) {
        const std::string submitted = chat_.update(input_, window_, commands_);
        if (!submitted.empty()) {
            chat_.addMessage("> " + submitted, ChatTone::Normal, glfwGetTime());
            CommandContext context{*world_, *player_, *inventory_, *survival_,
                                   timing_.worldTime, seed_,
                                   [this] { return gameMode(); },
                                   [this](GameMode mode) { setGameMode(mode); }, true, weather_.get(), sounds_.get()};
            const CommandResult result = commands_.execute(submitted, context);
            chat_.addMessage(result.text, result.tone, glfwGetTime());
        }
        synchronizeCursorCapture();
        return;
    }
    if (ui_.state() == GameState::Playing && input_.keyPressed(GLFW_KEY_SLASH)) {
        chat_.open();
        input_.consumeTypedCharacters(); // GLFW also reports the opening slash as text.
        synchronizeCursorCapture();
        return;
    }
    const bool chestWasOpen = ui_.state() == GameState::Chest;
    const glm::ivec3 chestPosition = ui_.openedContainerPosition();
    if (input_.cursorCaptured()) {
        const glm::dvec2 mouseDelta = input_.consumeMouseDelta();
        const float zoomSensitivityScale =
            zoomActive() ? std::clamp(currentFov_ / settings_.fov, 0.15f, 1.0f) : 1.0f;
        player_->addMouseMovement(
            mouseDelta.x * zoomSensitivityScale, mouseDelta.y * zoomSensitivityScale);

        const double scrollDelta = input_.consumeScrollDelta();
        if (scrollDelta != 0.0) {
            if (spectatorMode_) {
                player_->adjustSpectatorSpeed(scrollDelta);
            } else if (zoomActive()) {
                zoomFov_ = std::clamp(
                    zoomFov_ - static_cast<float>(scrollDelta) * 2.5f, 10.0f, 55.0f);
            } else {
                inventory_->cycleSlot(scrollDelta > 0.0 ? -1 : 1);
            }
        }
    }

    if (input_.actionPressed(ControlAction::Inventory) && !ui_.simulationPaused() &&
        !ui_.recipeBook().searchFocused) {
        ui_.toggleInventory(*inventory_);
    }
    if (input_.keyPressed(GLFW_KEY_ESCAPE)) {
        if (ui_.state() == GameState::Controls && activeControlBinding_ >= 0) {
            activeControlBinding_ = -1;
        } else {
            if (ui_.state() == GameState::VideoSettings) {
                world_->setRenderDistance(settings_.renderDistance);
                world_->setSimulationDistance(settings_.simulationDistance);
                saveSettings();
                activeSettingsSlider_ = -1;
            } else if (ui_.state() == GameState::AudioSettings || ui_.state() == GameState::Controls) {
                saveSettings();
                activeSettingsSlider_ = -1;
            }
            ui_.handleEscape(*inventory_);
        }
    }
    if (chestWasOpen && ui_.state() != GameState::Chest)
        sounds_->playChest(false, glm::vec3(chestPosition) + glm::vec3(.5f));
    if (input_.actionPressed(ControlAction::Fullbright) && !ui_.simulationPaused() &&
        !ui_.recipeBook().searchFocused) {
        fullbright_ = !fullbright_;
    }
    if (ui_.state() != GameState::Controls &&
        input_.actionPressed(ControlAction::Screenshot)) {
        screenshotRequested_ = true;
    }
    if (ui_.state() != GameState::Controls &&
        input_.actionPressed(ControlAction::Debug)) {
        showDebug_ = !showDebug_;
    }
    if (ui_.state() == GameState::Playing && input_.keyPressed(GLFW_KEY_F4)) {
        showWorldgenDebug_ = !showWorldgenDebug_;
    }

    if (ui_.state() == GameState::Playing && !spectatorMode_) {
        for (int slot = 0; slot < Inventory::HotbarSlots; ++slot) {
            if (input_.keyDown(GLFW_KEY_1 + slot)) {
                inventory_->selectSlot(slot);
            }
        }
    }

    if (smokeTest_.uiEnabled || smokeTest_.survivalEnabled || smokeTest_.commandEnabled ||
        smokeTest_.commandVisual ||
        smokeTest_.resetEnabled || smokeTest_.worldgenEnabled ||
        smokeTest_.spectatorEnabled || smokeTest_.billboardPreview) {
        saveOnExit_ = false;
    }

    if (smokeTest_.uiEnabled) {
        updateUiSmokeTest(glfwGetTime());
    }
    if (smokeTest_.craftingPreview) updateCraftingPreview(glfwGetTime());
    synchronizeCursorCapture();
}

void Game::updatePauseInterface() {
    if (ui_.state() == GameState::Controls && activeControlBinding_ >= 0) {
        const int pressedKey = input_.firstPressedKey();
        if (pressedKey >= 0 && pressedKey != GLFW_KEY_ESCAPE) {
            if (settings_.bindControl(
                    static_cast<ControlAction>(activeControlBinding_), pressedKey)) {
                input_.setBindings(settings_.controls);
                saveSettings();
            }
            activeControlBinding_ = -1;
        }
        return;
    }
    if (ui_.state() == GameState::ResetWorld) {
        for (int key = GLFW_KEY_0; key <= GLFW_KEY_9; ++key) {
            if (input_.keyPressed(key) && resetSeedText_.size() < 10) {
                resetSeedText_.push_back(static_cast<char>('0' + key - GLFW_KEY_0));
            }
        }
        for (int key = GLFW_KEY_KP_0; key <= GLFW_KEY_KP_9; ++key) {
            if (input_.keyPressed(key) && resetSeedText_.size() < 10) {
                resetSeedText_.push_back(static_cast<char>('0' + key - GLFW_KEY_KP_0));
            }
        }
        if (input_.keyPressed(GLFW_KEY_BACKSPACE) && !resetSeedText_.empty()) {
            resetSeedText_.pop_back();
        }
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
    glm::dvec2 cursor = input_.framebufferCursorPosition();
    const int hit = ui_.hoveredMenuItem(cursor, framebufferWidth, framebufferHeight);
    const float scale = ui_.state() == GameState::MainMenu ? 1.0f : UIManager::menuScale(framebufferWidth, framebufferHeight);
    cursor /= scale;
    framebufferWidth = static_cast<int>(framebufferWidth / scale);
    framebufferHeight = static_cast<int>(framebufferHeight / scale);

    if (ui_.state() == GameState::AudioSettings) {
        const bool leftDown = input_.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
        const float panelX = framebufferWidth * 0.5f - 220.0f;
        if (!leftDown && activeSettingsSlider_ >= 0) {
            activeSettingsSlider_ = -1;
            saveSettings();
        }
        if (input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT) && hit >= 0 && hit < 5 &&
            cursor.x >= panelX + 180.0f && cursor.x <= panelX + 350.0f) {
            activeSettingsSlider_ = hit;
            sounds_->playClick();
        }
        if (leftDown && activeSettingsSlider_ >= 0) {
            const float value = std::clamp(
                static_cast<float>((cursor.x - (panelX + 190.0f)) / 150.0f), 0.0f, 1.0f);
            switch (activeSettingsSlider_) {
            case 0: settings_.masterVolume = value; break;
            case 1: settings_.musicVolume = value; break;
            case 2: settings_.sfxVolume = value; break;
            case 3: settings_.passiveMobVolume = value; break;
            case 4: settings_.hostileMobVolume = value; break;
            default: break;
            }
            sounds_->setMasterVolume(settings_.masterVolume);
            sounds_->setCategoryVolumes(settings_.musicVolume, settings_.sfxVolume,
                                        settings_.passiveMobVolume, settings_.hostileMobVolume);
            return;
        }
        if (input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT) && hit == 5) {
            sounds_->playClick();
            ui_.openSettings();
        }
        return;
    }

    if (ui_.state()==GameState::WeatherSettings) {
        const bool down=input_.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
        const float x=MenuLayout::panel(framebufferWidth,framebufferHeight).x+340;
        if(!down && activeSettingsSlider_>=0) {activeSettingsSlider_=-1;saveSettings();}
        if(input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT) && (hit==42 || hit==45) && cursor.x>=x-10 && cursor.x<=x+180)
            activeSettingsSlider_=hit;
        if(down && activeSettingsSlider_>=40) {
            const float value=std::clamp(static_cast<float>((cursor.x-x)/170),0.0f,1.0f);
            if(activeSettingsSlider_==42) settings_.precipitationDensity=value;
            if(activeSettingsSlider_==45) settings_.lightningFlash=value;
            return;
        }
    }
    if (ui_.state() == GameState::VideoSettings) {
        const bool leftDown = input_.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
        const float sliderX = MenuLayout::panel(framebufferWidth, framebufferHeight).x + 340.0f;
        if (!leftDown && activeSettingsSlider_ >= 0) {
            if (world_) {
                world_->setRenderDistance(settings_.renderDistance);
                world_->setSimulationDistance(settings_.simulationDistance);
            }
            activeSettingsSlider_ = -1;
            saveSettings();
        }
        if (input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT) &&
            ((hit >= 0 && hit < 3) || hit == 5 || hit == 12) &&
            cursor.x >= sliderX - 10.0f && cursor.x <= sliderX + 180.0f) {
            activeSettingsSlider_ = hit;
            sounds_->playClick();
        }
        if (leftDown && activeSettingsSlider_ >= 0) {
            const float sliderStart = sliderX;
            const float sliderWidth = 170.0f;
            const float slider = std::clamp(
                static_cast<float>((cursor.x - sliderStart) / sliderWidth), 0.0f, 1.0f);
            switch (activeSettingsSlider_) {
            case 0:
                settings_.renderDistance = static_cast<int>(std::round(2.0f + slider * 62.0f));
                break;
            case 1:
                settings_.simulationDistance =
                    static_cast<int>(std::round(2.0f + slider * 30.0f));
                break;
            case 2:
                settings_.fov = 55.0f + slider * 50.0f;
                break;
            case 3:
                settings_.mouseSensitivity = 0.03f + slider * 0.27f;
                if (player_) player_->setMouseSensitivity(settings_.mouseSensitivity);
                break;
            case 5:
                settings_.brightness = slider;
                break;
            case 12:
                settings_.entityDistance = static_cast<int>(std::round(2.0f + slider * 62.0f));
                break;
            default:
                break;
            }
            settings_.clamp();
            settings_.graphicsPreset = GraphicsPreset::Custom;
            return;
        }
    }

    if (ui_.state() == GameState::Controls) {
        const bool down = input_.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
        if (!down && activeSettingsSlider_ >= 0) { activeSettingsSlider_ = -1; saveSettings(); }
        if (input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT) && hit == ControlActionCount + 2)
            activeSettingsSlider_ = 3;
        if (down && activeSettingsSlider_ == 3) {
            const auto r = MenuLayout::sensitivity(framebufferWidth, framebufferHeight);
            settings_.mouseSensitivity = .03f + .27f * std::clamp(static_cast<float>((cursor.x-r.x-190)/150),0.0f,1.0f);
            if (player_) player_->setMouseSensitivity(settings_.mouseSensitivity);
            return;
        }
    }
    if (ui_.state() == GameState::CreateWorld) {
        if (input_.keyPressed(GLFW_KEY_TAB)) worldNameField_ = 1 - worldNameField_;
        auto& value = worldNameField_ == 0 ? newWorldName_ : newWorldSeed_;
        if (input_.keyDown(GLFW_KEY_LEFT_CONTROL) && input_.keyPressed(GLFW_KEY_A)) value.clear();
        if (input_.keyPressed(GLFW_KEY_BACKSPACE) && !value.empty()) value.pop_back();
        for (unsigned char c : input_.consumeTypedCharacters())
            if (value.size() < (worldNameField_ == 0 ? 48U : 10U) &&
                (worldNameField_ == 0 ? c >= 32 && c < 127 : c >= '0' && c <= '9')) value.push_back(static_cast<char>(c));
        if (input_.keyPressed(GLFW_KEY_ENTER)) { createNamedWorld(); return; }
    }
    if (!input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        return;
    }
    if (hit < 0) {
        return;
    }
    sounds_->playClick();

    if (ui_.state() == GameState::MainMenu) {
        if (hit == 0) {
            if (Persistence::enabled()) {
                refreshWorlds();
                ui_.openWorldSelection();
            } else {
                createWorldAndSystems();
                ui_.resumeGame();
            }
        } else if (hit == 1) {
            ui_.openSettings();
        } else if (hit == 2) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        synchronizeCursorCapture();
        return;
    }

    if (ui_.state() == GameState::WorldSelection) {
        if (hit >= 0 && hit < 6) {
            const int index = worldPage_ * 6 + hit;
            if (index < static_cast<int>(savedWorlds_.size())) {
                selectedWorld_ = index;
                if (lastWorldClickIndex_ == index && glfwGetTime() - lastWorldClick_ < .35)
                    playSavedWorld(savedWorlds_[index]);
                lastWorldClickIndex_ = index;
                lastWorldClick_ = glfwGetTime();
            }
        } else if (hit == 10 && selectedWorld_ >= 0) playSavedWorld(savedWorlds_[selectedWorld_]);
        else if (hit == 11) { newWorldName_ = "New World"; newWorldSeed_.clear(); menuMessage_.clear(); worldNameField_ = 0; ui_.openCreateWorld(); }
        else if (hit == 12) ui_.openMainMenu();
        else if (hit == 13) worldPage_ = std::max(0, worldPage_-1);
        else if (hit == 14) worldPage_ = std::min(std::max(0,(static_cast<int>(savedWorlds_.size())-1)/6),worldPage_+1);
        return;
    }
    if (ui_.state() == GameState::CreateWorld) {
        if (hit < 2) worldNameField_ = hit;
        else if (hit == 2) newWorldMode_ = newWorldMode_ == GameMode::Survival ? GameMode::Creative : GameMode::Survival;
        else if (hit == 3) createNamedWorld();
        else if (hit == 4) { refreshWorlds(); ui_.openWorldSelection(); }
        return;
    }
    if(ui_.state()==GameState::WeatherSettings) {
        switch(hit) {
        case 40: settings_.weatherCycle=!settings_.weatherCycle; break;
        case 41: settings_.weatherQuality=(settings_.weatherQuality+1)%4; break;
        case 43: settings_.snowAccumulation=!settings_.snowAccumulation; break;
        case 44: settings_.lightningEffects=!settings_.lightningEffects; break;
        case 46: settings_.weatherFog=!settings_.weatherFog; break;
        case 47: settings_.weatherWind=!settings_.weatherWind; break;
        case 48: ui_.openSettings(); break;
        }
        saveSettings(); return;
    }
    if (hit >= 30 && hit <= 34) {
        activeSettingsSlider_ = -1;
        saveSettings();
        if (hit == 30) ui_.openVideoSettings();
        if (hit == 31) ui_.openAudioSettings();
        if (hit == 32) ui_.openControls();
        if (hit == 33) ui_.backFromSettings();
        if (hit == 34) ui_.openWeatherSettings();
        return;
    }
    if (ui_.state() == GameState::Controls) {
        if (hit < ControlActionCount)
            activeControlBinding_ = hit;
        else if (hit == ControlActionCount) {
            settings_.controls = defaultControlBindings();
            input_.setBindings(settings_.controls);
            saveSettings();
        } else {
            ui_.openSettings();
        }
        return;
    }

    if (ui_.state() == GameState::PauseMenu) {
        switch (hit) {
        case 0:
            ui_.resumeGame();
            break;
        case 1:
            ui_.openSettings();
            break;
        case 2:
            setGameMode(nextGameMode(gameMode()));
            break;
        case 3:
            resetSeedText_ = std::to_string(seed_);
            resetMode_ = gameMode();
            ui_.openResetWorld();
            break;
        case 4:
            if (Persistence::enabled())
                saveAll();
            else
                saveWarningUntil_ = glfwGetTime() + 4.0;
            break;
        case 5:
            returnToMainMenu();
            break;
        case 6:
            saveOnExit_ = false;
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
            break;
        default:
            break;
        }
        synchronizeCursorCapture();
        return;
    }

    if (ui_.state() == GameState::ResetWorld) {
        switch (hit) {
        case 0:
            resetMode_ = nextGameMode(resetMode_);
            break;
        case 1: {
            std::uint32_t newSeed = 0;
            try {
                newSeed = static_cast<std::uint32_t>(std::stoull(resetSeedText_));
            } catch (...) {
                newSeed = 0;
            }
            resetWorld(newSeed, resetMode_);
            break;
        }
        case 2:
            ui_.openPauseMenu();
            break;
        default:
            break;
        }
        synchronizeCursorCapture();
        return;
    }

    switch (hit) {
    case 4:
        ui_.openAudioSettings();
        return;
    case 6:
        settings_.antiAliasingSamples = settings_.antiAliasingSamples == 0
                                            ? 2
                                            : (settings_.antiAliasingSamples == 2 ? 4 : 0);
        destroyRenderTarget();
        break;
    case 7:
        settings_.fullscreen = !settings_.fullscreen;
        applyFullscreenSetting();
        break;
    case 8:
        settings_.vsync = !settings_.vsync;
        glfwSwapInterval(settings_.vsync ? 1 : 0);
        break;
    case 9:
        settings_.showFps = !settings_.showFps;
        break;
    case 10:
        settings_.showCoordinates = !settings_.showCoordinates;
        break;
    case 11:
        settings_.applyPreset(settings_.graphicsPreset == GraphicsPreset::Low
                                  ? GraphicsPreset::Medium
                                  : settings_.graphicsPreset == GraphicsPreset::Medium
                                        ? GraphicsPreset::High
                                        : GraphicsPreset::Low);
        if (world_) {
            world_->setRenderDistance(settings_.renderDistance);
            world_->setSimulationDistance(settings_.simulationDistance);
        }
        renderer_->setParticlePercent(settings_.particlePercent);
        renderer_->setEffectQuality(settings_.effectQuality);
        destroyRenderTarget();
        break;
    case 13:
        settings_.frameLimit = settings_.frameLimit == 0
                                   ? 30
                                   : (settings_.frameLimit == 30
                                          ? 60
                                          : (settings_.frameLimit == 60 ? 120 : 0));
        settings_.graphicsPreset = GraphicsPreset::Custom;
        break;
    case 14:
        ui_.openControls();
        break;
    case 15:
        ui_.openSettings();
        break;
    default:
        break;
    }
    if (hit >= 6 && hit <= 10)
        settings_.graphicsPreset = GraphicsPreset::Custom;
    saveSettings();
}

void Game::updateSimulation(float deltaTime) {
    if (!world_ || ui_.simulationPaused()) {
        interaction_.hasBlockTarget = false;
        interaction_.mobTarget = {};
        return;
    }

    timing_.worldTime += deltaTime;
    interaction_.attackCooldown = std::max(0.0f, interaction_.attackCooldown - deltaTime);
    interaction_.cameraKickDegrees =
        decayHitRecoilDegrees(interaction_.cameraKickDegrees, deltaTime);

    const bool chestWasOpen = ui_.state() == GameState::Chest;
    const glm::ivec3 chestPosition = ui_.openedContainerPosition();
    if (!ui_.validateOpenBlock(*world_, *player_, *inventory_)) {
        if (chestWasOpen)
            sounds_->playChest(false, glm::vec3(chestPosition) + glm::vec3(.5f));
        synchronizeCursorCapture();
    }
    updateTargets();

    const float oldHealth = player_->health();
    world_->updateStreaming(player_->cameraPosition(), 2);
    world_->updateFluids(deltaTime);
    world_->updateBlockEntities(deltaTime);
    const bool playerInputEnabled =
        ui_.state() == GameState::Playing && input_.cursorCaptured() && weatherBenchmark_.empty();
    const bool wasDead = player_->isDead();
    const bool wasBelowWorld = player_->position().y < -24.0f;
    player_->update(deltaTime, input_.playerInput(playerInputEnabled), *world_);
    if ((wasDead || wasBelowWorld) && !player_->isDead() && player_->position().y >= 0.0f)
        world_->prepareSpawnTerrain(player_->position());
    weather_->update(deltaTime, *world_, *player_, *survival_, *sounds_, settings_);
    farming_->update(deltaTime, *world_, player_->position());

    if (ui_.gameplayInterfaceOpen()) {
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
        if (!spectatorMode_) {
            ui_.updateInventoryInteraction(input_,
                                           framebufferWidth,
                                           framebufferHeight,
                                           *inventory_,
                                           *survival_,
                                           *world_,
                                           *player_,
                                           creativeMode_);
        }
        interaction_.breakProgress = 0.0f;
        interaction_.hasMiningTarget = false;
    } else if (ui_.state() == GameState::Playing && input_.cursorCaptured() &&
               !player_->isDead() && !spectatorMode_) {
        updatePlayingInteraction(deltaTime);
    }

    finishSimulationFrame(deltaTime, oldHealth);
}

void Game::updateTargets() {
    interaction_.hasBlockTarget = false;
    interaction_.mobTarget = {};
    if (ui_.state() != GameState::Playing || spectatorMode_) {
        return;
    }

    interaction_.hasBlockTarget = world_->raycast(
        player_->cameraPosition(), player_->lookDirection(), 6.0f, interaction_.blockTarget);
    const float blockDistance =
        interaction_.hasBlockTarget ? interaction_.blockTarget.distance : 6.0f;
    interaction_.mobTarget = survival_->raycastMob(
        player_->cameraPosition(), player_->lookDirection(), 5.0f, *world_, blockDistance);
}

void Game::updatePlayingInteraction(float deltaTime) {
    if ((input_.mousePressed(GLFW_MOUSE_BUTTON_MIDDLE) ||
         input_.actionPressed(ControlAction::PickBlock)) && interaction_.hasBlockTarget) {
        const glm::ivec3& position = interaction_.blockTarget.block;
        inventory_->pickBlock(blockToItem(world_->getBlock(position.x, position.y, position.z)),
                              creativeMode_);
    }
    handleDropAction();
    updateMobAttack();
    updateMining(deltaTime);
    handleUseAction();
}

void Game::handleDropAction() {
    if (!input_.actionPressed(ControlAction::Drop)) {
        return;
    }

    const bool entireStack = input_.actionDown(ControlAction::Sprint) ||
                             input_.keyDown(GLFW_KEY_LEFT_CONTROL) ||
                             input_.keyDown(GLFW_KEY_RIGHT_CONTROL);
    ItemStack dropped;
    if (creativeMode_) {
        dropped = inventory_->selectedStack();
        if (!entireStack && !dropped.empty())
            dropped.count = 1;
    } else {
        dropped = inventory_->takeSelected(entireStack);
    }
    if (dropped.empty()) {
        return;
    }

    const glm::vec3 lookDirection = player_->lookDirection();
    const glm::vec3 dropPosition =
        player_->cameraPosition() + lookDirection * 0.75f - glm::vec3(0.0f, 0.2f, 0.0f);
    const glm::vec3 dropVelocity = lookDirection * 4.0f + glm::vec3(0.0f, 2.2f, 0.0f);
    survival_->spawnDrop(dropPosition,
                         dropped.item,
                         dropped.count,
                         dropped.durability,
                         dropVelocity,
                         1.0f);
    interaction_.heldItemSwing = 0.55f;
}

void Game::damageSelectedToolWithSound() {
    const ItemStack before = inventory_->selectedStack();
    if (!isTool(before.item)) return;
    inventory_->damageSelectedTool();
    if (before.durability <= 1 && inventory_->selectedStack().item != before.item)
        sounds_->playToolBreak();
}

void Game::updateMobAttack() {
    if (!input_.mouseDown(GLFW_MOUSE_BUTTON_LEFT) || !interaction_.mobTarget.valid() ||
        interaction_.attackCooldown > 0.0f) {
        return;
    }

    interaction_.heldItemSwing = 1.0f;
    const Item weapon = inventory_->selectedItem();
    const bool critical = player_->isFalling();
    const AttackResult hit =
        survival_->attackMob(interaction_.mobTarget.index, weapon, critical, player_->position());
    interaction_.attackCooldown = attackCooldown(weapon);
    if (!hit.hit) {
        return;
    }

    if (isTool(weapon) && !creativeMode_) {
        damageSelectedToolWithSound();
    }
    renderer_->spawnHitParticles(hit.position, critical);
    interaction_.cameraKickDegrees =
        critical ? CriticalHitRecoilDegrees : NormalHitRecoilDegrees;
    interaction_.breakProgress = 0.0f;
    interaction_.hasMiningTarget = false;
}

void Game::updateMining(float deltaTime) {
    const bool breakInput = creativeMode_ ? input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT)
                                          : input_.mouseDown(GLFW_MOUSE_BUTTON_LEFT);
    if (!breakInput || !interaction_.hasBlockTarget || interaction_.mobTarget.valid()) {
        interaction_.breakProgress = 0.0f;
        interaction_.hasMiningTarget = false;
        return;
    }

    if (!interaction_.hasMiningTarget ||
        interaction_.blockTarget.block != interaction_.miningTarget) {
        interaction_.miningTarget = interaction_.blockTarget.block;
        interaction_.breakProgress = 0.0f;
        interaction_.hasMiningTarget = true;
    }

    const Block block = world_->getBlock(interaction_.blockTarget.block.x,
                                         interaction_.blockTarget.block.y,
                                         interaction_.blockTarget.block.z);
    if (creativeMode_) {
        interaction_.breakProgress = 1.0f;
    } else {
        const float toolMultiplier = toolBreakMultiplier(inventory_->selectedItem(), block);
        interaction_.breakProgress +=
            deltaTime / std::max(0.06f, blockHardness(block) / toolMultiplier);
        if (isCrop(block)) {
            interaction_.breakProgress = 1.0f;
        }
    }
    if (interaction_.breakProgress < 1.0f) {
        return;
    }

    const glm::ivec3 brokenPosition = interaction_.blockTarget.block;
    const Block supportedDoor = world_->getBlock(
        brokenPosition.x, brokenPosition.y + 1, brokenPosition.z);
    const bool removingDoorSupport = !isDoor(block) && isDoor(supportedDoor) &&
                                     !isDoorUpper(supportedDoor);
    const std::vector<ItemStack> storedItems = world_->takeBlockEntityContents(brokenPosition);
    glm::ivec3 dropBlockPosition = brokenPosition;
    if (isDoor(block) && block != Block::WoodenDoor) {
        const glm::ivec3 lowerPosition =
            isDoorUpper(block) ? brokenPosition - glm::ivec3(0, 1, 0) : brokenPosition;
        world_->setBlock(lowerPosition.x, lowerPosition.y + 1, lowerPosition.z, Block::Air);
        world_->setBlock(lowerPosition.x, lowerPosition.y, lowerPosition.z, Block::Air);
        dropBlockPosition = lowerPosition;
    } else {
        world_->setBlock(brokenPosition.x, brokenPosition.y, brokenPosition.z, Block::Air);
    }
    renderer_->spawnBreakParticles(interaction_.blockTarget.block, block);
    const glm::vec3 dropPosition = glm::vec3(dropBlockPosition) + glm::vec3(0.5f);

    if (!creativeMode_) {
        if (removingDoorSupport) {
            survival_->spawnDrop(glm::vec3(brokenPosition) + glm::vec3(.5f, 1.5f, .5f),
                                 Item::WoodenDoor);
        }
        if (block == Block::Crop3) {
            survival_->spawnDrop(
                dropPosition, Item::Wheat, 1 + (interaction_.blockTarget.block.x & 1));
            survival_->spawnDrop(
                dropPosition, Item::Seeds, 1 + std::abs(interaction_.blockTarget.block.z % 3));
            survival_->spawnExperience(dropPosition, 2);
        } else if (isCrop(block)) {
            survival_->spawnDrop(dropPosition, Item::Seeds, 1);
        } else {
            const bool valuableOre = isOre(block) || blockDefinition(block).requiredHarvestTier > 0;
            if (!valuableOre || canHarvestBlock(inventory_->selectedItem(), block)) {
                survival_->spawnDrop(dropPosition,
                                     block == Block::Farmland ? Item::Dirt : blockToItem(block),
                                     blockDefinition(block).dropCount);
            }
            if (block == Block::Leaves) {
                const std::uint32_t appleHash =
                    static_cast<std::uint32_t>(interaction_.blockTarget.block.x) * 73856093U ^
                    static_cast<std::uint32_t>(interaction_.blockTarget.block.y) * 19349663U ^
                    static_cast<std::uint32_t>(interaction_.blockTarget.block.z) * 83492791U ^
                    seed_;
                if ((appleHash & 15U) == 0U) {
                    survival_->spawnDrop(dropPosition, Item::Apple);
                }
            }
        }

        for (const ItemStack& stack : storedItems)
            survival_->spawnDrop(dropPosition, stack.item, stack.count, stack.durability);

        if (isOre(block) && canHarvestBlock(inventory_->selectedItem(), block)) {
            survival_->spawnExperience(dropPosition, oreExperience(block));
        }
        if (isTool(inventory_->selectedItem())) {
            damageSelectedToolWithSound();
        }
    }

    sounds_->playBlockBreak(block, glm::vec3(brokenPosition) + glm::vec3(.5f));
    interaction_.breakProgress = 0.0f;
    interaction_.hasMiningTarget = false;
}

void Game::handleUseAction() {
    if (!input_.mousePressed(GLFW_MOUSE_BUTTON_RIGHT)) {
        return;
    }

    const Item heldItem = inventory_->selectedItem();
    bool used = false;
    bool placed = false;

    const bool placeAgainstContainer = input_.actionDown(ControlAction::Sneak) &&
                                       itemToBlock(heldItem) != Block::Air;
    if (interaction_.hasBlockTarget && !placeAgainstContainer) {
        const Block usedBlock = world_->getBlock(interaction_.blockTarget.block.x,
                                                  interaction_.blockTarget.block.y,
                                                  interaction_.blockTarget.block.z);
        if (isDoor(usedBlock)) {
            const glm::ivec3 lowerPosition = isDoorUpper(usedBlock)
                                                   ? interaction_.blockTarget.block - glm::ivec3(0, 1, 0)
                                                   : interaction_.blockTarget.block;
            const int facing = doorFacing(usedBlock);
            const bool hingeRight = doorHingeRight(usedBlock);
            const bool open = !isDoorOpen(usedBlock);
            world_->setBlock(lowerPosition.x,
                             lowerPosition.y,
                             lowerPosition.z,
                             doorBlock(facing, false, hingeRight, open));
            const Block upper = world_->getBlock(
                lowerPosition.x, lowerPosition.y + 1, lowerPosition.z);
            if (upper == Block::Air || (isDoor(upper) && isDoorUpper(upper))) {
                world_->setBlock(lowerPosition.x,
                                 lowerPosition.y + 1,
                                 lowerPosition.z,
                                 doorBlock(facing, true, hingeRight, open));
            }
            used = true;
            sounds_->playDoor(open, glm::vec3(lowerPosition) + glm::vec3(.5f, 1.0f, .5f));
        } else if (usedBlock == Block::CraftingTable)
            ui_.openCraftingTable(interaction_.blockTarget.block);
        else if (usedBlock == Block::Furnace)
            ui_.openFurnace(interaction_.blockTarget.block);
        else if (usedBlock == Block::Chest) {
            ui_.openChest(interaction_.blockTarget.block);
            sounds_->playChest(true, glm::vec3(interaction_.blockTarget.block) +
                                          glm::vec3(.5f));
        }
        if (usedBlock == Block::CraftingTable || usedBlock == Block::Furnace ||
            usedBlock == Block::Chest) {
            synchronizeCursorCapture();
            interaction_.breakProgress = 0.0f;
            interaction_.hasMiningTarget = false;
            used = true;
        }
    }

    if (!used &&
        survival_->feedAnimal(player_->cameraPosition(), player_->lookDirection(), heldItem)) {
        if (!creativeMode_) {
            inventory_->consumeSelected();
        }
        interaction_.heldItemSwing = 1.0f;
        used = true;
    }
    if (!used && isFood(heldItem) && player_->hunger() < 20.0f) {
        player_->eat(foodValue(heldItem));
        sounds_->playEat();
        if (!creativeMode_) {
            inventory_->consumeSelected();
        }
        interaction_.heldItemSwing = 1.0f;
        used = true;
    }
    if (!used && interaction_.hasBlockTarget) {
        const glm::ivec3& targetPosition = interaction_.blockTarget.block;
        const glm::ivec3& adjacentPosition = interaction_.blockTarget.adjacent;
        const Block targetBlock =
            world_->getBlock(targetPosition.x, targetPosition.y, targetPosition.z);

        if (heldItem == Item::Seeds && (targetBlock == Block::Farmland || targetBlock == Block::WetFarmland) &&
            world_->getBlock(adjacentPosition.x, adjacentPosition.y, adjacentPosition.z) ==
                Block::Air) {
            farming_->plant(*world_, adjacentPosition);
            if (!creativeMode_) {
                inventory_->consumeSelected();
            }
            used = true;
            placed = true;
        } else if ((heldItem == Item::WoodShovel || heldItem == Item::StoneShovel ||
                    heldItem == Item::IronShovel || heldItem == Item::GoldShovel ||
                    heldItem == Item::DiamondShovel) &&
                   (targetBlock == Block::Dirt || targetBlock == Block::Grass) &&
                   interaction_.blockTarget.normal.y > 0) {
            world_->setBlock(targetPosition.x, targetPosition.y, targetPosition.z, Block::Farmland);
            if (!creativeMode_) {
                damageSelectedToolWithSound();
            }
            used = true;
            placed = true;
        } else {
            Block placedBlock = itemToBlock(heldItem);
            const glm::vec3 blockHitPoint = player_->cameraPosition() +
                                            player_->lookDirection() *
                                                (interaction_.blockTarget.distance + 0.001f);
            const float targetLocalY = blockHitPoint.y - std::floor(blockHitPoint.y);
            const bool slabHalvesMeet =
                (isTopSlab(targetBlock) &&
                 (interaction_.blockTarget.normal.y < 0 || targetLocalY < 0.5f)) ||
                (!isTopSlab(targetBlock) &&
                 (interaction_.blockTarget.normal.y > 0 || targetLocalY > 0.5f));
            if (slabHalvesMeet &&
                ((isWoodenSlab(targetBlock) && heldItem == Item::WoodenSlab) ||
                 (isStoneSlab(targetBlock) && heldItem == Item::StoneSlab))) {
                const Block combinedBlock =
                    heldItem == Item::WoodenSlab ? Block::Planks : Block::Stone;
                world_->setBlock(targetPosition.x,
                                 targetPosition.y,
                                 targetPosition.z,
                                 combinedBlock);
                if (!creativeMode_)
                    inventory_->consumeSelected();
                renderer_->spawnBreakParticles(targetPosition, combinedBlock);
                used = true;
                placed = true;
            }
            if (used)
                placedBlock = Block::Air;

            if (heldItem == Item::Ladder) {
                const glm::ivec3 normal = interaction_.blockTarget.normal;
                if (normal.y != 0 || !isSolid(targetBlock))
                    placedBlock = Block::Air;
                else if (normal.z > 0)
                    placedBlock = Block::LadderNorth;
                else if (normal.z < 0)
                    placedBlock = Block::LadderSouth;
                else if (normal.x > 0)
                    placedBlock = Block::LadderWest;
                else
                    placedBlock = Block::LadderEast;
            }
            if (heldItem == Item::Vine) {
                const glm::ivec3 normal = interaction_.blockTarget.normal;
                if (normal.y != 0) placedBlock = Block::Air;
                else if (normal.z > 0) placedBlock = Block::VineNorth;
                else if (normal.z < 0) placedBlock = Block::VineSouth;
                else if (normal.x > 0) placedBlock = Block::VineWest;
                else placedBlock = Block::VineEast;
            }
            if ((placedBlock == Block::SugarCane || isVine(placedBlock)) &&
                !world_->canPlacePlant(adjacentPosition, placedBlock))
                placedBlock = Block::Air;
            if (isSlab(placedBlock)) {
                const bool placeTop = interaction_.blockTarget.normal.y < 0 ||
                                      (interaction_.blockTarget.normal.y == 0 &&
                                       targetLocalY > 0.5f);
                if (placeTop) {
                    placedBlock = isWoodenSlab(placedBlock) ? Block::WoodenSlabTop
                                                            : Block::StoneSlabTop;
                }
            }

            const Block destination =
                world_->getBlock(adjacentPosition.x, adjacentPosition.y, adjacentPosition.z);
            auto placementIntersectsPlayer = [&](const glm::ivec3& position,
                                                  Block candidate) {
                const BlockGeometryProperties geometry = blockGeometry(candidate);
                const glm::vec3 minimum = glm::vec3(position) +
                    glm::vec3(geometry.minX, geometry.minY, geometry.minZ);
                const glm::vec3 maximum = glm::vec3(position) +
                    glm::vec3(geometry.maxX, geometry.maxY, geometry.maxZ);
                const glm::vec3 playerMinimum = player_->aabbMinimum();
                const glm::vec3 playerMaximum = player_->aabbMaximum();
                return minimum.x < playerMaximum.x && maximum.x > playerMinimum.x &&
                       minimum.y < playerMaximum.y && maximum.y > playerMinimum.y &&
                       minimum.z < playerMaximum.z && maximum.z > playerMinimum.z;
            };
            const bool obstructsPlayer =
                isSolid(placedBlock) && placementIntersectsPlayer(adjacentPosition, placedBlock);

            if (placedBlock == Block::WoodenDoor && destination == Block::Air &&
                adjacentPosition.y + 1 < WORLD_HEIGHT &&
                world_->getBlock(adjacentPosition.x, adjacentPosition.y + 1, adjacentPosition.z) ==
                    Block::Air &&
                isSolid(world_->getBlock(
                    adjacentPosition.x, adjacentPosition.y - 1, adjacentPosition.z)) &&
                blockGeometry(world_->getBlock(
                    adjacentPosition.x, adjacentPosition.y - 1, adjacentPosition.z)).maxY >=
                    1.0f) {
                const glm::vec3 look = player_->lookDirection();
                int facing = 0;
                if (std::abs(look.x) > std::abs(look.z))
                    facing = look.x > 0.0f ? 2 : 3;
                else
                    facing = look.z > 0.0f ? 1 : 0;
                // Facing order is N, S, E, W; these offsets are the player's
                // left and right as viewed along the placement direction.
                constexpr int leftX[4] = {-1, 1, 0, 0};
                constexpr int leftZ[4] = {0, 0, -1, 1};
                const glm::ivec3 left(leftX[facing], 0, leftZ[facing]);
                const glm::ivec3 right = -left;
                auto sideSupport = [&](const glm::ivec3& side) {
                    int support = 0;
                    for (int height = 0; height < 2; ++height) {
                        const glm::ivec3 at = adjacentPosition + side +
                                              glm::ivec3(0, height, 0);
                        const Block neighbor = world_->getBlock(at.x, at.y, at.z);
                        support += isSolid(neighbor) && !isDoor(neighbor) ? 1 : 0;
                    }
                    return support;
                };
                auto neighboringDoor = [&](const glm::ivec3& side) {
                    const glm::ivec3 at = adjacentPosition + side;
                    const Block neighbor = world_->getBlock(at.x, at.y, at.z);
                    return isDoor(neighbor) && !isDoorUpper(neighbor) &&
                           doorFacing(neighbor) == facing;
                };
                bool hingeRight = false;
                if (neighboringDoor(left) != neighboringDoor(right)) {
                    hingeRight = neighboringDoor(left);
                } else if (sideSupport(left) != sideSupport(right)) {
                    hingeRight = sideSupport(right) > sideSupport(left);
                } else {
                    const glm::vec3 local = blockHitPoint -
                        (glm::vec3(interaction_.blockTarget.block) + glm::vec3(.5f));
                    const float lateral = local.x * static_cast<float>(left.x) +
                                          local.z * static_cast<float>(left.z);
                    hingeRight = lateral < 0.0f;
                }
                const Block lowerDoor = doorBlock(facing, false, hingeRight, false);
                const Block upperDoor = doorBlock(facing, true, hingeRight, false);
                if (!placementIntersectsPlayer(adjacentPosition, lowerDoor) &&
                    !placementIntersectsPlayer(adjacentPosition + glm::ivec3(0, 1, 0),
                                               upperDoor)) {
                    world_->setBlock(adjacentPosition.x,
                                     adjacentPosition.y,
                                     adjacentPosition.z,
                                     lowerDoor);
                    world_->setBlock(adjacentPosition.x,
                                     adjacentPosition.y + 1,
                                     adjacentPosition.z,
                                     upperDoor);
                    if (!creativeMode_)
                        inventory_->consumeSelected();
                    renderer_->spawnBreakParticles(adjacentPosition, lowerDoor);
                    used = true;
                    placed = true;
                }
                placedBlock = Block::Air;
            }
            if (placedBlock != Block::Air &&
                (destination == Block::Air || isWater(destination)) && !obstructsPlayer) {
                world_->setBlock(
                    adjacentPosition.x, adjacentPosition.y, adjacentPosition.z, placedBlock);
                if (!creativeMode_) {
                    inventory_->consumeSelected();
                }
                renderer_->spawnBreakParticles(adjacentPosition, placedBlock);
                used = true;
                placed = true;
            }
        }
    }

    if (used && placed) {
        interaction_.heldItemSwing = 1.0f;
        const glm::ivec3 position = interaction_.blockTarget.adjacent;
        const Block placedMaterial = world_->getBlock(position.x, position.y, position.z);
        const glm::ivec3 target = interaction_.blockTarget.block;
        const Block targetMaterial = world_->getBlock(target.x, target.y, target.z);
        const Block soundMaterial = placedMaterial != Block::Air ? placedMaterial :
            itemToBlock(heldItem) != Block::Air ? itemToBlock(heldItem) : targetMaterial;
        sounds_->playBlockPlace(soundMaterial, glm::vec3(position) + glm::vec3(.5f));
    }
}

void Game::finishSimulationFrame(float deltaTime, float oldHealth) {
    sounds_->setListener(player_->cameraPosition(), player_->lookDirection());
    survival_->update(deltaTime, *world_, *player_, *inventory_, weather_->stormSpawning() ? 0.0f : daylight(timing_.worldTime));
    for (const glm::vec3& center : survival_->takeExplosionEffects())
        renderer_->spawnExplosionParticles(center);

    if (player_->health() < oldHealth) {
        if (player_->lastFallDamage() > 0.0f)
            sounds_->playFallDamage(player_->lastFallDamage() >= 6.0f);
        else
            sounds_->playPlayerHurt();
    }

    const bool inWater = player_->isSwimming();
    if (inWater && !wasInWater_ && glm::length(player_->velocity()) > 1.25f) {
        sounds_->playSplash(player_->position());
    }
    wasInWater_ = inWater;

    const bool walking = playerIsWalking();
    if (walking && !spectatorMode_) {
        timing_.bobTime += deltaTime * (player_->isSprinting() ? 13.0f : 9.0f);
    }
    timing_.stepTimer -= deltaTime;
    if (walking && !spectatorMode_ && timing_.stepTimer <= 0.0f) {
        const glm::vec3 position = player_->position();
        const Block support = world_->getBlock(
            static_cast<int>(std::floor(position.x)),
            static_cast<int>(std::floor(position.y - .12f)),
            static_cast<int>(std::floor(position.z)));
        sounds_->playFootstep(support, position, player_->isSneaking());
        timing_.stepTimer = player_->isSprinting() ? 0.28f : 0.42f;
    }

    renderer_->updateParticles(deltaTime);
    interaction_.heldItemSwing = std::max(0.0f, interaction_.heldItemSwing - deltaTime * 4.0f);
    if (Persistence::enabled() && glfwGetTime() - timing_.lastSave >= 12.0 &&
        !player_->isDead()) {
        saveAll();
    }
}

void Game::renderFrame(float deltaTime) {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }

    if (!world_) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const auto cursor = input_.framebufferCursorPosition();
        renderer_->renderMainMenu(width, height,
            ui_.hoveredMenuItem(cursor, width, height),
            input_.mouseDown(GLFW_MOUSE_BUTTON_LEFT),
            ui_.state() == GameState::MainMenu, !Persistence::enabled(), glfwGetTime());
        if (ui_.state() != GameState::MainMenu) renderMenuInterface(width, height);
        if (screenshotRequested_) {
            Screenshot::saveBmp(width, height);
            screenshotRequested_ = false;
        }
        glfwSwapBuffers(window_);
        return;
    }
    const bool zooming = zoomActive();
    const float targetFov = zooming
                                ? zoomFov_
                                : settings_.fov +
                                  (!spectatorMode_ && player_->isSprinting() ? 8.0f : 0.0f);
    currentFov_ = glm::mix(
        currentFov_, targetFov, 1.0f - std::exp(-(zooming ? 11.0f : 8.0f) * deltaTime));

    prepareRenderTarget(width, height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float farPlane = std::max(
        230.0f, static_cast<float>((world_->renderDistance() + 3) * CHUNK_SIZE) * 1.45f);
    const glm::mat4 projection = glm::perspective(
        glm::radians(currentFov_), static_cast<float>(width) / height, 0.10f, farPlane);
    glm::mat4 view = player_->viewMatrix();
    if (!spectatorMode_)
        view = applyHitRecoil(view, interaction_.cameraKickDegrees);
    if (!spectatorMode_ && playerIsWalking()) {
        const float strength = player_->isSprinting() ? 1.35f : 1.0f;
        view = glm::translate(view,
                              glm::vec3(std::sin(timing_.bobTime) * 0.012f * strength,
                                        std::abs(std::cos(timing_.bobTime)) * 0.018f * strength,
                                        0.0f));
    }

    const bool underwater = player_->isSwimming();
    renderer_->setWeather(weather_->intensity(), weather_->storm(),
        settings_.lightningEffects && settings_.weatherQuality > 0 ? weather_->flash()*settings_.lightningFlash : 0,
        settings_.weatherFog && settings_.weatherQuality >= 2, weather_->wetness());
    renderer_->renderSky(view, projection, timing_.worldTime);
    renderer_->renderWorld(*world_,
                           view,
                           projection,
                           player_->cameraPosition(),
                           timing_.worldTime,
                           settings_.brightness,
                           fullbright_,
                           underwater,
                           spectatorMode_ && isSolid(world_->getBlock(
                               static_cast<int>(std::floor(player_->cameraPosition().x)),
                               static_cast<int>(std::floor(player_->cameraPosition().y)),
                               static_cast<int>(std::floor(player_->cameraPosition().z)))));
    const float entityDistance = static_cast<float>(settings_.entityDistance * CHUNK_SIZE);
    thread_local std::vector<RenderCuboid> cuboidStorage;
    thread_local std::vector<RenderBillboard> billboardStorage;
    thread_local std::vector<RenderItemSprite> itemStorage;
    RenderScratch<RenderCuboid> cuboids(cuboidStorage, 256 * 1024);
    RenderScratch<RenderBillboard> billboards(billboardStorage, 256 * 1024);
    RenderScratch<RenderItemSprite> items(itemStorage, 256 * 1024);
    survival_->renderCuboids(cuboids.get());
    survival_->renderBillboards(billboards.get());
    survival_->renderItemSprites(items.get());
    renderer_->renderEntities(cuboids.get(), view, projection, timing_.worldTime, entityDistance);
    renderer_->renderBillboards(billboards.get(), *world_, view, projection,
                                timing_.worldTime, settings_.brightness, fullbright_, entityDistance);
    renderer_->renderItemSprites(items.get(), view, projection, timing_.worldTime, entityDistance);
    renderer_->renderParticles(view, projection, entityDistance);
    weatherRenderer_->render(*weather_, *world_, settings_, player_->cameraPosition(),
                             view, projection, timing_.worldTime, daylight(timing_.worldTime));

    if (!zooming) {
        if (!spectatorMode_ && ui_.state() == GameState::Playing &&
            interaction_.mobTarget.valid()) {
            renderer_->renderMobOutline(
                survival_->mobOutline(interaction_.mobTarget.index), view, projection);
        } else if (!spectatorMode_ && ui_.state() == GameState::Playing &&
                   interaction_.hasBlockTarget) {
            renderer_->renderSelectionOutline(
                interaction_.blockTarget.block,
                world_->getBlock(interaction_.blockTarget.block.x,
                                 interaction_.blockTarget.block.y,
                                 interaction_.blockTarget.block.z),
                view, projection);
        }

        const std::string status =
            player_->isDead()
                ? "YOU DIED  RESPAWNING"
                : (spectatorMode_
                       ? "SPECTATOR"
                       : creativeMode_ && player_->isFlying()
                       ? "CREATIVE  FLYING"
                       : (fullbright_ ? "FULLBRIGHT ON" : ""));
        renderer_->renderHud(width,
                             height,
                             *inventory_,
                             player_->health(),
                             player_->hurtFlash(),
                             interaction_.breakProgress,
                             buildDebugText(),
                             status,
                             spectatorMode_);
        if (!spectatorMode_) {
            renderer_->renderSurvivalUi(width,
                                        height,
                                        *inventory_,
                                        player_->hunger(),
                                        player_->xpProgress(),
                                        player_->xpLevel(),
                                        fullbright_);
        }

        if (!spectatorMode_ && ui_.state() == GameState::Playing) {
            renderer_->renderHeldItem(
                width, height, inventory_->selectedStack(), interaction_.heldItemSwing);
        }
    }
    if (underwater) {
        renderer_->renderUnderwaterOverlay(width, height);
    }

    const glm::dvec2 cursor = input_.framebufferCursorPosition();
    if (ui_.gameplayInterfaceOpen()) {
        if (input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT) ||
            input_.mousePressed(GLFW_MOUSE_BUTTON_RIGHT))
            sounds_->playClick();
        if (creativeMode_ && ui_.state() == GameState::Inventory) {
            renderer_->renderCreativeInventory(width, height, *inventory_, cursor.x, cursor.y);
        } else if (ui_.state() == GameState::Furnace) {
            if (const FurnaceData* furnace = world_->furnaceAt(ui_.openedContainerPosition(), false)) {
                renderer_->renderFurnace(width, height, *inventory_, *furnace, cursor.x, cursor.y);
            }
        } else if (ui_.state() == GameState::Chest) {
            if (const ChestData* chest = world_->chestAt(ui_.openedContainerPosition(), false)) {
                renderer_->renderChest(width, height, *inventory_, *chest, cursor.x, cursor.y);
            }
        } else {
            renderer_->renderInventory(
                width, height, *inventory_, ui_.craftingTableOpen(),
                ui_.recipeBook(), cursor.x, cursor.y);
        }
    }
    if (ui_.simulationPaused()) renderMenuInterface(width, height);

    renderer_->renderChat(width, height, chat_, glfwGetTime());
    resolveRenderTarget(width, height);
    if (screenshotRequested_) {
        Screenshot::saveBmp(width, height);
        screenshotRequested_ = false;
    }
    if (smokeTest_.worldgenEnabled) {
        const double elapsed = glfwGetTime() - smokeTest_.startTime;
        if (elapsed > 4.0 && !smokeTest_.worldgenScreenshotTaken) {
            Screenshot::saveBmp(width, height);
            smokeTest_.worldgenScreenshotTaken = true;
        }
    }
    glfwSwapBuffers(window_);

    if (smokeTest_.uiEnabled && glfwGetTime() - smokeTest_.startTime > 22.0) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
    if (smokeTest_.worldgenEnabled) {
        const double elapsed = glfwGetTime() - smokeTest_.startTime;
        if (elapsed > 5.0)
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
    if (smokeTest_.billboardPreview && glfwGetTime() - smokeTest_.startTime > 8.0)
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    if (smokeTest_.commandVisual && glfwGetTime() - smokeTest_.startTime > 2.0)
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    if (smokeTest_.craftingPreview && glfwGetTime() - smokeTest_.startTime > 24.0)
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

bool Game::zoomActive() const {
    return ui_.state() == GameState::Playing && input_.cursorCaptured() &&
           input_.actionDown(ControlAction::Zoom);
}

void Game::prepareRenderTarget(int width, int height) {
    const int samples = settings_.antiAliasingSamples;
    if (samples == 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    if (multisampleFramebuffer_ != 0 && multisampleWidth_ == width &&
        multisampleHeight_ == height && multisampleSamples_ == samples) {
        glBindFramebuffer(GL_FRAMEBUFFER, multisampleFramebuffer_);
        return;
    }

    destroyRenderTarget();
    glGenFramebuffers(1, &multisampleFramebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, multisampleFramebuffer_);

    glGenRenderbuffers(1, &multisampleColorBuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, multisampleColorBuffer_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleColorBuffer_);

    glGenRenderbuffers(1, &multisampleDepthBuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, multisampleDepthBuffer_);
    glRenderbufferStorageMultisample(
        GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, multisampleDepthBuffer_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        destroyRenderTarget();
        settings_.antiAliasingSamples = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    multisampleWidth_ = width;
    multisampleHeight_ = height;
    multisampleSamples_ = samples;
}

void Game::resolveRenderTarget(int width, int height) {
    if (multisampleFramebuffer_ == 0)
        return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleFramebuffer_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(
        0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Game::destroyRenderTarget() {
    if (multisampleDepthBuffer_ != 0)
        glDeleteRenderbuffers(1, &multisampleDepthBuffer_);
    if (multisampleColorBuffer_ != 0)
        glDeleteRenderbuffers(1, &multisampleColorBuffer_);
    if (multisampleFramebuffer_ != 0)
        glDeleteFramebuffers(1, &multisampleFramebuffer_);
    multisampleFramebuffer_ = multisampleColorBuffer_ = multisampleDepthBuffer_ = 0;
    multisampleWidth_ = multisampleHeight_ = multisampleSamples_ = 0;
}

std::string Game::buildDebugText() const {
    if (!showDebug_) {
        return {};
    }

    const glm::vec3 position = player_->position();
    const glm::vec3 direction = player_->lookDirection();
    std::ostringstream text;
    if (settings_.showFps) {
        text << std::fixed << std::setprecision(1) << "FPS " << timing_.fps << '\n';
    }
    if (settings_.showCoordinates) {
        text << "XYZ " << position.x << " / " << position.y << " / " << position.z << '\n'
             << "CHUNK " << chunkCoordinate(position.x) << " / " << chunkCoordinate(position.z)
             << '\n';
    }

    const int blockX = static_cast<int>(position.x);
    const int blockY = static_cast<int>(position.y + 1.0f);
    const int blockZ = static_cast<int>(position.z);
    const WorldGenerationDebug generation = world_->generationDebugAt(
        static_cast<int>(std::floor(position.x)),
        static_cast<int>(std::floor(position.z)),
        static_cast<int>(std::floor(position.y)), showWorldgenDebug_);
    const SurvivalWorld::MobDiagnostics mobs = survival_->diagnostics(position);
    text << "BIOME " << generation.biome << '\n'
         << "FACING " << facingDirection(direction) << '\n'
         << "MODE " << gameModeName(gameMode()) << '\n';
    if (world_->generationVersion() >= 2) {
        text << std::fixed << std::setprecision(2)
             << "CONTINENTAL " << generation.continentalness
             << " EROSION " << generation.erosion << '\n'
             << "TEMPERATURE " << generation.temperature
             << " HUMIDITY " << generation.humidity << '\n'
             << "WEIRDNESS " << generation.weirdness
             << " PEAK/VALLEY " << generation.peakValley << '\n'
             << "TERRAIN HEIGHT " << generation.terrainHeight << '\n';
        if(world_->generationVersion()>=9) {
            constexpr const char* regions[]={"FREEZING","COLD","TEMPERATE","WARM","HOT"};
            text << "CLIMATE " << regions[static_cast<int>(generation.climate)] << " EFFECTIVE TEMP "
                 << generation.effectiveTemperature << " GENERATOR " << world_->generationVersion() << '\n';
        }
        if (showWorldgenDebug_) {
            text << "CHEESE " << generation.caveCheese
                 << " SPAGHETTI " << generation.caveSpaghetti
                 << " NOODLE " << generation.caveNoodle << '\n'
                 << "CANYON REGION " << generation.canyonRegionX << " / "
                 << generation.canyonRegionZ << " ACTIVE "
                 << (generation.canyonActive ? "YES" : "NO")
                 << " SURFACE "
                 << (generation.canyonSurfaceExposure ? "YES" : "NO") << '\n'
                 << "CAVE ENTRANCE NEAR "
                 << (generation.caveEntranceCandidate ? "YES" : "NO")
                 << " PROXIMITY " << generation.caveEntranceInfluence << '\n'
                 << "DEPTH " << generation.depthBelowSurface
                 << " AQUIFER LEVEL " << generation.aquiferLevel << '\n';
        }
    } else {
        text << "GENERATOR LEGACY  TERRAIN HEIGHT " << generation.terrainHeight << '\n';
    }
    if (creativeMode_) {
        text << "FLYING " << (player_->isFlying() ? "ON" : "OFF") << '\n';
    } else if (spectatorMode_) {
        text << "SPECTATOR SPEED " << std::fixed << std::setprecision(2)
             << player_->spectatorSpeedMultiplier() << "X\n";
    }
    text << "SEED " << seed_ << '\n'
         << "LIGHT " << static_cast<int>(world_->sunlightAt(blockX, blockY, blockZ)) << " / "
         << static_cast<int>(world_->blockLightAt(blockX, blockY, blockZ)) << '\n'
         << "RENDER " << world_->renderDistance() << '\n'
         << "SIMULATION " << world_->simulationDistance() << '\n'
         << std::fixed << std::setprecision(2)
         << "FRAME " << timing_.frameMilliseconds << " MS\n"
#ifdef _WIN32
         << (timing_.memorySampleAvailable
                 ? "RAM " + std::to_string(timing_.workingSetBytes / 1048576ULL) +
                       " MB  PEAK " +
                       std::to_string(timing_.peakWorkingSetBytes / 1048576ULL) + " MB\n"
                 : "")
#endif
         << "CHUNKS " << world_->renderedChunkCount() << " RENDERED / "
         << world_->loadedChunkCount() << " LOADED\n"
         << "ENTITIES " << renderer_->visibleEntityCount() << " VISIBLE\n"
         << "MOBS " << mobs.passive << "/" << mobs.passiveCap << " PASSIVE  "
         << mobs.hostile << "/" << mobs.hostileCap << " HOSTILE\n"
         << (mobs.arrows > 0 ? "ARROWS " + std::to_string(mobs.arrows) + "\n" : "")
         << (interaction_.mobTarget.valid()
                 ? "TARGET " + survival_->mobName(interaction_.mobTarget.index) + "\n"
                 : "")
         << "SPAWN " << mobs.spawnAttempts << " ATTEMPTS  "
         << mobs.spawnSuccesses << " SUCCESS\n"
         << "AI " << mobs.aiMilliseconds << " MS  NAV "
         << mobs.navigationQueries << '\n'
         << "QUEUES " << world_->pendingChunkCount() << " GENERATE / "
         << world_->queuedMeshRebuildCount() << " DIRTY\n"
         << "MESH " << world_->pendingCpuMeshCount() << " CPU / "
         << world_->completedMeshCount() << " UPLOAD\n"
         << "VERTICES " << world_->uploadedVertexCount() << "  TRIANGLES "
         << world_->uploadedVertexCount() / 3 << "\n"
         << "BUILD " << world_->lastChunkRebuildMilliseconds() << " MS  UPLOAD "
         << world_->lastMeshUploadMilliseconds() << " MS\n"
         << "EDIT "
         << world_->lastBlockEditMilliseconds() << " MS";
    return text.str();
}

bool Game::playerIsWalking() const {
    if (ui_.state() != GameState::Playing || !input_.cursorCaptured() ||
        !player_->isGrounded() || player_->isFlying() || spectatorMode_) {
        return false;
    }
    return input_.actionDown(ControlAction::Forward) ||
           input_.actionDown(ControlAction::Left) ||
           input_.actionDown(ControlAction::Backward) ||
           input_.actionDown(ControlAction::Right);
}

void Game::runCommandSmokeTest() {
    runCraftingContentSmokeTest();
    std::string report;
    if (!CommandSystem::runSelfTest(report)) throw std::runtime_error("Command parser: " + report);
    if (!ChatUI::runSelfTest(report)) throw std::runtime_error("Chat UI: " + report);
    CommandContext context{*world_, *player_, *inventory_, *survival_, timing_.worldTime,
                           seed_, [this] { return gameMode(); },
                           [this](GameMode mode) { setGameMode(mode); }, true, weather_.get(), sounds_.get()};
    // The fixture edits the origin even when an existing profile loads far away.
    world_->generate(2, glm::vec3(10.0f,70.0f,10.0f));
    auto check = [&](const std::string& line, bool success) {
        const CommandResult result = commands_.execute(line, context);
        const bool actual = result.tone != ChatTone::Error;
        if (actual != success) throw std::runtime_error(line + ": " + result.text);
    };
    check("/help", true);
    check("/help gamemode", true);
    check("/gamemode creative", true);
    if (gameMode() != GameMode::Creative) throw std::runtime_error("creative mode");
    check("/gamemode spectator", true);
    check("/gamemode survival", true);
    check("/time set day", true);
    check("/time set night", true);
    if (std::abs(timing_.worldTime - 245.0f) > 0.01f) throw std::runtime_error("time set");
    check("/time add 30", true);
    if (std::abs(timing_.worldTime - 275.0f) > 0.01f) throw std::runtime_error("time add");
    inventory_->clear();
    check("/give @s stone 64", true);
    check("/give @s torch 1", true);
    if (inventory_->count(Item::Stone) != 64 || inventory_->count(Item::Torch) != 1)
        throw std::runtime_error("give inventory");
    check("/clear stone 4", true);
    if (inventory_->count(Item::Stone) != 60) throw std::runtime_error("clear count");
    check("/clear", true);
    if (inventory_->count(Item::Stone)) throw std::runtime_error("clear all");
    for (int id = static_cast<int>(Item::Paper); id < static_cast<int>(Item::Count); ++id) {
        inventory_->clear();
        const Item item = static_cast<Item>(id);
        std::string name = itemDefinition(item).displayName;
        for (char& c : name) c = c == ' ' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        check("/give @s " + name + " 1", true);
        if (inventory_->count(item) != 1) throw std::runtime_error("new command item lookup");
        if (itemToBlock(item) != Block::Air)
            check("/setblock 10 60 10 " + name, true);
        const auto suggestions = commands_.suggest("/give @s " + name);
        if (suggestions.empty()) throw std::runtime_error("new command item autocomplete");
    }
    check("/clear", true);
    check("/tp 10 70 -20", true);
    check("/tp ~ ~10 ~", true);
    if (std::abs(player_->position().y - 80.0f) > 0.01f) throw std::runtime_error("relative teleport");
    check("/summon cow", true);
    check("/summon HitoriGotoh", true);
    check("/summon NijikaIjichi", true);
    check("/seed", true);
    check("/setblock 10 60 10 stone", true);
    check("/fill 10 61 10 11 62 11 stone", true);
    check("/banana", false);
    check("/tp x y z", false);
    check("/give @s nonexistent_item 999999999999", false);
    check("/setblock 10 60 10 banana", false);
    check("/summon banana", false);
    check("/fill 0 60 0 100 100 100 stone", false);
    context.cheatsEnabled = false;
    check("/help", true); check("/seed", true); check("/give stone 1", false);
    std::cout << "Command smoke: parser, chat, commands, permissions OK\n";
}

void Game::runSurvivalSmokeTest() {
    runCraftingContentSmokeTest();
    const bool audioPassed = sounds_->verifyLibrary() && sounds_->verifyMusic() &&
        blockDefinition(Block::Snow).soundMaterial == SoundMaterial::Snow &&
        blockDefinition(Block::Sand).soundMaterial == SoundMaterial::Sand &&
        blockDefinition(Block::Log).soundMaterial == SoundMaterial::Wood;
    bool recoilPassed = true;
    for (const glm::vec3 eye : {glm::vec3(0.5f, 88.0f, 0.5f),
                                glm::vec3(987.4f, 146.2f, -731.8f),
                                glm::vec3(-25.1f, 34.8f, 40.9f)}) {
        const glm::mat4 baseView = glm::lookAt(
            eye, eye + glm::normalize(glm::vec3(0.45f, -0.25f, -1.0f)),
            glm::vec3(0.0f, 1.0f, 0.0f));
        for (float recoil : {NormalHitRecoilDegrees, CriticalHitRecoilDegrees,
                             1000.0f, std::nanf("")}) {
            const glm::mat4 view = applyHitRecoil(baseView, recoil);
            recoilPassed = recoilPassed &&
                glm::length(glm::vec3(view * glm::vec4(eye, 1.0f))) < 0.001f;
        }
    }
    for (float fps : {30.0f, 60.0f, 144.0f}) {
        float recoil = CriticalHitRecoilDegrees;
        const float deltaTime = 1.0f / fps;
        for (float elapsed = 0.0f; elapsed < 0.1f - 0.0001f;
             elapsed += deltaTime) {
            recoil = decayHitRecoilDegrees(recoil, deltaTime);
        }
        recoilPassed = recoilPassed && recoil < 0.08f;
        for (int frame = 0; frame < 300; ++frame) {
            if (frame % 6 == 0)
                recoil = frame % 12 == 0 ? CriticalHitRecoilDegrees
                                         : NormalHitRecoilDegrees;
            recoil = decayHitRecoilDegrees(recoil, deltaTime);
            recoilPassed = recoilPassed && recoil >= 0.0f &&
                           recoil <= MaximumHitRecoilDegrees;
        }
    }
    std::string craftingReport;
    std::string structureReport;
    const bool structuresPassed = StructureGenerator(seed_).runSelfTest(structureReport);
    std::string combatReport;
    std::string meshEditReport;
    std::string aoReport;
    std::string fluidReport;
    const bool craftingPassed = Inventory::runCraftingSelfTest(craftingReport);
    std::string recipeBookReport;
    const bool recipeBookPassed = UIManager::runRecipeBookSelfTest(recipeBookReport);
    const bool selectedCreativeMode = player_->isCreative();
    player_->setCreativeMode(false);
    const bool combatPassed =
        survival_->runCombatSelfTest(*world_, *player_, *inventory_, combatReport);
    player_->setCreativeMode(selectedCreativeMode);
    const bool meshEditsPassed = world_->runMeshEditSmokeTest(meshEditReport);
    const bool aoPassed = AmbientOcclusion::runSelfTest(aoReport);
    std::string asyncMeshReport;
    bool asyncMeshPassed = false;
    {
        World asyncMeshWorld(seed_);
        asyncMeshWorld.generate(2, player_->position());
        asyncMeshPassed = asyncMeshWorld.runAsyncMeshSmokeTest(asyncMeshReport);
    }
    const bool fluidsPassed = world_->runFluidSmokeTest(fluidReport);
    bool passed = recoilPassed && structuresPassed && craftingPassed && recipeBookPassed &&
                  combatPassed && meshEditsPassed && aoPassed &&
                  asyncMeshPassed && fluidsPassed;

    constexpr int uiWidth = 1280;
    constexpr int uiHeight = 720;
    bool uiLayoutPassed = true;
    const auto checkHit = [&](UiMode mode, const UiRect& rect, UiHit expected,
                              std::size_t creativeCount = 0) {
        const UiHit actual = UiLayout::hit(
            mode, rect.x + rect.width * .5f, rect.y + rect.height * .5f,
            uiWidth, uiHeight, creativeCount);
        uiLayoutPassed = uiLayoutPassed && actual == expected;
    };
    for (UiMode mode : {UiMode::Inventory, UiMode::CraftingTable,
                        UiMode::Furnace, UiMode::Chest, UiMode::Creative}) {
        for (int slot = 0; slot < Inventory::TotalSlots; ++slot) {
            if (mode != UiMode::Creative || slot < Inventory::HotbarSlots)
                checkHit(mode, UiLayout::playerSlot(mode, slot, uiWidth, uiHeight),
                         {UiSlotKind::PlayerInventorySlot, slot},
                         creativeCatalog().size());
        }
    }
    for (int slot = 0; slot < Inventory::CraftSlots; ++slot) {
        if (slot < 6 && slot % 3 < 2)
            checkHit(UiMode::Inventory, UiLayout::craftingSlot(slot, uiWidth, uiHeight),
                     {UiSlotKind::CraftingSlot, slot});
        checkHit(UiMode::CraftingTable, UiLayout::craftingSlot(slot, uiWidth, uiHeight),
                 {UiSlotKind::CraftingSlot, slot});
    }
    for (UiMode mode : {UiMode::Inventory, UiMode::CraftingTable})
        checkHit(mode, UiLayout::craftingOutput(uiWidth, uiHeight),
                 {UiSlotKind::CraftingOutput, 0});
    for (UiSlotKind kind : {UiSlotKind::FurnaceInput, UiSlotKind::FurnaceFuel,
                            UiSlotKind::FurnaceOutput})
        checkHit(UiMode::Furnace, UiLayout::furnaceSlot(kind, uiWidth, uiHeight),
                 {kind, 0});
    for (int slot = 0; slot < 27; ++slot)
        checkHit(UiMode::Chest, UiLayout::chestSlot(slot, uiWidth, uiHeight),
                 {UiSlotKind::ChestSlot, slot});
    for (int slot = 0; slot < static_cast<int>(creativeCatalog().size()); ++slot)
        checkHit(UiMode::Creative, UiLayout::creativeItem(slot, uiWidth, uiHeight),
                 {UiSlotKind::CreativeItem, slot}, creativeCatalog().size());
    UIManager settingsUi;
    settingsUi.openPauseMenu();
    settingsUi.openSettings();
    const float settingsScale=UIManager::menuScale(uiWidth,uiHeight);
    const int logicalWidth=static_cast<int>(uiWidth/settingsScale),logicalHeight=static_cast<int>(uiHeight/settingsScale);
    const auto checkMenuHit=[&](UiRect r,int expected) {
        return settingsUi.hoveredMenuItem({(r.x+r.width*.5f)*settingsScale,(r.y+r.height*.5f)*settingsScale},uiWidth,uiHeight)==expected;
    };
    for(int row=0;row<5;++row)
        uiLayoutPassed=uiLayoutPassed && checkMenuHit(MenuLayout::hubRow(row,logicalWidth,logicalHeight),row==3?34:row==4?33:30+row);
    settingsUi.openVideoSettings();
    for(int row=0;row<12;++row)
        uiLayoutPassed=uiLayoutPassed && checkMenuHit(MenuLayout::videoRow(row,logicalWidth,logicalHeight),MenuLayout::VideoActions[row]);
    uiLayoutPassed=uiLayoutPassed && checkMenuHit(MenuLayout::back(logicalWidth,logicalHeight),15);
    settingsUi.openWeatherSettings();
    for(int row=0;row<8;++row)
        uiLayoutPassed=uiLayoutPassed && checkMenuHit(MenuLayout::videoRow(row,logicalWidth,logicalHeight),40+row);
    uiLayoutPassed=uiLayoutPassed && checkMenuHit(MenuLayout::back(logicalWidth,logicalHeight),48);
    settingsUi.openControls();
    uiLayoutPassed=uiLayoutPassed && checkMenuHit({logicalWidth*.5f-185,logicalHeight*.5f-275,370,29},0);
    GameSettings bindingSettings;
    const int originalForward = bindingSettings.controls[static_cast<int>(ControlAction::Forward)];
    const int originalBackward = bindingSettings.controls[static_cast<int>(ControlAction::Backward)];
    uiLayoutPassed = uiLayoutPassed &&
        bindingSettings.bindControl(ControlAction::Forward, originalBackward) &&
        bindingSettings.controls[static_cast<int>(ControlAction::Forward)] == originalBackward &&
        bindingSettings.controls[static_cast<int>(ControlAction::Backward)] == originalForward &&
        !bindingSettings.bindControl(ControlAction::Forward, GLFW_KEY_ESCAPE);

    Inventory containerInventory;
    containerInventory.clear();
    containerInventory.setCreativeCursor(Item::Stone);
    const UiRect firstCraftSlot = UiLayout::craftingSlot(0, uiWidth, uiHeight);
    uiLayoutPassed = uiLayoutPassed &&
        containerInventory.handleInventoryClick(
            firstCraftSlot.x + 22, firstCraftSlot.y + 22, uiWidth, uiHeight,
            false, false) &&
        containerInventory.craftSlot(0).item == Item::Stone;
    UIManager containerUi;
    containerUi.openFurnace({1, 90, 1});
    containerUi.closeGameplayInterface(containerInventory);
    uiLayoutPassed = uiLayoutPassed && containerInventory.craftSlot(0).item == Item::Stone;
    containerUi.openChest({2, 90, 1});
    containerUi.closeGameplayInterface(containerInventory);
    uiLayoutPassed = uiLayoutPassed && containerInventory.craftSlot(0).item == Item::Stone;
    containerUi.openInventory();
    containerUi.closeGameplayInterface(containerInventory);
    uiLayoutPassed = uiLayoutPassed && containerInventory.craftSlot(0).empty() &&
                     containerInventory.count(Item::Stone) == 64;

    bool metadataPassed = foodValue(Item::Apple) > 0.0f;
    std::array<bool, 256> seenItemIds{};
    std::array<bool, 256> seenBlockIds{};
    const ItemAtlasLayout atlas = itemAtlasLayout();
    metadataPassed = metadataPassed && atlas.width == atlas.columns * atlas.tilePixels &&
                     atlas.height == atlas.rows * atlas.tilePixels &&
                     atlas.tilePixels == 64 && atlas.columns == 10 &&
                     atlas.rows == (static_cast<int>(Item::Count) + atlas.columns - 1) / atlas.columns;
    for (int value = 0; value < static_cast<int>(Item::Count); ++value) {
        const Item item = static_cast<Item>(value);
        const ItemDefinition& definition = itemDefinition(item);
        Item decoded = Item::None;
        metadataPassed = metadataPassed && !seenItemIds[definition.saveId] &&
                         itemFromSaveId(definition.saveId, decoded) && decoded == item &&
                         (item == Item::None ||
                          (definition.spriteIndex >= 0 &&
                           definition.spriteIndex < atlas.columns * atlas.rows));
        seenItemIds[definition.saveId] = true;
    }
    for (int value = 0; value < static_cast<int>(Block::Count); ++value) {
        const Block block = static_cast<Block>(value);
        const BlockDefinition& definition = blockDefinition(block);
        Block decoded = Block::Air;
        metadataPassed = metadataPassed && !seenBlockIds[definition.saveId] &&
                         blockFromSaveId(definition.saveId, decoded) && decoded == block;
        seenBlockIds[definition.saveId] = true;
    }
    passed = passed && uiLayoutPassed && metadataPassed;

    const int originalRenderDistance = world_->renderDistance();
    const int originalSimulationDistance = world_->simulationDistance();
    world_->setRenderDistance(64);
    world_->setSimulationDistance(32);
    bool distancePassed = world_->renderDistance() == 64 && world_->simulationDistance() == 32;
    world_->setRenderDistance(originalRenderDistance);
    world_->setSimulationDistance(originalSimulationDistance);

    bool streamingPassed = false;
    {
        World streamingWorld(seed_);
        streamingWorld.generate(2, glm::vec3(0.5f, 80.0f, 0.5f));
        streamingWorld.setRenderDistance(64);
        streamingWorld.setSimulationDistance(32);
        streamingWorld.updateStreaming(glm::vec3(0.5f, 80.0f, 0.5f), 0);
        streamingPassed = streamingWorld.renderDistance() == 64 &&
                          streamingWorld.simulationDistance() == 32 &&
                          streamingWorld.pendingChunkCount() <= 96 &&
                          streamingWorld.simulationActiveAt(0.5f, 0.5f) &&
                          !streamingWorld.simulationActiveAt(
                              CHUNK_SIZE * 40.0f, 0.5f);
        const glm::vec3 farPosition(CHUNK_SIZE * 500.0f + .5f, 80.0f,
                                    CHUNK_SIZE * 500.0f + .5f);
        streamingWorld.updateStreaming(farPosition, 0);
        streamingPassed = streamingPassed &&
                          streamingWorld.pendingChunkCount() <= 96 &&
                          streamingWorld.loadedChunkCount() <= 4;
        streamingWorld.setRenderDistance(32);
        streamingWorld.setSimulationDistance(4);
        streamingWorld.updateStreaming(farPosition, 0);
        streamingPassed = streamingPassed &&
                          streamingWorld.renderDistance() == 32 &&
                          streamingWorld.simulationDistance() == 4 &&
                          !streamingWorld.simulationActiveAt(
                              farPosition.x + CHUNK_SIZE * 8.0f, farPosition.z);
        for (int renderDistance : {8, 16, 32, 64}) {
            streamingWorld.setRenderDistance(renderDistance);
            streamingWorld.updateStreaming(farPosition, 0);
            streamingPassed = streamingPassed &&
                              streamingWorld.renderDistance() == renderDistance &&
                              streamingWorld.simulationDistance() == 4 &&
                              streamingWorld.pendingChunkCount() <= 96;
        }
    }
    passed = passed && streamingPassed;

    const std::string settingsTestPath = "voxel_settings_smoke.cfg";
    GameSettings testSettings;
    testSettings.renderDistance = 64;
    testSettings.simulationDistance = 32;
    testSettings.antiAliasingSamples = 4;
    testSettings.entityDistance = 18;
    testSettings.frameLimit = 60;
    testSettings.musicVolume = .5f;
    testSettings.sfxVolume = .25f;
    testSettings.passiveMobVolume = .75f;
    testSettings.hostileMobVolume = 0.0f;
    testSettings.brightness = .75f;
    testSettings.weatherCycle=false; testSettings.weatherQuality=1;
    testSettings.precipitationDensity=.37f; testSettings.lightningFlash=.45f;
    testSettings.snowAccumulation=false; testSettings.lightningEffects=false;
    testSettings.weatherFog=false; testSettings.weatherWind=false;
    GameSettings reloadedSettings;
    distancePassed = distancePassed && testSettings.save(settingsTestPath) &&
                     reloadedSettings.load(settingsTestPath) &&
                     reloadedSettings.renderDistance == 64 &&
                     reloadedSettings.simulationDistance == 32 &&
                     reloadedSettings.antiAliasingSamples == 4 &&
                     reloadedSettings.entityDistance == 18 &&
                     reloadedSettings.frameLimit == 60 &&
                     reloadedSettings.musicVolume == .5f &&
                     reloadedSettings.sfxVolume == .25f &&
                     reloadedSettings.passiveMobVolume == .75f &&
                     reloadedSettings.hostileMobVolume == 0.0f &&
                     reloadedSettings.brightness == .75f &&
                     !reloadedSettings.weatherCycle && reloadedSettings.weatherQuality==1 &&
                     reloadedSettings.precipitationDensity==.37f && reloadedSettings.lightningFlash==.45f &&
                     !reloadedSettings.snowAccumulation && !reloadedSettings.lightningEffects &&
                     !reloadedSettings.weatherFog && !reloadedSettings.weatherWind;
    std::filesystem::remove(settingsTestPath);

    GameSettings presetSettings;
    presetSettings.applyPreset(GraphicsPreset::Low);
    bool presetsPassed = presetSettings.renderDistance == 7 &&
                         presetSettings.simulationDistance == 5 &&
                         presetSettings.antiAliasingSamples == 0 &&
                         presetSettings.particlePercent == 35;
    presetSettings.applyPreset(GraphicsPreset::Medium);
    presetsPassed = presetsPassed && presetSettings.renderDistance == 12 &&
                    presetSettings.antiAliasingSamples == 2 &&
                    presetSettings.entityDistance == 12;
    presetSettings.applyPreset(GraphicsPreset::High);
    presetsPassed = presetsPassed && presetSettings.renderDistance == 24 &&
                    presetSettings.antiAliasingSamples == 4 &&
                    presetSettings.particlePercent == 100;
    presetSettings.graphicsPreset = GraphicsPreset::Custom;
    presetSettings.entityDistance = 17;
    presetsPassed = presetsPassed && presetSettings.graphicsPreset == GraphicsPreset::Custom &&
                    presetSettings.entityDistance == 17;
    distancePassed = distancePassed && presetsPassed;

    const int originalAntiAliasing = settings_.antiAliasingSamples;
    settings_.antiAliasingSamples = 2;
    prepareRenderTarget(320, 180);
    const bool twoSampleTargetPassed = multisampleFramebuffer_ != 0 &&
                                       multisampleSamples_ == 2;
    settings_.antiAliasingSamples = 4;
    prepareRenderTarget(320, 180);
    const bool fourSampleTargetPassed = multisampleFramebuffer_ != 0 &&
                                        multisampleSamples_ == 4;
    settings_.antiAliasingSamples = originalAntiAliasing;
    destroyRenderTarget();
    distancePassed = distancePassed && twoSampleTargetPassed && fourSampleTargetPassed;

    Inventory dropInventory;
    dropInventory.clear();
    dropInventory.add(Item::DiamondPickaxe, 1, 321);
    const ItemStack droppedTool = dropInventory.takeSelected(false);
    dropInventory.clear();
    dropInventory.add(Item::Stone, 17);
    const ItemStack droppedStack = dropInventory.takeSelected(true);
    const bool droppingPassed = droppedTool.item == Item::DiamondPickaxe &&
                                droppedTool.durability == 321 && droppedTool.count == 1 &&
                                droppedStack.item == Item::Stone && droppedStack.count == 17 &&
                                dropInventory.selectedStack().empty();
    int minimumSampledHeight = WORLD_HEIGHT;
    int maximumSampledHeight = 0;
    int maximumEightBlockRise = 0;
    int maximumBiomeBoundaryRise = 0;
    bool terrainPassed = WORLD_HEIGHT == 256;
    const std::array<std::uint32_t, 8> terrainSeeds{
        seed_, 42U, 1337U, 8675309U, 7U, 314159U, 982451653U, 271828U};
    for (const std::uint32_t terrainSeed : terrainSeeds) {
        // Retain the original v8 landform regression. Generator 9's expanded
        // climate/biome distribution is covered by the dedicated biome audit.
        World sampledWorld(terrainSeed, 8);
        int lowSamples = 0;
        int highSamples = 0;
        int mountainSamples = 0;
        int totalSamples = 0;
        for (int z = -1024; z <= 1024; z += 8) {
            for (int x = -1024; x <= 1024; x += 8) {
                const int height = sampledWorld.terrainHeight(x, z);
                const int eastHeight = sampledWorld.terrainHeight(x + 8, z);
                const int southHeight = sampledWorld.terrainHeight(x, z + 8);
                const std::string biome = sampledWorld.biomeNameAt(x, z);
                const std::string eastBiome = sampledWorld.biomeNameAt(x + 8, z);
                const std::string southBiome = sampledWorld.biomeNameAt(x, z + 8);

                minimumSampledHeight = std::min(minimumSampledHeight, height);
                maximumSampledHeight = std::max(maximumSampledHeight, height);
                maximumEightBlockRise = std::max(
                    maximumEightBlockRise,
                    std::max(std::abs(height - eastHeight), std::abs(height - southHeight)));
                if (biome != eastBiome)
                    maximumBiomeBoundaryRise =
                        std::max(maximumBiomeBoundaryRise, std::abs(height - eastHeight));
                if (biome != southBiome)
                    maximumBiomeBoundaryRise =
                        std::max(maximumBiomeBoundaryRise, std::abs(height - southHeight));

                lowSamples += height < 75 ? 1 : 0;
                highSamples += height >= 125 ? 1 : 0;
                mountainSamples += (biome == "MOUNTAINS" ||
                                    biome == "SNOWY SLOPES" ||
                                    biome == "STONY PEAKS" ||
                                    biome == "SNOWY PEAKS") ? 1 : 0;
                ++totalSamples;
            }
        }
        const bool seedPassed = lowSamples > totalSamples / 10 &&
                                highSamples > totalSamples / 500 &&
                                mountainSamples > totalSamples / 200 &&
                                mountainSamples < totalSamples * 2 / 3;
        if (!seedPassed) {
            std::cout << "Terrain seed " << terrainSeed << ": low " << lowSamples
                      << ", high " << highSamples << ", mountains "
                      << mountainSamples << " / " << totalSamples << '\n';
        }
        terrainPassed = terrainPassed && seedPassed;
    }
    terrainPassed = terrainPassed && maximumSampledHeight >= 125 &&
                    maximumSampledHeight - minimumSampledHeight >= 60 &&
                    // v8 intentionally permits steeper mountain slopes. Keep a
                    // bounded coarse slope and the unchanged biome-seam limit;
                    // adjacent chunk borders are checked separately below.
                    maximumEightBlockRise <= 40 && maximumBiomeBoundaryRise <= 32;
    std::string generationReport;
    std::string realStructureReport;
    glm::ivec3 structureChestPosition(0);
    bool structureChestLocated = false;
    for (const std::uint32_t terrainSeed : {seed_, 42U, 1337U}) {
        World sampledWorld(terrainSeed);
        std::string seedReport;
        const bool seedGenerated = sampledWorld.runGenerationSmokeTest(seedReport);
        generationReport += std::to_string(terrainSeed) + ": " + seedReport + "; ";
        terrainPassed = terrainPassed && seedGenerated;
        glm::ivec3* chestOutput = !structureChestLocated && terrainSeed == seed_
            ? &structureChestPosition : nullptr;
        const bool seedStructures = sampledWorld.runStructureGenerationSmokeTest(
            seedReport, chestOutput);
        structureChestLocated = structureChestLocated ||
                                (chestOutput != nullptr && seedStructures);
        realStructureReport += std::to_string(terrainSeed) + ": " + seedReport + "; ";
        terrainPassed = terrainPassed && seedStructures;
    }
    WorldgenSurvey aggregateSurvey;
    int canyonRegions = 0;
    int surfaceCanyonRegions = 0;
    for (std::uint32_t sample = 0; sample < 24U; ++sample) {
        const std::uint32_t surveySeed =
            0x9e3779b9U * (sample + 1U) ^ (seed_ + sample * 0x85ebca6bU);
        World surveyWorld(surveySeed);
        const WorldgenSurvey result = surveyWorld.runWorldgenSurvey();
        aggregateSurvey.surfaceColumns += result.surfaceColumns;
        aggregateSurvey.exposedColumns += result.exposedColumns;
        aggregateSurvey.entranceCandidates += result.entranceCandidates;
        aggregateSurvey.openEntrances += result.openEntrances;
        aggregateSurvey.entranceOpeningColumns += result.entranceOpeningColumns;
        aggregateSurvey.broadEntrances += result.broadEntrances;
        aggregateSurvey.dryCaveBlocks += result.dryCaveBlocks;
        aggregateSurvey.floodedCaveBlocks += result.floodedCaveBlocks;
        aggregateSurvey.maximumBorderRise = std::max(
            aggregateSurvey.maximumBorderRise, result.maximumBorderRise);
        aggregateSurvey.deterministic = aggregateSurvey.deterministic &&
                                        result.deterministic;
        const CanyonCarver carver(surveySeed);
        for (int regionZ = -8; regionZ <= 8; ++regionZ) {
            for (int regionX = -8; regionX <= 8; ++regionX) {
                canyonRegions += carver.hasCanyonInRegion(regionX, regionZ) ? 1 : 0;
                surfaceCanyonRegions += carver.hasSurfaceCanyonInRegion(
                    regionX, regionZ) ? 1 : 0;
            }
        }
    }
    const bool worldgenSurveyPassed = aggregateSurvey.deterministic &&
        aggregateSurvey.dryCaveBlocks > 0 &&
        aggregateSurvey.exposedColumns * 20 < aggregateSurvey.surfaceColumns &&
        aggregateSurvey.entranceCandidates >= 8 &&
        aggregateSurvey.openEntrances >= 4 &&
        aggregateSurvey.entranceOpeningColumns >=
            aggregateSurvey.openEntrances * 4 &&
        aggregateSurvey.broadEntrances >= 4 &&
        aggregateSurvey.maximumBorderRise <= 8 &&
        canyonRegions > 500 && canyonRegions < 1900 &&
        surfaceCanyonRegions > 5 &&
        surfaceCanyonRegions * 4 < canyonRegions;
    terrainPassed = terrainPassed && worldgenSurveyPassed;
    bool structureSavePassed = structureChestLocated;
    if (structureChestLocated) {
        const std::filesystem::path structureSavePath =
            std::filesystem::temp_directory_path() /
            ("voxel_structure_save_smoke_" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()) + ".vxw");
        const glm::vec3 testPosition =
            glm::vec3(structureChestPosition) + glm::vec3(0.5f, 2.0f, 0.5f);
        World structureWorld(seed_);
        structureWorld.generate(2, testPosition);
        const ChestData* original = structureWorld.chestAt(structureChestPosition, false);
        structureSavePassed = structureSavePassed && original != nullptr;
        std::array<ItemStack, 27> originalLoot{};
        if (original) originalLoot = original->slots;
        glm::ivec3 editedStructureBlock(0);
        bool foundEditableBlock = false;
        for (int offsetZ = -2; offsetZ <= 2 && !foundEditableBlock; ++offsetZ) {
            for (int offsetX = -2; offsetX <= 2 && !foundEditableBlock; ++offsetX) {
                const glm::ivec3 candidate = structureChestPosition +
                    glm::ivec3(offsetX, -1, offsetZ);
                const Block block = structureWorld.getBlock(
                    candidate.x, candidate.y, candidate.z);
                if (block != Block::Air && block != Block::Chest) {
                    editedStructureBlock = candidate;
                    foundEditableBlock = true;
                }
            }
        }
        structureSavePassed = structureSavePassed && foundEditableBlock;
        if (foundEditableBlock) {
            structureWorld.setBlock(editedStructureBlock.x,
                                    editedStructureBlock.y,
                                    editedStructureBlock.z,
                                    Block::Air);
        }
        structureSavePassed = structureSavePassed &&
            structureWorld.saveWorld(structureSavePath.string(), testPosition);
        World reloadedStructureWorld(seed_);
        glm::vec3 loadedPosition(0.0f);
        structureSavePassed = structureSavePassed &&
            reloadedStructureWorld.loadWorld(structureSavePath.string(), loadedPosition);
        if (structureSavePassed) {
            reloadedStructureWorld.generate(2, loadedPosition);
            const ChestData* loaded = reloadedStructureWorld.chestAt(
                structureChestPosition, false);
            structureSavePassed = loaded != nullptr;
            structureSavePassed = structureSavePassed &&
                reloadedStructureWorld.getBlock(editedStructureBlock.x,
                                                editedStructureBlock.y,
                                                editedStructureBlock.z) == Block::Air;
            if (loaded) {
                for (std::size_t slot = 0; slot < loaded->slots.size(); ++slot)
                    structureSavePassed = structureSavePassed &&
                        loaded->slots[slot].item == originalLoot[slot].item &&
                        loaded->slots[slot].count == originalLoot[slot].count;
            }
        }
        std::filesystem::remove(structureSavePath);
    }
    terrainPassed = terrainPassed && structureSavePassed;
    passed = passed && distancePassed && droppingPassed && terrainPassed;

    Inventory creativeInventory;
    creativeInventory.clear();
    creativeInventory.setCreativeCursor(Item::Stone);
    bool creativePassed = creativeCatalog().size() == static_cast<std::size_t>(Item::Count) - 1 &&
                          creativeInventory.cursorStack().item == Item::Stone &&
                          creativeInventory.cursorStack().count == 64;
    creativeInventory.setCreativeCursor(Item::IronPickaxe);
    creativePassed = creativePassed &&
                     creativeInventory.cursorStack().durability ==
                         Inventory::maxDurability(Item::IronPickaxe);
    creativeInventory.clear();
    creativePassed = creativePassed &&
                     !creativeInventory.pickBlock(Item::Stone, false) &&
                     creativeInventory.pickBlock(Item::Stone, true) &&
                     creativeInventory.selectedItem() == Item::Stone &&
                     creativeInventory.pickBlock(Item::Stone, false);

    bool sneakPassed = true;
    {
        World edgeWorld(42U);
        edgeWorld.generate(2, {0.5f, 249.01f, 0.5f});
        PlayerInput idleInput;
        idleInput.enabled = true;
        PlayerInput sneakInput = idleInput;
        sneakInput.moveForward = true;
        sneakInput.sneak = true;
        for (Block platform : {Block::Stone, Block::StoneSlab}) {
            edgeWorld.setBlock(0, 248, 0, platform);
            Player edgePlayer({0.5f, platform == Block::Stone ? 249.01f : 248.51f, 0.5f});
            for (int step = 0; step < 8; ++step)
                edgePlayer.update(0.016f, idleInput, edgeWorld);
            sneakPassed = sneakPassed && edgePlayer.isGrounded();
            for (int step = 0; step < 120; ++step)
                edgePlayer.update(0.016f, sneakInput, edgeWorld);
            sneakPassed = sneakPassed && edgePlayer.isGrounded() &&
                          edgePlayer.position().y > 248.45f;
        }
    }
    passed = passed && sneakPassed;

    Player creativePlayer(player_->position());
    creativePlayer.setCreativeMode(true);
    creativePlayer.damage(12.0f);
    PlayerInput flightInput;
    flightInput.enabled = true;
    flightInput.jump = true;
    creativePlayer.update(0.01f, flightInput, *world_);
    flightInput.jump = false;
    creativePlayer.update(0.05f, flightInput, *world_);
    flightInput.jump = true;
    creativePlayer.update(0.01f, flightInput, *world_);
    const float flightStartY = creativePlayer.position().y;
    creativePlayer.update(0.08f, flightInput, *world_);
    creativePassed = creativePassed && creativePlayer.health() == creativePlayer.maxHealth() &&
                     creativePlayer.hunger() == 20.0f && creativePlayer.isFlying() &&
                     creativePlayer.position().y > flightStartY;
    creativePlayer.setCreativeMode(false);
    creativePassed = creativePassed && !creativePlayer.isFlying();

    Player flightSpeedPlayer({0.5f, 210.0f, 0.5f});
    flightSpeedPlayer.setCreativeMode(true);
    flightSpeedPlayer.setFlying(true);
    PlayerInput speedInput;
    speedInput.enabled = true;
    speedInput.moveForward = true;
    auto flightStep = [&](float deltaTime) {
        const glm::vec3 before = flightSpeedPlayer.position();
        flightSpeedPlayer.update(deltaTime, speedInput, *world_);
        return glm::length(glm::vec2(flightSpeedPlayer.position().x - before.x,
                                      flightSpeedPlayer.position().z - before.z));
    };
    const float normalFlight = flightStep(0.5f);
    speedInput.sprint = true;
    const float sprintFlight = flightStep(0.5f);
    speedInput.sprint = false;
    const float resumedFlight = flightStep(1.0f / 60.0f);
    speedInput.moveForward = false;
    speedInput.jump = true;
    const float beforeAscent = flightSpeedPlayer.position().y;
    flightSpeedPlayer.update(0.5f, speedInput, *world_);
    const float normalAscent = flightSpeedPlayer.position().y - beforeAscent;
    speedInput.sprint = true;
    const float beforeSprintAscent = flightSpeedPlayer.position().y;
    flightSpeedPlayer.update(0.5f, speedInput, *world_);
    const float sprintAscent = flightSpeedPlayer.position().y - beforeSprintAscent;
    speedInput.jump = false;
    speedInput.sneak = true;
    const float beforeSneakDescent = flightSpeedPlayer.position().y;
    flightSpeedPlayer.update(0.5f, speedInput, *world_);
    const float sneakDescent = beforeSneakDescent - flightSpeedPlayer.position().y;
    const bool flightSpeedPassed =
        std::abs(normalFlight - 4.25f) < 0.05f &&
        std::abs(sprintFlight - 10.5f) < 0.05f &&
        std::abs(resumedFlight - 8.5f / 60.0f) < 0.01f &&
        std::abs(normalAscent - 4.25f) < 0.05f &&
        std::abs(sprintAscent - 10.5f) < 0.05f &&
        std::abs(sneakDescent - 7.0f) < 0.05f;
    creativePassed = creativePassed && flightSpeedPassed;

    const std::vector<std::pair<Block, Item>> newBlockItems{
        {Block::Cobblestone, Item::Cobblestone},
        {Block::StoneBricks, Item::StoneBricks},
        {Block::Bricks, Item::Bricks},
        {Block::Glass, Item::Glass},
        {Block::Gravel, Item::Gravel},
        {Block::SnowBlock, Item::SnowBlock},
        {Block::BirchPlanks, Item::BirchPlanks},
        {Block::BirchLog, Item::BirchLog},
        {Block::BirchLeaves, Item::BirchLeaves},
        {Block::Cactus, Item::Cactus},
        {Block::Furnace, Item::Furnace},
        {Block::Bookshelf, Item::Bookshelf},
        {Block::WoodenDoor, Item::WoodenDoor},
        {Block::WoodenSlab, Item::WoodenSlab},
        {Block::StoneSlab, Item::StoneSlab},
        {Block::Granite, Item::Granite},
        {Block::Diorite, Item::Diorite},
        {Block::Andesite, Item::Andesite},
        {Block::MossyCobblestone, Item::MossyCobblestone},
        {Block::MossyStoneBricks, Item::MossyStoneBricks},
        {Block::Ice, Item::Ice},
        {Block::Mud, Item::Mud},
        {Block::TallGrass, Item::TallGrass},
        {Block::RedFlower, Item::RedFlower},
        {Block::YellowFlower, Item::YellowFlower},
        {Block::LadderNorth, Item::Ladder},
        {Block::Chest, Item::Chest},
    };
    for (const auto& mapping : newBlockItems) {
        creativePassed = creativePassed && itemToBlock(mapping.second) == mapping.first &&
                         blockToItem(mapping.first) == mapping.second;
    }
    creativePassed = creativePassed && blockToItem(Block::Clay) == Item::ClayBall &&
                     blockToItem(Block::Snow) == Item::Snowball &&
                     blockToItem(Block::CoalOre) == Item::Coal &&
                     blockToItem(Block::IronOre) == Item::IronOre &&
                     blockToItem(Block::GoldOre) == Item::GoldOre &&
                     blockToItem(Block::CopperOre) == Item::CopperOre &&
                     blockToItem(Block::DiamondOre) == Item::Diamond;
    creativePassed = creativePassed && canHarvestBlock(Item::WoodPickaxe, Block::CoalOre) &&
                     !canHarvestBlock(Item::WoodPickaxe, Block::IronOre) &&
                     canHarvestBlock(Item::StonePickaxe, Block::IronOre) &&
                     !canHarvestBlock(Item::GoldPickaxe, Block::GoldOre) &&
                     canHarvestBlock(Item::IronPickaxe, Block::DiamondOre) &&
                     attackDamage(Item::DiamondSword) > attackDamage(Item::None) &&
                     Inventory::maxDurability(Item::GoldPickaxe) == 33 &&
                     Inventory::maxDurability(Item::DiamondPickaxe) == 1561;
    passed = passed && creativePassed;

    int compatibleSaveFiles = 0;
    auto checkExistingSave = [&](const std::string& path, const auto& loader) {
        if (!std::filesystem::exists(path)) {
            return;
        }
        ++compatibleSaveFiles;
        const bool loaded = loader();
        std::cout << "Compatibility " << path << ": " << (loaded ? "passed" : "FAILED")
                  << '\n';
        passed = passed && loaded;
    };
    checkExistingSave(worldSavePath(WorldSavePath), [&] {
        World loadedWorld(seed_);
        glm::vec3 loadedPosition(0.0f);
        return loadedWorld.loadWorld(worldSavePath(WorldSavePath), loadedPosition);
    });
    checkExistingSave(worldSavePath(InventorySavePath), [&] {
        Inventory loadedInventory;
        return loadedInventory.load(worldSavePath(InventorySavePath), seed_);
    });
    checkExistingSave(worldSavePath(PlayerSavePath), [&] {
        Player loadedPlayer(player_->position());
        return loadedPlayer.load(worldSavePath(PlayerSavePath), seed_);
    });
    checkExistingSave(worldSavePath(MobSavePath), [&] {
        SurvivalWorld loadedSurvival(seed_);
        return loadedSurvival.load(worldSavePath(MobSavePath), seed_);
    });
    checkExistingSave(SettingsPath, [&] {
        GameSettings loadedSettings;
        return loadedSettings.load(SettingsPath);
    });

    const std::string mobTestPath = "voxel_mobs_smoke.vxm";
    const std::string inventoryTestPath = "voxel_inventory_smoke.vxi";
    SurvivalWorld reloadedSurvival(seed_);
    const bool mobPersistencePassed =
        survival_->save(mobTestPath, seed_) && reloadedSurvival.load(mobTestPath, seed_);
    const std::filesystem::path structureMobTestPath =
        std::filesystem::temp_directory_path() /
        ("voxel_structure_mobs_smoke_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".vxm");
    SurvivalWorld structureMobWorld(seed_);
    const glm::ivec3 villagerMarker(17, 82, -9);
    const glm::ivec3 pillagerMarker(19, 82, -9);
    structureMobWorld.spawnStructureMob(villagerMarker, false);
    structureMobWorld.spawnStructureMob(villagerMarker, false);
    structureMobWorld.spawnStructureMob(pillagerMarker, true);
    const auto beforeReload = structureMobWorld.diagnostics(glm::vec3(18.0f, 82.0f, -9.0f));
    SurvivalWorld reloadedStructureMobs(seed_);
    bool structureMobPassed = beforeReload.passive == 1 && beforeReload.hostile == 1 &&
        structureMobWorld.save(structureMobTestPath.string(), seed_) &&
        reloadedStructureMobs.load(structureMobTestPath.string(), seed_);
    reloadedStructureMobs.spawnStructureMob(villagerMarker, false);
    reloadedStructureMobs.spawnStructureMob(pillagerMarker, true);
    const auto afterReload = reloadedStructureMobs.diagnostics(glm::vec3(18.0f, 82.0f, -9.0f));
    structureMobPassed = structureMobPassed && afterReload.passive == 1 &&
                         afterReload.hostile == 1;
    passed = passed && structureMobPassed;

    const std::vector<Item> newItems{
        Item::Cobblestone, Item::StoneBricks, Item::Bricks, Item::Glass, Item::Gravel,
        Item::Clay, Item::Snow, Item::SnowBlock, Item::BirchPlanks, Item::BirchLog,
        Item::BirchLeaves, Item::Cactus, Item::Furnace, Item::Bookshelf, Item::WoodenDoor,
        Item::WoodenSlab, Item::StoneSlab, Item::Coal, Item::IronIngot, Item::GoldIngot,
        Item::Diamond, Item::CopperIngot, Item::Apple, Item::Leather, Item::RawMutton,
        Item::CookedMutton,
        Item::Granite, Item::Diorite, Item::Andesite, Item::MossyCobblestone,
        Item::MossyStoneBricks, Item::Ice, Item::Mud, Item::TallGrass, Item::RedFlower,
        Item::YellowFlower, Item::Ladder, Item::Chest, Item::GoldPickaxe, Item::GoldAxe,
        Item::GoldShovel, Item::DiamondPickaxe, Item::DiamondAxe, Item::DiamondShovel,
        Item::WoodSword, Item::StoneSword, Item::IronSword, Item::GoldSword,
        Item::DiamondSword,
    };
    bool itemPersistencePassed = true;
    for (Item item : newItems) {
        Inventory persistenceInventory;
        persistenceInventory.clear();
        Inventory reloadedInventory;
        const bool itemPassed =
            persistenceInventory.add(item, 2) == 0 &&
            persistenceInventory.save(inventoryTestPath, seed_) &&
            reloadedInventory.load(inventoryTestPath, seed_) &&
            reloadedInventory.count(item) == 2;
        if (!itemPassed)
            std::cout << "Inventory persistence failed for "
                      << itemDefinition(item).displayName << '\n';
        itemPersistencePassed = itemPersistencePassed && itemPassed;
    }
    passed = passed && mobPersistencePassed && itemPersistencePassed;

    const std::string blockEntityTestPath = "voxel_block_entities_smoke.vxw";
    World blockEntityWorld(seed_);
    blockEntityWorld.generate(2, glm::vec3(0.5f, 80.0f, 0.5f));
    const glm::ivec3 furnacePosition(0, 90, 0);
    const glm::ivec3 chestPosition(1, 90, 0);
    const glm::ivec3 doorPosition(2, 90, 0);
    const glm::ivec3 hingedDoorPosition(2, 90, 1);
    const glm::ivec3 slabPosition(3, 90, 0);
    const glm::ivec3 torchPosition(4, WORLD_HEIGHT - 10, 0);
    blockEntityWorld.setBlock(furnacePosition.x, furnacePosition.y, furnacePosition.z, Block::Furnace);
    blockEntityWorld.setBlock(chestPosition.x, chestPosition.y, chestPosition.z, Block::Chest);
    blockEntityWorld.setBlock(
        doorPosition.x, doorPosition.y, doorPosition.z, Block::DoorOpenEastLower);
    blockEntityWorld.setBlock(
        doorPosition.x, doorPosition.y + 1, doorPosition.z, Block::DoorOpenEastUpper);
    blockEntityWorld.setBlock(hingedDoorPosition.x, hingedDoorPosition.y,
                              hingedDoorPosition.z, Block::DoorRightOpenWestLower);
    blockEntityWorld.setBlock(hingedDoorPosition.x, hingedDoorPosition.y + 1,
                              hingedDoorPosition.z, Block::DoorRightOpenWestUpper);
    blockEntityWorld.setBlock(
        slabPosition.x, slabPosition.y, slabPosition.z, Block::StoneSlabTop);
    blockEntityWorld.setBlock(
        torchPosition.x, torchPosition.y, torchPosition.z, Block::Torch);
    blockEntityWorld.updateStreaming(glm::vec3(0.5f, 91.0f, 0.5f), 0);
    const bool localLightingPassed =
        blockEntityWorld.blockLightAt(torchPosition.x, torchPosition.y, torchPosition.z) == 15 &&
        blockEntityWorld.blockLightAt(torchPosition.x + 1, torchPosition.y, torchPosition.z) == 14;
    blockEntityWorld.setBlock(
        torchPosition.x, torchPosition.y, torchPosition.z, Block::Air);
    blockEntityWorld.updateStreaming(glm::vec3(0.5f, 91.0f, 0.5f), 0);
    FurnaceData* furnace = blockEntityWorld.furnaceAt(furnacePosition, true);
    ChestData* chest = blockEntityWorld.chestAt(chestPosition, true);
    bool blockEntitiesPassed = furnace && chest && localLightingPassed &&
                               blockEntityWorld.blockLightAt(
                                   torchPosition.x, torchPosition.y, torchPosition.z) == 0;
    if (furnace && chest) {
        furnace->input = {Item::IronOre, 2, 0};
        furnace->fuel = {Item::Coal, 1, 0};
        chest->slots[0] = {Item::Diamond, 3, 0};
        blockEntityWorld.updateBlockEntities(2.5f);
        blockEntitiesPassed = furnace->output.empty() && furnace->progress > 0.49f &&
                              furnace->progress < 0.51f && furnace->fuelRemaining > 0.0f;
    }
    blockEntitiesPassed = blockEntitiesPassed &&
                          blockEntityWorld.saveWorld(blockEntityTestPath, glm::vec3(0.5f, 91, 0.5f));
    World reloadedBlockEntityWorld(seed_);
    glm::vec3 entityPlayerPosition(0.0f);
    blockEntitiesPassed = blockEntitiesPassed &&
                          reloadedBlockEntityWorld.loadWorld(blockEntityTestPath,
                                                             entityPlayerPosition);
    reloadedBlockEntityWorld.generate(2, entityPlayerPosition);
    FurnaceData* loadedFurnace = reloadedBlockEntityWorld.furnaceAt(furnacePosition, false);
    ChestData* loadedChest = reloadedBlockEntityWorld.chestAt(chestPosition, false);
    blockEntitiesPassed = blockEntitiesPassed && loadedFurnace && loadedChest &&
                          loadedFurnace->progress > 0.49f && loadedFurnace->progress < 0.51f &&
                          loadedChest->slots[0].item == Item::Diamond &&
                          loadedChest->slots[0].count == 3 &&
                          reloadedBlockEntityWorld.getBlock(
                              doorPosition.x, doorPosition.y, doorPosition.z) ==
                              Block::DoorOpenEastLower &&
                          reloadedBlockEntityWorld.getBlock(
                              doorPosition.x, doorPosition.y + 1, doorPosition.z) ==
                              Block::DoorOpenEastUpper &&
                          reloadedBlockEntityWorld.getBlock(
                              hingedDoorPosition.x, hingedDoorPosition.y,
                              hingedDoorPosition.z) == Block::DoorRightOpenWestLower &&
                          reloadedBlockEntityWorld.getBlock(
                              hingedDoorPosition.x, hingedDoorPosition.y + 1,
                              hingedDoorPosition.z) == Block::DoorRightOpenWestUpper &&
                          reloadedBlockEntityWorld.getBlock(
                              slabPosition.x, slabPosition.y, slabPosition.z) ==
                              Block::StoneSlabTop;
    reloadedBlockEntityWorld.updateBlockEntities(7.6f);
    loadedFurnace = reloadedBlockEntityWorld.furnaceAt(furnacePosition, false);
    blockEntitiesPassed = blockEntitiesPassed && loadedFurnace &&
                          loadedFurnace->output.item == Item::IronIngot &&
                          loadedFurnace->output.count == 2 && loadedFurnace->input.empty();
    if (loadedFurnace) {
        const std::vector<ItemStack> dropped =
            reloadedBlockEntityWorld.takeBlockEntityContents(furnacePosition);
        blockEntitiesPassed = blockEntitiesPassed && dropped.size() == 1 &&
                              dropped[0].item == Item::IronIngot && dropped[0].count == 2;
    }
    if (loadedChest) {
        const std::vector<ItemStack> dropped =
            reloadedBlockEntityWorld.takeBlockEntityContents(chestPosition);
        blockEntitiesPassed = blockEntitiesPassed && dropped.size() == 1 &&
                              dropped[0].item == Item::Diamond && dropped[0].count == 3;
    }
    passed = passed && blockEntitiesPassed;

    std::filesystem::remove(mobTestPath);
    std::filesystem::remove(structureMobTestPath);
    std::filesystem::remove(inventoryTestPath);
    std::filesystem::remove(blockEntityTestPath);

    std::cout << "Camera recoil smoke: fixed eye, bounded degrees, 30/60/144 FPS decay, "
              << "rapid hits " << (recoilPassed ? "passed" : "FAILED") << '\n'
              << "Structure smoke: " << structureReport << '\n'
              << "Crafting smoke: " << craftingReport << '\n'
              << "Recipe Book smoke: " << recipeBookReport << '\n'
              << "UI/container smoke: shared slot hits and container isolation "
              << (uiLayoutPassed ? "passed" : "FAILED") << '\n'
              << "Metadata smoke: stable IDs, atlas bounds, and Apple food "
              << (metadataPassed ? "passed" : "FAILED") << '\n'
              << "Combat smoke: " << combatReport << '\n'
              << "Mesh edit smoke: " << meshEditReport << '\n'
              << "Voxel AO smoke: " << aoReport << '\n'
              << "Async mesh smoke: " << asyncMeshReport << '\n'
              << "Fluid smoke: " << fluidReport << '\n'
              << "Distance/drop/MSAA smoke: saved limits, 2x/4x targets, and durable drops "
              << (distancePassed && droppingPassed ? "passed" : "FAILED") << '\n'
              << "Streaming smoke: 64/32 independent radii, bounded queue, fast relocation "
              << (streamingPassed ? "passed" : "FAILED") << '\n'
              << "Terrain smoke: eight seeds, elevation " << minimumSampledHeight << '-'
              << maximumSampledHeight << ", max 8-block rise " << maximumEightBlockRise
              << ", biome-boundary rise " << maximumBiomeBoundaryRise << ' '
              << (terrainPassed ? "passed" : "FAILED") << '\n'
              << "Generation smoke: " << generationReport << '\n'
              << "Worldgen survey: 24 seeds, surface openings "
              << aggregateSurvey.exposedColumns << '/' << aggregateSurvey.surfaceColumns
              << ", intentional cave mouths " << aggregateSurvey.openEntrances
              << '/' << aggregateSurvey.entranceCandidates
              << " (nearby open columns " << aggregateSurvey.entranceOpeningColumns
              << ", broad " << aggregateSurvey.broadEntrances << ')'
              << ", dry/flooded cave blocks " << aggregateSurvey.dryCaveBlocks
              << '/' << aggregateSurvey.floodedCaveBlocks
              << ", max border rise " << aggregateSurvey.maximumBorderRise
              << ", canyon/surface regions " << canyonRegions << '/'
              << surfaceCanyonRegions << ' '
              << (worldgenSurveyPassed ? "passed" : "FAILED") << '\n'
              << "Real structure smoke: " << realStructureReport << '\n'
              << "Structure chest save/reload: "
              << (structureSavePassed ? "passed" : "FAILED") << '\n'
              << "Creative smoke: catalog, cursor stacks, invulnerability, and flight "
              << (creativePassed ? "passed" : "FAILED") << std::endl
              << "Flight speed smoke: normal 8.5, sprint 21, Ctrl release 8.5, "
                 "vertical/sneak unchanged "
              << (flightSpeedPassed ? "passed" : "FAILED") << '\n'
              << "Control smoke: full-block/slab sneak edges and Pick Block "
              << (sneakPassed && creativePassed ? "passed" : "FAILED") << '\n'
              << "Persistence smoke: mobs "
              << (mobPersistencePassed ? "passed" : "FAILED") << ", items "
              << (itemPersistencePassed ? "passed" : "FAILED") << '\n'
              << "Structure mob smoke: marker deduplication and save/reload "
              << (structureMobPassed ? "passed" : "FAILED") << '\n'
              << "Furnace/chest/shape smoke: local torch light plus persisted contents/shapes "
              << (blockEntitiesPassed ? "passed" : "FAILED") << '\n'
              << "Audio smoke: OGG groups and material metadata "
              << (audioPassed ? "passed" : "FAILED") << '\n'
              << "Save compatibility smoke: " << compatibleSaveFiles << " existing files loaded\n"
              << "Day/night smoke: synchronized " << DayNightCycleSeconds << " second cycle\n";
    if (!passed || !audioPassed) {
        throw std::runtime_error("survival smoke test failed");
    }
}

void Game::runSpectatorSmokeTest() {
    bool passed = true;
    const glm::vec3 testArea(0.5f, 220.01f, 0.5f);
    world_->generate(2, testArea);
    for (int z = -3; z <= 3; ++z) {
        for (int x = -3; x <= 3; ++x)
            world_->setBlock(x, 219, z, Block::Stone);
    }
    for (int z = -2; z <= 0; ++z) {
        world_->setBlock(0, 220, z, Block::Stone);
        world_->setBlock(0, 221, z, Block::Stone);
    }
    world_->setBlock(1, 220, 0, Block::Water);
    world_->setBlock(2, 220, 0, Block::StoneSlab);
    world_->setBlock(-1, 220, 0, Block::Cactus);

    Player probe(testArea);
    probe.setSpectatorMode(true);
    const float initialHealth = probe.health();
    const float initialHunger = probe.hunger();
    PlayerInput movement;
    movement.enabled = true;
    movement.moveForward = true;
    probe.update(0.5f, movement, *world_);
    passed = passed && probe.position().z < -2.0f &&
             probe.isSpectator() && probe.isFlying() && !probe.isSwimming();
    probe.damage(8.0f);
    probe.eat(4.0f);
    probe.addExperience(5);
    passed = passed && probe.health() == initialHealth &&
             probe.hunger() == initialHunger && probe.experience() == 0;

    movement.moveForward = false;
    movement.jump = true;
    const float beforeAscent = probe.position().y;
    probe.update(0.5f, movement, *world_);
    const float normalAscent = probe.position().y - beforeAscent;
    movement.jump = false;
    movement.sneak = true;
    const float beforeDescent = probe.position().y;
    probe.update(0.5f, movement, *world_);
    const float descent = beforeDescent - probe.position().y;
    probe.adjustSpectatorSpeed(1.0);
    movement.sneak = false;
    movement.jump = true;
    movement.sprint = true;
    const float beforeBoostedAscent = probe.position().y;
    probe.update(0.5f, movement, *world_);
    const float boostedAscent = probe.position().y - beforeBoostedAscent;
    passed = passed && std::abs(normalAscent - 4.25f) < 0.05f &&
             std::abs(descent - 4.25f) < 0.05f &&
             std::abs(probe.spectatorSpeedMultiplier() - 1.25f) < 0.001f &&
             std::abs(boostedAscent - 7.96875f) < 0.05f;
    probe.adjustSpectatorSpeed(100.0);
    passed = passed && probe.spectatorSpeedMultiplier() == 8.0f;
    probe.adjustSpectatorSpeed(-100.0);
    passed = passed && probe.spectatorSpeedMultiplier() == 0.2f;

    Inventory spectatorInventory;
    spectatorInventory.clear();
    SurvivalWorld spectatorMobs(seed_);
    spectatorMobs.spawnDrop(probe.position(), Item::Stone);
    spectatorMobs.spawnExperience(probe.position(), 4);
    spectatorMobs.spawnStructureMob({0, 220, 2}, true);
    spectatorMobs.update(5.1f, *world_, probe, spectatorInventory, 0.0f);
    const auto mobStats = spectatorMobs.diagnostics(probe.position());
    passed = passed && spectatorInventory.count(Item::Stone) == 0 &&
             probe.experience() == 0 && probe.health() == initialHealth &&
             mobStats.spawnAttempts == 0;

    setGameMode(GameMode::Spectator);
    passed = passed && player_->isSpectator() && !player_->isCreative();
    player_->teleport(testArea);
    passed = passed && world_->aabbIntersectsSolid(
        player_->aabbMinimum(), player_->aabbMaximum());
    setGameMode(GameMode::Survival);
    passed = passed && !player_->isSpectator() &&
             !world_->aabbIntersectsSolid(player_->aabbMinimum(), player_->aabbMaximum());
    setGameMode(GameMode::Creative);
    setGameMode(GameMode::Spectator);
    passed = passed && player_->isSpectator() && !player_->isCreative();
    player_->teleport(testArea);
    saveAll();
    std::ifstream modeInput(worldSavePath(SeedPath));
    std::uint32_t savedSeed = 0;
    std::string savedMode;
    modeInput >> savedSeed >> savedMode;
    modeInput.close();
    glm::vec3 loadedPosition(0.0f);
    World loadedWorld(seed_);
    passed = passed && savedSeed == seed_ && savedMode == "spectator" &&
             loadedWorld.loadWorld(worldSavePath(WorldSavePath), loadedPosition) &&
             glm::distance(loadedPosition, testArea) < 0.01f;
    setGameMode(GameMode::Survival);
    passed = passed && !player_->isSpectator() &&
             !world_->aabbIntersectsSolid(player_->aabbMinimum(), player_->aabbMaximum());

    setGameMode(GameMode::Spectator);
    player_->teleport(testArea);
    saveAll();

    std::cout << "Spectator smoke: noclip, vertical/sprint/scroll speed, immunity, "
                 "no pickup/spawn, mode transitions, safe exit, and save/reload "
              << (passed ? "passed" : "FAILED") << '\n';
    if (!passed)
        throw std::runtime_error("spectator smoke test failed");
}

void Game::runResetSmokeTest() {
    if (!std::filesystem::exists(".voxel_test_sandbox")) {
        throw std::runtime_error("reset smoke requires .voxel_test_sandbox marker");
    }

    constexpr std::uint32_t ResetTestSeed = 13579;
    resetWorld(ResetTestSeed, GameMode::Creative);
    const bool filesCreated = std::filesystem::exists(worldSavePath(WorldSavePath)) &&
                              std::filesystem::exists(worldSavePath(InventorySavePath)) &&
                              std::filesystem::exists(worldSavePath(PlayerSavePath)) &&
                              std::filesystem::exists(worldSavePath(MobSavePath));
    const bool resetPassed = seed_ == ResetTestSeed && creativeMode_ && player_->isCreative() &&
                             inventory_->count(Item::Grass) == 0 && filesCreated;
    std::cout << "Reset smoke: fresh seed, Creative mode, player state, inventory, and saves "
              << (resetPassed ? "passed" : "FAILED") << '\n';
    if (!resetPassed) {
        throw std::runtime_error("reset world smoke test failed");
    }
}
void Game::updateUiSmokeTest(double now) {
    const int stage = static_cast<int>((now - smokeTest_.startTime) / 2.0);
    if (stage == smokeTest_.stage || stage > 10) {
        return;
    }
    smokeTest_.stage = stage;

    switch (stage) {
    case 0:
        ui_.resumeGame();
        break;
    case 1:
        ui_.openInventory();
        smokeTest_.inventoryStartTime = timing_.worldTime;
        break;
    case 2:
        smokeTest_.checksPassed &= timing_.worldTime - smokeTest_.inventoryStartTime > 1.0f;
        ui_.closeGameplayInterface(*inventory_);
        ui_.openPauseMenu();
        smokeTest_.pauseStartTime = timing_.worldTime;
        renderer_->setEffectQuality(1);
        break;
    case 3:
        ui_.openSettings();
        ui_.openControls();
        renderer_->setEffectQuality(0);
        break;
    case 4:
        smokeTest_.checksPassed &= std::abs(timing_.worldTime - smokeTest_.pauseStartTime) < 0.08f;
        ui_.resumeGame();
        fullbright_ = true;
        renderer_->setEffectQuality(settings_.effectQuality);
        break;
    case 5: {
        const glm::ivec3 tablePosition =
            glm::ivec3(glm::floor(player_->position())) + glm::ivec3(2, 0, 0);
        world_->setBlock(tablePosition.x, tablePosition.y, tablePosition.z, Block::CraftingTable);
        ui_.openCraftingTable(tablePosition);
        inventory_->add(Item::Cobblestone, 8);
        bool furnacePrepared = false;
        for (int index = 0; index < static_cast<int>(craftingRecipes().size()); ++index)
            if (craftingRecipes()[static_cast<std::size_t>(index)].output == Item::Furnace) {
                furnacePrepared = inventory_->fillRecipe(index, true, false) &&
                                  inventory_->craftingOutput(true).item == Item::Furnace;
                break;
            }
        smokeTest_.checksPassed &= furnacePrepared;
        smokeTest_.craftingStartTime = timing_.worldTime;
        fullbright_ = false;
        break;
    }
    case 6:
        smokeTest_.checksPassed &= timing_.worldTime - smokeTest_.craftingStartTime > 1.0f;
        ui_.closeGameplayInterface(*inventory_);
        {
            const glm::ivec3 furnacePosition =
                glm::ivec3(glm::floor(player_->position())) + glm::ivec3(3, 0, 0);
            world_->setBlock(
                furnacePosition.x, furnacePosition.y, furnacePosition.z, Block::Furnace);
            FurnaceData* furnace = world_->furnaceAt(furnacePosition, true);
            if (furnace) {
                furnace->input = {Item::Sand, 1, 0};
                furnace->fuel = {Item::Coal, 1, 0};
            }
            smokeTest_.checksPassed &= furnace != nullptr;
            ui_.openFurnace(furnacePosition);
            smokeTest_.craftingStartTime = timing_.worldTime;
        }
        break;
    case 7:
        smokeTest_.checksPassed &= timing_.worldTime - smokeTest_.craftingStartTime > 1.0f;
        ui_.closeGameplayInterface(*inventory_);
        {
            const glm::ivec3 chestPosition =
                glm::ivec3(glm::floor(player_->position())) + glm::ivec3(4, 0, 0);
            world_->setBlock(chestPosition.x, chestPosition.y, chestPosition.z, Block::Chest);
            ChestData* chest = world_->chestAt(chestPosition, true);
            if (chest)
                chest->slots[0] = {Item::DiamondSword, 1,
                                   Inventory::maxDurability(Item::DiamondSword)};
            smokeTest_.checksPassed &= chest != nullptr;
            ui_.openChest(chestPosition);
            smokeTest_.craftingStartTime = timing_.worldTime;
        }
        break;
    case 8:
        smokeTest_.checksPassed &= timing_.worldTime - smokeTest_.craftingStartTime > 1.0f;
        ui_.closeGameplayInterface(*inventory_);
        ui_.openResetWorld();
        smokeTest_.pauseStartTime = timing_.worldTime;
        break;
    case 9:
        smokeTest_.checksPassed &= std::abs(timing_.worldTime - smokeTest_.pauseStartTime) < 0.08f;
        ui_.openPauseMenu();
        break;
    case 10:
        ui_.resumeGame();
        std::cout << "UI simulation smoke: inventory, table, furnace, and chest advanced; pause, "
                     "settings, and reset confirmation froze world: "
                  << (smokeTest_.checksPassed ? "passed" : "FAILED") << '\n';
        if (!smokeTest_.checksPassed) {
            throw std::runtime_error("UI pause smoke failed");
        }
        break;
    default:
        break;
    }

    synchronizeCursorCapture();
    screenshotRequested_ = true;
}

void Game::renderMenuInterface(int width, int height) {
    const auto cursor = input_.framebufferCursorPosition();
    const int hover = ui_.hoveredMenuItem(cursor, width, height);
    const float scale = UIManager::menuScale(width, height);
    width = static_cast<int>(width / scale);
    height = static_cast<int>(height / scale);
    if (ui_.state() == GameState::WorldSelection || ui_.state() == GameState::CreateWorld) {
        renderer_->renderWorldMenu(width,height,hover,ui_.state()==GameState::CreateWorld,
            savedWorlds_,worldPage_,selectedWorld_,newWorldName_,newWorldSeed_,newWorldMode_,
            worldNameField_,menuMessage_,glfwGetTime());
    } else if(ui_.state()==GameState::WeatherSettings) {
        renderer_->renderWeatherSettings(width,height,hover,settings_);
    } else if (ui_.state() == GameState::Settings || ui_.state() == GameState::VideoSettings) {
        renderer_->renderSettingsCategories(width,height,hover,settings_,ui_.state()==GameState::VideoSettings);
    } else if (ui_.state() == GameState::ResetWorld) {
        renderer_->renderResetMenu(width,height,hover,resetSeedText_,resetMode_);
    } else if (ui_.state() == GameState::Controls) {
        renderer_->renderControlsMenu(width,height,hover,settings_,activeControlBinding_);
    } else {
        renderer_->renderMenu(width,height,false,hover,settings_,gameMode(),
            !Persistence::enabled(),glfwGetTime()<saveWarningUntil_,ui_.state()==GameState::AudioSettings);
    }
}

void Game::returnToMainMenu() {
    if (Persistence::enabled() && !saveAll()) {
        menuMessage_ = "Save failed. Check disk space; world remains open.";
        chat_.addMessage(menuMessage_, ChatTone::Error, glfwGetTime());
        return;
    }
    weather_.reset();
    if (weatherRenderer_) weatherRenderer_->clear();
    if (sounds_) sounds_->setRainAmbience(0,0);
    farming_.reset();
    survival_.reset();
    inventory_.reset();
    player_.reset();
    world_.reset();
    interaction_ = {};
    renderer_->clearParticles();
    chat_ = {};
    wasInWater_ = false;
    fullbright_ = false;
    activeSettingsSlider_ = activeControlBinding_ = -1;
    saveWarningUntil_ = 0.0;
    if (!Persistence::enabled()) {
        timing_.worldTime = 35.0f;
        creativeMode_ = spectatorMode_ = false;
    }
    activeWorld_ = {};
    timing_.bobTime = 0;
    timing_.stepTimer = 0;
    ui_.openMainMenu();
    synchronizeCursorCapture();
}

void Game::runMainMenuSmokeTest() {
    const auto check = [](bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    };
    check(!world_ && !player_ && !inventory_ && !survival_, "Menu created gameplay systems");
    check(ui_.state() == GameState::MainMenu && !input_.cursorCaptured(), "Menu input state");
    const auto capture = [this] {
        glfwPollEvents();
        screenshotRequested_ = true;
        renderFrame(0.0f);
    };
    capture();
    glfwSetWindowSize(window_, 1600, 600);
    capture();
    glfwSetWindowSize(window_, 800, 900);
    capture();
    glfwSetWindowSize(window_, 1280, 720);
    glfwPollEvents();
    for (const auto& sample : std::array<std::pair<double,const char*>,4>{{
             {0,"panorama-start"},{90,"panorama-half"},{179.99,"panorama-before-wrap"},{180.01,"panorama-after-wrap"}}}) {
        int width=0,height=0;
        glfwGetFramebufferSize(window_,&width,&height);
        glViewport(0,0,width,height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderer_->renderMainMenu(width,height,-1,false,false,!Persistence::enabled(),sample.first);
        check(Screenshot::saveBmp(width,height,sample.second),"Panorama capture failed");
    }
    ui_.openSettings();
    capture();
    ui_.openVideoSettings();
    for (const auto size : std::array<glm::ivec2,3>{{{1280,720},{800,600},{480,360}}}) {
        const float scale=UIManager::menuScale(size.x,size.y);
        const auto row=MenuLayout::videoRow(3,static_cast<int>(size.x/scale),static_cast<int>(size.y/scale));
        check(ui_.hoveredMenuItem({(row.x+row.width*.5)*scale,(row.y+row.height*.5)*scale},size.x,size.y)==5,
              "Responsive Video slider hit area");
    }
    capture();
    ui_.handleEscape(nullptr);
    check(ui_.state() == GameState::Settings, "Video Back destination");
    ui_.openAudioSettings();
    capture();
    ui_.handleEscape(nullptr);
    check(ui_.state() == GameState::Settings, "Audio Back destination");
    ui_.openControls();
    capture();
    ui_.handleEscape(nullptr);
    ui_.backFromSettings();
    check(ui_.state() == GameState::MainMenu, "Settings Back destination");
    for (int index = 0; index < 3; ++index) {
        const auto rect = UIManager::mainMenuButton(index, 1280, 720);
        check(ui_.hoveredMenuItem({rect.x + rect.width * .5f, rect.y + rect.height * .5f},
                                 1280, 720) == index, "Main menu button hit area");
    }
    SavedWorld testWorld;
    if (Persistence::enabled()) {
        refreshWorlds();
        ui_.openWorldSelection();
        capture();
        ui_.openCreateWorld();
        capture();
        testWorld = savedWorlds_.empty() ? WorldLibrary::create("Menu Test", seed_, gameMode()) : savedWorlds_.front();
        playSavedWorld(testWorld);
    } else {
        createWorldAndSystems(); ui_.resumeGame(); synchronizeCursorCapture();
    }
    check(world_ && player_ && inventory_ && survival_ && input_.cursorCaptured(), "PLAY startup");
    const auto position = player_->position();
    const int initialDiamonds = inventory_->count(Item::Diamond);
    inventory_->add(Item::Diamond, 1);
    timing_.worldTime = 290.0f;
    weather_->set(WeatherType::Thunder,179);
    returnToMainMenu();
    capture();
    check(!world_ && !input_.cursorCaptured(), "Return-to-menu teardown");
    if (Persistence::enabled()) {
        refreshWorlds();
        for (const auto& entry : savedWorlds_) if (entry.directory == testWorld.directory) testWorld = entry;
        playSavedWorld(testWorld);
    } else {
        createWorldAndSystems(); ui_.resumeGame(); synchronizeCursorCapture();
    }
    if (Persistence::enabled()) {
        check(glm::distance(player_->position(), position) < .01f &&
              std::abs(timing_.worldTime - 290.0f) < .01f &&
              inventory_->count(Item::Diamond) == initialDiamonds + 1 &&
              weather_->type()==WeatherType::Thunder && weather_->remaining()==179, "Full game save/load/weather");
    } else {
        check(timing_.worldTime == 35.0f && weather_->type()==WeatherType::Clear && weather_->intensity()==0 && gameMode() == GameMode::Survival &&
              inventory_->count(Item::Diamond) == initialDiamonds,
              "Demo did not reset its session");
    }
    ui_.openPauseMenu();
    capture();
    returnToMainMenu();
    if (Persistence::enabled()) {
        std::array<SavedWorld,3> separate;
        std::array<glm::vec3,3> positions;
        const glm::ivec3 edit(0,240,0);
        for (int index=0; index<3; ++index) {
            newWorldName_ = "World / CON " + std::to_string(index);
            newWorldSeed_ = std::to_string(123456U+index);
            newWorldMode_ = index==1 ? GameMode::Creative : GameMode::Survival;
            createNamedWorld();
            separate[index] = activeWorld_;
            check(world_ != nullptr, "Create separate world failed");
            world_->prepareSpawnTerrain({.5f,240,.5f});
            world_->setBlock(edit.x,edit.y,edit.z,index==1 ? Block::GoldBlock : Block::DiamondBlock);
            inventory_->clear(); inventory_->add(Item::Diamond,index+2);
            positions[index] = player_->position() + glm::vec3(index+1.0f,2,0);
            player_->teleport(positions[index]);
            check(survival_->summonMob(index==1 ? "NijikaIjichi" : "cow",positions[index]+glm::vec3(2,0,0)), "Mob initialization");
            timing_.worldTime = 100.0f+index*50;
            weather_->set(static_cast<WeatherType>(index),180.0f+index);
            returnToMainMenu();
            check(!world_, "Separate world teardown failed");
        }
        refreshWorlds();
        ui_.openWorldSelection(); capture();
        for (int index=0; index<3; ++index) {
            for (const auto& entry : savedWorlds_) if (entry.directory==separate[index].directory) separate[index]=entry;
            playSavedWorld(separate[index]);
            check(world_ && world_->seed()==123456U+index && inventory_->count(Item::Diamond)==index+2 &&
                  glm::distance(player_->position(),positions[index])<.01f &&
                  std::abs(timing_.worldTime-(100.0f+index*50))<.01f &&
                  gameMode()==(index==1 ? GameMode::Creative : GameMode::Survival),"World state mixed across saves");
            check(weather_->type()==static_cast<WeatherType>(index) && weather_->remaining()==180.0f+index,"Weather mixed across worlds");
            world_->prepareSpawnTerrain({.5f,240,.5f});
            check(world_->getBlock(edit.x,edit.y,edit.z)==(index==1 ? Block::GoldBlock : Block::DiamondBlock),"Terrain edit did not persist");
            if (index==1) {
                const auto mobs = survival_->renderBillboards();
                check(mobs.size()==1 && mobs[0].variant==2,"Billboard mob identity did not persist");
            } else check(!survival_->renderCuboids().empty() && survival_->renderBillboards().empty(),"Passive mob did not persist independently");
            returnToMainMenu();
        }
        std::cout << "Three independent worlds: seed, mode, terrain, player, inventory, mobs and time passed\n";
    } else check(!std::filesystem::exists("saves"),"Demo created saved-world directories");
    std::cout << "Main menu smoke passed: panorama resize, settings/back, PLAY, return, reload\n" << std::flush;
}
