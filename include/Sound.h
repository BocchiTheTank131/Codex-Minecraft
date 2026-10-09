#pragma once

#include "Block.h"

#include <glm/vec3.hpp>
#include <filesystem>
#include <memory>

enum class MobSoundType { Cow = 0, Pig = 1, Sheep = 2, Villager = 4,
                          Pillager = 5, BillboardHostile = 6 };

class SoundSystem {
public:
    explicit SoundSystem(const std::filesystem::path& executablePath = {});
    ~SoundSystem();
    SoundSystem(const SoundSystem&) = delete;
    SoundSystem& operator=(const SoundSystem&) = delete;

    void setMasterVolume(float value);
    void setCategoryVolumes(float music, float sfx, float passiveMobs, float hostileMobs);
    void startMusic();
    void setRainAmbience(float outdoor, float sheltered);
    void playLightning(const glm::vec3& position, bool thunder, float muffle);
    bool verifyWeather();
    void setListener(const glm::vec3& position, const glm::vec3& forward);
    void update(float deltaTime);
    void playBlockBreak(Block block, const glm::vec3& position);
    void playBlockPlace(Block block, const glm::vec3& position);
    void playFootstep(Block surface, const glm::vec3& position, bool sneaking);
    void playToolBreak();
    void playPlayerHurt();
    void playFallDamage(bool severe);
    void playSplash(const glm::vec3& position);
    void playItemPickup();
    void playXpPickup();
    void playEat();
    void playDoor(bool open, const glm::vec3& position);
    void playChest(bool open, const glm::vec3& position);
    void playClick();
    void playMobAmbient(MobSoundType type, const glm::vec3& position);
    void playMobHurt(MobSoundType type, const glm::vec3& position);
    void playMobDeath(MobSoundType type, const glm::vec3& position);
    void playMobStep(MobSoundType type, const glm::vec3& position);
    bool verifyLibrary();
    bool verifyMusic();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
