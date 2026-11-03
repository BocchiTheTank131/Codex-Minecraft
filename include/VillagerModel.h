#pragma once
#include "Survival.h"
#include <cmath>

// Original voxel tailoring, shared by all professions. Local +Z is the face.
// No textures, allocations or graphics resources belong to an individual NPC.
inline void appendVillagerModel(std::vector<RenderCuboid>& out,glm::vec3 feet,
    float scale,float yaw,float gait,float motion,float clock,unsigned job,
    unsigned routine,bool talking,bool working,float hurt) {
    const glm::vec3 skin(.76f,.53f,.35f),hair(.20f,.12f,.08f),boots(.18f,.14f,.12f);
    const glm::vec3 coats[]={{.37f,.46f,.23f},{.28f,.33f,.38f},{.32f,.28f,.56f},{.12f,.46f,.53f}};
    const auto coat=coats[std::min(job,3u)];
    const float breathing=std::sin(clock*1.8f)*.006f;
    const float swing=std::sin(gait)*.50f*std::min(1.f,motion);
    auto rotate=[&](glm::vec3 p){return glm::vec3(std::cos(yaw)*p.x+std::sin(yaw)*p.z,p.y,-std::sin(yaw)*p.x+std::cos(yaw)*p.z);};
    auto part=[&](glm::vec3 p,glm::vec3 size,glm::vec3 color,glm::vec3 direction=glm::vec3(0)){
        if(hurt>0)color=glm::mix(color,glm::vec3(1,.08f,.06f),.65f);
        RenderCuboid box{feet+rotate(p)*scale,size*scale,color,direction,-yaw};box.instanced=true;out.push_back(box);
    };
    // Paired limbs pivot at hips/shoulders; hands and shoes share their limb transform.
    for(int side:{-1,1}){
        float angle=swing*side;
        auto limb=[&](glm::vec3 pivot,float length,float width,glm::vec3 color,float a,bool arm){
            glm::vec3 down(0,-std::cos(a),std::sin(a));
            part(pivot+down*(length*.5f),{width,length,width},color,-down);
            part(pivot+down*(length-.025f)+glm::vec3(0,0,arm?0:.025f),arm?glm::vec3(.12f,.12f,.13f):glm::vec3(.17f,.12f,.23f),arm?skin:boots,-down);
        };
        limb({side*.105f,.61f,0},.54f,.15f,{.22f,.25f,.30f},angle,false);
        float gesture=talking?.65f+.12f*std::sin(clock*4):working?.6f+.3f*std::sin(clock*5):routine==1?.12f*std::sin(clock*2):0;
        limb({side*.225f,1.16f+breathing,0},.43f,.13f,coat,-angle+gesture,true);
    }
    part({0,.91f+breathing,0},{.34f,.56f,.25f},coat);
    part({0,.65f,0},{.35f,.08f,.27f},boots); // belt
    part({.015f,.65f,.146f},{.07f,.06f,.035f},{.80f,.64f,.25f});
    part({0,1.22f+breathing,0},{.13f,.12f,.14f},skin);
    part({0,1.405f+breathing,0},{.32f,.34f,.30f},skin);
    part({0,1.55f+breathing,-.015f},{.34f,.07f,.30f},hair);
    part({0,1.42f+breathing,-.145f},{.32f,.23f,.04f},hair);
    for(int side:{-1,1}){
        part({side*.075f,1.45f+breathing,.154f},{.066f,.05f,.015f},{.92f,.91f,.80f});
        part({side*.070f,1.45f+breathing,.165f},{.025f,.035f,.012f},{.10f,.16f,.17f});
        part({side*.075f,1.495f+breathing,.157f},{.075f,.015f,.015f},hair);
    }
    part({0,1.395f+breathing,.177f},{.055f,.065f,.06f},skin*.92f);
    part({0,1.335f+breathing,.154f},{.09f,.018f,.018f},{.33f,.18f,.13f});
    if(job==0){ // brim and low woven crown
        part({0,1.565f,0},{.48f,.035f,.43f},{.79f,.66f,.30f});
        part({0,1.60f,0},{.31f,.05f,.29f},{.66f,.50f,.20f});
    }else if(job==1){
        part({0,.89f,.14f},{.28f,.41f,.035f},{.14f,.12f,.12f});
        part({0,1.14f,.14f},{.15f,.09f,.035f},{.18f,.15f,.13f});
    }else if(job==2){
        part({0,1.14f,.145f},{.30f,.09f,.04f},{.82f,.71f,.49f});
        part({-.17f,.80f,.12f},{.09f,.19f,.12f},{.55f,.19f,.16f}); // bound book
        part({-.17f,.80f,.184f},{.065f,.14f,.012f},{.88f,.80f,.60f});
    }else{
        part({.18f,.72f,.05f},{.14f,.19f,.18f},{.48f,.28f,.12f});
        part({0,1.08f,.14f},{.27f,.07f,.035f},{.90f,.58f,.21f});
    }
}
