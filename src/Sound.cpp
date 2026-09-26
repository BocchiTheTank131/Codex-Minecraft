#include "Sound.h"
#include "Definitions.h"

// miniaudio's Vorbis decoder uses this bundled, single-file adapter.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244 4245 4456 4457 4458 4267 4701 4702 4706)
#endif
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
constexpr std::size_t MaximumVoices = 32;

const char* materialGroup(SoundMaterial material) {
    switch (material) {
    case SoundMaterial::Wood: return "dig/wood";
    case SoundMaterial::Grass: return "dig/grass";
    case SoundMaterial::Gravel: return "dig/gravel";
    case SoundMaterial::Sand: return "dig/sand";
    case SoundMaterial::Snow: return "dig/snow";
    default: return "dig/stone";
    }
}

const char* mobFolder(MobSoundType type) {
    switch (type) {
    case MobSoundType::Cow: return "cow/";
    case MobSoundType::Pig: return "pig/";
    case MobSoundType::Sheep: return "sheep/";
    default: return "";
    }
}
} // namespace

struct SoundSystem::Impl {
    struct Asset {
        std::unique_ptr<ma_sound> source;
    };
    struct Voice {
        std::unique_ptr<ma_sound> sound;
        int priority = 0;
    };

    ma_engine engine{};
    bool engineReady = false;
    std::filesystem::path root;
    std::vector<Asset> assets;
    std::unordered_map<std::string, std::vector<std::size_t>> groups;
    std::unordered_map<std::string, std::size_t> lastVariant;
    std::unordered_set<std::string> warnedGroups;
    std::vector<Voice> voices;
    std::mt19937 random{std::random_device{}()};
    glm::vec3 listener{0.0f};
    float masterVolume = 1.0f;
    float pickupCooldown = 0.0f;
    float xpCooldown = 0.0f;
    float eatBurpDelay = -1.0f;

