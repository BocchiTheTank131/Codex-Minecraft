#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>

struct GLFWwindow;
class World;

class Player {
public:
    static constexpr float Width=0.60f, Height=1.80f, EyeHeight=1.62f;
    explicit Player(const glm::vec3& spawnPosition);
    void update(float deltaTime,GLFWwindow* window,const World& world);
    void addMouseMovement(double xOffset,double yOffset); void setMouseSensitivity(float value){mouseSensitivity_=value;}
    void damage(float amount); void heal(float amount); void eat(float foodPoints);
    void addExperience(int amount); void respawn();
    bool save(const std::string& path,std::uint32_t seed) const;
    bool load(const std::string& path,std::uint32_t seed);
    glm::vec3 cameraPosition() const; glm::vec3 lookDirection() const; glm::mat4 viewMatrix() const;
    glm::vec3 aabbMinimum() const; glm::vec3 aabbMaximum() const;
    const glm::vec3& position() const{return position_;}
    bool isGrounded()const{return grounded_;} bool isDead()const{return dead_;}
    bool isSwimming()const{return inWater_;} bool isSprinting()const{return sprinting_;} bool isSneaking()const{return sneaking_;}
    float health()const{return health_;} float maxHealth()const{return 20.f;} float hurtFlash()const{return hurtFlash_;}
    float hunger()const{return hunger_;} int experience()const{return experience_;}
    int xpLevel()const; float xpProgress()const;
private:
    glm::vec3 position_,spawnPosition_,velocity_{0.f};
    float yaw_=-90.f,pitch_=-12.f,health_=20.f,hunger_=20.f,exhaustion_=0.f,regenTimer_=0.f;
    float fallDistance_=0.f,respawnTimer_=0.f,hurtFlash_=0.f,mouseSensitivity_=0.10f;
    int experience_=0; bool grounded_=false,dead_=false,inWater_=false,sprinting_=false,sneaking_=false;
    void moveAndCollide(float deltaTime,const World& world); void resolveAxis(int axis,float amount,const World& world);
};
