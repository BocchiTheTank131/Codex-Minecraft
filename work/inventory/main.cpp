#include "Farming.h"
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
struct State{Player*player=nullptr;Inventory*inventory=nullptr;bool captured=true,first=true;double x=0,y=0;};
void resize(GLFWwindow*,int w,int h){glViewport(0,0,w,h);}void mouse(GLFWwindow*w,double x,double y){auto*s=(State*)glfwGetWindowUserPointer(w);if(!s||!s->captured)return;if(s->first){s->x=x;s->y=y;s->first=false;return;}s->player->addMouseMovement(x-s->x,s->y-y);s->x=x;s->y=y;}
void scroll(GLFWwindow*w,double,double y){auto*s=(State*)glfwGetWindowUserPointer(w);if(s&&s->captured&&y)s->inventory->cycleSlot(y>0?-1:1);}int chunk(float v){return(int)std::floor(v/CHUNK_SIZE);}
std::uint32_t seedFrom(int argc,char**argv){std::uint32_t seed=20260917;{std::ifstream f("world_seed.txt");std::uint64_t n;if(f>>n)seed=(std::uint32_t)n;}for(int i=1;i<argc;++i)try{std::string a=argv[i];if(a.rfind("--seed=",0)==0)seed=(std::uint32_t)std::stoull(a.substr(7));else if(a=="--seed"&&i+1<argc)seed=(std::uint32_t)std::stoull(argv[++i]);}catch(...){ }std::ofstream("world_seed.txt")<<seed<<'\n';return seed;}
float daylight(float t){float a=t/210.f*glm::two_pi<float>()+.35f;return glm::smoothstep(-.13f,.17f,std::sin(a));}
void capture(GLFWwindow*w,State&s,bool yes){s.captured=yes;s.first=true;glfwSetInputMode(w,GLFW_CURSOR,yes?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);}
bool ore(Block b){return b==Block::CoalOre||b==Block::CopperOre||b==Block::IronOre||b==Block::GoldOre||b==Block::DiamondOre;}
int oreXp(Block b){return b==Block::DiamondOre?7:(b==Block::GoldOre?5:(b==Block::IronOre?3:2));}
int inventoryHit(double mx,double my,int w,int h,bool table){float px=w*.5f-325,py=h*.5f-280,s=44,g=4,start=px+100;auto in=[&](float x,float y){return mx>=x&&mx<x+s&&my>=y&&my<y+s;};if(in(px+330,py+125))return 100;int n=table?3:2;for(int y=0;y<n;++y)for(int x=0;x<n;++x)if(in(px+70+x*(s+g),py+80+y*(s+g)))return 200+y*3+x;for(int y=0;y<3;++y)for(int x=0;x<9;++x)if(in(start+x*(s+g),py+300+y*(s+g)))return 9+y*9+x;for(int x=0;x<9;++x)if(in(start+x*(s+g),py+470))return x;return -1;}
}
int main(int argc,char**argv){
 if(!glfwInit())return EXIT_FAILURE;glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);GLFWwindow*w=glfwCreateWindow(1280,720,"Voxel Frontier: Survival",nullptr,nullptr);if(!w){glfwTerminate();return EXIT_FAILURE;}glfwMakeContextCurrent(w);glfwSwapInterval(1);if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){glfwTerminate();return EXIT_FAILURE;}glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
 int result=EXIT_SUCCESS;try{
  Renderer renderer;SoundSystem sounds;std::uint32_t seed=seedFrom(argc,argv);World world(seed);glm::vec3 spawn(.5f,0.f,.5f);bool loadedWorld=world.loadWorld("voxel_world.vxw",spawn);glm::vec3 safeSpawn=world.findSafeSpawnNear(0,0);float spawnDx=spawn.x-safeSpawn.x,spawnDz=spawn.z-safeSpawn.z;bool legacyBadSpawn=loadedWorld&&spawnDx*spawnDx+spawnDz*spawnDz<256.f&&spawn.y<safeSpawn.y-1.f;if(!loadedWorld||legacyBadSpawn)spawn=safeSpawn;world.generate(7,spawn);Player player(spawn);player.load("voxel_player.vps",seed);Inventory inv;inv.load("voxel_inventory.vxi",seed);SurvivalWorld survival(seed);FarmingSystem farming;State state{&player,&inv};glfwSetWindowUserPointer(w,&state);glfwSetFramebufferSizeCallback(w,resize);glfwSetCursorPosCallback(w,mouse);glfwSetScrollCallback(w,scroll);capture(w,state,true);
  std::cout<<"WASD move, Ctrl sprint, Shift sneak/swim down, Space jump/swim, E inventory, G fullbright, F3 debug, 1-9 hotbar\n";
  double prev=glfwGetTime(),fpsAt=prev,saved=prev;int frames=0,dragHit=-1;float fps=0,time=35,stepTimer=0,breakProgress=0;glm::ivec3 breakTarget(0);bool hasTarget=false,showDebug=true,inventoryOpen=false,fullbright=false,leftWas=false,rightWas=false,eWas=false,gWas=false,f3Was=false,escWas=false,wasWater=false;
  while(!glfwWindowShouldClose(w)){
   double now=glfwGetTime();float dt=std::min((float)(now-prev),.05f);prev=now;time+=dt;++frames;if(now-fpsAt>=.5){fps=(float)(frames/(now-fpsAt));frames=0;fpsAt=now;}glfwPollEvents();
   bool e=glfwGetKey(w,GLFW_KEY_E)==GLFW_PRESS;if(e&&!eWas){inventoryOpen=!inventoryOpen;capture(w,state,!inventoryOpen);}eWas=e;bool esc=glfwGetKey(w,GLFW_KEY_ESCAPE)==GLFW_PRESS;if(esc&&!escWas){if(inventoryOpen){inventoryOpen=false;capture(w,state,true);}else capture(w,state,!state.captured);}escWas=esc;
   bool g=glfwGetKey(w,GLFW_KEY_G)==GLFW_PRESS;if(g&&!gWas)fullbright=!fullbright;gWas=g;bool f3=glfwGetKey(w,GLFW_KEY_F3)==GLFW_PRESS;if(f3&&!f3Was)showDebug=!showDebug;f3Was=f3;
   if(!inventoryOpen)for(int i=0;i<9;++i)if(glfwGetKey(w,GLFW_KEY_1+i)==GLFW_PRESS)inv.selectSlot(i);
   world.updateStreaming(player.cameraPosition(),2);float oldHealth=player.health();if(state.captured&&!inventoryOpen)player.update(dt,w,world);farming.update(dt,world,player.position());bool table=world.hasBlockNear(player.position(),Block::CraftingTable,4.f);
   bool left=glfwGetMouseButton(w,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS,right=glfwGetMouseButton(w,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
   if(inventoryOpen){int fw,fh,ww,wh;glfwGetFramebufferSize(w,&fw,&fh);glfwGetWindowSize(w,&ww,&wh);double mx,my;glfwGetCursorPos(w,&mx,&my);mx*=ww?((double)fw/ww):1;my*=wh?((double)fh/wh):1;int hit=inventoryHit(mx,my,fw,fh,table);bool edge=(left&&!leftWas)||(right&&!rightWas);bool shift=glfwGetKey(w,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS||glfwGetKey(w,GLFW_KEY_RIGHT_SHIFT)==GLFW_PRESS;if(edge){bool handled=inv.handleInventoryClick(mx,my,fw,fh,right&&!rightWas,table,shift);dragHit=hit;if(!handled&&!inv.cursorStack().empty()){ItemStack d=inv.takeCursor(right&&!rightWas);survival.spawnDrop(player.cameraPosition()+player.lookDirection()*1.1f,d.item,d.count,d.durability);}}else if(right&&hit>=0&&hit!=dragHit){inv.handleInventoryClick(mx,my,fw,fh,true,table,false);dragHit=hit;}if(!right)dragHit=-1;breakProgress=0;hasTarget=false;   }else if(state.captured&&!player.isDead()){
    RayHit hit;bool hasHit=world.raycast(player.cameraPosition(),player.lookDirection(),6.f,hit);
    if(left&&!leftWas&&survival.attackAnimal(player.cameraPosition(),player.lookDirection(),inv.selectedItem())){if(isTool(inv.selectedItem()))inv.damageSelectedTool();sounds.playDamage();hasHit=false;}
    if(left&&hasHit){if(!hasTarget||hit.block!=breakTarget){breakTarget=hit.block;breakProgress=0;hasTarget=true;}Block b=world.getBlock(hit.block.x,hit.block.y,hit.block.z);float duration=std::max(.06f,blockHardness(b)/toolBreakMultiplier(inv.selectedItem(),b));breakProgress+=dt/duration;if(b==Block::Crop0||b==Block::Crop1||b==Block::Crop2)breakProgress=1;if(b==Block::Crop3)breakProgress=1;
     if(breakProgress>=1){world.setBlock(hit.block.x,hit.block.y,hit.block.z,Block::Air);renderer.spawnBreakParticles(hit.block,b);glm::vec3 p=glm::vec3(hit.block)+glm::vec3(.5f);if(b==Block::Crop3){survival.spawnDrop(p,Item::Wheat,1+(hit.block.x&1));survival.spawnDrop(p,Item::Seeds,1+std::abs(hit.block.z%3));survival.spawnExperience(p,2);}else if(isCrop(b))survival.spawnDrop(p,Item::Seeds,1);else{Item drop=b==Block::Farmland?Item::Dirt:blockToItem(b);survival.spawnDrop(p,drop);}if(ore(b))survival.spawnExperience(p,oreXp(b));if(isTool(inv.selectedItem()))inv.damageSelectedTool();sounds.playBreak();breakProgress=0;hasTarget=false;}
    }else{breakProgress=0;hasTarget=false;}
    if(right&&!rightWas){Item held=inv.selectedItem();bool used=false;if((held==Item::Wheat||held==Item::Seeds||held==Item::RawMeat)&&survival.feedAnimal(player.cameraPosition(),player.lookDirection(),held)){inv.consumeSelected();used=true;sounds.playPlace();}
     if(!used&&isFood(held)&&player.hunger()<20){player.eat(foodValue(held));inv.consumeSelected();used=true;sounds.playPlace();}
     if(!used&&hasHit){Block target=world.getBlock(hit.block.x,hit.block.y,hit.block.z);if(held==Item::Seeds&&target==Block::Farmland&&world.getBlock(hit.adjacent.x,hit.adjacent.y,hit.adjacent.z)==Block::Air){farming.plant(world,hit.adjacent);inv.consumeSelected();used=true;}
      else if((held==Item::WoodShovel||held==Item::StoneShovel||held==Item::IronShovel)&&(target==Block::Dirt||target==Block::Grass)&&hit.normal.y>0){world.setBlock(hit.block.x,hit.block.y,hit.block.z,Block::Farmland);inv.damageSelectedTool();used=true;}
      else{Block b=itemToBlock(held),dest=world.getBlock(hit.adjacent.x,hit.adjacent.y,hit.adjacent.z);bool obstruct=isSolid(b)&&world.blockIntersectsAabb(hit.adjacent,player.aabbMinimum(),player.aabbMaximum());if(b!=Block::Air&&(dest==Block::Air||dest==Block::Water)&&!obstruct){world.setBlock(hit.adjacent.x,hit.adjacent.y,hit.adjacent.z,b);inv.consumeSelected();renderer.spawnBreakParticles(hit.adjacent,b);used=true;}}if(used)sounds.playPlace();}
    }
   }
   leftWas=left;rightWas=right;survival.update(dt,world,player,inv,daylight(time));if(player.health()<oldHealth)sounds.playDamage();bool water=player.isSwimming();if(water&&!wasWater)sounds.playSplash();wasWater=water;bool walking=state.captured&&player.isGrounded()&&(glfwGetKey(w,GLFW_KEY_W)==GLFW_PRESS||glfwGetKey(w,GLFW_KEY_A)==GLFW_PRESS||glfwGetKey(w,GLFW_KEY_S)==GLFW_PRESS||glfwGetKey(w,GLFW_KEY_D)==GLFW_PRESS);stepTimer-=dt;if(walking&&stepTimer<=0){sounds.playFootstep();stepTimer=player.isSprinting()?.28f:.42f;}renderer.updateParticles(dt);
   if(now-saved>=12&&!player.isDead()){world.saveWorld("voxel_world.vxw",player.position());inv.save("voxel_inventory.vxi",seed);player.save("voxel_player.vps",seed);saved=now;}
   int W,H;glfwGetFramebufferSize(w,&W,&H);if(W<=0||H<=0)continue;glViewport(0,0,W,H);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glm::mat4 proj=glm::perspective(glm::radians(74.f),(float)W/H,.08f,230.f),view=player.viewMatrix();renderer.renderSky(view,proj,time);renderer.renderWorld(world,view,proj,player.cameraPosition(),time,fullbright);renderer.renderEntities(survival.renderCuboids(),view,proj,time);renderer.renderParticles(view,proj);
   std::string debug;if(showDebug){auto p=player.position();std::ostringstream s;s<<std::fixed<<std::setprecision(1)<<"FPS "<<fps<<'\n'<<"XYZ "<<p.x<<" / "<<p.y<<" / "<<p.z<<'\n'<<"CHUNK "<<chunk(p.x)<<" / "<<chunk(p.z)<<'\n'<<"RENDER "<<world.renderDistance()<<'\n'<<"HUNGER "<<player.hunger()<<" XP "<<player.experience()<<(fullbright?" FULLBRIGHT":"");debug=s.str();}std::string status=player.isDead()?"YOU DIED  RESPAWNING":"";renderer.renderHud(W,H,inv,player.health(),player.hurtFlash(),breakProgress,debug,status);renderer.renderSurvivalUi(W,H,inv,player.hunger(),player.xpProgress(),player.xpLevel(),fullbright);if(inventoryOpen){double mx,my;int ww,wh;glfwGetCursorPos(w,&mx,&my);glfwGetWindowSize(w,&ww,&wh);mx*=ww?((double)W/ww):1;my*=wh?((double)H/wh):1;renderer.renderInventory(W,H,inv,table,mx,my);}glfwSwapBuffers(w);
  }
  if(!player.isDead())world.saveWorld("voxel_world.vxw",player.position());inv.save("voxel_inventory.vxi",seed);player.save("voxel_player.vps",seed);
 }catch(const std::exception&e){std::cerr<<"Fatal: "<<e.what()<<'\n';result=EXIT_FAILURE;}glfwDestroyWindow(w);glfwTerminate();return result;
}
