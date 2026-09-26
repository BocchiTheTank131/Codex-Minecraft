#include "Sound.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <mmsystem.h>
#endif

namespace {
void append16(std::vector<std::uint8_t>& data, std::uint16_t value) {
    data.push_back(static_cast<std::uint8_t>(value & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void append32(std::vector<std::uint8_t>& data, std::uint32_t value) {
    append16(data, static_cast<std::uint16_t>(value & 0xffffU));
    append16(data, static_cast<std::uint16_t>((value >> 16U) & 0xffffU));
}
}

SoundSystem::SoundSystem()
    : footstep_(createSound(92.0f, 0.075f, 0.72f, 11U)),
      break_(createSound(175.0f, 0.12f, 0.88f, 29U)),
      place_(createSound(118.0f, 0.075f, 0.28f, 47U)) {}

std::vector<std::uint8_t> SoundSystem::createSound(float frequency, float duration,
                                                    float noiseAmount, std::uint32_t seed) {
    constexpr std::uint32_t sampleRate = 22050;
    constexpr std::uint16_t channels = 1;
    constexpr std::uint16_t bits = 16;
    const std::uint32_t sampleCount = static_cast<std::uint32_t>(duration * sampleRate);
    const std::uint32_t dataBytes = sampleCount * sizeof(std::int16_t);
    std::vector<std::uint8_t> wave;
    wave.reserve(44U + dataBytes);
    const auto tag = [&](const char* text) { wave.insert(wave.end(), text, text + 4); };
    tag("RIFF"); append32(wave, 36U + dataBytes); tag("WAVE"); tag("fmt ");
    append32(wave, 16U); append16(wave, 1U); append16(wave, channels);
    append32(wave, sampleRate); append32(wave, sampleRate * channels * bits / 8U);
    append16(wave, channels * bits / 8U); append16(wave, bits); tag("data"); append32(wave, dataBytes);

    std::mt19937 generator(seed);
    std::uniform_real_distribution<float> random(-1.0f, 1.0f);
    constexpr float pi = 3.14159265358979323846f;
    for (std::uint32_t i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / sampleRate;
        const float envelope = std::pow(1.0f - static_cast<float>(i) / sampleCount, 2.2f);
        const float tone = std::sin(2.0f * pi * frequency * t) * (1.0f - noiseAmount);
        const float noise = random(generator) * noiseAmount;
        const float sample = std::clamp((tone + noise) * envelope * 0.38f, -1.0f, 1.0f);
        append16(wave, static_cast<std::uint16_t>(static_cast<std::int16_t>(sample * 32767.0f)));
    }
    return wave;
}

void SoundSystem::play(const std::vector<std::uint8_t>& sound) {
#ifdef _WIN32
    PlaySoundA(reinterpret_cast<LPCSTR>(sound.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
    (void)sound;
#endif
}

void SoundSystem::playFootstep() const { play(footstep_); }
void SoundSystem::playBreak() const { play(break_); }
void SoundSystem::playPlace() const { play(place_); }

