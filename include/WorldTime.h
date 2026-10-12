#pragma once
#include <cmath>
#include <istream>
#include <sstream>
#include <string>

namespace WorldTime {
inline constexpr float CycleSeconds = 1200.f;
inline constexpr float LegacyCycleSeconds = 420.f;
inline constexpr float DefaultSeconds = 100.f; // The original 35/420 morning phase.
// Keep the existing solar phase offset; noon/midnight align with the extrema.
inline constexpr float NoonSeconds = CycleSeconds * (.25f - .35f / 6.28318530718f);
inline constexpr float NightSeconds = 700.f;
inline constexpr float MidnightSeconds = NoonSeconds + CycleSeconds * .5f;

inline float angle(float seconds) { return seconds / CycleSeconds * 6.28318530718f + .35f; }

inline float wrap(double seconds) {
    if (!std::isfinite(seconds)) return DefaultSeconds;
    return static_cast<float>(std::fmod(std::fmod(seconds, double(CycleSeconds)) + CycleSeconds, double(CycleSeconds)));
}
// The optional metadata tail explicitly records the time unit's cycle. Missing
// metadata means a legacy save; never infer duration from terrain generator IDs.
inline float restore(float seconds, std::istream& metadata) {
    double duration = LegacyCycleSeconds;
    std::string line;
    while (std::getline(metadata, line)) {
        std::istringstream record(line);
        std::string key; double value;
        if (record >> key >> value && key == "TIME_CYCLE1" &&
            std::isfinite(value) && value > 0 && value <= 86400)
            duration = value;
    }
    if (!std::isfinite(seconds) || seconds < 0) return DefaultSeconds;
    return wrap(std::fmod(double(seconds), duration) / duration * CycleSeconds);
}
bool selfTest(std::string& report);
}
