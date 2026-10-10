#include "World.h"
#include "Weather.h"
#include "BiomeContent.h"
#include <array>
#include <chrono>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>

namespace {
std::uint64_t blockHash(const std::vector<Block>& blocks) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (Block block : blocks) { hash ^= static_cast<std::uint8_t>(block); hash *= 1099511628211ULL; }
    return hash;
}
struct MapPixel {
    std::uint8_t biome, height;
    std::uint16_t reserved = 0;
    float temperature, humidity, continentalness, river, macroTemperature, macroHumidity;
};
static_assert(sizeof(MapPixel) == 28, "Geography audit binary layout");
}

bool World::runGeographyAudit(const std::string& directory, std::string& report) const {
    namespace fs = std::filesystem;
    fs::create_directories(directory);
    constexpr std::array<std::uint32_t, 3> seeds{20260917U, 123456789U, 42U};
    for (auto seed : seeds) {
        PerlinNoise noise(seed);
        for (int i = 0; i < 32; ++i) {
            const float x = (i - 17) * 3.713f, z = (i - 13) * -7.219f;
            auto column = noise.column(x, z);
            for (int y = -256; y < 256; ++y) {
                const float ny = y * .071f;
                const float expected = noise.noise(x, ny, z), actual = column.noise(ny);
                if (std::memcmp(&expected, &actual, sizeof(float))) { report = "Noise column cache changed bits"; return false; }
            }
        }
        // Include lattice boundaries and signed-zero inputs, not only the
        // non-integer coordinates used by terrain columns.
        for (float x : {-256.f, -1.f, -0.f, 0.f, .25f, 1.f, 255.f})
            for (float z : {-256.f, -1.f, -0.f, 0.f, .75f, 1.f, 255.f}) {
                auto column = noise.column(x, z);
                for (float y : {-256.f, -1.001f, -1.f, -.001f, -0.f, 0.f, .001f, .5f, 1.f, 255.f}) {
                    const float expected = noise.noise(x, y, z), actual = column.noise(y);
                    if (std::memcmp(&expected, &actual, sizeof(float))) { report = "Noise column lattice boundary changed bits"; return false; }
                }
            }
    }
    std::cout << "Noise column cache: 50,622 bit-identical samples\n";
    std::ofstream generation(directory + "/generation.csv");
    generation << "seed,version,x,z,hash,ms\n";
    for (auto seed : seeds) {
        World probe(seed);
        for (unsigned version = 1; version <= 10; ++version) {
            probe.generationVersion_ = version;
            for (int i = 0; i < 16; ++i) {
                const int x = (i % 4) * 23 - 34, z = (i / 4) * 19 - 28;
                const auto start = std::chrono::steady_clock::now();
                const auto data = probe.generateChunkData(x, z);
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                generation << seed << ',' << version << ',' << x << ',' << z << ',' << blockHash(data.blocks) << ',' << ms << '\n';
            }
        }
    }
    std::ofstream locations(directory + "/locations.csv"), distribution(directory + "/distribution.csv");
    locations << "seed,version,biome,x,z,height,temperature,humidity\n";
    distribution << "seed,version,extent,biome,samples\n";
    std::array<std::uint64_t, static_cast<int>(Biome::Count)> total{};
    std::array<std::uint64_t, static_cast<int>(Block::Count)> features{};
    std::ofstream columns(directory + "/columns.csv");
    columns << "seed,version,samples,ms,mean_height,max_adjacent_height_delta\n";
    std::ofstream regional(directory + "/regional-generation.csv");
    regional << "seed,version,x,z,repeat,hash,ms\n";
    for (auto seed : seeds) for (unsigned version : {9U, 10U}) {
        World probe(seed, version);
        std::array<bool, static_cast<int>(Biome::Count)> located{};
        std::array<glm::ivec2, static_cast<int>(Biome::Count)> sites{};
        int maximumRise = 0;
        for (int extent : {30720, 4096}) {
            constexpr int side = 512;
            const std::string stem = directory + "/" + std::to_string(seed) + "-v" + std::to_string(version) + "-" + std::to_string(extent);
            std::ofstream binary(stem + ".bin", std::ios::binary);
            std::array<unsigned, static_cast<int>(Biome::Count)> counts{};
            for (int z = 0; z < side; ++z) for (int x = 0; x < side; ++x) {
                const int wx = -extent / 2 + x * extent / side, wz = -extent / 2 + z * extent / side;
                const auto sample = probe.sampleTerrain(wx, wz);
                const int biome = static_cast<int>(sample.biome);
                ++counts[biome];
                if (version == 10) ++total[biome];
                if (!located[biome]) {
                    located[biome] = true;
                    sites[biome] = {wx, wz};
                    locations << seed << ',' << version << ',' << biomeDefinition(sample.biome).name << ',' << wx << ',' << wz << ',' <<
                        sample.height << ',' << sample.temperature << ',' << sample.humidity << '\n';
                }
                // Prefer an interior for vegetation checks rather than the
                // first shallow coastal pixel of a forest family.
                if (sample.biome != Biome::Swamp && biomeDefinition(sample.biome).trees != TreeStyle::None && sample.height >= SEA_LEVEL + 5 &&
                    probe.terrainHeight(sites[biome].x, sites[biome].y) < SEA_LEVEL + 5)
                    sites[biome] = {wx, wz};
                if (sample.biome == Biome::Swamp && sample.height < SEA_LEVEL &&
                    probe.terrainHeight(sites[biome].x, sites[biome].y) >= SEA_LEVEL)
                    sites[biome] = {wx, wz};
                if (sample.height < 6 || sample.height >= WORLD_HEIGHT - 10 || sample.temperature < 0 || sample.temperature > 1 ||
                    sample.humidity < 0 || sample.humidity > 1) { report = "Geography climate/height bounds"; return false; }
                const MapPixel pixel{static_cast<std::uint8_t>(biome), static_cast<std::uint8_t>(sample.height), 0,
                    sample.temperature, sample.humidity, sample.continentalness, sample.river,
                    version == 10 ? sample.macroTemperature : sample.temperature,
                    version == 10 ? sample.macroHumidity : sample.humidity};
                binary.write(reinterpret_cast<const char*>(&pixel), sizeof(pixel));
                if (version == 10 && x % 16 == 0 && z % 16 == 0) {
                    for (const glm::ivec2 offset : {glm::ivec2(1, 0), glm::ivec2(0, 1)}) {
                        const auto next = probe.sampleTerrain(wx + offset.x, wz + offset.y);
                        maximumRise = std::max(maximumRise, std::abs(next.height - sample.height));
                        if (std::abs(next.temperature - sample.temperature) > .02f || std::abs(next.humidity - sample.humidity) > .02f ||
                            std::abs(next.height - sample.height) > 12) {
                            report = "Geography adjacent-column discontinuity"; return false;
                        }
                    }
                }
            }
            if (!binary) { report = "Geography map write failed"; return false; }
            for (int b = 0; b < static_cast<int>(Biome::Count); ++b)
                distribution << seed << ',' << version << ',' << extent << ',' << biomeDefinition(static_cast<Biome>(b)).name << ',' << counts[b] << '\n';
        }
        // Time only column sampling, separately from caves, structures and trees.
        std::uint64_t checksum = 0;
        const auto columnStart = std::chrono::steady_clock::now();
        constexpr int columnCount = 65536;
        for (int i = 0; i < columnCount; ++i)
            checksum += probe.sampleTerrain((i % 256) * 120 - 15360, (i / 256) * 120 - 15360).height;
        const double columnMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - columnStart).count();
        columns << seed << ',' << version << ',' << columnCount << ',' << columnMs << ',' <<
            static_cast<double>(checksum) / columnCount << ',' << maximumRise << '\n';
        if (!checksum) { report = "Column benchmark missing output"; return false; }
        for (int i = 0; i < 25; ++i) {
            const int cx = (i % 5) * 400 - 800, cz = (i / 5) * 400 - 800;
            std::uint64_t expected = 0;
            for (int repeat = 0; repeat < 3; ++repeat) {
                const auto start = std::chrono::steady_clock::now();
                const auto data = probe.generateChunkData(cx, cz);
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                const auto hash = blockHash(data.blocks);
                if (repeat && hash != expected) { report = "Repeated regional generation changed"; return false; }
                expected = hash;
                regional << seed << ',' << version << ',' << cx << ',' << cz << ',' << repeat << ',' << hash << ',' << ms << '\n';
            }
        }
        if (version == 10) {
            Weather weather(seed);
            for (int biome = 0; biome < static_cast<int>(Biome::Count); ++biome) if (located[biome]) {
                const auto site = sites[biome];
                const auto climate = probe.biomeClimateAt(site.x, site.y);
                const auto expected = climate.dry ? Precipitation::None : climate.freezes ? Precipitation::Snow : Precipitation::Rain;
                if (weather.precipitation(probe, site.x, site.y) != expected) { report = "Geography precipitation routing"; return false; }
                for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx) {
                    const auto data = probe.generateChunkData(floorDiv(site.x, CHUNK_SIZE) + dx, floorDiv(site.y, CHUNK_SIZE) + dz);
                    for (Block block : data.blocks) ++features[static_cast<int>(block)];
                }
            }
            // Parallel/reversed requests must produce the same chunk bytes.
            std::array<std::future<std::uint64_t>, 4> parallel;
            for (int i = 0; i < 4; ++i) parallel[i] = std::async(std::launch::async, [&probe, i] {
                return blockHash(probe.generateChunkData(15 + i, -17).blocks);
            });
            for (int i = 3; i >= 0; --i) {
                const auto serial = probe.generateChunkData(15 + i, -17);
                if (parallel[i].get() != blockHash(serial.blocks)) { report = "Generation order/thread determinism"; return false; }
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const int x = (15 + i) * CHUNK_SIZE + 15, wz = -17 * CHUNK_SIZE + z;
                    const auto a = probe.sampleTerrain(x, wz), b = probe.sampleTerrain(x + 1, wz);
                    if (std::abs(a.temperature - b.temperature) > .02f || std::abs(a.humidity - b.humidity) > .02f ||
                        std::abs(a.height - b.height) > 16) { report = "Chunk-border continuity"; return false; }
                }
            }
            std::string structures;
            if (!probe.runStructureGenerationSmokeTest(structures, nullptr, 48)) { report = structures; return false; }
            std::cout << "Generator 10 structures " << seed << ": " << structures << '\n';
        }
    }
    int found = 0;
    for (auto count : total) found += count > 0;
    std::cout << "Geography biome coverage " << found << "/35\n";
    if (found != static_cast<int>(Biome::Count)) { report = "Missing natural biome in geography survey"; return false; }
    std::ofstream content(directory + "/features.csv");
    content << "block,count\n";
    for (const auto& entry : biomeContent()) content << entry.name << ',' << features[static_cast<int>(entry.block)] << '\n';
    for (Block required : {Block::SpruceLog, Block::JungleLog, Block::AcaciaLog, Block::DarkOakLog,
        Block::Podzol, Block::Mycelium, Block::CoarseDirt, Block::PackedIce, Block::BlueIce,
        Block::TerracottaOrange, Block::Bamboo, Block::Fern, Block::DeadBush, Block::LilyPad,
        Block::RedMushroom, Block::BrownMushroom, Block::MushroomStem})
        if (!features[static_cast<int>(required)]) { report = "Missing natural geography content"; return false; }
    // Version metadata is already part of the world format; no new file or
    // save-format version is needed. Old worlds restore their old algorithm.
    const auto savePath = directory + "/world.vxw";
    for (unsigned version = 1; version <= 10; ++version) {
        World original(123456789U, version), loaded(123456789U);
        if (!original.saveWorld(savePath, {.5f, 150.f, .5f})) { report = "Geography save failed"; return false; }
        glm::vec3 player;
        if (!loaded.loadWorld(savePath, player) || loaded.generationVersion() != static_cast<int>(version) ||
            original.generateChunkData(5, -6).blocks != loaded.generateChunkData(5, -6).blocks) {
            report = "Geography version/save compatibility"; return false;
        }
    }
    report = "Geography maps, all biomes, threaded determinism, boundaries, structures and generators 1-10 save/load passed";
    return bool(generation) && bool(distribution) && bool(locations);
}
