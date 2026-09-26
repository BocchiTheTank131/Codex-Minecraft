#pragma once

#include <array>
#include <cstdint>

class PerlinNoise {
public:
    explicit PerlinNoise(std::uint32_t seed = 1337);

    float noise(float x, float y, float z) const;
    float fractal2D(float x, float z, int octaves, float lacunarity, float persistence) const;
    float
    fractal3D(float x, float y, float z, int octaves, float lacunarity, float persistence) const;

private:
    std::array<int, 512> permutation_{};

    static float fade(float t);
    static float lerp(float a, float b, float t);
    static float grad(int hash, float x, float y, float z);
};
