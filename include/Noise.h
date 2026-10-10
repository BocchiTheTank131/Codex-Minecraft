#pragma once

#include <array>
#include <cstdint>

class PerlinNoise {
public:
    explicit PerlinNoise(std::uint32_t seed = 1337);

    // Private-to-worker column state: x/z fractions and lattice hashes can be
    // reused as y advances, without approximating any noise value.
    class Column {
    public:
        Column() = default;
        float noise(float y);
    private:
        friend class PerlinNoise;
        const PerlinNoise* owner_ = nullptr;
        int px0_ = 0, px1_ = 0, zi_ = 0, yi_ = -1;
        float xf_ = 0, zf_ = 0, u_ = 0, w_ = 0;
        std::array<float, 8> constants_{};
        std::array<std::int8_t, 8> ySigns_{};
    };
    Column column(float x, float z) const;

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
