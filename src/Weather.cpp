#include "Weather.h"
#include "World.h"
#include "Player.h"
#include "Survival.h"
#include "Sound.h"
#include "Settings.h"
#include "Definitions.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

Weather::Weather(std::uint32_t seed) : random_(static_cast<std::uint64_t>(seed) + 0x9e3779b97f4a7c15ULL) {
    bolts_.reserve(16); thunder_.reserve(32); fires_.reserve(64);
    remaining_ = range(240, 720);
}
std::uint32_t Weather::nextRandom() {
    random_ ^= random_ >> 12; random_ ^= random_ << 25; random_ ^= random_ >> 27;
    return static_cast<std::uint32_t>((random_ * 2685821657736338717ULL) >> 32);
}
float Weather::range(float low, float high) {
    return low + (high-low) * (nextRandom() / 4294967296.0f);
}
const char* Weather::name(WeatherType type) {
    return type == WeatherType::Clear ? "Clear" : type == WeatherType::Rain ? "Rain" : "Thunderstorm";
}
void Weather::set(WeatherType type, float seconds) {
    type_ = type;
    remaining_ = seconds > 0 ? seconds : type == WeatherType::Clear ? range(240,720) :
                 type == WeatherType::Rain ? range(120,300) : range(60,180);
}
Precipitation Weather::precipitation(const World& world, int x, int z) {
    const auto climate=world.biomeClimateAt(x,z,world.precipitationHeight(x,z));
    return climate.dry?Precipitation::None:climate.freezes?Precipitation::Snow:Precipitation::Rain;
}
bool Weather::exposed(const World& world, const glm::vec3& position) {
    const float surface = world.precipitationHeight(static_cast<int>(std::floor(position.x)),
                                                  static_cast<int>(std::floor(position.z)));
    return surface >= 0 && position.y >= surface;
}
float Weather::flash() const {
    float value = 0;
    for (const auto& bolt : bolts_)
        value = std::max(value, std::exp(-bolt.age * 12.0f) * (.7f + .3f * std::cos(bolt.age*70)));
    return value;
}
void Weather::strike(const glm::vec3& requested, World& world, Player& player,
                     SurvivalWorld& survival, SoundSystem& sounds) {
    glm::vec3 position = requested;
    // Bounded local surface queries; rods must themselves be sky exposed.
    float nearest = 32*32.0f;
    const int cx = static_cast<int>(std::floor(position.x)), cz = static_cast<int>(std::floor(position.z));
    for (int z=cz-32; z<=cz+32; ++z) for (int x=cx-32; x<=cx+32; ++x) {
        const float top = world.precipitationHeight(x,z);
        const int y = static_cast<int>(std::ceil(top))-1;
        if (top < 0 || world.getBlock(x,y,z) != Block::LightningRod) continue;
        const glm::vec3 rod(x+.5f,top,z+.5f);
        const float distance = glm::dot(rod-requested,rod-requested);
        if (distance < nearest) { nearest = distance; position = rod; }
    }
    if (bolts_.size() == 16) bolts_.erase(bolts_.begin());
    bolts_.push_back({position,0,nextRandom()});
    if (!player.isCreative() && !player.isSpectator() && !player.isDead() &&
        glm::distance(player.position()+glm::vec3(0,.9f,0),position) < 3.0f) {
        RayHit hit;
        const glm::vec3 target = player.position()+glm::vec3(0,.9f,0);
        const glm::vec3 delta = target-position;
        const float distance = glm::length(delta);
        if (distance < .01f || !world.raycast(position+glm::vec3(0,.15f,0),glm::normalize(delta),distance,hit))
            player.damage(5.0f);
    }
    survival.lightningDamage(position, world);
    const float distance = glm::distance(position,player.cameraPosition());
    const float muffle = exposed(world,player.cameraPosition()) ? 1.0f : .42f;
    if (distance < 64) sounds.playLightning(position,false,muffle);
    if (distance < 320 && thunder_.size()<32) thunder_.push_back({position,distance/343.0f,muffle});
    // Ignition is local, bounded and transient; rain extinguishes exposed fire.
    const glm::ivec3 base(glm::floor(position));
    for (int i=0;i<5;++i) {
        const int x=base.x+static_cast<int>(nextRandom()%3)-1;
        const int z=base.z+static_cast<int>(nextRandom()%3)-1;
        const int y=static_cast<int>(std::ceil(world.precipitationHeight(x,z)));
        const auto support=world.getBlock(x,y-1,z);
        if (world.hasLoadedChunkAt(x,z) && y>0 && y<WORLD_HEIGHT &&
            world.getBlock(x,y,z)==Block::Air && blockDefinition(support).soundMaterial==SoundMaterial::Wood &&
            nextRandom()%3==0 && fires_.size()<64) {
            world.setBlock(x,y,z,Block::Fire); fires_.push_back({{x,y,z},range(8,16)});
        }
    }
}
void Weather::environmentColumn(World& world, int x, int z, const GameSettings& settings) {
        const float surface=world.precipitationHeight(x,z);
        if (surface<0) return;
        const int y=static_cast<int>(std::ceil(surface));
        if (y<1 || y>=WORLD_HEIGHT) return;
        const Precipitation kind=precipitation(world,x,z);
        const Block top=world.getBlock(x,y-1,z);
        if (world.getBlock(x,y,z)==Block::Fire &&
            ((intensity_>.15f && kind==Precipitation::Rain) || nextRandom()%4==0))
            world.setBlock(x,y,z,Block::Air);
        if (top==Block::WetFarmland && (intensity_<.05f || kind==Precipitation::None))
            world.setBlock(x,y-1,z,Block::Farmland);
        const int layers=snowLayers(top);
        if(settings.snowAccumulation && kind==Precipitation::Snow && intensity_>.4f &&
           world.blockLightAt(x,y,z)<10 && layers>0 && layers<8)
            world.setBlock(x,y-1,z,snowLayerBlock(layers+1));
        else if (settings.snowAccumulation && intensity_>.4f && kind==Precipitation::Snow &&
            world.getBlock(x,y,z)==Block::Air && isSolid(top) && !isLeaf(top) &&
            blockGeometry(top).shape==BlockShape::Cube && world.blockLightAt(x,y,z)<10)
            world.setBlock(x,y,z,Block::Snow);
        else if (layers && settings.snowAccumulation &&
                 (kind!=Precipitation::Snow || world.blockLightAt(x,y-1,z)>11))
            world.setBlock(x,y-1,z,snowLayerBlock(layers-1));
        // Natural ice responds to the same effective climate as precipitation.
        // Packed/Blue Ice are permanent building materials and never melt here.
        if(world.generationVersion()>=9 && settings.snowAccumulation) {
            if(isWater(top) && kind==Precipitation::Snow && world.getBlock(x,y,z)==Block::Air &&
               world.blockLightAt(x,y,z)<10) world.setBlock(x,y-1,z,Block::Ice);
            else if(top==Block::Ice && (kind!=Precipitation::Snow || world.blockLightAt(x,y,z)>11))
                world.setBlock(x,y-1,z,Block::Water);
        }
        // Wet farmland is a new appended state, sharing old farmland geometry.
        if (intensity_>.15f && kind==Precipitation::Rain && top==Block::Farmland)
            world.setBlock(x,y-1,z,Block::WetFarmland);
        if (intensity_>.15f && kind==Precipitation::Rain && top==Block::Fire)
            world.setBlock(x,y-1,z,Block::Air);
}
void Weather::environment(World& world, const glm::vec3& player, const GameSettings& settings) {
    const int radius=std::min(48,world.simulationDistance()*CHUNK_SIZE);
    for (int i=0;i<12;++i) {
        const int x=static_cast<int>(std::floor(player.x))+static_cast<int>(nextRandom()%(2*radius+1))-radius;
        const int z=static_cast<int>(std::floor(player.z))+static_cast<int>(nextRandom()%(2*radius+1))-radius;
        environmentColumn(world,x,z,settings);
    }
}
void Weather::update(float dt, World& world, Player& player, SurvivalWorld& survival,
                     SoundSystem& sounds, const GameSettings& settings) {
    if (!std::isfinite(dt) || dt<=0) return;
    dt=std::min(dt,.25f);
    if (settings.weatherCycle) {
        remaining_-=dt;
        if (remaining_<=0) set(type_==WeatherType::Clear ?
            (nextRandom()%4==0 ? WeatherType::Thunder : WeatherType::Rain) : WeatherType::Clear);
    }
    const float blend=1-std::exp(-dt*.35f);
    intensity_+=(type_==WeatherType::Clear ? -intensity_ : 1-intensity_)*blend;
    storm_+=(type_==WeatherType::Thunder ? 1-storm_ : -storm_)*blend;
    for (auto& bolt : bolts_) bolt.age+=dt;
    bolts_.erase(std::remove_if(bolts_.begin(),bolts_.end(),[](const Bolt& b){return b.age>.45f;}),bolts_.end());
    for (auto& sound : thunder_) {
        sound.delay-=dt;
        if (sound.delay<=0) sounds.playLightning(sound.position,true,sound.muffle);
    }
    thunder_.erase(std::remove_if(thunder_.begin(),thunder_.end(),[](const DelayedThunder& s){return s.delay<=0;}),thunder_.end());
    exposureTimer_-=dt;
    if(exposureTimer_<=0) {
        exposureTimer_=.25f;
        const auto camera=player.cameraPosition();
        const int x=static_cast<int>(std::floor(camera.x)),z=static_cast<int>(std::floor(camera.z));
        localRain_=precipitation(world,x,z)==Precipitation::Rain;
        if(!localRain_ || player.isSwimming()) {outdoor_=shelter_=0;}
        else if(exposed(world,camera)) {outdoor_=1;shelter_=0;}
        else {
            int openings=0;
            for(const glm::ivec2 offset : {glm::ivec2(5,0),glm::ivec2(-5,0),glm::ivec2(0,5),glm::ivec2(0,-5)})
                if(world.precipitationHeight(x+offset.x,z+offset.y)<=camera.y) ++openings;
            const float light=world.sunlightAt(x,static_cast<int>(camera.y),z)/15.0f;
            outdoor_=0; shelter_=std::clamp(openings*.20f+light*.45f,0.0f,1.0f);
        }
    }
    sounds.setRainAmbience(outdoor_*intensity_,shelter_*intensity_);
    for(auto& fire:fires_) {
        fire.life=std::max(0.0f,fire.life-dt);
        if(world.hasLoadedChunkAt(fire.position.x,fire.position.z) && fire.life<=0 &&
            world.getBlock(fire.position.x,fire.position.y,fire.position.z)==Block::Fire)
            world.setBlock(fire.position.x,fire.position.y,fire.position.z,Block::Air);
    }
    fires_.erase(std::remove_if(fires_.begin(),fires_.end(),[&](const Burning& fire){
        return (world.hasLoadedChunkAt(fire.position.x,fire.position.z) &&
            world.getBlock(fire.position.x,fire.position.y,fire.position.z)!=Block::Fire);
    }),fires_.end());
    fireDamageTimer_-=dt;
    if(fireDamageTimer_<=0) {
        fireDamageTimer_=1;
        if(!player.isCreative() && !player.isSpectator() && world.aabbTouchesBlock(player.aabbMinimum(),player.aabbMaximum(),Block::Fire))
            player.damage(1);
    }
    environmentTimer_-=dt;
    if (environmentTimer_<=0) { environmentTimer_=.25f; environment(world,player.position(),settings); }
    lightningTimer_-=dt;
    if (lightningTimer_<=0) {
        lightningTimer_=range(6,16);
        if (type_==WeatherType::Thunder && storm_>.5f) {
            const int x=static_cast<int>(player.position().x+range(-96,96));
            const int z=static_cast<int>(player.position().z+range(-96,96));
            const float surface=world.precipitationHeight(x,z);
            if (surface>0 && precipitation(world,x,z)==Precipitation::Rain)
                strike({x+.5f,surface+.05f,z+.5f},world,player,survival,sounds);
        }
    }
}
void Weather::write(std::ostream& output) const {
    output << "WEATHER1 " << static_cast<int>(type_) << ' ' <<
        std::setprecision(std::numeric_limits<float>::max_digits10) << remaining_ << ' ' << intensity_ << ' ' <<
        storm_ << ' ' << environmentTimer_ << ' ' << lightningTimer_ << ' ' << random_ << '\n';
    output << "FIRES1 " << fires_.size() << '\n';
    for(const auto& fire : fires_) output << fire.position.x << ' ' << fire.position.y << ' ' << fire.position.z << ' ' << fire.life << '\n';
}
bool Weather::read(std::istream& input) {
    std::string magic; int type; float remaining,intensity,storm,environment,lightning; std::uint64_t random;
    if (!(input>>magic>>type>>remaining>>intensity>>storm>>environment>>lightning>>random) || magic!="WEATHER1" ||
        type<0 || type>2 || !std::isfinite(remaining) || remaining<=0 || remaining>86400 ||
        !std::isfinite(intensity) || intensity<0 || intensity>1 || !std::isfinite(storm) || storm<0 || storm>1 ||
        !std::isfinite(environment) || environment<0 || environment>1 ||
        !std::isfinite(lightning) || lightning<0 || lightning>16 || random==0) return false;
    type_=static_cast<WeatherType>(type); remaining_=remaining; intensity_=intensity; storm_=storm;
    environmentTimer_=environment; lightningTimer_=lightning; random_=random;
    // Optional transient-fire tail; old metadata without it remains valid.
    std::string tail; std::size_t count=0;
    if(input>>tail>>count && tail=="FIRES1" && count<=64) {
        std::vector<Burning> restored; restored.reserve(count);
        for(std::size_t i=0;i<count;++i) {
            Burning fire;
            if(!(input>>fire.position.x>>fire.position.y>>fire.position.z>>fire.life) ||
               fire.position.y<0 || fire.position.y>=WORLD_HEIGHT || !std::isfinite(fire.life) || fire.life<0 || fire.life>16)
                return true;
            restored.push_back(fire);
        }
        fires_=std::move(restored);
    }
    return true;
}