    explicit Impl(const std::filesystem::path& executablePath) {
        std::vector<std::filesystem::path> candidates;
        std::error_code error;
        if (!executablePath.empty()) {
            const auto absolute = std::filesystem::absolute(executablePath, error);
            if (!error && std::filesystem::is_regular_file(absolute, error))
                candidates.push_back(absolute.parent_path() / "sounds");
        }
#ifdef _WIN32
        wchar_t executableBuffer[32768]{};
        const DWORD length = GetModuleFileNameW(nullptr, executableBuffer, 32768);
        if (length > 0 && length < 32768)
            candidates.push_back(std::filesystem::path(executableBuffer).parent_path() /
                                 "sounds");
#elif defined(__linux__)
        const auto processExecutable = std::filesystem::read_symlink("/proc/self/exe", error);
        if (!error)
            candidates.push_back(processExecutable.parent_path() / "sounds");
#endif
        auto current = std::filesystem::current_path(error);
        for (int depth = 0; !error && depth < 5; ++depth) {
            candidates.push_back(current / "sounds");
            if (current == current.parent_path()) break;
            current = current.parent_path();
        }
        for (const auto& candidate : candidates) {
            error.clear();
            if (std::filesystem::is_directory(candidate, error)) {
                root = candidate;
                break;
            }
        }
        if (root.empty()) {
            std::cerr << "Audio: sounds directory not found; effects disabled\n";
            return;
        }
        const ma_engine_config config = ma_engine_config_init();
        if (ma_engine_init(&config, &engine) != MA_SUCCESS) {
            std::cerr << "Audio: output device unavailable; effects disabled\n";
            return;
        }
        engineReady = true;
        error.clear();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
            if (error) break;
            if (!entry.is_regular_file() || entry.path().extension() != ".ogg") continue;
            const auto relative = std::filesystem::relative(entry.path(), root, error);
            if (error) continue;
            const std::string name = relative.generic_string();
            std::string group = name.substr(0, name.find_last_of('.'));
            while (!group.empty() && group.back() >= '0' && group.back() <= '9')
                group.pop_back();
            auto source = std::make_unique<ma_sound>();
            const std::string path = entry.path().string();
            if (ma_sound_init_from_file(&engine, path.c_str(), MA_SOUND_FLAG_DECODE,
                                        nullptr, nullptr, source.get()) != MA_SUCCESS) {
                std::cerr << "Audio: cannot decode " << name << '\n';
                continue;
            }
            ma_sound_set_spatialization_enabled(source.get(), MA_FALSE);
            groups[group].push_back(assets.size());
            assets.push_back({std::move(source)});
        }
        for (const char* required : {"break", "pop", "orb", "splash", "click",
                                     "dig/stone", "dig/wood", "dig/grass"}) {
            if (groups.find(required) == groups.end()) {
                std::cerr << "Audio: missing sound group " << required << '\n';
                warnedGroups.insert(required);
            }
        }
        std::cout << "Audio: loaded " << assets.size() << " OGG effects from "
                  << root.string() << '\n';
    }

    ~Impl() {
        if (!engineReady) return;
        for (auto& voice : voices) ma_sound_uninit(voice.sound.get());
        for (auto& asset : assets) ma_sound_uninit(asset.source.get());
        ma_engine_uninit(&engine);
    }

    void play(const std::string& group, float gain, int priority,
              const glm::vec3* position = nullptr, float pitchRange = 0.06f) {
        if (!engineReady || masterVolume <= 0.001f) return;
        const auto found = groups.find(group);
        if (found == groups.end() || found->second.empty()) {
            if (warnedGroups.insert(group).second)
                std::cerr << "Audio: missing sound group " << group << '\n';
            return;
        }
        if (position && glm::distance(*position, listener) > 28.0f) return;
        voices.erase(std::remove_if(voices.begin(), voices.end(), [](Voice& voice) {
            if (ma_sound_is_playing(voice.sound.get())) return false;
            ma_sound_uninit(voice.sound.get());
            return true;
        }), voices.end());
        if (voices.size() >= MaximumVoices) {
            auto weakest = std::min_element(voices.begin(), voices.end(),
                [](const Voice& a, const Voice& b) { return a.priority < b.priority; });
            if (weakest == voices.end() || weakest->priority >= priority) return;
            ma_sound_uninit(weakest->sound.get());
            voices.erase(weakest);
        }
        const auto& variants = found->second;
        std::uniform_int_distribution<std::size_t> selection(0, variants.size() - 1);
        std::size_t variant = selection(random);
        const auto previous = lastVariant.find(group);
        if (variants.size() > 1 && previous != lastVariant.end() &&
            variant == previous->second)
            variant = (variant + 1) % variants.size();
        lastVariant[group] = variant;
        auto sound = std::make_unique<ma_sound>();
        if (ma_sound_init_copy(&engine, assets[variants[variant]].source.get(),
                               0, nullptr, sound.get()) != MA_SUCCESS) return;
        ma_sound_set_volume(sound.get(), gain);
        if (pitchRange > 0.0f) {
            std::uniform_real_distribution<float> pitch(1.0f - pitchRange,
                                                        1.0f + pitchRange);
            ma_sound_set_pitch(sound.get(), pitch(random));
        }
        if (position) {
            ma_sound_set_spatialization_enabled(sound.get(), MA_TRUE);
            ma_sound_set_position(sound.get(), position->x, position->y, position->z);
            ma_sound_set_min_distance(sound.get(), 2.0f);
            ma_sound_set_max_distance(sound.get(), 28.0f);
            ma_sound_set_attenuation_model(sound.get(), ma_attenuation_model_linear);
        } else {
            ma_sound_set_spatialization_enabled(sound.get(), MA_FALSE);
        }
        ma_sound_start(sound.get());
        voices.push_back({std::move(sound), priority});
    }

    void tick(float deltaTime) {
        pickupCooldown = std::max(0.0f, pickupCooldown - deltaTime);
        xpCooldown = std::max(0.0f, xpCooldown - deltaTime);
        if (eatBurpDelay >= 0.0f) {
            eatBurpDelay -= deltaTime;
            if (eatBurpDelay < 0.0f) play("burp", 0.55f, 4, nullptr, 0.0f);
        }
    }
};

SoundSystem::SoundSystem(const std::filesystem::path& path)
    : impl_(std::make_unique<Impl>(path)) {}
