#include "Noise.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

PerlinNoise::PerlinNoise(std::uint32_t seed) {
    std::array<int, 256> values{};
    std::iota(values.begin(), values.end(), 0);
    std::mt19937 generator(seed);
    std::shuffle(values.begin(), values.end(), generator);
    for (int i = 0; i < 512; ++i) {
        permutation_[i] = values[static_cast<std::size_t>(i) & 255U];
    }
}

float PerlinNoise::fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float PerlinNoise::lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float PerlinNoise::grad(int hash, float x, float y, float z) {
    const int h = hash & 15;
    const float u = h < 8 ? x : y;
    const float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float PerlinNoise::noise(float x, float y, float z) const {
    const int xi = static_cast<int>(std::floor(x)) & 255;
    const int yi = static_cast<int>(std::floor(y)) & 255;
    const int zi = static_cast<int>(std::floor(z)) & 255;

    const float xf = x - std::floor(x);
    const float yf = y - std::floor(y);
    const float zf = z - std::floor(z);
    const float u = fade(xf);
    const float v = fade(yf);
    const float w = fade(zf);

    const int aaa = permutation_[permutation_[permutation_[xi] + yi] + zi];
    const int aba = permutation_[permutation_[permutation_[xi] + yi + 1] + zi];
    const int aab = permutation_[permutation_[permutation_[xi] + yi] + zi + 1];
    const int abb = permutation_[permutation_[permutation_[xi] + yi + 1] + zi + 1];
    const int baa = permutation_[permutation_[permutation_[xi + 1] + yi] + zi];
    const int bba = permutation_[permutation_[permutation_[xi + 1] + yi + 1] + zi];
    const int bab = permutation_[permutation_[permutation_[xi + 1] + yi] + zi + 1];
    const int bbb = permutation_[permutation_[permutation_[xi + 1] + yi + 1] + zi + 1];

    const float x1 = lerp(grad(aaa, xf, yf, zf), grad(baa, xf - 1.0f, yf, zf), u);
    const float x2 = lerp(grad(aba, xf, yf - 1.0f, zf), grad(bba, xf - 1.0f, yf - 1.0f, zf), u);
    const float y1 = lerp(x1, x2, v);
    const float x3 = lerp(grad(aab, xf, yf, zf - 1.0f), grad(bab, xf - 1.0f, yf, zf - 1.0f), u);
    const float x4 =
        lerp(grad(abb, xf, yf - 1.0f, zf - 1.0f), grad(bbb, xf - 1.0f, yf - 1.0f, zf - 1.0f), u);
    return lerp(y1, lerp(x3, x4, v), w);
}

PerlinNoise::Column PerlinNoise::column(float x, float z) const {
    Column result;
    result.owner_ = this;
    const int xi = static_cast<int>(std::floor(x)) & 255;
    result.px0_ = permutation_[xi];
    result.px1_ = permutation_[xi + 1];
    result.zi_ = static_cast<int>(std::floor(z)) & 255;
    result.xf_ = x - std::floor(x);
    result.zf_ = z - std::floor(z);
    result.u_ = fade(result.xf_);
    result.w_ = fade(result.zf_);
    return result;
}

float PerlinNoise::Column::noise(float y) {
    const int yi = static_cast<int>(std::floor(y)) & 255;
    if (yi != yi_) {
        yi_ = yi;
        const auto& p = owner_->permutation_;
        const std::array<int, 8> hashes{p[p[px0_ + yi] + zi_], p[p[px0_ + yi + 1] + zi_],
            p[p[px0_ + yi] + zi_ + 1], p[p[px0_ + yi + 1] + zi_ + 1],
            p[p[px1_ + yi] + zi_], p[p[px1_ + yi + 1] + zi_],
            p[p[px1_ + yi] + zi_ + 1], p[p[px1_ + yi + 1] + zi_ + 1]};
        for (int corner = 0; corner < 8; ++corner) {
            const int h = hashes[corner] & 15;
            const float x = corner >= 4 ? xf_ - 1.f : xf_;
            const float z = corner % 4 >= 2 ? zf_ - 1.f : zf_;
            const float u = h < 8 ? ((h & 1) ? -x : x) : 0.f;
            const float v = h < 4 ? 0.f : ((h & 2) ? -(h == 12 || h == 14 ? x : z) : (h == 12 || h == 14 ? x : z));
            // Exactly the original two signed operands. At most one is Y.
            // Cache fixed operands; do not approximate gradients or re-associate
            // a multi-term floating-point expression.
            constants_[corner] = h >= 8 ? v : h < 4 ? u : u + v;
            ySigns_[corner] = static_cast<std::int8_t>(h >= 8 ? ((h & 1) ? -1 : 1) : h < 4 ? ((h & 2) ? -1 : 1) : 0);
        }
    }
    const float yf = y - std::floor(y), v = fade(yf);
    const auto gradient = [&](int corner, float value) {
        return ySigns_[corner] == 0 ? constants_[corner] :
            constants_[corner] + (ySigns_[corner] < 0 ? -value : value);
    };
    const float x1 = lerp(gradient(0, yf), gradient(4, yf), u_);
    const float x2 = lerp(gradient(1, yf - 1.f), gradient(5, yf - 1.f), u_);
    const float y1 = lerp(x1, x2, v);
    const float x3 = lerp(gradient(2, yf), gradient(6, yf), u_);
    const float x4 = lerp(gradient(3, yf - 1.f), gradient(7, yf - 1.f), u_);
    return lerp(y1, lerp(x3, x4, v), w_);
}

float PerlinNoise::fractal2D(
    float x, float z, int octaves, float lacunarity, float persistence) const {
    float value = 0.0f;
    float amplitude = 1.0f;
    float totalAmplitude = 0.0f;
    float frequency = 1.0f;
    for (int octave = 0; octave < octaves; ++octave) {
        value += noise(x * frequency, 0.0f, z * frequency) * amplitude;
        totalAmplitude += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return value / totalAmplitude;
}

float PerlinNoise::fractal3D(
    float x, float y, float z, int octaves, float lacunarity, float persistence) const {
    float value = 0.0f;
    float amplitude = 1.0f;
    float totalAmplitude = 0.0f;
    float frequency = 1.0f;
    for (int octave = 0; octave < octaves; ++octave) {
        value += noise(x * frequency, y * frequency, z * frequency) * amplitude;
        totalAmplitude += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return value / totalAmplitude;
}
