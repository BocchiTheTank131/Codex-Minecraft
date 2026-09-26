#pragma once

#include <cstdint>
#include <vector>

class SoundSystem {
public:
    SoundSystem();

    void playFootstep() const;
    void playBreak() const;
    void playPlace() const;

private:
    std::vector<std::uint8_t> footstep_;
    std::vector<std::uint8_t> break_;
    std::vector<std::uint8_t> place_;

    static std::vector<std::uint8_t> createSound(float frequency, float duration,
                                                  float noiseAmount, std::uint32_t seed);
    static void play(const std::vector<std::uint8_t>& sound);
};

