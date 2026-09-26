#include "Player.h"
#include "Renderer.h"
#include "Sound.h"
#include "Survival.h"
#include "World.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
struct GameState {
    Player* player = nullptr;
    Inventory* inventory = nullptr;
    bool mouseCaptured = true;
    bool firstMouseSample = true;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
};

void framebufferSizeCallback(GLFWwindow*,int width,int height){glViewport(0,0,width,height);}
void mouseCallback(GLFWwindow* window,double x,double y){
    auto* state=static_cast<GameState*>(glfwGetWindowUserPointer(window));if(!state||!state->mouseCaptured||!state->player)return;
    if(state->firstMouseSample){state->lastMouseX=x;state->lastMouseY=y;state->firstMouseSample=false;return;}
    state->player->addMouseMovement(x-state->lastMouseX,state->lastMouseY-y);state->lastMouseX=x;state->lastMouseY=y;
}
void scrollCallback(GLFWwindow* window,double,double yOffset){auto* state=static_cast<GameState*>(glfwGetWindowUserPointer(window));if(state&&state->inventory&&yOffset!=0)state->inventory->cycleSlot(yOffset>0?-1:1);}
void keyCallback(GLFWwindow* window,int key,int,int action,int){if(key!=GLFW_KEY_ESCAPE||action!=GLFW_PRESS)return;auto* state=static_cast<GameState*>(glfwGetWindowUserPointer(window));if(!state)return;state->mouseCaptured=!state->mouseCaptured;state->firstMouseSample=true;glfwSetInputMode(window,GLFW_CURSOR,state->mouseCaptured?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);}
int floorChunk(float coordinate){return static_cast<int>(std::floor(coordinate/static_cast<float>(CHUNK_SIZE)));}

std::uint32_t configuredSeed(int argc,char** argv){
    std::uint32_t seed=20260917U;{std::ifstream input("world_seed.txt");std::uint64_t value=0;if(input>>value)seed=static_cast<std::uint32_t>(value);}
    for(int i=1;i<argc;++i){const std::string argument=argv[i];try{if(argument.rfind("--seed=",0)==0)seed=static_cast<std::uint32_t>(std::stoull(argument.substr(7)));else if(argument=="--seed"&&i+1<argc)seed=static_cast<std::uint32_t>(std::stoull(argv[++i]));}catch(const std::exception&){std::cerr<<"Ignoring invalid seed argument: "<<argument<<'\n';}}
    std::ofstream output("world_seed.txt",std::ios::trunc);if(output)output<<seed<<'\n';return seed;
}

float daylightAt(float worldTime){constexpr float cycleSeconds=210.0f;const float angle=worldTime/cycleSeconds*glm::two_pi<float>()+0.35f;return glm::smoothstep(-0.13f,0.17f,std::sin(angle));}

void printControls(){std::cout<<"Voxel Frontier survival controls:\n"
    <<"  WASD / Mouse   move and look\n  Space          jump\n"
    <<"  Hold LMB       mine block\n  RMB            place selected block\n"
    <<"  1-9 / wheel    select hotbar slot\n  C               crafting panel\n"
    <<"  R / V           next recipe / craft\n  F3              debug HUD\n"
    <<"  Escape          release/capture mouse\n"<<std::flush;}
}

