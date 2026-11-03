#pragma once
#include "World.h"
#include "Building.h"
#include <algorithm>
#include <cmath>

// Local, bounded body sweep. Uses loaded terrain only and never edits terrain.
inline glm::vec3 moveLivestock(const World& world,glm::vec3 start,
    glm::vec3 horizontal,float dt,bool cow,glm::vec3& velocity,bool& grounded) {
    const float half=.30f,height=cow?1.18f:.89f;
    auto clear=[&](glm::vec3 p){
        for(int z=int(std::floor(p.z-half));z<=int(std::floor(p.z+half));++z)
            for(int x=int(std::floor(p.x-half));x<=int(std::floor(p.x+half));++x)
                if(!world.hasLoadedChunkAt(x,z))return false;
        return !world.aabbIntersectsSolid(p+glm::vec3(-half,.002f,-half),p+glm::vec3(half,height,half));
    };
    if(!std::isfinite(dt)||dt<=0||!clear(start))return start;
    auto forward=start+glm::vec3(horizontal.x,0,horizontal.z)*std::min(dt,.15f);
    bool enclosure=false;
    for(int z=int(std::floor(forward.z-half));z<=int(std::floor(forward.z+half));++z)
        for(int x=int(std::floor(forward.x-half));x<=int(std::floor(forward.x+half));++x)
            for(int y=int(std::floor(start.y));y<=int(std::floor(start.y))+1;++y){
                auto b=world.getBlock(x,y,z);auto def=buildingDefinition(b);
                if(def&&(def->shape==BuildingShape::Fence||def->shape==BuildingShape::Wall||
                    (def->shape==BuildingShape::FenceGate&&!(blockProperties(b)&8))))enclosure=true;
            }
    if(grounded&&!enclosure&&glm::dot(horizontal,horizontal)>.001f&&!clear(forward)&&
       clear(start+glm::vec3(0,.3f,0))&&clear(start+glm::vec3(0,1.03f,0))&&
       clear(forward+glm::vec3(0,1.03f,0))){velocity.y=6.6f;grounded=false;}
    const int steps=std::clamp(int(std::ceil(std::max(glm::length(horizontal)*dt,
        (std::abs(velocity.y)+18*dt)*dt)/.06f)),1,64);
    const float step=dt/steps;glm::vec3 p=start;
    for(int i=0;i<steps;++i){
        velocity.y-=18*step;
        auto next=p+glm::vec3(0,velocity.y*step,0);
        if(clear(next)){p=next;grounded=false;}
        else{grounded=velocity.y<=0;velocity.y=0;}
        for(int axis:{0,2}){next=p;next[axis]+=horizontal[axis]*step;if(clear(next))p=next;}
    }
    if(velocity.y<=0&&!clear(p-glm::vec3(0,.025f,0)))grounded=true;
    return p;
}
