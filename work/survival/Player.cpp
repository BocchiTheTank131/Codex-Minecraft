#include "Player.h"

#include "World.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

Player::Player(const glm::vec3& spawnPosition) : position_(spawnPosition), spawnPosition_(spawnPosition) {}

glm::vec3 Player::lookDirection() const {
    const float yaw = glm::radians(yaw_);
    const float pitch = glm::radians(pitch_);
    return glm::normalize(glm::vec3(std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                                    std::sin(yaw) * std::cos(pitch)));
}

glm::vec3 Player::cameraPosition() const { return position_ + glm::vec3(0.0f, EyeHeight, 0.0f); }
glm::mat4 Player::viewMatrix() const { const glm::vec3 eye=cameraPosition(); return glm::lookAt(eye,eye+lookDirection(),{0,1,0}); }
glm::vec3 Player::aabbMinimum() const { return position_ + glm::vec3(-Width*0.5f,0,-Width*0.5f); }
glm::vec3 Player::aabbMaximum() const { return position_ + glm::vec3(Width*0.5f,Height,Width*0.5f); }

void Player::addMouseMovement(double xOffset,double yOffset) {
    constexpr float sensitivity=0.10f;yaw_+=static_cast<float>(xOffset)*sensitivity;
    pitch_+=static_cast<float>(yOffset)*sensitivity;pitch_=std::clamp(pitch_,-89.0f,89.0f);
}

void Player::damage(float amount) {
    if(dead_||amount<=0.0f)return;health_=std::max(0.0f,health_-amount);hurtFlash_=0.32f;
    if(health_<=0.0f){dead_=true;respawnTimer_=2.0f;velocity_={0,0,0};}
}

void Player::respawn() {
    position_=spawnPosition_;velocity_={0,0,0};health_=20.0f;fallDistance_=0;dead_=false;grounded_=false;hurtFlash_=0;
}

void Player::update(float deltaTime,GLFWwindow* window,const World& world) {
    hurtFlash_=std::max(0.0f,hurtFlash_-deltaTime);
    if(dead_){respawnTimer_-=deltaTime;if(respawnTimer_<=0.0f)respawn();return;}
    if(position_.y<-24.0f){damage(100.0f);return;}
    const glm::vec3 look=lookDirection();glm::vec3 forward(look.x,0,look.z);forward=glm::normalize(forward);
    const glm::vec3 right=glm::normalize(glm::cross(forward,glm::vec3(0,1,0)));glm::vec3 input(0);
    if(glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS)input+=forward;if(glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS)input-=forward;
    if(glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS)input+=right;if(glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS)input-=right;
    if(glm::dot(input,input)>0.0f)input=glm::normalize(input);
    constexpr float walkSpeed=6.2f;const glm::vec2 desired(input.x*walkSpeed,input.z*walkSpeed);
    const float response=grounded_?18.0f:5.0f,blend=1.0f-std::exp(-response*deltaTime);
    velocity_.x=glm::mix(velocity_.x,desired.x,blend);velocity_.z=glm::mix(velocity_.z,desired.y,blend);
    const bool wasGrounded=grounded_;
    if(grounded_&&glfwGetKey(window,GLFW_KEY_SPACE)==GLFW_PRESS){velocity_.y=8.3f;grounded_=false;fallDistance_=0.0f;}
    velocity_.y=std::max(velocity_.y-24.0f*deltaTime,-45.0f);const float downwardVelocity=velocity_.y;
    moveAndCollide(deltaTime,world);
    if(!grounded_&&downwardVelocity<0.0f)fallDistance_+=-downwardVelocity*deltaTime;
    if(grounded_&&!wasGrounded){if(fallDistance_>3.5f)damage((fallDistance_-3.5f)*1.65f);fallDistance_=0.0f;}
}

void Player::moveAndCollide(float deltaTime,const World& world) {
    const glm::vec3 displacement=velocity_*deltaTime;const float longest=std::max({std::abs(displacement.x),std::abs(displacement.y),std::abs(displacement.z)});
    const int steps=std::max(1,static_cast<int>(std::ceil(longest/0.40f)));const glm::vec3 step=displacement/static_cast<float>(steps);
    grounded_=false;for(int i=0;i<steps;++i){if(velocity_.x!=0)resolveAxis(0,step.x,world);if(velocity_.y!=0)resolveAxis(1,step.y,world);if(velocity_.z!=0)resolveAxis(2,step.z,world);}
}

void Player::resolveAxis(int axis,float amount,const World& world) {
    if(amount==0)return;position_[axis]+=amount;constexpr float epsilon=0.0001f;const glm::vec3 minimum=aabbMinimum(),maximum=aabbMaximum();
    const glm::ivec3 first=glm::ivec3(glm::floor(minimum+glm::vec3(epsilon))),last=glm::ivec3(glm::floor(maximum-glm::vec3(epsilon)));
    for(int y=first.y;y<=last.y;++y)for(int z=first.z;z<=last.z;++z)for(int x=first.x;x<=last.x;++x){if(!world.isSolidAt(x,y,z))continue;
        if(axis==0){position_.x=amount>0?static_cast<float>(x)-Width*0.5f-epsilon:static_cast<float>(x+1)+Width*0.5f+epsilon;velocity_.x=0;}
        else if(axis==1){if(amount>0)position_.y=static_cast<float>(y)-Height-epsilon;else{position_.y=static_cast<float>(y+1)+epsilon;grounded_=true;}velocity_.y=0;}
        else{position_.z=amount>0?static_cast<float>(z)-Width*0.5f-epsilon:static_cast<float>(z+1)+Width*0.5f+epsilon;velocity_.z=0;}}
}

