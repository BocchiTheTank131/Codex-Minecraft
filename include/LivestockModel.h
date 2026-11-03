#pragma once
#include "Survival.h"
#include <cmath>

// Presentation only. Distance drives stride; exponential responses behave the
// same at different simulation rates. Neither blocked motion nor idling walks.
inline void advanceLivestockPose(float& yaw,float& gait,float& motion,float& clock,
    glm::vec2 displacement,glm::vec2 heading,float dt,bool grounded,bool cow) {
    if(!std::isfinite(dt)||dt<=0)return;
    const float distance=glm::length(displacement);
    clock=std::fmod(clock+dt,62.831853f);
    if(grounded)gait=std::fmod(gait+distance*(cow?6.f:8.f),6.2831853f);
    const float target=grounded?std::min(1.f,distance/dt):0;
    motion+=(target-motion)*(1-std::exp(-12*dt));
    auto facing=distance>.0001f?displacement:heading;
    if(glm::dot(facing,facing)>.0001f){
        const float delta=std::remainder(std::atan2(facing.x,facing.y)-yaw,6.2831853f);
        yaw=std::remainder(yaw+delta*(1-std::exp(-10*dt)),6.2831853f);
    }
}

// Original voxel models, local +Z forward. Share the existing instanced cube
// batch with villagers; no animal owns graphics resources or source textures.
inline void appendLivestockModel(std::vector<RenderCuboid>& out,glm::vec3 feet,
    float scale,float yaw,float gait,float motion,float clock,bool cow,
    bool grounded,float hurt) {
    const glm::vec3 coat=cow?glm::vec3(.83f,.79f,.67f):glm::vec3(.91f,.54f,.55f);
    const glm::vec3 dark=cow?glm::vec3(.22f,.13f,.095f):glm::vec3(.63f,.28f,.30f);
    const glm::vec3 muzzle=cow?glm::vec3(.68f,.43f,.37f):glm::vec3(.96f,.64f,.63f);
    const float breath=std::sin(clock*2)*.004f;
    auto rotate=[&](glm::vec3 p){return glm::vec3(std::cos(yaw)*p.x+std::sin(yaw)*p.z,p.y,-std::sin(yaw)*p.x+std::cos(yaw)*p.z);};
    auto part=[&](glm::vec3 p,glm::vec3 size,glm::vec3 color,glm::vec3 direction=glm::vec3(0)){
        if(hurt>0)color=glm::mix(color,glm::vec3(1,.08f,.06f),.65f);
        RenderCuboid box{feet+rotate(p)*scale,size*scale,color,direction,-yaw};
        box.instanced=true;out.push_back(box);
    };
    part({0,(cow?.67f:.49f)+breath,0},cow?glm::vec3(.64f,.52f,.98f):glm::vec3(.60f,.39f,.90f),coat);
    if(cow){
        // Asymmetric coat markings are shallow solid patches, with no coplanar faces.
        for(int side:{-1,1}){
            part({side*.325f,.73f+breath,side>0?-.18f:.17f},{.014f,.31f,.29f},dark);
            part({side*.325f,.59f+breath,side>0?.20f:-.24f},{.014f,.19f,.21f},dark);
        }
        part({-.12f,.934f+breath,-.22f},{.27f,.014f,.30f},dark);
        part({.12f,.934f+breath,.14f},{.24f,.014f,.23f},dark);
        part({0,.39f,-.17f},{.24f,.15f,.25f},muzzle); // udder
    }
    // Diagonally opposite hooves share phase; the other diagonal alternates.
    for(int side:{-1,1})for(int front:{-1,1}){
        const float angle=grounded?std::sin(gait)*side*front*.48f*motion:.20f*front;
        const glm::vec3 down(0,-std::cos(angle),std::sin(angle));
        const float length=cow?.40f:.28f;
        const glm::vec3 pivot(side*(cow?.225f:.205f),cow?.43f:.31f,front*(cow?.32f:.28f));
        part(pivot+down*(length*.5f),{.145f,length,.15f},cow?coat:coat*.94f,-down);
        // Two toes make the cloven hoof visible without another texture.
        for(int toe:{-1,1})part(pivot+down*(length-.025f)+glm::vec3(toe*.041f,0,.018f),{.073f,.11f,.18f},dark,-down);
    }
    const float pitch=(1-std::min(1.f,motion))*(.045f+.035f*std::sin(clock*2));
    const glm::vec3 neck(0,cow?.83f:.62f,cow?.39f:.38f);
    auto head=[&](glm::vec3 p,glm::vec3 size,glm::vec3 color){
        auto d=p-neck;d={d.x,std::cos(pitch)*d.y-std::sin(pitch)*d.z,std::sin(pitch)*d.y+std::cos(pitch)*d.z};
        part(neck+d+glm::vec3(0,breath,0),size,color,{0,std::cos(pitch),std::sin(pitch)});
    };
    head({0,cow?.89f:.65f,cow?.46f:.43f},cow?glm::vec3(.35f,.33f,.36f):glm::vec3(.35f,.30f,.32f),coat);
    head({0,cow?.79f:.59f,cow?.64f:.61f},cow?glm::vec3(.33f,.18f,.20f):glm::vec3(.27f,.15f,.14f),muzzle);
    for(int side:{-1,1}){
        head({side*.075f,cow?.81f:.60f,cow?.745f:.684f},{.035f,.037f,.012f},dark);
        head({side*.115f,cow?.945f:.70f,cow?.647f:.597f},{.064f,.066f,.014f},{.96f,.94f,.85f});
        head({side*.108f,cow?.945f:.70f,cow?.659f:.608f},{.027f,.043f,.012f},{.075f,.063f,.055f});
        head({side*(cow?.235f:.20f),cow?1.0f:.80f,cow?.43f:.40f},cow?glm::vec3(.15f,.075f,.14f):glm::vec3(.13f,.15f,.10f),coat*.90f);
        head({side*(cow?.235f:.20f),cow?1.0f:.80f,cow?.505f:.455f},cow?glm::vec3(.09f,.035f,.014f):glm::vec3(.075f,.09f,.014f),muzzle);
        if(cow){head({side*.125f,1.09f,.39f},{.065f,.13f,.075f},{.70f,.63f,.43f});head({side*.13f,1.16f,.385f},{.045f,.045f,.055f},{.37f,.30f,.20f});}
    }
    if(cow){
        const float sway=std::sin(clock*2)*.045f;
        part({sway,.54f,-.50f},{.055f,.34f,.055f},coat);
        part({sway,.35f,-.53f},{.10f,.12f,.10f},dark);
    }else{
        // Small stepped curl attached to the rump.
        const float wag=std::sin(clock*2)*.015f;
        part({wag,.55f,-.49f},{.055f,.055f,.13f},muzzle);
        part({wag+.035f,.59f,-.55f},{.08f,.10f,.045f},muzzle);
        part({wag+.07f,.63f,-.52f},{.045f,.04f,.09f},muzzle);
    }
}

// Conservative world-aligned selection bounds follow the rendered heading.
// This does not change locomotion collision or farm interactions/ownership.
inline RenderCuboid livestockModelBounds(glm::vec3 feet,float scale,float yaw,bool cow){
    const glm::vec3 size=cow?glm::vec3(.70f,1.23f,1.38f):glm::vec3(.62f,.92f,1.30f);
    const float forward=cow?.10f:.07f,c=std::abs(std::cos(yaw)),s=std::abs(std::sin(yaw));
    return {feet+glm::vec3(std::sin(yaw)*forward,size.y*.5f,std::cos(yaw)*forward)*scale,
            glm::vec3(c*size.x+s*size.z,size.y,s*size.x+c*size.z)*scale,{1,1,1}};
}