SoundSystem::~SoundSystem() = default;
void SoundSystem::setMasterVolume(float value) {
    impl_->masterVolume = std::clamp(value, 0.0f, 1.0f);
    if (impl_->engineReady) ma_engine_set_volume(&impl_->engine, impl_->masterVolume);
}
void SoundSystem::setListener(const glm::vec3& position, const glm::vec3& forward) {
    impl_->listener = position;
    if (!impl_->engineReady) return;
    ma_engine_listener_set_position(&impl_->engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&impl_->engine, 0, forward.x, forward.y, forward.z);
}
void SoundSystem::update(float deltaTime) { impl_->tick(deltaTime); }
void SoundSystem::playBlockBreak(Block block, const glm::vec3& position) {
    impl_->play(materialGroup(blockDefinition(block).soundMaterial), .85f, 6, &position);
}
void SoundSystem::playBlockPlace(Block block, const glm::vec3& position) {
    impl_->play(materialGroup(blockDefinition(block).soundMaterial), .50f, 5, &position);
}
void SoundSystem::playFootstep(Block surface, const glm::vec3& position, bool sneaking) {
    impl_->play(materialGroup(blockDefinition(surface).soundMaterial),
                sneaking ? .16f : .29f, 2, &position);
}
void SoundSystem::playToolBreak() { impl_->play("break", .85f, 8); }
void SoundSystem::playPlayerHurt() { impl_->play("damage/hit", .80f, 9); }
void SoundSystem::playFallDamage(bool severe) {
    impl_->play(severe ? "damage/fallbig" : "damage/fallsmall", .85f, 9);
}
void SoundSystem::playSplash(const glm::vec3& position) {
    impl_->play("splash", .65f, 5, &position);
}
void SoundSystem::playItemPickup() {
    if (impl_->pickupCooldown > 0.0f) return;
    impl_->pickupCooldown = .07f;
    impl_->play("pop", .50f, 5);
}
void SoundSystem::playXpPickup() {
    if (impl_->xpCooldown > 0.0f) return;
    impl_->xpCooldown = .08f;
    impl_->play("orb", .55f, 5);
}
void SoundSystem::playEat() {
    impl_->play("eat", .70f, 5);
    impl_->eatBurpDelay = .20f;
}
void SoundSystem::playDoor(bool open, const glm::vec3& position) {
    impl_->play(open ? "wooden_door/open" : "wooden_door/close", .85f, 7, &position);
}
void SoundSystem::playChest(bool open, const glm::vec3& position) {
    impl_->play(open ? "chest/open" : "chest/close", .80f, 6, &position);
}
void SoundSystem::playClick() { impl_->play("click", .38f, 3, nullptr, 0.0f); }
void SoundSystem::playMobAmbient(MobSoundType type, const glm::vec3& position) {
    const std::string folder = mobFolder(type);
    if (!folder.empty()) impl_->play(folder + "say", .70f, 2, &position);
}
void SoundSystem::playMobHurt(MobSoundType type, const glm::vec3& position) {
    const std::string folder = mobFolder(type);
    if (!folder.empty() && impl_->groups.count(folder + "hurt"))
        impl_->play(folder + "hurt", .78f, 6, &position);
}
void SoundSystem::playMobDeath(MobSoundType type, const glm::vec3& position) {
    const std::string folder = mobFolder(type);
    if (folder.empty()) return;
    if (impl_->groups.count(folder + "death"))
        impl_->play(folder + "death", .80f, 7, &position);
    else if (impl_->groups.count(folder + "hurt"))
        impl_->play(folder + "hurt", .80f, 7, &position);
}
void SoundSystem::playMobStep(MobSoundType type, const glm::vec3& position) {
    const std::string folder = mobFolder(type);
    if (!folder.empty()) impl_->play(folder + "step", .24f, 1, &position);
}
bool SoundSystem::verifyLibrary() {
    if (!impl_->engineReady) return true; // Headless systems should remain playable.
    for (const char* group : {
             "break", "pop", "orb", "splash", "click", "burp", "eat",
             "damage/hit", "damage/fallsmall", "damage/fallbig",
             "dig/stone", "dig/wood", "dig/grass", "dig/gravel",
             "dig/sand", "dig/snow", "wooden_door/open",
             "wooden_door/close", "chest/open", "chest/close",
             "cow/say", "cow/hurt", "cow/step", "pig/say",
             "pig/step", "pig/death", "sheep/say", "sheep/step"}) {
        const auto found = impl_->groups.find(group);
        if (found == impl_->groups.end() || found->second.empty()) return false;
    }
    if (impl_->assets.size() < 70) return false;
    const float savedVolume = impl_->masterVolume;
    setMasterVolume(1.0f);
    const std::size_t before = impl_->voices.size();
    impl_->play("click", .1f, 3, nullptr, 0.0f);
    impl_->play("pop", .1f, 3, nullptr, 0.0f);
    impl_->play("orb", .1f, 3, nullptr, 0.0f);
    const bool concurrent = impl_->voices.size() >= before + 3;
    setMasterVolume(0.0f);
    const std::size_t mutedCount = impl_->voices.size();
    impl_->play("click", .1f, 3, nullptr, 0.0f);
    const bool mutePassed = impl_->voices.size() == mutedCount;
    setMasterVolume(savedVolume);
    return concurrent && mutePassed;
}