int main(int argc,char** argv){
    if(!glfwInit()){std::cerr<<"GLFW initialization failed.\n";return EXIT_FAILURE;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GLFW_TRUE);
#endif
    GLFWwindow* window=glfwCreateWindow(1280,720,"Voxel Frontier: Survival World",nullptr,nullptr);if(!window){glfwTerminate();return EXIT_FAILURE;}
    glfwMakeContextCurrent(window);glfwSwapInterval(1);if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))){glfwDestroyWindow(window);glfwTerminate();return EXIT_FAILURE;}
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CCW);

    int result=EXIT_SUCCESS;
    try{
        Renderer renderer;SoundSystem sounds;const std::uint32_t seed=configuredSeed(argc,argv);World world(seed);
        glm::vec3 spawnPosition(0.5f,static_cast<float>(world.terrainHeight(0,0)+2),0.5f);const bool loadedSave=world.loadWorld("voxel_world.vxw",spawnPosition);
        std::cout<<(loadedSave?"Loading saved survival world...":"Preparing new survival world...")<<" seed="<<seed<<std::endl;
        world.generate(7,spawnPosition);Player player(spawnPosition);Inventory inventory;inventory.load("voxel_inventory.vxi",seed);SurvivalWorld survival(seed);
        GameState state{&player,&inventory};glfwSetWindowUserPointer(window,&state);glfwSetFramebufferSizeCallback(window,framebufferSizeCallback);glfwSetCursorPosCallback(window,mouseCallback);glfwSetScrollCallback(window,scrollCallback);glfwSetKeyCallback(window,keyCallback);glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);printControls();

        double previousTime=glfwGetTime(),fpsTimer=previousTime,lastSaveTime=previousTime;int frameCounter=0;float displayedFps=0,worldTime=35.0f,footstepTimer=0;
        bool rightWasDown=false,f3WasDown=false,cWasDown=false,rWasDown=false,vWasDown=false,inWater=false,showDebug=true,showCrafting=false;
        std::size_t recipeIndex=0;bool hasBreakingTarget=false;glm::ivec3 breakingTarget(0);float breakProgress=0;

        while(!glfwWindowShouldClose(window)){
            const double now=glfwGetTime();const float dt=std::min(static_cast<float>(now-previousTime),0.05f);previousTime=now;worldTime+=dt;++frameCounter;
            if(now-fpsTimer>=0.5){displayedFps=static_cast<float>(frameCounter/(now-fpsTimer));frameCounter=0;fpsTimer=now;}glfwPollEvents();
            for(int i=0;i<9;++i)if(glfwGetKey(window,GLFW_KEY_1+i)==GLFW_PRESS)inventory.selectSlot(i);
            const bool f3=glfwGetKey(window,GLFW_KEY_F3)==GLFW_PRESS;if(f3&&!f3WasDown)showDebug=!showDebug;f3WasDown=f3;
            const bool cKey=glfwGetKey(window,GLFW_KEY_C)==GLFW_PRESS;if(cKey&&!cWasDown)showCrafting=!showCrafting;cWasDown=cKey;
            const bool rKey=glfwGetKey(window,GLFW_KEY_R)==GLFW_PRESS;if(showCrafting&&rKey&&!rWasDown)recipeIndex=(recipeIndex+1)%inventory.recipes().size();rWasDown=rKey;
            const bool nearTable=world.hasBlockNear(player.position(),Block::CraftingTable,4.0f);
            const bool vKey=glfwGetKey(window,GLFW_KEY_V)==GLFW_PRESS;if(showCrafting&&vKey&&!vWasDown&&inventory.craft(recipeIndex,nearTable))sounds.playPlace();vWasDown=vKey;

            world.updateStreaming(player.cameraPosition(),2);const float healthBefore=player.health();
            if(state.mouseCaptured){
                player.update(dt,window,world);const bool leftDown=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;const bool rightDown=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;RayHit hit;
                if(leftDown&&!player.isDead()&&world.raycast(player.cameraPosition(),player.lookDirection(),6.0f,hit)){
                    if(!hasBreakingTarget||hit.block!=breakingTarget){breakingTarget=hit.block;breakProgress=0;hasBreakingTarget=true;}
                    const Block block=world.getBlock(hit.block.x,hit.block.y,hit.block.z);const float duration=std::max(0.06f,blockHardness(block)/toolBreakMultiplier(inventory.selectedItem(),block));breakProgress+=dt/duration;
                    if(breakProgress>=1.0f){world.setBlock(hit.block.x,hit.block.y,hit.block.z,Block::Air);renderer.spawnBreakParticles(hit.block,block);survival.spawnDrop(glm::vec3(hit.block)+glm::vec3(0.5f),blockToItem(block));sounds.playBreak();breakProgress=0;hasBreakingTarget=false;}
                }else{breakProgress=0;hasBreakingTarget=false;}
                if(rightDown&&!rightWasDown&&!player.isDead()&&world.raycast(player.cameraPosition(),player.lookDirection(),6.0f,hit)){
                    const Item selected=inventory.selectedItem();const Block block=itemToBlock(selected);const Block destination=world.getBlock(hit.adjacent.x,hit.adjacent.y,hit.adjacent.z);
                    const bool obstructs=isSolid(block)&&world.blockIntersectsAabb(hit.adjacent,player.aabbMinimum(),player.aabbMaximum());
                    if(block!=Block::Air&&inventory.count(selected)>0&&(destination==Block::Air||destination==Block::Water)&&!obstructs){world.setBlock(hit.adjacent.x,hit.adjacent.y,hit.adjacent.z,block);inventory.remove(selected);renderer.spawnBreakParticles(hit.adjacent,block);sounds.playPlace();}
                }rightWasDown=rightDown;
            }else{breakProgress=0;hasBreakingTarget=false;rightWasDown=false;}

            survival.update(dt,world,player,inventory,daylightAt(worldTime));if(player.health()<healthBefore)sounds.playDamage();
            const bool nowInWater=world.getBlock(static_cast<int>(std::floor(player.position().x)),static_cast<int>(std::floor(player.position().y+0.2f)),static_cast<int>(std::floor(player.position().z)))==Block::Water;
            if(nowInWater&&!inWater)sounds.playSplash();inWater=nowInWater;
            const bool walking=state.mouseCaptured&&player.isGrounded()&&!nowInWater&&(glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS||glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS||glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS||glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS);
            footstepTimer-=dt;if(walking&&footstepTimer<=0){sounds.playFootstep();footstepTimer=0.42f;}if(!walking)footstepTimer=std::min(footstepTimer,0.08f);renderer.updateParticles(dt);
            if(now-lastSaveTime>=12.0&&!player.isDead()){world.saveWorld("voxel_world.vxw",player.position());inventory.save("voxel_inventory.vxi",seed);lastSaveTime=now;}

            int width=0,height=0;glfwGetFramebufferSize(window,&width,&height);if(width<=0||height<=0)continue;glViewport(0,0,width,height);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            const glm::mat4 projection=glm::perspective(glm::radians(74.0f),static_cast<float>(width)/height,0.08f,230.0f),view=player.viewMatrix();renderer.renderSky(view,projection,worldTime);renderer.renderWorld(world,view,projection,player.cameraPosition(),worldTime);renderer.renderEntities(survival.renderCuboids(),view,projection,worldTime);renderer.renderParticles(view,projection);
            std::string debugText;if(showDebug){const glm::vec3 p=player.position();std::ostringstream debug;debug<<std::fixed<<std::setprecision(1)<<"FPS "<<displayedFps<<'\n'<<"XYZ "<<p.x<<" / "<<p.y<<" / "<<p.z<<'\n'<<"CHUNK "<<floorChunk(p.x)<<" / "<<floorChunk(p.z)<<'\n'<<"RENDER "<<world.renderDistance()<<'\n'<<"HEALTH "<<player.health();debugText=debug.str();}
            std::string craftingText;if(player.isDead())craftingText="YOU DIED  RESPAWNING";else if(showCrafting)craftingText=inventory.recipeStatus(recipeIndex,nearTable);renderer.renderHud(width,height,inventory,player.health(),player.hurtFlash(),breakProgress,debugText,craftingText);glfwSwapBuffers(window);
        }
        if(!player.isDead())world.saveWorld("voxel_world.vxw",player.position());inventory.save("voxel_inventory.vxi",seed);
    }catch(const std::exception& error){std::cerr<<"Fatal error: "<<error.what()<<'\n';result=EXIT_FAILURE;}
    glfwDestroyWindow(window);glfwTerminate();return result;
}

