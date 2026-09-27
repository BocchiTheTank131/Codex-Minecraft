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
#include <chrono>
#include <cmath>
#include <numeric>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#ifdef VOXEL_STANDALONE
#include "EmbeddedResources.h"
#endif
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
constexpr std::size_t MaximumVoices = 32;
enum class AudioCategory { Sfx, PassiveMob, HostileMob };

AudioCategory mobCategory(MobSoundType type) {
    return type == MobSoundType::Pillager || type == MobSoundType::BillboardHostile
               ? AudioCategory::HostileMob
                                          : AudioCategory::PassiveMob;
}

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
#ifdef VOXEL_STANDALONE
        std::vector<float> pcm;
        ma_uint64 frameCount = 0;
        ma_uint32 channels = 0;
        ma_uint32 sampleRate = 0;
#else
        std::unique_ptr<ma_sound> source;
#endif
    };
    struct Voice {
        std::unique_ptr<ma_sound> sound;
#ifdef VOXEL_STANDALONE
        std::unique_ptr<ma_audio_buffer> buffer;
#endif
        int priority = 0;
        float baseGain = 1.0f;
        AudioCategory category = AudioCategory::Sfx;
    };
    struct MusicTrack {
        std::string name;
#ifdef VOXEL_STANDALONE
        const void* bytes = nullptr;
        std::size_t byteCount = 0;
#else
        std::filesystem::path path;
#endif
    };

    ma_engine engine{};
    bool engineReady = false;
    std::filesystem::path root;
    std::vector<Asset> assets;
    std::unordered_map<std::string, std::vector<std::size_t>> groups;
    std::unordered_map<std::string, std::size_t> lastVariant;
    std::unordered_set<std::string> warnedGroups;
    std::vector<Voice> voices;
    std::vector<MusicTrack> musicTracks;
    std::vector<std::size_t> musicBag;
    std::size_t lastMusic = static_cast<std::size_t>(-1);
    std::size_t musicGeneration = 0;
    std::unique_ptr<ma_decoder> musicDecoder;
    std::unique_ptr<ma_sound> musicSound;
    std::mt19937 random{std::random_device{}()};
    glm::vec3 listener{0.0f};
    float masterVolume = 1.0f;
    float musicVolume = 1.0f;
    float sfxVolume = 1.0f;
    float passiveMobVolume = 1.0f;
    float hostileMobVolume = 1.0f;
    bool musicFadeOutStarted = false;
    ma_uint64 musicLengthFrames = 0;
    ma_uint32 musicSampleRate = 0;
    float pickupCooldown = 0.0f;
    float xpCooldown = 0.0f;
    float eatBurpDelay = -1.0f;

    explicit Impl(const std::filesystem::path& executablePath) {
#ifndef VOXEL_STANDALONE
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
#else
        (void)executablePath;
#endif
        const ma_engine_config config = ma_engine_config_init();
        if (ma_engine_init(&config, &engine) != MA_SUCCESS) {
            std::cerr << "Audio: output device unavailable; effects disabled\n";
            return;
        }
        engineReady = true;
#ifdef VOXEL_STANDALONE
        for (const auto& embedded : EmbeddedSounds) {
            const HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(embedded.id),
                                                MAKEINTRESOURCEW(10));
            const HGLOBAL loaded = resource ? LoadResource(nullptr, resource) : nullptr;
            const void* bytes = loaded ? LockResource(loaded) : nullptr;
            const DWORD length = resource ? SizeofResource(nullptr, resource) : 0;
            if (!bytes || length == 0) {
                std::cerr << "Audio: missing embedded sound " << embedded.name << '\n';
                continue;
            }
            const std::string name = embedded.name;
            if (name.rfind("background/", 0) == 0) {
                musicTracks.push_back({name, bytes, length});
                continue;
            }
            ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
            ma_uint64 frameCount = 0;
            void* decoded = nullptr;
            if (ma_decode_memory(bytes, length, &decoderConfig, &frameCount, &decoded) != MA_SUCCESS ||
                !decoded || frameCount == 0 || decoderConfig.channels == 0) {
                std::cerr << "Audio: cannot decode " << embedded.name << '\n';
                if (decoded) ma_free(decoded, nullptr);
                continue;
            }
            Asset asset;
            asset.frameCount = frameCount;
            asset.channels = decoderConfig.channels;
            asset.sampleRate = decoderConfig.sampleRate;
            const auto* samples = static_cast<const float*>(decoded);
            asset.pcm.assign(samples, samples + frameCount * asset.channels);
            ma_free(decoded, nullptr);
            std::string group = name.substr(0, name.find_last_of('.'));
            while (!group.empty() && group.back() >= '0' && group.back() <= '9')
                group.pop_back();
            groups[group].push_back(assets.size());
            assets.push_back(std::move(asset));
        }
