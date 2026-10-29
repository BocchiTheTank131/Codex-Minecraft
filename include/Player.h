#pragma once

#include <cstdint>
#include "Burning.h"
#include <glm/glm.hpp>
#include <string>
#include <functional>

class World;
class Inventory;
class Transportation;

enum class PlayerDamageSource { General, Projectile, Burning, Melee, Explosion, Fall, Cactus, Starvation, Void };

struct PlayerInput {
    bool enabled = false;
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool jump = false;
    bool sprint = false;
    bool sneak = false;
};

class Player {
    friend class AdventureTest;
    friend class EngineOptimizationTest;
    friend class AutomationTest;
    friend class FarmingTest;
    friend class BuildingTest;
    friend class QolTest;
public:
    static constexpr float Width = 0.60f, Height = 1.80f, EyeHeight = 1.62f;
    explicit Player(const glm::vec3& spawnPosition);
    void update(float deltaTime, const PlayerInput& input, const World& world);
    void addMouseMovement(double xOffset, double yOffset);
    void setMouseSensitivity(float value) {
        mouseSensitivity_ = value;
    }
    void damage(float amount, PlayerDamageSource source = PlayerDamageSource::General);
    void setEquipment(Inventory* inventory) { equipment_ = inventory; }
    void setBlocking(bool enabled);
    bool isBlocking() const;
    float shieldFeedback() const { return shieldFeedback_; }
    bool damagePhysical(float amount, PlayerDamageSource source, const glm::vec3& origin);
    void killByCommand();
    void applyImpulse(const glm::vec3& impulse);
    void heal(float amount);
    void eat(float foodPoints, float saturation = 0);
    float saturation() const { return saturation_; }
    void exhaust(float amount);
    void addExperience(int amount);
    void respawn(const World& world);
    void setPersonalBed(glm::ivec3 bed){personalBed_=bed;hasPersonalBed_=true;}
    bool hasPersonalBed() const {return hasPersonalBed_;}
    glm::ivec3 personalBed() const {return personalBed_;}
    void setCreativeMode(bool enabled);
    void setSpectatorMode(bool enabled);
    void adjustSpectatorSpeed(double scrollSteps);
    void teleport(const glm::vec3& position);
    void setTransportation(const Transportation* value){transportation_=value;}
    void setRiding(bool value){riding_=value;fallDistance_=0;flying_=false;sprinting_=false;inWater_=false;sneaking_=false;}
    bool isRiding() const{return riding_;}
    std::uint32_t positionRevision() const{return positionRevision_;}
    void setRidePosition(glm::vec3 position,glm::vec3 velocity){position_=position;velocity_=velocity;fallDistance_=0;grounded_=false;}
    void setFlying(bool enabled);
    bool save(const std::string& path, std::uint32_t seed) const;
    std::function<bool()> captureSave(const std::string& path,std::uint32_t seed) const;
    bool load(const std::string& path, std::uint32_t seed);
    glm::vec3 cameraPosition() const;
    glm::vec3 lookDirection() const;
    glm::mat4 viewMatrix() const;
    glm::vec3 aabbMinimum() const;
    glm::vec3 aabbMaximum() const;
    const glm::vec3& position() const {
        return position_;
    }
    bool isGrounded() const {
        return grounded_;
    }
    bool isFalling() const {
        return !grounded_ && velocity_.y < -.6f;
    }
    bool isDead() const {
        return dead_;
    }
    bool isSwimming() const {
        return inWater_;
    }
    bool isSprinting() const {
        return sprinting_;
    }
    bool isSneaking() const {
        return sneaking_;
    }
    bool isCreative() const {
        return creativeMode_;
    }
    bool isSpectator() const { return spectatorMode_; }
    float spectatorSpeedMultiplier() const { return spectatorSpeedMultiplier_; }
    bool isFlying() const {
        return flying_;
    }
    bool isBurning() const {return burning_.remaining>0;}
    PlayerDamageSource lastDamageSource() const {return lastDamageSource_;}
    float health() const {
        return health_;
    }
    float maxHealth() const {
        return 20.f;
    }
    float hurtFlash() const {
        return hurtFlash_;
    }
    float lastFallDamage() const { return lastFallDamage_; }
    const glm::vec3& velocity() const { return velocity_; }
    float hunger() const {
        return hunger_;
    }
    int experience() const {
        return experience_;
    }
    int xpLevel() const;
    float xpProgress() const;

private:
    bool walkStep_=false;
    bool hasPersonalBed_=false;
    glm::ivec3 personalBed_{0};
    const Transportation* transportation_=nullptr;
    bool riding_=false;
    std::uint32_t positionRevision_=0;
    Inventory* equipment_ = nullptr;
    bool blocking_ = false;
    float shieldFeedback_ = 0;
    Burning burning_;
    PlayerDamageSource lastDamageSource_=PlayerDamageSource::General;
    glm::vec3 position_, velocity_{0.f};
    float yaw_ = -90.f, pitch_ = -12.f, health_ = 20.f, hunger_ = 20.f, saturation_ = 5.f, exhaustion_ = 0.f,
          regenTimer_ = 0.f;
    float fallDistance_ = 0.f, respawnTimer_ = 0.f, hurtFlash_ = 0.f, damageInvulnerability_ = 0.f,
          mouseSensitivity_ = 0.10f;
    float lastFallDamage_ = 0.0f;
    float respawnProtection_ = 0.0f;
    int experience_ = 0;
    float spaceTapTimer_ = 0.0f;
    bool grounded_ = false, dead_ = false, inWater_ = false, sprinting_ = false, sneaking_ = false;
    bool creativeMode_ = false, spectatorMode_ = false, flying_ = false,
         jumpWasDown_ = false;
    float spectatorSpeedMultiplier_ = 1.0f;
    void moveAndCollide(float deltaTime, const World& world);
    void resolveAxis(int axis, float amount, const World& world);
    void applyShoreClimbAssist(
        const PlayerInput& input, const World& world, const glm::vec3& forward, bool atSurface);
};
