#include "Player.h"
#include "SaveFile.h"
#include "World.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr char Magic[8] = {'V', 'X', 'P', 'S', 'T', '2', '\0', '\0'};
template <class T> bool wr(std::ofstream& f, const T& v) {
    f.write((const char*)&v, sizeof(v));
    return !!f;
}
template <class T> bool rd(std::ifstream& f, T& v) {
    f.read((char*)&v, sizeof(v));
    return !!f;
}
} // namespace
Player::Player(const glm::vec3& s) : position_(s), spawnPosition_(s) {}
glm::vec3 Player::lookDirection() const {
    float y = glm::radians(yaw_), p = glm::radians(pitch_);
    return glm::normalize(
        glm::vec3(std::cos(y) * std::cos(p), std::sin(p), std::sin(y) * std::cos(p)));
}
glm::vec3 Player::cameraPosition() const {
    return position_ + glm::vec3(0, EyeHeight, 0);
}
glm::mat4 Player::viewMatrix() const {
    auto e = cameraPosition();
    return glm::lookAt(e, e + lookDirection(), {0, 1, 0});
}
glm::vec3 Player::aabbMinimum() const {
    return position_ + glm::vec3(-Width * .5f, 0, -Width * .5f);
}
glm::vec3 Player::aabbMaximum() const {
    return position_ + glm::vec3(Width * .5f, Height, Width * .5f);
}
void Player::addMouseMovement(double x, double y) {
    yaw_ += (float)x * mouseSensitivity_;
    pitch_ = std::clamp(pitch_ + (float)y * mouseSensitivity_, -89.f, 89.f);
}
void Player::damage(float a) {
    if (creativeMode_ || spectatorMode_ || dead_ || a <= 0 ||
        damageInvulnerability_ > 0)
        return;
    damageInvulnerability_ = .65f;
    health_ = std::max(0.f, health_ - a);
    hurtFlash_ = .32f;
    regenTimer_ = 0;
    if (health_ <= 0) {
        dead_ = true;
        respawnTimer_ = 2;
        velocity_ = {0, 0, 0};
    }
}
void Player::killByCommand() {
    health_ = 0.0f;
    dead_ = true;
    respawnTimer_ = 2.0f;
    velocity_ = glm::vec3(0.0f);
    fallDistance_ = 0.0f;
}
void Player::applyImpulse(const glm::vec3& impulse) {
    if (creativeMode_ || spectatorMode_ || dead_) return;
    velocity_ += impulse;
}
void Player::heal(float a) {
    if (!dead_)
        health_ = std::min(20.f, health_ + a);
}
void Player::eat(float f) {
    if (spectatorMode_)
        return;
    hunger_ = std::min(20.f, hunger_ + f);
}
int Player::xpLevel() const {
    int level = 0, x = experience_;
    while (x >= 7 + level * 3) {
        x -= 7 + level * 3;
        ++level;
    }
    return level;
}
float Player::xpProgress() const {
    int level = 0, x = experience_;
    while (x >= 7 + level * 3) {
        x -= 7 + level * 3;
        ++level;
    }
    return (float)x / (7 + level * 3);
}
void Player::addExperience(int a) {
    if (spectatorMode_)
        return;
    experience_ = std::max(0, experience_ + a);
}
void Player::respawn() {
    position_ = spawnPosition_;
    velocity_ = {0, 0, 0};
    health_ = 20;
    hunger_ = 20;
    fallDistance_ = 0;
    dead_ = false;
    grounded_ = false;
    flying_ = false;
    hurtFlash_ = 0;
    damageInvulnerability_ = 0;
}