#else
        error.clear();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
            if (error) break;
            if (!entry.is_regular_file() || entry.path().extension() != ".ogg") continue;
            const auto relative = std::filesystem::relative(entry.path(), root, error);
            if (error) continue;
            const std::string name = relative.generic_string();
            if (name.rfind("background/", 0) == 0) {
                musicTracks.push_back({name, entry.path()});
                continue;
            }
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
#endif
        for (const char* required : {"break", "pop", "orb", "splash", "click",
                                     "dig/stone", "dig/wood", "dig/grass"}) {
            if (groups.find(required) == groups.end()) {
                std::cerr << "Audio: missing sound group " << required << '\n';
                warnedGroups.insert(required);
            }
        }
        std::cout << "Audio: loaded " << assets.size() << " OGG effects"
#ifdef VOXEL_STANDALONE
                  << " from embedded resources\n";
#else
                  << " from " << root.string() << '\n';
#endif
        if (musicTracks.empty())
            std::cerr << "Audio: no background OGG tracks found; music disabled\n";
        else
            std::cout << "Audio: indexed " << musicTracks.size() << " background tracks\n";
    }

    ~Impl() {
        if (!engineReady) return;
        stopMusic();
        for (auto& voice : voices) releaseVoice(voice);
#ifndef VOXEL_STANDALONE
        for (auto& asset : assets) ma_sound_uninit(asset.source.get());
#endif
        ma_engine_uninit(&engine);
    }

    static void releaseVoice(Voice& voice) {
        ma_sound_uninit(voice.sound.get());
#ifdef VOXEL_STANDALONE
        ma_audio_buffer_uninit(voice.buffer.get());
#endif
    }

    float categoryVolume(AudioCategory category) const {
        switch (category) {
        case AudioCategory::PassiveMob: return passiveMobVolume;
        case AudioCategory::HostileMob: return hostileMobVolume;
        default: return sfxVolume;
        }
    }

    void stopMusic() {
        if (musicSound) {
            ma_sound_uninit(musicSound.get());
            musicSound.reset();
        }
        if (musicDecoder) {
            ma_decoder_uninit(musicDecoder.get());
            musicDecoder.reset();
        }
        musicFadeOutStarted = false;
        musicLengthFrames = 0;
        musicSampleRate = 0;
    }

    void refillMusicBag() {
        musicBag.resize(musicTracks.size());
        std::iota(musicBag.begin(), musicBag.end(), 0);
        std::shuffle(musicBag.begin(), musicBag.end(), random);
        if (musicBag.size() > 1 && musicBag.back() == lastMusic)
            std::swap(musicBag.back(), musicBag.front());
    }

    void startNextMusic() {
        stopMusic();
        while (!musicTracks.empty()) {
            if (musicBag.empty()) refillMusicBag();
            const std::size_t index = musicBag.back();
            musicBag.pop_back();
            const MusicTrack& track = musicTracks[index];
            auto decoder = std::make_unique<ma_decoder>();
            const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
#ifdef VOXEL_STANDALONE
            const ma_result decoded = ma_decoder_init_memory(
                track.bytes, track.byteCount, &config, decoder.get());
#else
            const ma_result decoded = ma_decoder_init_file(
                track.path.string().c_str(), &config, decoder.get());
#endif
            if (decoded != MA_SUCCESS) {
                std::cerr << "Audio: cannot stream background track " << track.name << '\n';
                musicTracks.erase(musicTracks.begin() + static_cast<std::ptrdiff_t>(index));
                musicBag.clear();
                lastMusic = static_cast<std::size_t>(-1);
                continue;
            }
            auto sound = std::make_unique<ma_sound>();
            if (ma_sound_init_from_data_source(
                    &engine, reinterpret_cast<ma_data_source*>(decoder.get()),
                    0, nullptr, sound.get()) != MA_SUCCESS) {
                ma_decoder_uninit(decoder.get());
                std::cerr << "Audio: cannot play background track " << track.name << '\n';
                musicTracks.erase(musicTracks.begin() + static_cast<std::ptrdiff_t>(index));
                musicBag.clear();
                lastMusic = static_cast<std::size_t>(-1);
                continue;
            }
            ma_sound_set_spatialization_enabled(sound.get(), MA_FALSE);
            ma_sound_set_volume(sound.get(), musicVolume * 0.30f);
            ma_sound_set_fade_in_milliseconds(sound.get(), 0.0f, 1.0f, 800);
            if (ma_sound_start(sound.get()) != MA_SUCCESS) {
                ma_sound_uninit(sound.get());
                ma_decoder_uninit(decoder.get());
                std::cerr << "Audio: cannot start background track " << track.name << '\n';
                musicTracks.erase(musicTracks.begin() + static_cast<std::ptrdiff_t>(index));
                musicBag.clear();
                lastMusic = static_cast<std::size_t>(-1);
                continue;
            }
            lastMusic = index;
            ++musicGeneration;
            musicDecoder = std::move(decoder);
            musicSound = std::move(sound);
            ma_sound_get_length_in_pcm_frames(musicSound.get(), &musicLengthFrames);
            ma_decoder_get_data_format(musicDecoder.get(), nullptr, nullptr, &musicSampleRate,
                                       nullptr, 0);
            std::cout << "Audio: playing background track " << track.name << '\n';
            return;
        }
        std::cerr << "Audio: no playable background tracks; music disabled\n";
    }

    void updateMusic() {
        if (!musicSound) return;
        if (ma_sound_at_end(musicSound.get())) {
            startNextMusic();
            return;
        }
        if (!musicFadeOutStarted) {
            ma_uint64 cursor = 0;
            if (ma_sound_get_cursor_in_pcm_frames(musicSound.get(), &cursor) == MA_SUCCESS &&
                musicSampleRate > 0 && musicLengthFrames > cursor &&
                musicLengthFrames - cursor <= musicSampleRate) {
                ma_sound_set_fade_in_milliseconds(musicSound.get(), -1.0f, 0.0f, 900);
                musicFadeOutStarted = true;
            }
        }
    }

    void play(const std::string& group, float gain, int priority,
              const glm::vec3* position = nullptr, float pitchRange = 0.06f,
              AudioCategory category = AudioCategory::Sfx) {
        if (!engineReady || masterVolume <= 0.001f ||
            categoryVolume(category) <= 0.001f) return;
        const auto found = groups.find(group);
        if (found == groups.end() || found->second.empty()) {
            if (warnedGroups.insert(group).second)
                std::cerr << "Audio: missing sound group " << group << '\n';
            return;
        }
        if (position && glm::distance(*position, listener) > 28.0f) return;
        voices.erase(std::remove_if(voices.begin(), voices.end(), [](Voice& voice) {
            if (ma_sound_is_playing(voice.sound.get())) return false;
            releaseVoice(voice);
            return true;
        }), voices.end());
        if (voices.size() >= MaximumVoices) {
            auto weakest = std::min_element(voices.begin(), voices.end(),
                [](const Voice& a, const Voice& b) { return a.priority < b.priority; });
            if (weakest == voices.end() || weakest->priority >= priority) return;
            releaseVoice(*weakest);
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
#ifdef VOXEL_STANDALONE
        const Asset& asset = assets[variants[variant]];
        auto buffer = std::make_unique<ma_audio_buffer>();
        ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
            ma_format_f32, asset.channels, asset.frameCount, asset.pcm.data(), nullptr);
        bufferConfig.sampleRate = asset.sampleRate;
        if (ma_audio_buffer_init(&bufferConfig, buffer.get()) != MA_SUCCESS) return;
        if (ma_sound_init_from_data_source(&engine,
                reinterpret_cast<ma_data_source*>(buffer.get()), 0, nullptr,
                sound.get()) != MA_SUCCESS) {
            ma_audio_buffer_uninit(buffer.get());
            return;
        }
#else
        if (ma_sound_init_copy(&engine, assets[variants[variant]].source.get(),
                               0, nullptr, sound.get()) != MA_SUCCESS) return;
#endif
        ma_sound_set_volume(sound.get(), gain * categoryVolume(category));
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
#ifdef VOXEL_STANDALONE
        voices.push_back({std::move(sound), std::move(buffer), priority, gain, category});
#else
        voices.push_back({std::move(sound), priority, gain, category});
#endif
    }

    void tick(float deltaTime) {
        updateMusic();
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
void SoundSystem::setCategoryVolumes(float music, float sfx, float passiveMobs,
                                     float hostileMobs) {
    impl_->musicVolume = std::clamp(music, 0.0f, 1.0f);
    impl_->sfxVolume = std::clamp(sfx, 0.0f, 1.0f);
    impl_->passiveMobVolume = std::clamp(passiveMobs, 0.0f, 1.0f);
    impl_->hostileMobVolume = std::clamp(hostileMobs, 0.0f, 1.0f);
    if (impl_->musicSound)
        ma_sound_set_volume(impl_->musicSound.get(), impl_->musicVolume * 0.30f);
    for (auto& voice : impl_->voices)
        ma_sound_set_volume(voice.sound.get(),
                            voice.baseGain * impl_->categoryVolume(voice.category));
}
void SoundSystem::startMusic() {
    if (impl_->engineReady && !impl_->musicSound && !impl_->musicTracks.empty())
        impl_->startNextMusic();
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
    if (!folder.empty()) impl_->play(folder + "say", .70f, 2, &position, .06f,
                                     mobCategory(type));
}
void SoundSystem::playMobHurt(MobSoundType type, const glm::vec3& position) {
    const std::string folder = mobFolder(type);
    if (!folder.empty() && impl_->groups.count(folder + "hurt"))
        impl_->play(folder + "hurt", .78f, 6, &position, .06f, mobCategory(type));
}
void SoundSystem::playMobDeath(MobSoundType type, const glm::vec3& position) {
    if (type == MobSoundType::BillboardHostile) {
        impl_->play("boowomp", .80f, 7, &position, .06f, AudioCategory::HostileMob);
        return;
    }
    const std::string folder = mobFolder(type);
    if (folder.empty()) return;
    if (impl_->groups.count(folder + "death"))
        impl_->play(folder + "death", .80f, 7, &position, .06f, mobCategory(type));
    else if (impl_->groups.count(folder + "hurt"))
        impl_->play(folder + "hurt", .80f, 7, &position, .06f, mobCategory(type));
}
void SoundSystem::playMobStep(MobSoundType type, const glm::vec3& position) {
    const std::string folder = mobFolder(type);
    if (!folder.empty()) impl_->play(folder + "step", .24f, 1, &position, .06f,
                                     mobCategory(type));
}
bool SoundSystem::verifyLibrary() {
    if (!impl_->engineReady) return true; // Headless systems should remain playable.
    for (const char* group : {
             "break", "pop", "orb", "splash", "click", "burp", "eat",
             "damage/hit", "damage/fallsmall", "damage/fallbig",
             "dig/stone", "dig/wood", "dig/grass", "dig/gravel",
             "dig/sand", "dig/snow", "wooden_door/open",
             "wooden_door/close", "chest/open", "chest/close",
             "boowomp",
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
    const float music = impl_->musicVolume;
    const float sfx = impl_->sfxVolume;
    const float passive = impl_->passiveMobVolume;
    const float hostile = impl_->hostileMobVolume;
    setCategoryVolumes(music, 0.0f, passive, hostile);
    impl_->play("click", .1f, 3, nullptr, 0.0f);
    const bool sfxMutePassed = impl_->voices.size() == mutedCount;
    setCategoryVolumes(music, sfx, 0.0f, hostile);
    impl_->play("cow/say", .1f, 3, nullptr, 0.0f, AudioCategory::PassiveMob);
    const bool passiveMutePassed = impl_->voices.size() == mutedCount;
    setCategoryVolumes(music, sfx, passive, 0.0f);
    impl_->play("click", .1f, 3, nullptr, 0.0f, AudioCategory::HostileMob);
    const bool hostileMutePassed = impl_->voices.size() == mutedCount;
    setCategoryVolumes(music, sfx, passive, hostile);
    return concurrent && mutePassed && sfxMutePassed && passiveMutePassed &&
           hostileMutePassed;
}

bool SoundSystem::verifyMusic() {
    if (!impl_->engineReady || impl_->musicTracks.empty()) return true;
    if (!impl_->musicSound || !impl_->musicDecoder) return false;
    const float savedMaster = impl_->masterVolume;
    const float savedMusic = impl_->musicVolume;
    const float savedSfx = impl_->sfxVolume;
    const float savedPassive = impl_->passiveMobVolume;
    const float savedHostile = impl_->hostileMobVolume;
    const auto checkGain = [&](float master, float music, float expected) {
        setMasterVolume(master);
        setCategoryVolumes(music, savedSfx, savedPassive, savedHostile);
        const float actual = ma_engine_get_volume(&impl_->engine) *
                             ma_sound_get_volume(impl_->musicSound.get());
        return std::abs(actual - expected) < 0.0001f;
    };
    const bool gainPassed = checkGain(1.0f, 1.0f, .30f) &&
                            checkGain(1.0f, .5f, .15f) &&
                            checkGain(1.0f, .1f, .03f) &&
                            checkGain(.5f, 1.0f, .15f) &&
                            checkGain(0.0f, 1.0f, 0.0f);
    setMasterVolume(savedMaster);
    setCategoryVolumes(savedMusic, savedSfx, savedPassive, savedHostile);
    if (!gainPassed) return false;

    const std::size_t trackCount = impl_->musicTracks.size();
    std::unordered_set<std::string> heard;
    std::string previous;
    for (std::size_t i = 0; i <= trackCount; ++i) {
        if (!impl_->musicSound || impl_->lastMusic >= impl_->musicTracks.size()) return false;
        const std::string current = impl_->musicTracks[impl_->lastMusic].name;
        if (trackCount > 1 && current == previous) return false;
        if (i < trackCount && !heard.insert(current).second) return false;
        previous = current;
        if (i == trackCount) break;
        ma_uint64 length = 0;
        if (ma_sound_get_length_in_pcm_frames(impl_->musicSound.get(), &length) != MA_SUCCESS ||
            length < 2000) return false;
        if (ma_sound_seek_to_pcm_frame(impl_->musicSound.get(), length - 1000) != MA_SUCCESS)
            return false;
        const std::size_t generation = impl_->musicGeneration;
        for (int attempt = 0; attempt < 100 && impl_->musicGeneration == generation; ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            impl_->updateMusic();
        }
        if (impl_->musicGeneration == generation) return false;
    }
    std::cout << "Music smoke: " << heard.size()
              << " unique streamed tracks, automatic transitions, shuffle boundary, "
                 "and exact category gain passed\n";
    return heard.size() == trackCount;
}
