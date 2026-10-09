#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <iosfwd>
#include <vector>
#include <string>

class World;
class Player;
class SurvivalWorld;
class SoundSystem;
struct GameSettings;

enum class WeatherType : int { Clear, Rain, Thunder };
enum class Precipitation : int { None, Rain, Snow };

// Simulation is independent of graphics quality. All storage is bounded and
// belongs to the active world/session, never shared across saved worlds.
class Weather {
public:
    struct Bolt { glm::vec3 position; float age = 0; std::uint32_t shape = 0; };
    explicit Weather(std::uint32_t seed);
    void set(WeatherType type, float seconds = 0);
    void update(float dt, World& world, Player& player, SurvivalWorld& survival,
                SoundSystem& sounds, const GameSettings& settings);
    void strike(const glm::vec3& position, World& world, Player& player,
                SurvivalWorld& survival, SoundSystem& sounds);
    bool read(std::istream& input);
    void write(std::ostream& output) const;
    WeatherType type() const { return type_; }
    float remaining() const { return remaining_; }
    float intensity() const { return intensity_; }
    float storm() const { return storm_; }
    float daylightScale() const { return 1.0f - .25f * intensity_ - .55f * storm_; }
    bool stormSpawning() const { return storm_ > .70f; }
    float flash() const;
    float wetness() const { return localRain_ ? intensity_ : 0.0f; }
    const std::vector<Bolt>& bolts() const { return bolts_; }
    static const char* name(WeatherType type);
    static Precipitation precipitation(const World& world, int x, int z);
    static bool exposed(const World& world, const glm::vec3& position);
    static bool runSelfTest(World& world, SoundSystem& sounds, SurvivalWorld& survival, std::string& report);
private:
    struct DelayedThunder { glm::vec3 position; float delay, muffle; };
    WeatherType type_ = WeatherType::Clear;
    float remaining_ = 360, intensity_ = 0, storm_ = 0;
    float environmentTimer_ = 0, lightningTimer_ = 8;
    float exposureTimer_ = 0, outdoor_ = 1, shelter_ = 0;
    bool localRain_=false;
    std::uint64_t random_;
    struct Burning { glm::ivec3 position; float life; };
    std::vector<Burning> fires_;
    float fireDamageTimer_=0;
    std::vector<Bolt> bolts_;
    std::vector<DelayedThunder> thunder_;
    std::uint32_t nextRandom();
    float range(float low, float high);
    void environmentColumn(World& world, int x, int z, const GameSettings& settings);
    void environment(World& world, const glm::vec3& player, const GameSettings& settings);
};
