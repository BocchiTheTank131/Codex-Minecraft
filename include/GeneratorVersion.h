#pragma once

// The saved generator, rather than the executable version, owns all natural
// world decisions. Never advance an existing world's value during loading.
inline constexpr unsigned LatestGeneratorVersion = 23;
inline constexpr bool validGeneratorVersion(unsigned version) {
    return version >= 1 && version <= LatestGeneratorVersion;
}