void Player::setCreativeMode(bool enabled) {
    spectatorMode_ = false;
    creativeMode_ = enabled;
    health_ = 20.0f;
    hunger_ = 20.0f;
    dead_ = false;
    respawnTimer_ = 0.0f;
    fallDistance_ = 0.0f;
    if (!creativeMode_)
        flying_ = false;
}
void Player::setSpectatorMode(bool enabled) {
    spectatorMode_ = enabled;
    if (enabled) {
        creativeMode_ = false;
        flying_ = true;
        grounded_ = false;
        inWater_ = false;
        dead_ = false;
        health_ = std::max(1.0f, health_);
        fallDistance_ = 0.0f;
        hurtFlash_ = 0.0f;
        velocity_ = glm::vec3(0.0f);
        sprinting_ = false;
        sneaking_ = false;
        spaceTapTimer_ = 0.0f;
        jumpWasDown_ = false;
    } else {
        flying_ = false;
        velocity_ = glm::vec3(0.0f);
        fallDistance_ = 0.0f;
        grounded_ = false;
    }
}

void Player::adjustSpectatorSpeed(double scrollSteps) {
    spectatorSpeedMultiplier_ = std::clamp(
        spectatorSpeedMultiplier_ * std::pow(1.25f, static_cast<float>(scrollSteps)),
        0.2f, 8.0f);
}

