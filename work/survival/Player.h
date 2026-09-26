#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;
class World;

class Player {
public:
    static constexpr float Width = 0.60f;
    static constexpr float Height = 1.80f;
    static constexpr float EyeHeight = 1.62f;

    explicit Player(const glm::vec3& spawnPosition);

    void update(float deltaTime, GLFWwindow* window, const World& world);
    void addMouseMovement(double xOffset, double yOffset);
    void damage(float amount);
    void respawn();

    glm::vec3 cameraPosition() const;
    glm::vec3 lookDirection() const;
    glm::mat4 viewMatrix() const;
    glm::vec3 aabbMinimum() const;
    glm::vec3 aabbMaximum() const;
    const glm::vec3& position() const { return position_; }
    bool isGrounded() const { return grounded_; }
    float health() const { return health_; }
    float maxHealth() const { return 20.0f; }
    bool isDead() const { return dead_; }
    float hurtFlash() const { return hurtFlash_; }

private:
    glm::vec3 position_;
    glm::vec3 spawnPosition_;
    glm::vec3 velocity_{0.0f};
    float yaw_ = -90.0f;
    float pitch_ = -12.0f;
    float health_ = 20.0f;
    float fallDistance_ = 0.0f;
    float respawnTimer_ = 0.0f;
    float hurtFlash_ = 0.0f;
    bool grounded_ = false;
    bool dead_ = false;

    void moveAndCollide(float deltaTime, const World& world);
    void resolveAxis(int axis, float amount, const World& world);
};

