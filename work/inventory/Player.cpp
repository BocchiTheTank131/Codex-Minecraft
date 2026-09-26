#include "Player.h"
#include "World.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace { constexpr char Magic[8]={'V','X','P','S','T','2','\0','\0'}; template<class T>bool wr(std::ofstream&f,const T&v){f.write((const char*)&v,sizeof(v));return!!f;}template<class T>bool rd(std::ifstream&f,T&v){f.read((char*)&v,sizeof(v));return!!f;} }
Player::Player(const glm::vec3&s):position_(s),spawnPosition_(s){}
glm::vec3 Player::lookDirection()const{float y=glm::radians(yaw_),p=glm::radians(pitch_);return glm::normalize(glm::vec3(std::cos(y)*std::cos(p),std::sin(p),std::sin(y)*std::cos(p)));}
glm::vec3 Player::cameraPosition()const{return position_+glm::vec3(0,EyeHeight,0);}glm::mat4 Player::viewMatrix()const{auto e=cameraPosition();return glm::lookAt(e,e+lookDirection(),{0,1,0});}
glm::vec3 Player::aabbMinimum()const{return position_+glm::vec3(-Width*.5f,0,-Width*.5f);}glm::vec3 Player::aabbMaximum()const{return position_+glm::vec3(Width*.5f,Height,Width*.5f);}
void Player::addMouseMovement(double x,double y){yaw_+=(float)x*mouseSensitivity_;pitch_=std::clamp(pitch_+(float)y*mouseSensitivity_,-89.f,89.f);}
void Player::damage(float a){if(dead_||a<=0)return;health_=std::max(0.f,health_-a);hurtFlash_=.32f;regenTimer_=0;if(health_<=0){dead_=true;respawnTimer_=2;velocity_={0,0,0};}}
void Player::heal(float a){if(!dead_)health_=std::min(20.f,health_+a);}void Player::eat(float f){hunger_=std::min(20.f,hunger_+f);}
int Player::xpLevel()const{int level=0,x=experience_;while(x>=7+level*3){x-=7+level*3;++level;}return level;}
float Player::xpProgress()const{int level=0,x=experience_;while(x>=7+level*3){x-=7+level*3;++level;}return (float)x/(7+level*3);}
void Player::addExperience(int a){experience_=std::max(0,experience_+a);}
void Player::respawn(){position_=spawnPosition_;velocity_={0,0,0};health_=20;hunger_=20;fallDistance_=0;dead_=false;grounded_=false;hurtFlash_=0;}
bool Player::save(const std::string&p,std::uint32_t seed)const{std::ofstream f(p,std::ios::binary|std::ios::trunc);if(!f)return false;f.write(Magic,8);return wr(f,seed)&&wr(f,health_)&&wr(f,hunger_)&&wr(f,experience_);}
bool Player::load(const std::string&p,std::uint32_t seed){std::ifstream f(p,std::ios::binary);if(!f)return false;char m[8]{};std::uint32_t s=0;f.read(m,8);if(std::memcmp(m,Magic,8)||!rd(f,s)||s!=seed||!rd(f,health_)||!rd(f,hunger_)||!rd(f,experience_))return false;health_=std::clamp(health_,.1f,20.f);hunger_=std::clamp(hunger_,0.f,20.f);experience_=std::max(0,experience_);return true;}
void Player::update(float dt,GLFWwindow*w,const World&world){
 hurtFlash_=std::max(0.f,hurtFlash_-dt);if(dead_){respawnTimer_-=dt;if(respawnTimer_<=0)respawn();return;}if(position_.y<-24){damage(100);return;}
 auto blockAt=[&](float y){return world.getBlock((int)std::floor(position_.x),(int)std::floor(y),(int)std::floor(position_.z));};inWater_=blockAt(position_.y+.2f)==Block::Water||blockAt(position_.y+1.2f)==Block::Water;
 glm::vec3 look=lookDirection(),forward(look.x,0,look.z);if(glm::dot(forward,forward)>.001f)forward=glm::normalize(forward);glm::vec3 right=glm::normalize(glm::cross(forward,glm::vec3(0,1,0))),input(0);
 if(glfwGetKey(w,GLFW_KEY_W)==GLFW_PRESS)input+=forward;if(glfwGetKey(w,GLFW_KEY_S)==GLFW_PRESS)input-=forward;if(glfwGetKey(w,GLFW_KEY_D)==GLFW_PRESS)input+=right;if(glfwGetKey(w,GLFW_KEY_A)==GLFW_PRESS)input-=right;if(glm::dot(input,input)>0)input=glm::normalize(input);
 sneaking_=glfwGetKey(w,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS;sprinting_=!sneaking_&&!inWater_&&hunger_>3&&glfwGetKey(w,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS&&glm::dot(input,input)>0;
 float speed=inWater_?3.4f:(sneaking_?2.3f:(sprinting_?8.6f:6.2f));glm::vec2 desired(input.x*speed,input.z*speed);float blend=1-std::exp(-(grounded_?18.f:5.f)*dt);velocity_.x=glm::mix(velocity_.x,desired.x,blend);velocity_.z=glm::mix(velocity_.z,desired.y,blend);bool wasGrounded=grounded_;
 if(inWater_){float swim=0;if(glfwGetKey(w,GLFW_KEY_SPACE)==GLFW_PRESS)swim+=4.2f;if(sneaking_)swim-=3.5f;velocity_.y=glm::mix(velocity_.y,swim,1-std::exp(-4.f*dt));velocity_.y-=2.1f*dt;fallDistance_=0;}
 else{if(grounded_&&glfwGetKey(w,GLFW_KEY_SPACE)==GLFW_PRESS){velocity_.y=8.3f;grounded_=false;fallDistance_=0;exhaustion_+=.25f;}velocity_.y=std::max(velocity_.y-24.f*dt,-45.f);}
 float down=velocity_.y;moveAndCollide(dt,world);if(!inWater_&&!grounded_&&down<0)fallDistance_+=-down*dt;if(!inWater_&&grounded_&&!wasGrounded){if(fallDistance_>3.5f)damage((fallDistance_-3.5f)*1.65f);fallDistance_=0;}
 if(glm::dot(input,input)>0)exhaustion_+=dt*(sprinting_?.12f:.018f);if(exhaustion_>=4){exhaustion_-=4;hunger_=std::max(0.f,hunger_-1.f);}regenTimer_+=dt;
 if(regenTimer_>=4){regenTimer_=0;if(hunger_>=18&&health_<20){heal(1);hunger_=std::max(0.f,hunger_-.5f);}else if(hunger_<=0)damage(1);}
}
void Player::moveAndCollide(float dt,const World&w){glm::vec3 d=velocity_*dt;float longest=std::max({std::abs(d.x),std::abs(d.y),std::abs(d.z)});int steps=std::max(1,(int)std::ceil(longest/.4f));glm::vec3 step=d/(float)steps;grounded_=false;for(int i=0;i<steps;++i){if(velocity_.x)resolveAxis(0,step.x,w);if(velocity_.y)resolveAxis(1,step.y,w);if(velocity_.z)resolveAxis(2,step.z,w);}}
void Player::resolveAxis(int axis,float amount,const World&w){if(amount==0)return;position_[axis]+=amount;constexpr float e=.0001f;auto mn=aabbMinimum(),mx=aabbMaximum();glm::ivec3 first=glm::ivec3(glm::floor(mn+glm::vec3(e))),last=glm::ivec3(glm::floor(mx-glm::vec3(e)));for(int y=first.y;y<=last.y;++y)for(int z=first.z;z<=last.z;++z)for(int x=first.x;x<=last.x;++x){if(!w.isSolidAt(x,y,z))continue;if(axis==0){position_.x=amount>0?x-Width*.5f-e:x+1+Width*.5f+e;velocity_.x=0;}else if(axis==1){if(amount>0)position_.y=y-Height-e;else{position_.y=y+1+e;grounded_=true;}velocity_.y=0;}else{position_.z=amount>0?z-Width*.5f-e:z+1+Width*.5f+e;velocity_.z=0;}}}