void Player::teleport(const glm::vec3& position) {
    position_ = position;
    velocity_ = glm::vec3(0.0f);
    fallDistance_ = 0.0f;
    grounded_ = false;
}
void Player::setFlying(bool enabled) {
    flying_ = creativeMode_ && enabled;
    if (flying_)
        velocity_ = glm::vec3(0.0f);
}
bool Player::save(const std::string& p, std::uint32_t seed) const {
    return SaveFile::write(p, std::ios::binary, [&](std::ofstream& f) {
        f.write(Magic, 8);
        return wr(f, seed) && wr(f, health_) && wr(f, hunger_) && wr(f, experience_);
    });
}
bool Player::load(const std::string& p, std::uint32_t seed) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
        return false;
    char m[8]{};
    std::uint32_t s = 0;
    f.read(m, 8);
    if (std::memcmp(m, Magic, 8) || !rd(f, s) || s != seed || !rd(f, health_) || !rd(f, hunger_) ||
        !rd(f, experience_))
        return false;
    health_ = std::clamp(health_, .1f, 20.f);
    hunger_ = std::clamp(hunger_, 0.f, 20.f);
    experience_ = std::max(0, experience_);
    return true;
}
void Player::applyShoreClimbAssist(const PlayerInput& input,
                                   const World& world,
                                   const glm::vec3& forward,
                                   bool atSurface) {
    if (!input.enabled || !input.moveForward || !input.jump || !atSurface)
        return;

    const glm::vec3 horizontalProbe = forward * 0.48f;
    const glm::vec3 probeOffset(horizontalProbe.x, 0.04f, horizontalProbe.z);
    const bool shorelineAhead =
        world.aabbIntersectsSolid(aabbMinimum() + probeOffset, aabbMaximum() + probeOffset);
    if (!shorelineAhead)
        return;

    const glm::vec3 climbOffset = probeOffset + glm::vec3(0.0f, 1.05f, 0.0f);
    const bool roomToClimb =
        !world.aabbIntersectsSolid(aabbMinimum() + climbOffset, aabbMaximum() + climbOffset);
    if (!roomToClimb)
        return;

    velocity_.y = std::max(velocity_.y, 7.2f);
    const glm::vec2 horizontalVelocity(velocity_.x, velocity_.z);
    const glm::vec2 horizontalForward(forward.x, forward.z);
    const float forwardSpeed = glm::dot(horizontalVelocity, horizontalForward);
    if (forwardSpeed < 4.8f) {
        const glm::vec2 boost = horizontalForward * (4.8f - forwardSpeed);
        velocity_.x += boost.x;
        velocity_.z += boost.y;
    }
}
void Player::update(float deltaTime, const PlayerInput& input, const World& world) {
    lastFallDamage_ = 0.0f;
    hurtFlash_ = std::max(0.0f, hurtFlash_ - deltaTime);
    damageInvulnerability_ = std::max(0.0f, damageInvulnerability_ - deltaTime);
    spaceTapTimer_ = std::max(0.0f, spaceTapTimer_ - deltaTime);

    if (dead_) {
        respawnTimer_ -= deltaTime;
        if (respawnTimer_ <= 0.0f) respawn();
        return;
    }

    if (spectatorMode_) {
        const glm::vec3 forward = lookDirection();
        const glm::vec3 right = glm::normalize(
            glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 direction(0.0f);
        if (input.enabled && input.moveForward) direction += forward;
        if (input.enabled && input.moveBackward) direction -= forward;
        if (input.enabled && input.moveRight) direction += right;
        if (input.enabled && input.moveLeft) direction -= right;
        if (input.enabled && input.jump) direction.y += 1.0f;
        if (input.enabled && input.sneak) direction.y -= 1.0f;
        if (glm::dot(direction, direction) > 0.0f)
            direction = glm::normalize(direction);
        const float speed = 8.5f * spectatorSpeedMultiplier_ *
                            (input.enabled && input.sprint ? 1.5f : 1.0f);
        const float blend = 1.0f - std::exp(-30.0f * deltaTime);
        velocity_ = glm::mix(velocity_, direction * speed, blend);
        position_ += velocity_ * deltaTime;
        flying_ = true;
        grounded_ = false;
        inWater_ = false;
        sprinting_ = input.enabled && input.sprint;
        sneaking_ = input.enabled && input.sneak;
        fallDistance_ = 0.0f;
        return;
    }

    if (creativeMode_) {
        if (input.enabled && input.jump && !jumpWasDown_) {
            if (spaceTapTimer_ > 0.0f) {
                flying_ = !flying_;
                spaceTapTimer_ = 0.0f;
                velocity_.y = 0.0f;
                fallDistance_ = 0.0f;
            } else {
                spaceTapTimer_ = 0.30f;
            }
        }
        jumpWasDown_ = input.enabled && input.jump;
        health_ = 20.0f;
        hunger_ = 20.0f;
    } else {
        jumpWasDown_ = false;
    }

    if (position_.y < -24.0f) {
        if (creativeMode_)
            respawn();
        else
            damage(100.0f);
        return;
    }

    const auto submergedAt = [&](float sampleY) {
        constexpr std::array<float, 2> SampleOffsets{-0.24f, 0.24f};
        const int blockY = static_cast<int>(std::floor(sampleY));
        const float heightInBlock = sampleY - static_cast<float>(blockY);
        for (float offsetX : SampleOffsets) {
            for (float offsetZ : SampleOffsets) {
                const Block block =
                    world.getBlock(static_cast<int>(std::floor(position_.x + offsetX)),
                                   blockY,
                                   static_cast<int>(std::floor(position_.z + offsetZ)));
                if (isWater(block) && heightInBlock <= waterHeight(block) + 0.01f)
                    return true;
            }
        }
        return false;
    };
    inWater_ = submergedAt(position_.y + 0.2f) || submergedAt(position_.y + 1.2f);
    const bool atWaterSurface = inWater_ && !submergedAt(position_.y + 1.55f);

    const glm::vec3 look = lookDirection();
    glm::vec3 forward(look.x, 0.0f, look.z);
    if (glm::dot(forward, forward) > 0.001f) {
        forward = glm::normalize(forward);
    }
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 movement(0.0f);
    if (input.enabled && input.moveForward) {
        movement += forward;
    }
    if (input.enabled && input.moveBackward) {
        movement -= forward;
    }
    if (input.enabled && input.moveRight) {
        movement += right;
    }
    if (input.enabled && input.moveLeft) {
        movement -= right;
    }
    if (glm::dot(movement, movement) > 0.0f) {
        movement = glm::normalize(movement);
    }

    const bool wasSprintFlying = flying_ && sprinting_;
    sneaking_ = input.enabled && input.sneak;
    sprinting_ = input.enabled && input.sprint && !sneaking_ &&
                 (creativeMode_ || hunger_ > 3.0f) &&
                 glm::dot(movement, movement) > 0.0f;
    if (!input.enabled) {
        velocity_.x = 0.0f;
        velocity_.z = 0.0f;
    }

    const float flightSpeed = input.sprint
        ? (input.sneak ? 14.0f : 21.0f)
        : 8.5f;
    const float speed =
        flying_ ? flightSpeed
                : (inWater_ ? 3.4f : (sneaking_ ? 2.3f : (sprinting_ ? 8.6f : 6.2f)));
    const glm::vec2 desiredVelocity(movement.x * speed, movement.z * speed);
    const float horizontalBlend =
        1.0f - std::exp(-(flying_ ? 30.0f : (grounded_ ? 18.0f : 5.0f)) * deltaTime);
    if (wasSprintFlying && !input.sprint) {
        velocity_.x = desiredVelocity.x;
        velocity_.z = desiredVelocity.y;
    } else {
        velocity_.x = glm::mix(velocity_.x, desiredVelocity.x, horizontalBlend);
        velocity_.z = glm::mix(velocity_.z, desiredVelocity.y, horizontalBlend);
    }

    // Keep a grounded, sneaking player's feet over actual collision geometry.
    // The narrow support probe also works on slabs without treating nearby
    // walls or a midair jump as a floor.
    if (sneaking_ && grounded_ && !input.jump && !flying_ && !inWater_ &&
        velocity_.y <= 0.0f) {
        auto supported = [&](float nextX, float nextZ) {
            const glm::vec3 minimum(nextX - Width * 0.28f, position_.y - 0.16f,
                                    nextZ - Width * 0.28f);
            const glm::vec3 maximum(nextX + Width * 0.28f, position_.y + 0.02f,
                                    nextZ + Width * 0.28f);
            return world.aabbIntersectsSolid(minimum, maximum);
        };
        if (!supported(position_.x + velocity_.x * deltaTime, position_.z))
            velocity_.x = 0.0f;
        if (!supported(position_.x, position_.z + velocity_.z * deltaTime))
            velocity_.z = 0.0f;
    }

    const bool wasGrounded = grounded_;
    const bool onLadder = !flying_ &&
                          world.hasClimbableNear(position_ + glm::vec3(0.0f, 0.9f, 0.0f), 0.46f);
    if (flying_) {
        const float verticalSpeed = flightSpeed;
        velocity_.y = input.enabled && input.jump ? verticalSpeed : 0.0f;
        if (input.enabled && input.sneak)
            velocity_.y -= verticalSpeed;
        grounded_ = false;
        fallDistance_ = 0.0f;
    } else if (onLadder) {
        if (input.enabled && (input.jump || input.moveForward))
            velocity_.y = 4.2f;
        else if (input.enabled && input.sneak)
            velocity_.y = -3.0f;
        else
            velocity_.y = std::max(velocity_.y, -0.8f);
        fallDistance_ = 0.0f;
    } else if (inWater_) {
        float swimVelocity = 0.0f;
        if (input.enabled && input.jump) {
            swimVelocity += 4.2f;
        }
        if (sneaking_) {
            swimVelocity -= 3.5f;
        }
        velocity_.y =
            glm::mix(velocity_.y, swimVelocity, 1.0f - std::exp(-4.0f * deltaTime));
        velocity_.y -= 2.1f * deltaTime;
        applyShoreClimbAssist(input, world, forward, atWaterSurface);
        fallDistance_ = 0.0f;
    } else {
        if (input.enabled && input.jump && grounded_) {
            velocity_.y = 8.3f;
            grounded_ = false;
            fallDistance_ = 0.0f;
            exhaustion_ += 0.25f;
        }
        velocity_.y = std::max(velocity_.y - 24.0f * deltaTime, -45.0f);
    }

    const float downwardVelocity = velocity_.y;
    moveAndCollide(deltaTime, world);
    if (!inWater_ && !grounded_ && downwardVelocity < 0.0f) {
        fallDistance_ += -downwardVelocity * deltaTime;
    }
    if (!creativeMode_ && !inWater_ && grounded_ && !wasGrounded) {
        if (fallDistance_ > 3.5f) {
            const float healthBefore = health_;
            damage((fallDistance_ - 3.5f) * 1.65f);
            lastFallDamage_ = healthBefore - health_;
        }
        fallDistance_ = 0.0f;
    }

    if (!creativeMode_ && world.aabbTouchesBlock(aabbMinimum(), aabbMaximum(), Block::Cactus)) {
        damage(1.0f);
    }

    if (creativeMode_) {
        exhaustion_ = 0.0f;
        regenTimer_ = 0.0f;
        health_ = 20.0f;
        hunger_ = 20.0f;
    } else {
        if (glm::dot(movement, movement) > 0.0f)
            exhaustion_ += deltaTime * (sprinting_ ? 0.12f : 0.018f);
        if (exhaustion_ >= 4.0f) {
            exhaustion_ -= 4.0f;
            hunger_ = std::max(0.0f, hunger_ - 1.0f);
        }

        regenTimer_ += deltaTime;
        if (regenTimer_ >= 4.0f) {
            regenTimer_ = 0.0f;
            if (hunger_ >= 18.0f && health_ < 20.0f) {
                heal(1.0f);
                hunger_ = std::max(0.0f, hunger_ - 0.5f);
            } else if (hunger_ <= 0.0f) {
                damage(1.0f);
            }
        }
    }
}
void Player::moveAndCollide(float dt, const World& w) {
    glm::vec3 d = velocity_ * dt;
    float longest = std::max({std::abs(d.x), std::abs(d.y), std::abs(d.z)});
    int steps = std::max(1, (int)std::ceil(longest / .4f));
    glm::vec3 step = d / (float)steps;
    grounded_ = false;
    for (int i = 0; i < steps; ++i) {
        if (velocity_.x)
            resolveAxis(0, step.x, w);
        if (velocity_.y)
            resolveAxis(1, step.y, w);
        if (velocity_.z)
            resolveAxis(2, step.z, w);
    }
}
void Player::resolveAxis(int axis, float amount, const World& w) {
    if (amount == 0)
        return;
    position_[axis] += amount;
    constexpr float e = .0001f;
    auto mn = aabbMinimum(), mx = aabbMaximum();
    glm::ivec3 first = glm::ivec3(glm::floor(mn + glm::vec3(e))),
               last = glm::ivec3(glm::floor(mx - glm::vec3(e)));
    for (int y = first.y; y <= last.y; ++y)
        for (int z = first.z; z <= last.z; ++z)
            for (int x = first.x; x <= last.x; ++x) {
                const Block block = w.getBlock(x, y, z);
                if (!isSolid(block) ||
                    !w.blockIntersectsAabb({x, y, z}, mn, mx))
                    continue;
                glm::vec3 blockMinimum;
                glm::vec3 blockMaximum;
                if (!w.blockCollisionBounds({x, y, z}, blockMinimum, blockMaximum))
                    continue;
                if (axis == 0) {
                    position_.x = amount > 0 ? blockMinimum.x - Width * .5f - e
                                             : blockMaximum.x + Width * .5f + e;
                    velocity_.x = 0;
                } else if (axis == 1) {
                    if (amount > 0)
                        position_.y = blockMinimum.y - Height - e;
                    else {
                        position_.y = blockMaximum.y + e;
                        grounded_ = true;
                    }
                    velocity_.y = 0;
                } else {
                    position_.z = amount > 0 ? blockMinimum.z - Width * .5f - e
                                             : blockMaximum.z + Width * .5f + e;
                    velocity_.z = 0;
                }
            }
}
